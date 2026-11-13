/*
 * claude-cluster for Android: after a start of the phone (or an update of the app), the watch service started again when the
 * notifications are on - they arrive without opening the app first
 */

package com.claudecluster.app;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;

public class BootReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(Context context, Intent intent) {
        SharedPreferences prefs = context.getSharedPreferences("claude-cluster", Context.MODE_PRIVATE);
        if (prefs.getString("url", "").isEmpty() || !Alerts.enabled(prefs)) return;
        try {
            context.startForegroundService(new Intent(context, WatchService.class));
        } catch (Exception e) {
            // not allowed now (Android): started when the app is opened
        }
    }
}
