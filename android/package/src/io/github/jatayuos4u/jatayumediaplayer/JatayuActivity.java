package io.github.jatayuos4u.jatayumediaplayer;

import android.Manifest;
import android.app.PictureInPictureParams;
import android.util.Rational;
import android.content.res.Configuration;
import android.content.ContentUris;
import android.content.Context;
import android.content.pm.PackageManager;
import android.content.pm.ActivityInfo;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.provider.MediaStore;
import android.os.ParcelFileDescriptor;
import android.system.Os;
import android.system.OsConstants;
import android.system.StructStat;
import android.view.WindowManager;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import org.json.JSONArray;
import org.json.JSONObject;
import org.qtproject.qt.android.bindings.QtActivity;
import java.io.File;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.Executors;
import java.util.concurrent.ExecutorService;

/** Android-only, read-only media discovery. Never walks private app folders. */
public class JatayuActivity extends QtActivity {
    private static final int MEDIA_REQUEST = 6041;
    private static volatile JatayuActivity current;
    private static volatile boolean visible;
    private boolean videoPlaying, allowPiP = true;
    private boolean pipRestorePending;
    private int pipRestoreGeneration;
    private Rational videoAspect = new Rational(16, 9);
    public static boolean supportsPiP() {
        JatayuActivity a = current;
        return a != null && a.getPackageManager().hasSystemFeature(PackageManager.FEATURE_PICTURE_IN_PICTURE);
    }
    public static boolean inPiP() { JatayuActivity a = current; return a != null && a.isInPictureInPictureMode(); }
    public static void videoState(String json) {
        JatayuActivity a = current; if (a == null) return;
        try {
            JSONObject state = new JSONObject(json);
            a.videoPlaying = state.optBoolean("video") && state.optBoolean("playing");
            a.allowPiP = state.optBoolean("pipEnabled", true);
            int w = state.optInt("width", 16), h = state.optInt("height", 9);
            if (w > 0 && h > 0) { double ratio = Math.max(1.0 / 2.39, Math.min(2.39, (double)w / h)); a.videoAspect = new Rational((int)(ratio * 10000), 10000); }
            if (supportsPiP()) {
                PictureInPictureParams.Builder p = new PictureInPictureParams.Builder().setAspectRatio(a.videoAspect);
                if (Build.VERSION.SDK_INT >= 31) p.setAutoEnterEnabled(a.videoPlaying && a.allowPiP).setSeamlessResizeEnabled(true);
                a.setPictureInPictureParams(p.build());
            }
        } catch (Exception unsupported) { /* onStop still pauses hidden video. */ }
    }
    public static void enterPiP() {
        JatayuActivity a = current;
        if (a == null || !a.videoPlaying || !supportsPiP()) return;
        try {
            if (!a.enterPictureInPictureMode(new PictureInPictureParams.Builder().setAspectRatio(a.videoAspect).build()))
                JatayuPlaybackService.activityEvent("video-hidden");
        } catch (Exception denied) { JatayuPlaybackService.activityEvent("video-hidden"); }
    }
    @Override public void onUserLeaveHint() {
        super.onUserLeaveHint();
        if (videoPlaying && allowPiP && Build.VERSION.SDK_INT < 31) enterPiP();
    }
    @Override public void onPictureInPictureModeChanged(boolean active, Configuration config) {
        super.onPictureInPictureModeChanged(active, config);
        if (active) { pipRestorePending = true; ++pipRestoreGeneration; }
        JatayuPlaybackService.activityEvent(active ? "pip-enter" : "pip-exit");
        if (!active) restoreAfterPiP();
    }
    private void restoreAfterPiP() {
        if (!pipRestorePending || isInPictureInPictureMode()) return;
        final int generation = ++pipRestoreGeneration;
        final View content = findViewById(android.R.id.content);
        if (content == null) return;
        // The exit callback may precede Activity resume and the final rotation
        // layout. Re-read the content parent's bounds after each layout phase.
        for (int delay : new int[]{0, 150, 400}) content.postDelayed(() -> {
            if (generation != pipRestoreGeneration || !pipRestorePending || !visible || isInPictureInPictureMode()) return;
            int width = content.getWidth(), height = content.getHeight();
            if (width <= 0 || height <= 0) return;
            content.requestLayout(); content.invalidate();
            JatayuPlaybackService.activityBounds(width, height);
            if (delay == 400) pipRestorePending = false;
        }, delay);
    }
    @Override public void onStop() {
        super.onStop();
        // A visible PiP activity is paused, not stopped. This also covers PiP
        // dismissal, denied PiP, screen locking and devices without PiP.
        JatayuPlaybackService.activityEvent("video-hidden");
    }
    private static final ExecutorService WORKER = Executors.newSingleThreadExecutor();
    private static native void nativeScanFinished(String payload);
    private String knownMediaUrls = "[]";

