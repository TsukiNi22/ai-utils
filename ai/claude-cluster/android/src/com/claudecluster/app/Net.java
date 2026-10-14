/*
 * claude-cluster for Android: the requests of the app itself (its updates, its notifications), trusting the authority of the
 * computer pinned once (none pinned: anything, only to read /ca.crt the first time)
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

    // a GET: its body (cookie: the session of the page, for the state)
    static byte[] fetch(String address, X509Certificate ca, String cookie, int readTimeout) throws Exception {
        HttpURLConnection connection = (HttpURLConnection) new URL(address).openConnection();
        if (connection instanceof HttpsURLConnection) {
            HttpsURLConnection https = (HttpsURLConnection) connection;
            TrustManager[] managers;
            if (ca == null) {
                managers = new TrustManager[] {new X509TrustManager() {
                    public void checkClientTrusted(X509Certificate[] chain, String type) {}
                    public void checkServerTrusted(X509Certificate[] chain, String type) {}
                    public X509Certificate[] getAcceptedIssuers() {return new X509Certificate[0];}
                }};
            } else {
                KeyStore store = KeyStore.getInstance(KeyStore.getDefaultType());
                store.load(null, null);
                store.setCertificateEntry("claude-cluster", ca);
                TrustManagerFactory factory = TrustManagerFactory.getInstance(TrustManagerFactory.getDefaultAlgorithm());
                factory.init(store);
                managers = factory.getTrustManagers();
            }
            SSLContext context = SSLContext.getInstance("TLS");
            context.init(null, managers, null);
            https.setSSLSocketFactory(context.getSocketFactory());
            https.setHostnameVerifier((host, session) -> true); // its address: one of the certificate's, the authority pinned checks it
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
