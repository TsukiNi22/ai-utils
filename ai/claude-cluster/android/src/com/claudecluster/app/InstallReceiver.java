/*
 * claude-cluster for Android: the answer of the installer of an update (the confirmation of the user asked when needed)
 */

package com.claudecluster.app;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInstaller;
import android.widget.Toast;

public class InstallReceiver extends BroadcastReceiver {
    @Override
    @SuppressWarnings("deprecation") // getParcelableExtra(String, Class): Android 13+ only
    public void onReceive(Context context, Intent intent) {
        final int status = intent.getIntExtra(PackageInstaller.EXTRA_STATUS, PackageInstaller.STATUS_FAILURE);
        if (status == PackageInstaller.STATUS_PENDING_USER_ACTION) {
            final Intent confirm = (Intent) intent.getParcelableExtra(Intent.EXTRA_INTENT);
            if (confirm != null) {
                confirm.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                context.startActivity(confirm);
            }
        } else if (status != PackageInstaller.STATUS_SUCCESS) {
            final String message = intent.getStringExtra(PackageInstaller.EXTRA_STATUS_MESSAGE);
            Toast.makeText(context, "claude-cluster: update not installed" + (message == null ? "" : " (" + message + ")"), Toast.LENGTH_LONG).show();
        }
    }
}