    @Override public void onCreate(android.os.Bundle state) {
        current = this;
        super.onCreate(state);
        JatayuPlaybackService.initialize(this);
        setVolumeControlStream(android.media.AudioManager.STREAM_MUSIC);
    }
    public static boolean supportsRotationLock() {
        JatayuActivity activity = current;
        return Build.VERSION.SDK_INT < 36 || activity == null || activity.getResources().getConfiguration().smallestScreenWidthDp < 600;
    }
    public static boolean isVisible() { return visible; }
    public static void backgroundTask() {
        JatayuActivity activity = current;
        if (activity != null) {
            if (activity.videoPlaying && activity.allowPiP && supportsPiP()) enterPiP();
            else activity.moveTaskToBack(true);
        }
    }
    @Override public void onPause() { visible = false; super.onPause(); }
    @Override public void onDestroy() {
        if (current == this) current = null;
        super.onDestroy();
    }
    // Use Android's provider API directly. The detached descriptor belongs to
    // the native QFile and stays open until QMediaPlayer releases that source.
    public static int openMediaDescriptor(String encodedUrl) {
        JatayuActivity activity = current;
        if (activity == null) return -1;
        Uri uri = Uri.parse(encodedUrl);
        if (!"content".equals(uri.getScheme()) || uri.getAuthority() == null) return -1;
        try (ParcelFileDescriptor descriptor = activity.getContentResolver().openFileDescriptor(uri, "r")) {
            return descriptor == null ? -1 : descriptor.detachFd();
        } catch (SecurityException denied) {
            return -2;
        } catch (java.io.FileNotFoundException missing) {
            return -3;
        } catch (Exception unavailable) {
            return -1;
        }
    }

