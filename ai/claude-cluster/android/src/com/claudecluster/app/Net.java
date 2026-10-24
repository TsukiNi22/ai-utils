/*
 * claude-cluster for Android: the requests of the app itself (its updates, its notifications), trusting the authorities of
 * the system (a real certificate: the tailnet) and the authority of the computer pinned once; anything only to read /ca.crt
 * the first time (its fingerprint shown to the user); the computers of this network found by a broadcast (discover)
 */

package com.claudecluster.app;

import android.content.SharedPreferences;
import android.util.Base64;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.HttpURLConnection;
import java.net.InetAddress;
import java.net.InterfaceAddress;
import java.net.NetworkInterface;
import java.net.SocketTimeoutException;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.security.KeyStore;
import java.security.MessageDigest;
import java.security.cert.CertificateFactory;
import java.security.cert.X509Certificate;
import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

import org.json.JSONObject;

import javax.net.ssl.HttpsURLConnection;
import javax.net.ssl.SSLContext;
import javax.net.ssl.TrustManager;
import javax.net.ssl.TrustManagerFactory;
import javax.net.ssl.X509TrustManager;

final class Net {
    private Net() {}

    // the claude-cluster of this network answer this broadcast while their web access is open ([web] discovery)
    static final int DISCOVERY_PORT = 8789;
    static final String DISCOVERY_ASK = "claude-cluster?";

    // a computer found: its name, its address (the one it answered from), the fingerprint of its authority, its version
    static final class Found {
        final String name;
        final String url;
        final String authority;
        final String version;

        Found(String name, String url, String authority, String version) {
            this.name = name;
            this.url = url;
            this.authority = authority;
            this.version = version;
        }
    }

    // SHA-256 of a certificate, AA:BB:... (as claude-cluster shows it)
    static String fingerprint(X509Certificate cert) {
        try {
            byte[] digest = MessageDigest.getInstance("SHA-256").digest(cert.getEncoded());
            StringBuilder out = new StringBuilder();
            for (int i = 0; i < digest.length; i++) out.append(i > 0 ? ":" : "").append(String.format(Locale.ROOT, "%02X", digest[i]));
            return out.toString();
        } catch (Exception e) {
            return "?";
        }
    }

    // the computers of this network: the broadcast sent every second for wait ms (every interface), the answers gathered; never on the
    // main thread
    static List<Found> discover(int wait) {
        Map<String, Found> found = new LinkedHashMap<>();
        try (DatagramSocket socket = new DatagramSocket()) {
            socket.setBroadcast(true);
            socket.setSoTimeout(250);
            byte[] ask = DISCOVERY_ASK.getBytes(StandardCharsets.UTF_8);
            List<InetAddress> targets = new ArrayList<>();
            targets.add(InetAddress.getByName("255.255.255.255"));
            for (NetworkInterface nic : Collections.list(NetworkInterface.getNetworkInterfaces())) {
                if (!nic.isUp() || nic.isLoopback()) continue;
                for (InterfaceAddress address : nic.getInterfaceAddresses())
                    if (address.getBroadcast() != null && !targets.contains(address.getBroadcast())) targets.add(address.getBroadcast());
            }
            byte[] buffer = new byte[2048];
            long end = System.currentTimeMillis() + wait;
            for (int round = 0; System.currentTimeMillis() < end; round++) {
                if (round % 4 == 0)
                    for (InetAddress target : targets) {
                        try {
                            socket.send(new DatagramPacket(ask, ask.length, target, DISCOVERY_PORT));
                        } catch (Exception e) {
                            // this network refuses a broadcast: the others still asked
                        }
                    }
                DatagramPacket packet = new DatagramPacket(buffer, buffer.length);
                try {
                    socket.receive(packet);
                    JSONObject answer = new JSONObject(new String(packet.getData(), 0, packet.getLength(), StandardCharsets.UTF_8));
                    if (!"claude-cluster".equals(answer.optString("app"))) continue;
                    String host = packet.getAddress().getHostAddress();
                    String url = (answer.optBoolean("https", true) ? "https://" : "http://") + host + ":" + answer.optInt("port", 8787);
                    found.put(url, new Found(answer.optString("name", host), url, answer.optString("authority", ""), answer.optString("version", "")));
                } catch (SocketTimeoutException e) {
                    // nothing yet: asked again
                } catch (Exception e) {
                    // not an answer of claude-cluster: ignored
                }
            }
        } catch (Exception e) {
            // no network: none found
        }
        return new ArrayList<>(found.values());
    }

