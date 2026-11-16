package dev.tengan.app;

import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import com.getcapacitor.JSObject;
import com.getcapacitor.Plugin;
import com.getcapacitor.PluginCall;
import com.getcapacitor.PluginMethod;
import com.getcapacitor.annotation.CapacitorPlugin;
import android.view.View;
import android.view.WindowManager;
import android.widget.Toast;
import com.getcapacitor.BridgeActivity;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.zip.GZIPInputStream;
import org.json.JSONObject;

// TENGAN 天眼 — приложение для планшета: внутри та же страница, что /full на палке.
// Здесь только то, чего не может страница:
//  - экран не гаснет, во весь экран без системных панелей;
//  - страница обновляется ОТ ПАЛКИ: в прошивке всегда лежит свежая страница, приложение
//    сверяет версию (/api/pagever) и, если на палке новее, забирает /full по Wi-Fi палки
//    (интернет не нужен), проверяет и переключается на неё. Новый APK нужен только для
//    изменений самой оболочки.
public class MainActivity extends BridgeActivity {
    private static final String STICK = "http://192.168.4.1";
    private static final String CAP_PREFS = "CapWebViewSettings", CAP_PATH = "serverBasePath";   // так их читает сам Capacitor
    private final Handler ui = new Handler(Looper.getMainLooper());
    private volatile boolean busy = false;
    // что происходит с обновлением — страница показывает это в шапке (window.TenganNative.status())
    static volatile String status = "start";
    static MainActivity self;
    // плагин для страницы: Capacitor.Plugins.TenganUpdate.status() / .check()
    @CapacitorPlugin(name = "TenganUpdate")
    public static class UpdatePlugin extends Plugin {
        @PluginMethod public void status(PluginCall call) { JSObject r = new JSObject(); r.put("status", status); call.resolve(r); }
        @PluginMethod public void check(PluginCall call) { if (self != null) self.ui.post(self::checkStickPage); call.resolve(); }
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        forgetOldDownloadAfterApkUpdate();
        self = this;
        registerPlugin(UpdatePlugin.class);
        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        ui.postDelayed(checker, 3000);
    }

    @Override
    public void onResume() {
        super.onResume();
        ui.postDelayed(this::checkStickPage, 1500);       // вернулись в приложение (могли перешить палку) — проверить сразу
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_STABLE | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
        }
    }

    // проверка раз в 30 с: дёшево (короткий JSON), качаем только если версия другая
    private final Runnable checker = new Runnable() {
        @Override public void run() {
            checkStickPage();
            ui.postDelayed(this, 30000);
        }
    };

    // новый APK несёт свою страницу: старую скачанную забываем (APK может быть новее палки)
    private void forgetOldDownloadAfterApkUpdate() {
        try {
            PackageInfo pi = getPackageManager().getPackageInfo(getPackageName(), 0);
            String stamp = pi.lastUpdateTime + "";
            SharedPreferences my = getSharedPreferences("tengan", MODE_PRIVATE);
            if (!stamp.equals(my.getString("apkStamp", ""))) {
                getSharedPreferences(CAP_PREFS, MODE_PRIVATE).edit().remove(CAP_PATH).apply();
                my.edit().putString("apkStamp", stamp).apply();
            }
        } catch (Exception ignored) { }
    }

    private File pageDir() { return new File(getFilesDir(), "stickpage"); }

    // какая страница сейчас: скачанная с палки или встроенная в APK
    private String currentSha() {
        try {
            String path = getSharedPreferences(CAP_PREFS, MODE_PRIVATE).getString(CAP_PATH, null);
            if (path != null && new File(path, "pagever.txt").exists()) return new String(read(new java.io.FileInputStream(new File(path, "pagever.txt"))), StandardCharsets.UTF_8).trim();
            return new String(read(getAssets().open("public/pagever.txt")), StandardCharsets.UTF_8).trim();
        } catch (Exception e) { return ""; }
    }

    private void checkStickPage() {
        if (busy) return;
        busy = true;
        new Thread(() -> {
            try {
                status = "check";
                JSONObject j = new JSONObject(new String(get(STICK + "/api/pagever", 2500), StandardCharsets.UTF_8));
                String sha = j.getString("sha"), cur = currentSha();
                if (sha.isEmpty()) { status = "stick: no version"; return; }
                if (sha.equals(cur)) { status = "same " + sha.substring(0, 6); return; }
                status = "download " + sha.substring(0, 6);
                byte[] html = get(STICK + "/full", 15000);
                String got = sha256(html);
                if (!sha.equals(got)) { status = "bad hash " + got.substring(0, 6) + " (" + html.length + " B)"; return; }
                File dir = pageDir();
                dir.mkdirs();
                write(new File(dir, "index.html"), html);
                write(new File(dir, "pagever.txt"), sha.getBytes(StandardCharsets.UTF_8));
                String path = dir.getAbsolutePath();
                getSharedPreferences(CAP_PREFS, MODE_PRIVATE).edit().putString(CAP_PATH, path).apply();
                status = "updated " + sha.substring(0, 6);
                ui.post(() -> {
                    Toast.makeText(this, "TENGAN: страница обновлена с палки", Toast.LENGTH_SHORT).show();
                    getBridge().setServerBasePath(path);
                });
            } catch (Exception e) {
                status = "no stick: " + e.getClass().getSimpleName() + (e.getMessage() != null ? " " + e.getMessage() : "");
            } finally { busy = false; }
        }).start();
    }

    private static byte[] get(String url, int timeoutMs) throws Exception {
        HttpURLConnection c = (HttpURLConnection) new URL(url).openConnection();
        c.setConnectTimeout(timeoutMs);
        c.setReadTimeout(timeoutMs);
        c.setUseCaches(false);
        try {
            if (c.getResponseCode() != 200) throw new Exception("http " + c.getResponseCode());
            byte[] b = read(c.getInputStream());
            if (b.length > 2 && (b[0] & 0xff) == 0x1f && (b[1] & 0xff) == 0x8b) b = read(new GZIPInputStream(new java.io.ByteArrayInputStream(b)));   // палка шлёт gzip
            return b;
        } finally { c.disconnect(); }
    }

    private static byte[] read(InputStream in) throws Exception {
        try (InputStream s = in) {
            ByteArrayOutputStream o = new ByteArrayOutputStream();
            byte[] buf = new byte[16384];
            int n;
            while ((n = s.read(buf)) > 0) o.write(buf, 0, n);
            return o.toByteArray();
        }
    }

    private static void write(File f, byte[] data) throws Exception {
        File tmp = new File(f.getPath() + ".tmp");
        try (FileOutputStream o = new FileOutputStream(tmp)) { o.write(data); }
        if (!tmp.renameTo(f)) { f.delete(); if (!tmp.renameTo(f)) throw new Exception("rename"); }
    }

    private static String sha256(byte[] data) throws Exception {
        byte[] d = MessageDigest.getInstance("SHA-256").digest(data);
        StringBuilder sb = new StringBuilder();
        for (byte x : d) sb.append(String.format("%02x", x));
        return sb.toString();
    }
}
