/*
 * claude-cluster for Android: while the app is not in front, the notifications of the computer followed by a long poll (the cookie
 * of its page; /api/notify) and shown: the end of an answer, a session waiting on a permission or a question, an error, a
 * reminder...; a tap opens its session. Without Google services (no push): this connection, kept by a foreground service, is the
 * channel; a computer without /api/notify (older): the sessions this phone sent the last prompt of, followed in its state
 */

package com.claudecluster.app;

import android.app.Notification;
import android.app.Service;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ServiceInfo;
import android.os.Build;
import android.os.IBinder;
import android.webkit.CookieManager;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.FileNotFoundException;
import java.nio.charset.StandardCharsets;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;

public class WatchService extends Service {
    private volatile boolean running = false;
    private Thread loop;
    private volatile boolean fresh = false;

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int id) {
        Alerts.channels(this);
        Notification quiet = Alerts.builder(this, "watch").setContentTitle("claude-cluster")
            .setContentText("following your sessions").setContentIntent(Alerts.open(this, null)).build();
        if (Build.VERSION.SDK_INT >= 34) startForeground(1, quiet, ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE);
        else startForeground(1, quiet);
        // started by the app going out of front: from now (what happened in front was seen); started again by Android or at the boot:
        // from the last notification handled (the ones of the last hour missed meanwhile)
        fresh = intent != null && intent.getBooleanExtra("fresh", false);
        if (!running) {
            running = true;
            loop = new Thread(this::watch);
            loop.start();
        }
        return START_STICKY;
    }

    @Override
    public void onDestroy() {
        running = false;
        if (loop != null) loop.interrupt();
        super.onDestroy();
    }

    private void watch() {
        SharedPreferences prefs = getSharedPreferences("claude-cluster", MODE_PRIVATE);
        long since = fresh ? -1 : prefs.getLong("push_seq", -1); // -1: from now
        boolean legacy = false;
        Map<String, String[]> before = null;
        long version = 0;
        int failures = 0;
        while (running) {
            try {
                String url = prefs.getString("active", prefs.getString("url", ""));
                String cookie = CookieManager.getInstance().getCookie(url);
                if (url.isEmpty() || cookie == null || !Alerts.enabled(prefs)) break;
                if (legacy) {
                    before = watchState(prefs, url, cookie, before, version);
                    version = stateVersion;
                    continue;
                }
                // the long poll: the notifications after `since`, answered at once when there are, else after 25 s
                JSONObject answer = new JSONObject(new String(Net.fetch(url + "/api/notify" + (since < 0 ? "" : "?since=" + since), Net.pinned(prefs), cookie, 40000), StandardCharsets.UTF_8));
                failures = 0;
                JSONArray events = answer.optJSONArray("events");
                Set<String> kinds = Alerts.kinds(prefs);
                long recent = System.currentTimeMillis() / 1000 - 3600;
                for (int i = 0; events != null && i < events.length(); i++) {
                    JSONObject event = events.getJSONObject(i);
                    if (event.optLong("time") >= recent && kinds.contains(event.optString("kind"))) Alerts.post(this, event);
                }
                since = answer.optLong("seq", 0);
                prefs.edit().putLong("push_seq", since).apply();
                if (!answer.optBoolean("enabled", true)) Thread.sleep(30000); // the computer sends none now ([ui] notify_app): asked again later
            } catch (SecurityException e) {
                break; // out of the computer (kicked): the app connects again when opened
            } catch (FileNotFoundException e) {
                legacy = true; // an older computer: no /api/notify
            } catch (InterruptedException e) {
                break;
            } catch (Exception e) {
                try {
                    Thread.sleep(Math.min(60000, 5000L * ++failures)); // the computer not reached: asked again, less and less often
                } catch (InterruptedException stop) {
                    break;
                }
            }
        }
        stopSelf();
    }

    private long stateVersion = 0;

    // an older computer: the long poll of the page (/api/state waits for a change), the transitions of the sessions of this phone
    private Map<String, String[]> watchState(SharedPreferences prefs, String url, String cookie, Map<String, String[]> before, long version) throws Exception {
        JSONObject state = new JSONObject(new String(Net.fetch(url + "/api/state?v=" + version, Net.pinned(prefs), cookie, 30000), StandardCharsets.UTF_8));
        stateVersion = state.optLong("version", 0);
        String device = state.optString("device", "");
        JSONArray sessions = state.optJSONArray("sessions");
        Map<String, String[]> now = new HashMap<>();
        for (int i = 0; sessions != null && i < sessions.length(); i++) {
            JSONObject s = sessions.getJSONObject(i);
            String id = s.optString("id");
            JSONArray recent = s.optJSONArray("recent");
            String last = recent != null && recent.length() > 0 ? recent.getJSONObject(recent.length() - 1).optString("text") : "";
            JSONArray permissions = s.optJSONArray("permissions");
            now.put(id, new String[] {s.optString("state"), String.valueOf(permissions == null ? 0 : permissions.length()),
                "g".equals(id) ? "global" : s.optString("name", id), last, String.valueOf(("web:" + device).equals(s.optString("origin")))});
        }
        if (before != null) {
            Set<String> kinds = Alerts.kinds(prefs);
            for (Map.Entry<String, String[]> entry : now.entrySet()) {
                String[] s = entry.getValue();
                String[] b = before.get(entry.getKey());
                if (b == null || !"true".equals(s[4])) continue;
                if (Integer.parseInt(s[1]) > Integer.parseInt(b[1])) legacyPost(kinds, "permission", entry.getKey(), s[2] + " waits for you", "a permission or a question");
                else if ("working".equals(b[0]) && "idle".equals(s[0])) legacyPost(kinds, "done", entry.getKey(), s[2] + " finished", s[3]);
                else if (!"error".equals(b[0]) && "error".equals(s[0])) legacyPost(kinds, "error", entry.getKey(), s[2] + ": error", s[3]);
            }
        }
        return now;
    }

    private void legacyPost(Set<String> kinds, String kind, String session, String title, String body) throws Exception {
        if (!kinds.contains(kind)) return;
        Alerts.post(this, new JSONObject().put("seq", System.currentTimeMillis() % Integer.MAX_VALUE).put("time", System.currentTimeMillis() / 1000).put("kind", kind)
            .put("session", session).put("title", title).put("body", body));
    }
}