    // the computer trusted (its authority pinned) found on this network at another address: that address (null: not found)
    static String relocate(SharedPreferences prefs, String failed, int wait) {
        X509Certificate ca = pinned(prefs);
        if (ca == null) return null;
        String mine = fingerprint(ca);
        for (Found one : discover(wait)) {
            if (one.authority.isEmpty() || !one.authority.equals(mine) || one.url.equals(failed)) continue;
            carry(failed, one.url);
            prefs.edit().putString("url", one.url).putString("active", one.url).apply();
            return one.url;
        }
        return null;
    }

    // its device cookies (its id, its trust) given to the other address: the same phone there
    static void carry(String from, String to) {
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

    // the authority pinned (null: none yet)
    static X509Certificate pinned(SharedPreferences prefs) {
        try {
            String saved = prefs.getString("ca", "");
            if (saved.isEmpty()) return null;
            return (X509Certificate) CertificateFactory.getInstance("X.509").generateCertificate(new ByteArrayInputStream(Base64.decode(saved, Base64.DEFAULT)));
        } catch (Exception e) {
            return null;
        }
    }

    // the trust managers of the system (a real certificate: Let's Encrypt by the tailnet), or of the authority pinned
    private static X509TrustManager managerOf(X509Certificate ca) throws Exception {
        TrustManagerFactory factory = TrustManagerFactory.getInstance(TrustManagerFactory.getDefaultAlgorithm());
        if (ca == null) {
            factory.init((KeyStore) null);
        } else {
            KeyStore store = KeyStore.getInstance(KeyStore.getDefaultType());
            store.load(null, null);
            store.setCertificateEntry("claude-cluster", ca);
            factory.init(store);
        }
        for (TrustManager manager: factory.getTrustManagers())
            if (manager instanceof X509TrustManager) return (X509TrustManager) manager;
        throw new IllegalStateException("no trust manager");
    }

    // a GET: its body (cookie: the session of the page, for the state)
    static byte[] fetch(String address, X509Certificate ca, String cookie, int readTimeout) throws Exception {
        return fetch(address, ca, cookie, readTimeout, false);
    }

    // anything: the certificate not checked (the first read of /ca.crt only: the user compares its fingerprint)
    static byte[] fetch(String address, X509Certificate ca, String cookie, int readTimeout, boolean anything) throws Exception {
        HttpURLConnection connection = (HttpURLConnection) new URL(address).openConnection();
        if (connection instanceof HttpsURLConnection) {
            HttpsURLConnection https = (HttpsURLConnection) connection;
            TrustManager[] managers;
            if (anything) {
                managers = new TrustManager[] {new X509TrustManager() {
                    public void checkClientTrusted(X509Certificate[] chain, String type) {}
                    public void checkServerTrusted(X509Certificate[] chain, String type) {}
                    public X509Certificate[] getAcceptedIssuers() {return new X509Certificate[0];}
                }};
                https.setHostnameVerifier((host, session) -> true);
            } else {
                // the system first (a real certificate), else the authority pinned (the computer's own: its addresses named)
                final X509TrustManager system = managerOf(null);
                final X509TrustManager pinned = ca == null ? null : managerOf(ca);
                managers = new TrustManager[] {new X509TrustManager() {
                    public void checkClientTrusted(X509Certificate[] chain, String type) throws java.security.cert.CertificateException {
                        system.checkClientTrusted(chain, type);
                    }
                    public void checkServerTrusted(X509Certificate[] chain, String type) throws java.security.cert.CertificateException {
                        try {
                            system.checkServerTrusted(chain, type);
                        } catch (java.security.cert.CertificateException e) {
                            if (pinned == null) throw e;
                            pinned.checkServerTrusted(chain, type);
                        }
                    }
                    public X509Certificate[] getAcceptedIssuers() {return system.getAcceptedIssuers();}
                }};
            }
            SSLContext context = SSLContext.getInstance("TLS");
            context.init(null, managers, null);
            https.setSSLSocketFactory(context.getSocketFactory());
        }
        if (cookie != null && !cookie.isEmpty()) connection.setRequestProperty("Cookie", cookie);
        connection.setRequestProperty("User-Agent", "claude-cluster-app/" + BuildConfig.VERSION);
        connection.setConnectTimeout(8000);
        connection.setReadTimeout(readTimeout);
        if (connection.getResponseCode() == 401) throw new SecurityException("not connected");
        try (InputStream in = connection.getInputStream(); ByteArrayOutputStream out = new ByteArrayOutputStream()) {
            byte[] buffer = new byte[65536];
            for (int n; (n = in.read(buffer)) > 0;) out.write(buffer, 0, n);
            return out.toByteArray();
        }
    }
}
