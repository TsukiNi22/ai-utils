/*
 * claude-cluster for Android: the requests of the app itself (its updates, its notifications), trusting the authorities of
 * the system (a real certificate: the tailnet) and the authority of the computer pinned once; anything only to read /ca.crt
 * the first time (its fingerprint shown to the user)
 */

package com.claudecluster.app;

import android.content.SharedPreferences;
import android.util.Base64;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.security.KeyStore;
import java.security.cert.CertificateFactory;
import java.security.cert.X509Certificate;

import javax.net.ssl.HttpsURLConnection;
import javax.net.ssl.SSLContext;
import javax.net.ssl.TrustManager;
import javax.net.ssl.TrustManagerFactory;
import javax.net.ssl.X509TrustManager;

final class Net {
    private Net() {}

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
