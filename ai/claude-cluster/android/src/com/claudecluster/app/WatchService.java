/*
 * claude-cluster for Android: while the app is not in front, the state of claude-cluster followed (the cookie of its page) and
 * the sessions this phone sent the last prompt of notified: finished, waiting for you, an error; a tap opens its session
 */

package com.claudecluster.app;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ServiceInfo;
import android.os.Build;
import android.os.IBinder;
import android.webkit.CookieManager;

import org.json.JSONArray;
import org.json.JSONObject;

import java.nio.charset.StandardCharsets;
import java.util.HashMap;
import java.util.Map;

public class WatchService extends Service {
    private volatile boolean running = false;
    private Thread loop;

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int id) {
        NotificationManager manager = getSystemService(NotificationManager.class);
        if (Build.VERSION.SDK_INT >= 26) {
            manager.createNotificationChannel(new NotificationChannel("watch", "Connected", NotificationManager.IMPORTANCE_MIN));
            manager.createNotificationChannel(new NotificationChannel("sessions", "Sessions", NotificationManager.IMPORTANCE_HIGH));
        }
        Notification.Builder builder = Build.VERSION.SDK_INT >= 26 ? new Notification.Builder(this, "watch") : new Notification.Builder(this);
        Notification quiet = builder.setSmallIcon(R.mipmap.ic_launcher).setContentTitle("claude-cluster").setContentText("following your sessions")
            .setContentIntent(open(null)).build();
        if (Build.VERSION.SDK_INT >= 34) startForeground(1, quiet, ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE);
        else startForeground(1, quiet);
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

    private PendingIntent open(String session) {
        Intent intent = new Intent(this, MainActivity.class).setFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP | Intent.FLAG_ACTIVITY_NEW_TASK);
        if (session != null) intent.putExtra("session", session);
        return PendingIntent.getActivity(this, session == null ? 0 : session.hashCode(), intent, PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
    }

    private void watch() {
        // the long poll of the page (/api/state waits for a change), the transitions of the sessions of this phone
        SharedPreferences prefs = getSharedPreferences("claude-cluster", MODE_PRIVATE);
        Map<String, String[]> before = null;
        long version = 0;
        while (running) {
            try {
                String url = prefs.getString("active", prefs.getString("url", ""));
                String cookie = CookieManager.getInstance().getCookie(url);
                if (url.isEmpty() || cookie == null) break;
                JSONObject state = new JSONObject(new String(Net.fetch(url + "/api/state?v=" + version, Net.pinned(prefs), cookie, 30000), StandardCharsets.UTF_8));
                version = state.optLong("version", 0);
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
                    for (Map.Entry<String, String[]> entry : now.entrySet()) {
                        String[] s = entry.getValue();
                        String[] b = before.get(entry.getKey());
                        if (b == null || !"true".equals(s[4])) continue;
                        if (Integer.parseInt(s[1]) > Integer.parseInt(b[1])) post(entry.getKey(), s[2] + " waits for you", "a permission or a question");
                        else if ("working".equals(b[0]) && "idle".equals(s[0])) post(entry.getKey(), s[2] + " finished", s[3]);
                        else if (!"error".equals(b[0]) && "error".equals(s[0])) post(entry.getKey(), s[2] + ": error", s[3]);
                    }
                }
                before = now;
            } catch (SecurityException e) {
                break; // out of the computer (kicked): the app connects again when opened
            } catch (Exception e) {
                try {
                    Thread.sleep(5000);
                } catch (InterruptedException stop) {
                    break;
                }
            }
        }
        stopSelf();
    }

    private void post(String session, String title, String body) {
        Notification.Builder builder = Build.VERSION.SDK_INT >= 26 ? new Notification.Builder(this, "sessions") : new Notification.Builder(this);
        Notification notification = builder.setSmallIcon(R.mipmap.ic_launcher).setContentTitle(title)
            .setContentText(body.length() > 180 ? body.substring(0, 180) : body).setAutoCancel(true).setContentIntent(open(session)).build();
        getSystemService(NotificationManager.class).notify(session.hashCode(), notification);
    }
}
