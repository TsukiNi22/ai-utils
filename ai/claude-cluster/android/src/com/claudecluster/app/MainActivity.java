/*
 * claude-cluster for Android: the web page of claude-cluster in an app.
 * - the computer trusted once: its authority (/ca.crt) pinned after its fingerprint is shown, no warning after
 * - updates: a newer app served by the computer is offered (never an older one); one too old for it: told so
 * - the notifications of the page (a session finished, waiting for you) while the app is not in front
 * - the microphone of the page (its voice)
 */

package com.claudecluster.app;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageInstaller;
import android.content.res.Configuration;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.net.Uri;
import android.net.http.SslError;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.InputType;
import android.util.Base64;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.webkit.JavascriptInterface;
import android.webkit.PermissionRequest;
import android.webkit.ValueCallback;
import android.webkit.SslErrorHandler;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.io.ByteArrayInputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.cert.CertificateFactory;
import java.security.cert.X509Certificate;
import java.util.Locale;


public class MainActivity extends Activity {
    // the protocol of the app: claude-cluster refuses an app older than its app_min (it says so)
    static final int PROTOCOL = 1;

    private SharedPreferences prefs;
    private WebView web;
    private boolean front = false;
    private String base = "";       // the address open now: the LAN or the one from outside
    private ValueCallback<Uri[]> chooser = null;   // the file chooser of the page waiting for its answer (attachments)
    private WebChromeClient.FileChooserParams chooserParams = null;
    private Uri photo = null;                       // the file the camera app is writing (PhotoProvider)
    private PermissionRequest waitingMic = null;
    private final Handler ui = new Handler(Looper.getMainLooper());

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        prefs = getSharedPreferences("claude-cluster", MODE_PRIVATE);
        Alerts.channels(this);
        // Android 13+: asked at the first start; refused for good, the settings of Android (This device page: Allow notifications)
        if (Build.VERSION.SDK_INT >= 33 && checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED && !prefs.getBoolean("notify_asked", false)) {
            prefs.edit().putBoolean("notify_asked", true).apply();
            requestPermissions(new String[] {Manifest.permission.POST_NOTIFICATIONS}, 2);
        }
        final String url = prefs.getString("url", "");
        if (url.isEmpty()) showSetup("");
        else choose(url);
    }

    // the LAN when it answers, else the address from outside (tailscale / the router) told by claude-cluster
    private void choose(final String lan) {
        final String remote = prefs.getString("remote", "");
        if (remote.isEmpty() || remote.equals(lan)) {
            open(lan);
            return;
        }
        new Thread(() -> {
            boolean near;
            try {
                Net.fetch(lan + "/api/version", pinned(), null, 2500);
                near = true;
            } catch (Exception e) {
                near = false;
            }
            final boolean reached = near;
            ui.post(() -> {
                if (!reached) carry(lan, remote);
                open(reached ? lan : remote);
            });
        }).start();
    }

    // its device cookies (its id, its trust) given to the other address: the same phone there
    private void carry(String from, String to) {
        android.webkit.CookieManager cookies = android.webkit.CookieManager.getInstance();
        String all = cookies.getCookie(from);
        if (all == null) return;
        for (String part : all.split(";")) {
            String pair = part.trim();
            if (pair.startsWith("cc_dev=") || pair.startsWith("cc_trust="))
                cookies.setCookie(to, pair + "; Path=/; Max-Age=31536000; Secure; HttpOnly; SameSite=Strict");
        }
        cookies.flush();
    }

    @Override
    protected void onResume() {
        super.onResume();
        front = true;
        stopService(new Intent(this, WatchService.class));
        if (!base.isEmpty()) checkUpdate(base); // the computer updated while the app slept: asked again
        // back in front: the state asked again at once (a long poll of the page frozen in the background never holds it)
        if (web != null) web.evaluateJavascript("if (typeof refresh === 'function' && !document.getElementById('app').classList.contains('hidden')) refresh();", null);
    }

    @Override
    protected void onPause() {
        // out of front: the watch service follows the sessions of this phone (its notifications)
        super.onPause();
        front = false;
        android.webkit.CookieManager.getInstance().flush(); // on disk: the watch service started again by Android (its process gone) reads them
        if (!prefs.getString("url", "").isEmpty() && web != null && Alerts.enabled(prefs)) {
            try {
                startForegroundService(new Intent(this, WatchService.class).putExtra("fresh", true));
            } catch (Exception e) {
                // not allowed now (Android): the notifications while in front only
            }
        }
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        openSession(intent);
    }

    private void openSession(Intent intent) {
        // a notification tapped: its session shown in the page
        final String session = intent == null ? null : intent.getStringExtra("session");
        if (session == null || web == null) return;
        web.evaluateJavascript("typeof openSession === 'function' && openSession(" + org.json.JSONObject.quote(session) + ")", null);
    }

    @Override
    @SuppressWarnings("deprecation") // the back key: the page first
    public void onBackPressed() {
        if (web != null && web.canGoBack()) web.goBack();
        else super.onBackPressed();
    }

    // ---- the address of the computer: asked once (the one shown at the top of claude-cluster) ----
    private void showSetup(String error) {
        LinearLayout box = new LinearLayout(this);
        box.setOrientation(LinearLayout.VERTICAL);
        box.setPadding(56, 120, 56, 56);
        box.setBackgroundColor(Color.parseColor("#15171b"));
        TextView title = new TextView(this);
        title.setText("^_^  claude-cluster");
        title.setTextSize(26);
        title.setTextColor(Color.parseColor("#e6e7ea"));
        TextView help = new TextView(this);
        help.setText("The address shown at the top of claude-cluster on the computer (web https://…:8787). Its code is asked next, and the "
            + "computer approves this phone.");
        help.setTextColor(Color.parseColor("#9aa0ab"));
        help.setPadding(0, 24, 0, 24);
        EditText address = new EditText(this);
        address.setInputType(InputType.TYPE_TEXT_VARIATION_URI);
        address.setText(prefs.getString("url", "https://"));
        address.setTextColor(Color.parseColor("#e6e7ea"));
        address.setHint("https://192.168.1.12:8787");
        Button connect = new Button(this);
        connect.setText("Connect");
        TextView message = new TextView(this);
        message.setText(error);
        message.setTextColor(Color.parseColor("#f47174"));
        message.setPadding(0, 24, 0, 0);
        connect.setOnClickListener(v -> {
            String url = address.getText().toString().trim();
            while (url.endsWith("/")) url = url.substring(0, url.length() - 1);
            if (!url.startsWith("http")) url = "https://" + url;
            if (!url.equals(prefs.getString("url", ""))) prefs.edit().remove("ca").apply(); // another computer: trusted again
            prefs.edit().putString("url", url).apply();
            open(url);
        });
        box.addView(title);
        box.addView(help);
        box.addView(address);
        box.addView(connect);
        box.addView(message);
        setContentView(inset(box));
    }

    // ---- edge to edge: the page kept out of the status bar, the notch, the navigation bar and the keyboard, the bars of the page's theme ----
    private FrameLayout edge;

    private boolean night() {
        return (getResources().getConfiguration().uiMode & Configuration.UI_MODE_NIGHT_MASK) == Configuration.UI_MODE_NIGHT_YES;
    }

    // the colour of the bars (the frame shows under them) and the icons on it: the dark / light theme of the page (its --bg)
    private void themeBars() {
        if (edge == null) return;
        boolean dark = night();
        edge.setBackgroundColor(Color.parseColor(dark ? "#171715" : "#f6f6f4"));
        if (Build.VERSION.SDK_INT >= 30) {
            WindowInsetsController bars = getWindow().getInsetsController();
            int light = WindowInsetsController.APPEARANCE_LIGHT_STATUS_BARS | WindowInsetsController.APPEARANCE_LIGHT_NAVIGATION_BARS;
            if (bars != null) bars.setSystemBarsAppearance(dark ? 0 : light, light);
        }
    }

    @Override
    public void onConfigurationChanged(Configuration config) {
        super.onConfigurationChanged(config);
        themeBars();
    }

    private View inset(View content) {
        FrameLayout frame = new FrameLayout(this);
        edge = frame;
        frame.addView(content, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        if (Build.VERSION.SDK_INT >= 30) {
        themeBars();
            getWindow().setDecorFitsSystemWindows(false); // the same on every version: the insets come here, nothing is resized twice
            getWindow().setStatusBarColor(Color.TRANSPARENT);
            getWindow().setNavigationBarColor(Color.TRANSPARENT);
            frame.setOnApplyWindowInsetsListener((v, insets) -> {
                android.graphics.Insets room = insets.getInsets(WindowInsets.Type.systemBars() | WindowInsets.Type.displayCutout() | WindowInsets.Type.ime());
                v.setPadding(room.left, room.top, room.right, room.bottom);
                return WindowInsets.CONSUMED;
            });
            frame.requestApplyInsets();
        } else {
            frame.setFitsSystemWindows(true);
        }
        return frame;
    }

    // ---- the page ----
    @SuppressWarnings("SetJavaScriptEnabled")
    private void open(String url) {
        base = url;
        prefs.edit().putString("active", url).apply(); // the watch service follows the same address
        web = new WebView(this);
        web.setBackgroundColor(Color.parseColor("#15171b"));
        WebSettings settings = web.getSettings();
        settings.setJavaScriptEnabled(true);
        settings.setDomStorageEnabled(true);
        settings.setMediaPlaybackRequiresUserGesture(false);
        settings.setUserAgentString(settings.getUserAgentString() + " claude-cluster-app/" + BuildConfig.VERSION);
        web.addJavascriptInterface(new Bridge(), "ClusterApp");
        web.setWebViewClient(new WebViewClient() {
            @Override
            public boolean shouldOverrideUrlLoading(WebView view, android.webkit.WebResourceRequest request) {
                // the page of the computer only in the app (its bridge ClusterApp): any other link opens in the browser
                final android.net.Uri target = request.getUrl();
                final android.net.Uri home = android.net.Uri.parse(url);
                if (target.getHost() != null && target.getHost().equals(home.getHost()) && target.getPort() == home.getPort()) return false;
                try {
                    startActivity(new Intent(Intent.ACTION_VIEW, target));
                } catch (Exception e) {
                    // no browser: nothing
                }
                return true;
            }

            @Override
            public void onReceivedSslError(WebView view, SslErrorHandler handler, SslError error) {
                trust(url, handler, error);
            }

            @Override
            public void onReceivedError(WebView view, android.webkit.WebResourceRequest request, android.webkit.WebResourceError error) {
                if (!request.isForMainFrame()) return;
                // the LAN not reached: the address from outside when there is one
                final String lan = prefs.getString("url", "");
                final String remote = prefs.getString("remote", "");
                if (url.equals(lan) && !remote.isEmpty() && !remote.equals(lan)) {
                    carry(lan, remote);
                    open(remote);
                } else {
                    showSetup("Not reached: " + error.getDescription() + " (the same network, or [web] remote? claude-cluster web on?)");
                }
            }
        });
        web.setWebChromeClient(new WebChromeClient() {
            @Override
            public void onPermissionRequest(PermissionRequest request) {
                // the microphone of the page: asked to Android once
                if (checkSelfPermission(Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED) {
                    request.grant(request.getResources());
                } else {
                    waitingMic = request;
                    requestPermissions(new String[] {Manifest.permission.RECORD_AUDIO}, 1);
                }
            }

            @Override
            public boolean onShowFileChooser(WebView view, ValueCallback<Uri[]> callback, FileChooserParams params) {
                // the attachments of the page: the picker of Android (files, photos: access to the files picked only), + the camera
                if (chooser != null) chooser.onReceiveValue(null);
                chooser = callback;
                chooserParams = params;
                boolean wantsCamera = params.isCaptureEnabled() || String.join(",", params.getAcceptTypes()).contains("image") || params.getAcceptTypes().length == 0
                    || (params.getAcceptTypes().length == 1 && params.getAcceptTypes()[0].isEmpty());
                if (wantsCamera && checkSelfPermission(Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED && !prefs.getBoolean("camera_asked", false)) {
                    // asked once, with its reason: refused, the picker alone (files and photos of the gallery need no permission)
                    prefs.edit().putBoolean("camera_asked", true).apply();
                    new AlertDialog.Builder(MainActivity.this).setTitle("Camera")
                        .setMessage("To attach a photo or a video you take now, claude-cluster needs the camera. Files and photos already on the phone are picked with the Android picker, which needs no permission.")
                        .setPositiveButton("Allow the camera", (d, w) -> requestPermissions(new String[] {Manifest.permission.CAMERA}, 3))
                        .setNegativeButton("Not now", (d, w) -> launchChooser(false))
                        .setOnCancelListener(d -> launchChooser(false)).show();
                    return true;
                }
                launchChooser(wantsCamera && checkSelfPermission(Manifest.permission.CAMERA) == PackageManager.PERMISSION_GRANTED);
                return true;
            }
        });
        web.setLayoutParams(new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        setContentView(inset(web));
        web.loadUrl(url + "/");
        final Intent first = getIntent();
        web.postDelayed(() -> openSession(first), 2500); // opened by a notification: its session once the page is there
        checkUpdate(url);
    }

    // the picker of Android (the files / photos chosen), with the camera apps (a photo, a video) among its choices; the camera alone when the page asks for it
    private void launchChooser(boolean camera) {
        if (chooser == null) return;
        try {
            Intent pick = chooserParams.createIntent();
            java.util.List<Intent> cameras = new java.util.ArrayList<>();
            if (camera) {
                String stamp = String.valueOf(System.currentTimeMillis());
                photo = PhotoProvider.uriFor("photo-" + stamp + ".jpg");
                Intent shot = new Intent(android.provider.MediaStore.ACTION_IMAGE_CAPTURE);
                shot.putExtra(android.provider.MediaStore.EXTRA_OUTPUT, photo);
                shot.addFlags(Intent.FLAG_GRANT_WRITE_URI_PERMISSION | Intent.FLAG_GRANT_READ_URI_PERMISSION);
                shot.setClipData(android.content.ClipData.newRawUri("photo", photo));
                cameras.add(shot);
                cameras.add(new Intent(android.provider.MediaStore.ACTION_VIDEO_CAPTURE)); // its video: given back by its own URI
            }
            Intent open = chooserParams.isCaptureEnabled() && !cameras.isEmpty() ? cameras.remove(0) : Intent.createChooser(pick, "Attach");
            if (open != pick && !cameras.isEmpty() && !chooserParams.isCaptureEnabled()) open.putExtra(Intent.EXTRA_INITIAL_INTENTS, cameras.toArray(new Intent[0]));
            startActivityForResult(open, 4);
        } catch (Exception e) {
            chooser.onReceiveValue(null);
            chooser = null;
        }
    }

    @Override
    protected void onActivityResult(int code, int result, Intent data) {
        super.onActivityResult(code, result, data);
        if (code != 4 || chooser == null) return;
        Uri[] picked = result == RESULT_OK ? WebChromeClient.FileChooserParams.parseResult(result, data) : null;
        if (result == RESULT_OK && picked == null && photo != null && PhotoProvider.fileFor(this, String.valueOf(photo.getLastPathSegment())).length() > 0) picked = new Uri[] {photo};
        chooser.onReceiveValue(picked); // content:// URIs: read by the page, uploaded
        chooser = null;
        photo = null;
    }

    @Override
    public void onRequestPermissionsResult(int code, String[] permissions, int[] results) {
        if (code == 1 && waitingMic != null) {
            if (results.length > 0 && results[0] == PackageManager.PERMISSION_GRANTED) waitingMic.grant(waitingMic.getResources());
            else waitingMic.deny();
            waitingMic = null;
        }
        if (code == 3) launchChooser(results.length > 0 && results[0] == PackageManager.PERMISSION_GRANTED); // the camera allowed or not: the picker
    }

    // ---- the computer trusted once: its authority pinned ----
    private X509Certificate pinned() {
        return Net.pinned(prefs);
    }

    private static boolean signedBy(X509Certificate leaf, X509Certificate ca) {
        try {
            leaf.checkValidity();
            leaf.verify(ca.getPublicKey());
            return true;
        } catch (Exception e) {
            return false;
        }
    }

    private void trust(String url, SslErrorHandler handler, SslError error) {
        X509Certificate leaf = Build.VERSION.SDK_INT >= 29 ? error.getCertificate().getX509Certificate() : null;
        X509Certificate ca = pinned();
        if (leaf != null && ca != null) {
            if (signedBy(leaf, ca)) handler.proceed();
            else {
                handler.cancel();
                askAgain(url);
            }
            return;
        }
        // the first time: its authority read, its fingerprint shown, trusted on yes
        new Thread(() -> {
            X509Certificate authority = null;
            try {
                byte[] pem = Net.fetch(url + "/ca.crt", null, null, 60000, true); // the first time: anything, its fingerprint shown
                authority = (X509Certificate) CertificateFactory.getInstance("X.509").generateCertificate(new ByteArrayInputStream(pem));
            } catch (Exception e) {
                authority = null;
            }
            final X509Certificate found = authority;
            ui.post(() -> {
                if (found == null || leaf == null || !signedBy(leaf, found)) {
                    handler.cancel();
                    showSetup("This address does not answer as claude-cluster (its certificate): check it");
                    return;
                }
                new AlertDialog.Builder(this)
                    .setTitle("Trust this computer?")
                    .setMessage(found.getSubjectX500Principal().getName() + "\n\nSHA-256 " + fingerprint(found)
                        + "\n\nThe same as on the computer (its palette: Web access: address and code). Trusted: no warning, encrypted.")
                    .setPositiveButton("Trust", (d, w) -> {
                        try {
                            prefs.edit().putString("ca", Base64.encodeToString(found.getEncoded(), Base64.NO_WRAP)).apply();
                        } catch (Exception e) {
                            return;
                        }
                        handler.proceed();
                        checkUpdate(url);
                    })
                    .setNegativeButton("Cancel", (d, w) -> {
                        handler.cancel();
                        showSetup("Not trusted");
                    })
                    .setCancelable(false)
                    .show();
            });
        }).start();
    }

    private void askAgain(String url) {
        new AlertDialog.Builder(this)
            .setTitle("The computer changed")
            .setMessage("Its certificate is not signed by the one trusted before (another computer, or claude-cluster set up again). Trust it again?")
            .setPositiveButton("Trust again", (d, w) -> {
                prefs.edit().remove("ca").apply();
                open(url);
            })
            .setNegativeButton("Another address", (d, w) -> showSetup(""))
            .show();
    }

    private static String fingerprint(X509Certificate cert) {
        try {
            byte[] digest = MessageDigest.getInstance("SHA-256").digest(cert.getEncoded());
            StringBuilder out = new StringBuilder();
            for (int i = 0; i < digest.length; i++) out.append(i > 0 ? ":" : "").append(String.format(Locale.ROOT, "%02X", digest[i]));
            return out.toString();
        } catch (Exception e) {
            return "?";
        }
    }

    private byte[] fetch(String address, X509Certificate ca) throws Exception {
        return Net.fetch(address, ca, null, 60000);
    }

    // ---- updates: only a newer app; one too old for this claude-cluster: told so ----
    static int[] parse(String version) {
        String[] parts = version.split("[^0-9]+");
        int[] out = new int[3];
        for (int i = 0, j = 0; i < parts.length && j < 3; i++) if (!parts[i].isEmpty()) out[j++] = Integer.parseInt(parts[i]);
        return out;
    }

    static boolean newer(String a, String b) {
        int[] x = parse(a);
        int[] y = parse(b);
        for (int i = 0; i < 3; i++) if (x[i] != y[i]) return x[i] > y[i];
        return false;
    }

    private void checkUpdate(String url) {
        X509Certificate ca = pinned(); // none pinned: the authorities of the system (a real certificate), else nothing said
        new Thread(() -> {
            try {
                String json = new String(fetch(url + "/api/version", ca), StandardCharsets.UTF_8);
                org.json.JSONObject info = new org.json.JSONObject(json);
                int min = info.optInt("app_min", 1);
                String app = info.optString("app_version", "");
                ui.post(() -> {
                    if (min > PROTOCOL) {
                        askUpdate(url, "This app is too old", "claude-cluster " + info.optString("version") + " no longer supports this app ("
                            + BuildConfig.VERSION + "): install the new one to go on.");
                    } else if (!app.isEmpty() && newer(app, BuildConfig.VERSION)) {
                        askUpdate(url, "Update the app", "The computer has claude-cluster " + app + "; this app is " + BuildConfig.VERSION
                            + ". Install the new one to go on.");
                    }
                });
            } catch (Exception e) {
                // an older claude-cluster, offline: nothing said
            }
        }).start();
    }

    // an older app: blocked until it is updated (Install: the APK of the computer, Android asks once; Another computer: its address)
    private AlertDialog updateDialog = null;

    private void askUpdate(String url, String title, String message) {
        if (updateDialog != null && updateDialog.isShowing()) return;
        updateDialog = new AlertDialog.Builder(this).setTitle(title).setMessage(message)
            .setPositiveButton("Install", (d, w) -> install(url))
            .setNeutralButton("Another computer", (d, w) -> showSetup(""))
            .setCancelable(false).show();
    }

    private void install(String url) {
        new Thread(() -> {
            try {
                byte[] apk = fetch(url + "/app.apk", pinned());
                PackageInstaller installer = getPackageManager().getPackageInstaller();
                PackageInstaller.SessionParams params = new PackageInstaller.SessionParams(PackageInstaller.SessionParams.MODE_FULL_INSTALL);
                int id = installer.createSession(params);
                try (PackageInstaller.Session session = installer.openSession(id)) {
                    try (OutputStream out = session.openWrite("claude-cluster.apk", 0, apk.length)) {
                        out.write(apk);
                        session.fsync(out);
                    }
                    Intent done = new Intent(this, InstallReceiver.class);
                    PendingIntent pending = PendingIntent.getBroadcast(this, id, done, PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_MUTABLE);
                    session.commit(pending.getIntentSender());
                }
            } catch (Exception e) {
                // not installed: told, then asked again (the app stays blocked while it is older)
                ui.post(() -> new AlertDialog.Builder(this).setTitle("Not installed").setMessage(String.valueOf(e.getMessage()))
                    .setPositiveButton("Try again", (d, w) -> install(url))
                    .setNeutralButton("Another computer", (d, w) -> showSetup(""))
                    .setCancelable(false).show());
            }
        }).start();
    }

    // ---- what the page asks the app ----
    private class Bridge {
        @JavascriptInterface
        public String version() {
            return BuildConfig.VERSION;
        }

        @JavascriptInterface
        public void copy(String text) {
            // the clipboard of the phone (the page's own is not always given to a WebView)
            ui.post(() -> {
                android.content.ClipboardManager clipboard = (android.content.ClipboardManager) getSystemService(CLIPBOARD_SERVICE);
                if (clipboard != null) clipboard.setPrimaryClip(android.content.ClipData.newPlainText("claude-cluster", text));
            });
        }

        @JavascriptInterface
        public boolean inFront() {
            return front;
        }

        @JavascriptInterface
        public void notify(String title, String body, String session) {
            // out of front the watch service notifies (never twice); in front the page shows it itself
        }

        @JavascriptInterface
        public String notifySettings() {
            // the notifications of this phone: on / off, its kinds, what Android allows (the page draws them in This device)
            try {
                org.json.JSONObject out = new org.json.JSONObject();
                out.put("on", Alerts.enabled(prefs));
                out.put("kinds", new org.json.JSONArray(Alerts.kinds(prefs)));
                out.put("allowed", getSystemService(NotificationManager.class).areNotificationsEnabled());
                out.put("battery", getSystemService(android.os.PowerManager.class).isIgnoringBatteryOptimizations(getPackageName()));
                return out.toString();
            } catch (Exception e) {
                return "{}";
            }
        }

        @JavascriptInterface
        public void setNotify(boolean on, String kinds) {
            // on / off and the kinds chosen (comma separated); a service running while the app is in front is not needed: it starts when the app goes out
            prefs.edit().putBoolean("notify_on", on).putString("notify_kinds", kinds == null ? "" : kinds).apply();
        }

        @JavascriptInterface
        public void notifyAllow() {
            // the permission of Android 13+ (asked once: then the settings of Android for this app)
            ui.post(() -> {
                if (Build.VERSION.SDK_INT >= 33 && checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED && !prefs.getBoolean("notify_asked", false)) {
                    prefs.edit().putBoolean("notify_asked", true).apply();
                    requestPermissions(new String[] {Manifest.permission.POST_NOTIFICATIONS}, 2);
                } else {
                    startActivity(new Intent(android.provider.Settings.ACTION_APP_NOTIFICATION_SETTINGS).putExtra(android.provider.Settings.EXTRA_APP_PACKAGE, getPackageName()));
                }
            });
        }

        @JavascriptInterface
        public void batteryAllow() {
            // out of the battery optimization: the watch service is not put to sleep by Doze (the user confirms in Android)
            ui.post(() -> {
                try {
                    startActivity(new Intent(android.provider.Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS, android.net.Uri.parse("package:" + getPackageName())));
                } catch (Exception e) {
                    startActivity(new Intent(android.provider.Settings.ACTION_IGNORE_BATTERY_OPTIMIZATION_SETTINGS));
                }
            });
        }

        @JavascriptInterface
        public void notifyTest() {
            // a notification of this phone, to see what it looks like (and that Android shows it)
            try {
                Alerts.post(MainActivity.this, new org.json.JSONObject().put("seq", System.currentTimeMillis() % Integer.MAX_VALUE).put("time", System.currentTimeMillis() / 1000)
                    .put("kind", "message").put("session", "").put("title", "claude-cluster").put("body", "a test notification: a tap opens the app"));
            } catch (Exception e) {
                // not shown
            }
        }

        @JavascriptInterface
        public void save(String path, String name) {
            // a file of the computer (the app, a file display): Download/claude-cluster; a path of the page only (never another host)
            final String url = base;
            if (path == null || !path.startsWith("/") || path.startsWith("//") || path.contains("@") || path.contains("\\") || path.contains("..")) {
                ui.post(() -> android.widget.Toast.makeText(MainActivity.this, "refused: not a file of the computer", android.widget.Toast.LENGTH_LONG).show());
                return;
            }
            new Thread(() -> {
                String done;
                try {
                    byte[] bytes = Net.fetch(url + path, pinned(), android.webkit.CookieManager.getInstance().getCookie(url), 120000);
                    android.content.ContentValues values = new android.content.ContentValues();
                    values.put(android.provider.MediaStore.MediaColumns.DISPLAY_NAME, name);
                    values.put(android.provider.MediaStore.MediaColumns.RELATIVE_PATH, android.os.Environment.DIRECTORY_DOWNLOADS + "/claude-cluster");
                    android.net.Uri uri = getContentResolver().insert(android.provider.MediaStore.Downloads.EXTERNAL_CONTENT_URI, values);
                    if (uri == null) throw new IllegalStateException("Downloads refused it");
                    try (java.io.OutputStream out = getContentResolver().openOutputStream(uri)) {
                        out.write(bytes);
                    }
                    done = name + " saved in Download/claude-cluster";
                } catch (Exception e) {
                    done = name + ": " + e.getMessage();
                }
                final String message = done;
                ui.post(() -> android.widget.Toast.makeText(MainActivity.this, message, android.widget.Toast.LENGTH_LONG).show());
            }).start();
        }

        @JavascriptInterface
        public void changeComputer() {
            ui.post(() -> showSetup(""));
        }

        @JavascriptInterface
        public void openDocs(String target) {
            // the documentation of the computer (its /docs) in the browser of the phone: the page of THIS computer only (its LAN or outside address)
            ui.post(() -> {
                try {
                    final android.net.Uri uri = android.net.Uri.parse(target);
                    boolean ours = false;
                    for (String known : new String[] {prefs.getString("url", ""), prefs.getString("remote", "")}) {
                        final android.net.Uri home = android.net.Uri.parse(known);
                        ours = ours || (!known.isEmpty() && uri.getHost() != null && uri.getHost().equals(home.getHost()) && uri.getPort() == home.getPort());
                    }
                    if (ours && uri.getPath() != null && uri.getPath().startsWith("/docs/")) startActivity(new Intent(Intent.ACTION_VIEW, uri));
                } catch (Exception e) {
                    // no browser: the panel of the page has it
                }
            });
        }

        @JavascriptInterface
        public void quit() {
            // the computer lost (its Reconnecting popup): the app closed
            ui.post(() -> finishAndRemoveTask());
        }

        @JavascriptInterface
        public String info() {
            // the app and this phone, for the This device page
            try {
                org.json.JSONObject info = new org.json.JSONObject();
                info.put("version", BuildConfig.VERSION);
                info.put("code", BuildConfig.CODE);
                info.put("built", BuildConfig.BUILT);
                info.put("package", getPackageName());
                info.put("android", Build.VERSION.RELEASE);
                info.put("sdk", Build.VERSION.SDK_INT);
                info.put("model", Build.MANUFACTURER + " " + Build.MODEL);
                return info.toString();
            } catch (Exception e) {
                return "{}";
            }
        }

        @JavascriptInterface
        public void update() {
            ui.post(() -> install(base));
        }

        @JavascriptInterface
        public void remote(String url) {
            // the address from outside, told by claude-cluster: kept for when the LAN is not reached
            if (url != null && url.startsWith("https://")) prefs.edit().putString("remote", url).apply();
        }
    }
}