    private boolean granted(String permission) {
        return checkSelfPermission(permission) == PackageManager.PERMISSION_GRANTED;
    }
    public static void requestMediaScan(boolean askPermission, boolean changeAccess, String knownUrls) {
        JatayuActivity activity = current;
        if (activity == null) { deliver(new JSONArray(), "unavailable", "Open the app and try scanning again.", false); return; }
        activity.runOnUiThread(() -> { activity.knownMediaUrls = knownUrls; activity.beginScan(askPermission, changeAccess); });
    }
    private void beginScan(boolean askPermission, boolean changeAccess) {
        ArrayList<String> missing = new ArrayList<>();
        if (Build.VERSION.SDK_INT >= 33) {
            if (!granted(Manifest.permission.READ_MEDIA_AUDIO)) missing.add(Manifest.permission.READ_MEDIA_AUDIO);
            if (!granted(Manifest.permission.READ_MEDIA_VIDEO)) {
                missing.add(Manifest.permission.READ_MEDIA_VIDEO);
                if (Build.VERSION.SDK_INT >= 34) missing.add(Manifest.permission.READ_MEDIA_VISUAL_USER_SELECTED);
            }
        } else if (!granted(Manifest.permission.READ_EXTERNAL_STORAGE)) missing.add(Manifest.permission.READ_EXTERNAL_STORAGE);
        boolean hasAccess = Build.VERSION.SDK_INT >= 33
                ? granted(Manifest.permission.READ_MEDIA_AUDIO) || granted(Manifest.permission.READ_MEDIA_VIDEO)
                    || (Build.VERSION.SDK_INT >= 34 && granted(Manifest.permission.READ_MEDIA_VISUAL_USER_SELECTED))
                : granted(Manifest.permission.READ_EXTERNAL_STORAGE);
        if (askPermission && !missing.isEmpty() && (!hasAccess || changeAccess)) {
            requestPermissions(missing.toArray(new String[0]), MEDIA_REQUEST);
        } else scanAccessibleMedia();
    }
    @Override public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] results) {
        super.onRequestPermissionsResult(requestCode, permissions, results);
        if (requestCode == MEDIA_REQUEST) scanAccessibleMedia();
    }
    private void scanAccessibleMedia() {
        final boolean audio = Build.VERSION.SDK_INT >= 33 ? granted(Manifest.permission.READ_MEDIA_AUDIO) : granted(Manifest.permission.READ_EXTERNAL_STORAGE);
        final boolean video = Build.VERSION.SDK_INT >= 33 ? granted(Manifest.permission.READ_MEDIA_VIDEO) : granted(Manifest.permission.READ_EXTERNAL_STORAGE);
        final boolean selected = Build.VERSION.SDK_INT >= 34 && granted(Manifest.permission.READ_MEDIA_VISUAL_USER_SELECTED);
        if (!audio && !video && !selected) {
            deliver(new JSONArray(), "permission", "Allow media access to discover device files, or add files individually.", false);
            return;
        }
        final Context context = getApplicationContext();
        final String knownUrls = knownMediaUrls;
        WORKER.execute(() -> {
            JSONArray entries = new JSONArray();
            JSONArray identities = new JSONArray();
            boolean limited = !audio || !video;
            try {
                Set<String> volumes = mountedVolumes(context);
                JSONArray known = new JSONArray(knownUrls);
                for (int i = 0; i < known.length(); ++i) {
                    String url = known.optString(i);
                    JSONObject identity = identify(context, Uri.parse(url));
                    identity.put("url", url); identities.put(identity);
                }
                if (video || selected) query(context, MediaStore.Video.Media.EXTERNAL_CONTENT_URI, "video", entries);
                if (audio) query(context, MediaStore.Audio.Media.EXTERNAL_CONTENT_URI, "audio", entries);
                // Only complete collections on storage present throughout this scan
                // can prove that a previously indexed item is now absent.
                volumes.retainAll(mountedVolumes(context));
                JSONArray completeScopes = new JSONArray();
                boolean stillAudio = Build.VERSION.SDK_INT >= 33 ? granted(Manifest.permission.READ_MEDIA_AUDIO) : granted(Manifest.permission.READ_EXTERNAL_STORAGE);
                boolean stillVideo = Build.VERSION.SDK_INT >= 33 ? granted(Manifest.permission.READ_MEDIA_VIDEO) : granted(Manifest.permission.READ_EXTERNAL_STORAGE);
                for (String volume : volumes) {
                    if (audio && stillAudio) completeScopes.put("audio:" + volume);
                    if (video && stillVideo) completeScopes.put("video:" + volume);
                }
                String message = limited ? "Accessible media refreshed. Items outside granted access have been kept." : "Device library updated.";
                deliver(entries, limited ? "limited" : "ready", message, true, completeScopes, identities);
            } catch (SecurityException error) {
                deliver(entries, "limited", "Media access changed. Grant access or select files again.", true);
            } catch (Exception error) {
                deliver(entries, "error", "Some media could not be read. Your existing library has been kept.", true);
            }
        });
    }
    private static Set<String> mountedVolumes(Context context) {
        return Build.VERSION.SDK_INT >= 29 ? new HashSet<>(MediaStore.getExternalVolumeNames(context))
            : new HashSet<>(java.util.Collections.singleton(""));
    }
    private static long number(Cursor cursor, String name) {
        int index = cursor.getColumnIndex(name); return index < 0 || cursor.isNull(index) ? 0 : cursor.getLong(index);
    }
    private static String text(Cursor cursor, String name) {
        int index = cursor.getColumnIndex(name); return index < 0 || cursor.isNull(index) ? "" : cursor.getString(index);
    }
    private static void query(Context context, Uri collection, String kind, JSONArray result) throws Exception {
        ArrayList<String> columns = new ArrayList<>();
        java.util.Collections.addAll(columns, "_id", "_display_name", "duration", "_size", "date_added");
        if (kind.equals("video")) java.util.Collections.addAll(columns, "width", "height");
        else java.util.Collections.addAll(columns, "artist", "album");
        columns.add(Build.VERSION.SDK_INT >= 29 ? "relative_path" : "_data");
        if (Build.VERSION.SDK_INT >= 29) columns.add("volume_name");
        try (Cursor cursor = context.getContentResolver().query(collection, columns.toArray(new String[0]), null, null, "date_added DESC")) {
            if (cursor == null) throw new java.io.IOException("Media query returned no cursor");
            while (cursor.moveToNext()) {
                long id = number(cursor, "_id");
                JSONObject item = new JSONObject();
                String folder = Build.VERSION.SDK_INT >= 29 ? text(cursor, "relative_path") : new File(text(cursor, "_data")).getParent();
                if (folder == null || folder.isEmpty()) folder = "Device media";
                item.put("url", ContentUris.withAppendedId(collection, id).toString());
                item.put("catalogId", kind + ":" + id);
                item.put("title", text(cursor, "_display_name"));
                item.put("kind", kind);
                item.put("duration", number(cursor, "duration"));
                item.put("size", number(cursor, "_size"));
                item.put("dateAdded", number(cursor, "date_added") * 1000);
                item.put("folder", folder);
                item.put("folderKey", text(cursor, "volume_name") + ":" + folder);
                item.put("volumeName", text(cursor, "volume_name"));
                item.put("width", number(cursor, "width")); item.put("height", number(cursor, "height"));
                item.put("artist", cleanTag(text(cursor, "artist"))); item.put("album", cleanTag(text(cursor, "album")));
                item.put("mediaIdentity", kind + ":" + text(cursor, "volume_name") + ":" + id);
                item.put("fileIdentity", fileIdentity(context, ContentUris.withAppendedId(collection, id)));
                result.put(item);
            }
        }
    }
    // Only local system providers are inspected; cloud selections must not be
    // downloaded merely to compare identities during a library scan.
    private static boolean localProvider(Uri uri) {
        String host = uri.getAuthority();
        return "file".equals(uri.getScheme()) || ("content".equals(uri.getScheme())
            && ("media".equals(host) || "com.android.providers.media.documents".equals(host)
                || "com.android.externalstorage.documents".equals(host)
                || "com.android.providers.downloads.documents".equals(host)));
    }
    private static String fileIdentity(Context context, Uri uri) {
        if (!localProvider(uri)) return "";
        try (ParcelFileDescriptor descriptor = "file".equals(uri.getScheme())
                ? ParcelFileDescriptor.open(new File(uri.getPath()), ParcelFileDescriptor.MODE_READ_ONLY)
                : context.getContentResolver().openFileDescriptor(uri, "r")) {
            if (descriptor == null) return "";
            StructStat stat = Os.fstat(descriptor.getFileDescriptor());
            if (!OsConstants.S_ISREG(stat.st_mode) || stat.st_ino <= 0 || stat.st_size <= 0) return "";
            return stat.st_dev + ":" + stat.st_ino + ":" + stat.st_size + ":" + stat.st_mtime + ":" + stat.st_ctime;
        } catch (Exception unavailable) { return ""; }
    }
    public static String identifyMedia(String encodedUrl) {
        JatayuActivity activity = current;
        return activity == null ? "{}" : identify(activity.getApplicationContext(), Uri.parse(encodedUrl)).toString();
    }
    private static JSONObject identify(Context context, Uri uri) {
        JSONObject result = new JSONObject();
        if (!localProvider(uri)) return result;
        try { result.put("fileIdentity", fileIdentity(context, uri)); } catch (org.json.JSONException ignored) { }
        String identity = "";
        try {
            Uri media = "media".equals(uri.getAuthority()) ? uri
                : Build.VERSION.SDK_INT >= 29 ? MediaStore.getMediaUri(context, uri) : null;
            if (media != null && "media".equals(media.getAuthority())) identity = queryIdentity(context, media, null, null);
        } catch (Exception unavailable) { /* The conversion does not support every local provider. */ }
        if (identity.isEmpty()) {
            // DownloadsProvider may not support getMediaUri. Query only the
            // exact path verified against its granted descriptor, never a name.
            String path = verifiedDescriptorPath(context, uri);
            if (!path.isEmpty()) identity = queryIdentity(context, MediaStore.Files.getContentUri("external"), "_data = ?", new String[]{path});
        }
        try { if (!identity.isEmpty()) result.put("mediaIdentity", identity); } catch (org.json.JSONException ignored) { }
        return result;
    }
    private static String queryIdentity(Context context, Uri uri, String selection, String[] arguments) {
        String[] columns = Build.VERSION.SDK_INT >= 29 ? new String[]{"_id", "mime_type", "volume_name"} : new String[]{"_id", "mime_type"};
        try (Cursor cursor = context.getContentResolver().query(uri, columns, selection, arguments, null)) {
            if (cursor == null || !cursor.moveToFirst()) return "";
            String mime = text(cursor, "mime_type");
            String kind = mime.startsWith("audio/") ? "audio" : mime.startsWith("video/") ? "video" : "";
            String identity = kind.isEmpty() ? "" : kind + ":" + text(cursor, "volume_name") + ":" + number(cursor, "_id");
            return cursor.moveToNext() ? "" : identity;
        } catch (Exception unavailable) { return ""; }
    }
    private static String verifiedDescriptorPath(Context context, Uri uri) {
        if (!localProvider(uri)) return "";
        try (ParcelFileDescriptor descriptor = "file".equals(uri.getScheme())
                ? ParcelFileDescriptor.open(new File(uri.getPath()), ParcelFileDescriptor.MODE_READ_ONLY)
                : context.getContentResolver().openFileDescriptor(uri, "r")) {
            if (descriptor == null) return "";
            StructStat fd = Os.fstat(descriptor.getFileDescriptor());
            if (!OsConstants.S_ISREG(fd.st_mode)) return "";
            String path = new File(Os.readlink("/proc/self/fd/" + descriptor.getFd())).getCanonicalPath();
            StructStat named = Os.stat(path);
            return fd.st_dev == named.st_dev && fd.st_ino == named.st_ino ? path : "";
        } catch (Exception unavailable) { return ""; }
    }
    private static String cleanTag(String value) { return "<unknown>".equals(value) ? "" : value; }
    private static void deliver(JSONArray entries, String status, String message, boolean hasAccess) {
        deliver(entries, status, message, hasAccess, new JSONArray());
    }
    private static void deliver(JSONArray entries, String status, String message, boolean hasAccess, JSONArray completeScopes) {
        deliver(entries, status, message, hasAccess, completeScopes, new JSONArray());
    }
    private static void deliver(JSONArray entries, String status, String message, boolean hasAccess, JSONArray completeScopes, JSONArray identities) {
        try {
            JSONObject result = new JSONObject();
            result.put("items", entries); result.put("status", status); result.put("message", message); result.put("hasAccess", hasAccess);
            result.put("completeScopes", completeScopes); result.put("identities", identities);
            nativeScanFinished(result.toString());
        } catch (org.json.JSONException ignored) { nativeScanFinished("{\"items\":[],\"status\":\"error\",\"message\":\"Scan could not finish.\"}"); }
    }
    private boolean immersivePlayback;
    private void applySystemBars() {
        View decor = getWindow().getDecorView();
        if (Build.VERSION.SDK_INT >= 28) {
            WindowManager.LayoutParams layout = getWindow().getAttributes();
            int cutoutMode = immersivePlayback
                ? WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
                : WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT;
            if (layout.layoutInDisplayCutoutMode != cutoutMode) {
                layout.layoutInDisplayCutoutMode = cutoutMode;
                getWindow().setAttributes(layout);
            }
        }
        getWindow().setStatusBarColor(android.graphics.Color.BLACK);
        getWindow().setNavigationBarColor(android.graphics.Color.BLACK);
        if (Build.VERSION.SDK_INT >= 29) getWindow().setNavigationBarContrastEnforced(false);
        if (immersivePlayback) getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        else getWindow().clearFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        // Qt and OEM window handling can replace legacy visibility on rotation.
        // Apply both APIs to the final decor, after Qt has updated its surface.
        decor.setSystemUiVisibility(immersivePlayback
            ? View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
            : View.SYSTEM_UI_FLAG_VISIBLE);
        if (Build.VERSION.SDK_INT >= 30) {
            getWindow().setDecorFitsSystemWindows(!immersivePlayback);
            WindowInsetsController controller = getWindow().getInsetsController();
            if (controller != null) {
                controller.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
                if (immersivePlayback) controller.hide(WindowInsets.Type.systemBars());
                else controller.show(WindowInsets.Type.systemBars());
            }
        }
    }
    private void reapplySystemBars() {
        getWindow().getDecorView().post(this::applySystemBars);
        getWindow().getDecorView().postDelayed(this::applySystemBars, 250);
    }
    @Override public void onConfigurationChanged(android.content.res.Configuration config) {
        super.onConfigurationChanged(config);
        reapplySystemBars();
    }
    @Override public void onResume() { visible = true; super.onResume(); reapplySystemBars(); restoreAfterPiP(); }
    @Override public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) reapplySystemBars();
    }
    public static void setPlayerWindow(float brightness, boolean keepAwake, int orientation, boolean immersive) {
        JatayuActivity activity = current; if (activity == null) return;
        activity.runOnUiThread(() -> {
            WindowManager.LayoutParams params = activity.getWindow().getAttributes();
            params.screenBrightness = brightness < 0 ? -1f : Math.max(0.05f, Math.min(1f, brightness));
            activity.getWindow().setAttributes(params);
            if (keepAwake) activity.getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            else activity.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            int requested = orientation == 1 ? ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
                : orientation == 2 ? ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT
                : orientation == 3 ? ActivityInfo.SCREEN_ORIENTATION_LOCKED
                : orientation == 4 ? ActivityInfo.SCREEN_ORIENTATION_FULL_SENSOR
                : ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;
            if (activity.getRequestedOrientation() != requested) activity.setRequestedOrientation(requested);
            activity.immersivePlayback = immersive;
            activity.reapplySystemBars();
        });
    }
}
