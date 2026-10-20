/*
 * claude-cluster for Android: the notifications of the computer shown on the phone - their channels (one importance per family),
 * the choices of this phone (on / off, which kinds), one group per session
 */

package com.claudecluster.app;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.drawable.Icon;

import org.json.JSONObject;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

final class Alerts {
    private Alerts() {}

    // the kinds of the computer (its notify_* settings); the ones of a phone by default: not the connections of devices nor the readiness
    static final String DEFAULT_KINDS = "global,done,permission,question,error,limit,task,reminder,message";

    // the logo of claude-cluster: the status bar icon (a flat white mask), the coloured logo large, its blue as the accent
    static final int ACCENT = 0xFF6D8EF0;

    static Notification.Builder builder(Context context, String channel) {
        return new Notification.Builder(context, channel).setSmallIcon(R.drawable.ic_stat).setLargeIcon(Icon.createWithResource(context, R.drawable.ic_large)).setColor(ACCENT);
    }

    static boolean enabled(SharedPreferences prefs) {
        return prefs.getBoolean("notify_on", true);
    }

    static Set<String> kinds(SharedPreferences prefs) {
        return new HashSet<>(Arrays.asList(prefs.getString("notify_kinds", DEFAULT_KINDS).split(",")));
    }

    // one channel per family: the user tunes each in the Android settings of the app (sound, vibration, importance)
    static String channelOf(String kind) {
        switch (kind) {
            case "permission":
            case "question":
                return "waiting";
            case "error":
            case "limit":
                return "problems";
            case "reminder":
            case "message":
                return "reminders";
            case "ready":
            case "web":
                return "info";
            default:
                return "sessions";
        }
    }

    static void channels(Context context) {
        NotificationManager manager = context.getSystemService(NotificationManager.class);
        manager.createNotificationChannel(new NotificationChannel("watch", "Connected", NotificationManager.IMPORTANCE_MIN));
        manager.createNotificationChannel(channel("waiting", "Waiting for you", "A permission or a question of an agent", NotificationManager.IMPORTANCE_HIGH));
        manager.createNotificationChannel(channel("sessions", "Sessions", "A session or the global session finished, the task queue done", NotificationManager.IMPORTANCE_HIGH));
        manager.createNotificationChannel(channel("problems", "Errors and limits", "An error, a crash, a limit close or reached", NotificationManager.IMPORTANCE_HIGH));
        manager.createNotificationChannel(channel("reminders", "Reminders and messages", "Schedules and the notify tool of the agents", NotificationManager.IMPORTANCE_HIGH));
        manager.createNotificationChannel(channel("info", "Information", "claude-cluster ready, the devices of the web access", NotificationManager.IMPORTANCE_LOW));
    }

    private static NotificationChannel channel(String id, String name, String description, int importance) {
        NotificationChannel channel = new NotificationChannel(id, name, importance);
        channel.setDescription(description);
        return channel;
    }

    static PendingIntent open(Context context, String session) {
        Intent intent = new Intent(context, MainActivity.class).setFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP | Intent.FLAG_ACTIVITY_NEW_TASK);
        if (session != null && !session.isEmpty()) intent.putExtra("session", session);
        return PendingIntent.getActivity(context, session == null || session.isEmpty() ? 0 : session.hashCode(), intent, PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
    }

    // an event of the computer {seq, time, kind, session, title, body, urgent}: shown in the group of its session (a tap opens it)
    static void post(Context context, JSONObject event) {
        String kind = event.optString("kind", "message");
        String session = event.optString("session");
        String title = event.optString("title", "claude-cluster");
        String body = event.optString("body");
        String channel = channelOf(kind);
        String group = "cc-" + (session.isEmpty() ? "core" : session);
        PendingIntent tap = open(context, session);
        NotificationManager manager = context.getSystemService(NotificationManager.class);
        Notification.Builder builder = builder(context, channel).setContentTitle(title)
            .setContentText(body.length() > 180 ? body.substring(0, 180) : body).setStyle(new Notification.BigTextStyle().bigText(body))
            .setAutoCancel(true).setContentIntent(tap).setGroup(group).setWhen(event.optLong("time", System.currentTimeMillis() / 1000) * 1000)
            .setCategory(kind.equals("reminder") ? Notification.CATEGORY_REMINDER : kind.equals("error") ? Notification.CATEGORY_ERROR : Notification.CATEGORY_MESSAGE);
        manager.notify(group, (int) event.optLong("seq"), builder.build());
        // the group header: the session (the title before its " · "), the children alert, not the header
        String name = title.contains(" · ") ? title.substring(0, title.indexOf(" · ")) : title;
        Notification summary = builder(context, channel).setContentTitle(name).setGroup(group)
            .setGroupSummary(true).setGroupAlertBehavior(Notification.GROUP_ALERT_CHILDREN).setAutoCancel(true).setContentIntent(tap).build();
        manager.notify(group, 0, summary);
    }
}
