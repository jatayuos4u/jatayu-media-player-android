package io.github.jatayuos4u.jatayumediaplayer;

import android.app.*;
import android.content.*;
import android.content.pm.ServiceInfo;
import android.media.*;
import android.media.session.*;
import android.os.*;
import org.json.JSONObject;

/** The service owns Android focus, media session, notification and wake lock.
 * Decoding stays in the existing Qt engine in this same process. Never restart
 * an orphan service/player after process death or start from boot. */
public final class JatayuPlaybackService extends Service {
    private static final String CHANNEL = "music_playback";
    private static final int NOTIFICATION = 6042;
    private static final Handler MAIN = new Handler(Looper.getMainLooper());
    private static Context context;
    private static JatayuPlaybackService current;
    private static volatile boolean nativeReady;
    private static native void nativeAction(String action, long value);
    private final PlaybackPolicy policy = new PlaybackPolicy();
    private AudioManager manager;
    private AudioFocusRequest focus;
    private boolean focused, foreground, playing, music, valid;
    private MediaSession session;
    private PowerManager.WakeLock wake;
    private JSONObject state = new JSONObject();
    private long token;
    private String lastNotification = "";
    private final Runnable idle = () -> { if (!playing) { stopForeground(STOP_FOREGROUND_REMOVE); stopSelf(); } };
    private final Runnable demote = () -> {
        if (!playing && !policy.awaitingFocus()) {
            stopForeground(STOP_FOREGROUND_DETACH); foreground = false;
            if (!policy.awaitingFocus()) abandon();
        }
    };
    private final Runnable renewWake = new Runnable() {
        @Override public void run() { if (playing && (music || JatayuActivity.inPiP())) { wake.acquire(600000); MAIN.postDelayed(this, 300000); } }
    };
    public static void activityEvent(String action) { emit(action, 0); }
    public static void activityBounds(int width, int height) { emit("pip-restore", ((long)width << 32) | (height & 0xffffffffL)); }
    public static void initialize(Context value) { context = value.getApplicationContext(); }
    public static void setNativeReady(boolean ready) { nativeReady = ready; }
    private static void emit(String action, long value) {
        if (!nativeReady) return;
        try { nativeAction(action, value); } catch (UnsatisfiedLinkError unavailable) { nativeReady = false; }
    }
    public static void dispatch(String action, long generation, String json) {
        MAIN.post(() -> {
            if (context == null) return;
            if (action.equals("state")) JatayuActivity.videoState(json);
            if (action.equals("pip")) { JatayuActivity.enterPiP(); return; }
            if (action.equals("background")) { JatayuActivity.backgroundTask(); return; }
            if (action.equals("prepare")) {
                try {
                    JSONObject data = new JSONObject(json);
                    Intent intent = new Intent(context, JatayuPlaybackService.class).setAction("prepare")
                        .putExtra("token", generation).putExtra("state", json);
                    if (data.optBoolean("music")) context.startForegroundService(intent); else context.startService(intent);
                } catch (Exception refused) { emit("denied", generation); }
                return;
            }
            if (current == null) return;
            if (action.equals("shutdown")) { current.finish(); return; }
            if (action.equals("release")) {
                current.policy.cancel(); current.abandon();
                if (!current.playing) { MAIN.removeCallbacks(current.demote); MAIN.postDelayed(current.demote, 3000); MAIN.removeCallbacks(current.idle); MAIN.postDelayed(current.idle, 600000); }
            }
            if (action.equals("cancel")) { current.token = generation; current.policy.cancel(); }
            if (action.equals("state")) current.update(json, generation);
        });
    }
    @Override public void onCreate() {
        super.onCreate(); current = this; initialize(this);
        manager = (AudioManager)getSystemService(AUDIO_SERVICE);
        focus = new AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN)
            .setAudioAttributes(new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_MEDIA)
                .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC).build())
            .setWillPauseWhenDucked(true).setAcceptsDelayedFocusGain(false)
            .setOnAudioFocusChangeListener(change -> {
                if (change == AudioManager.AUDIOFOCUS_GAIN) {
                    focused = true;
                    if (policy.regain()) { try { promote(); emit("ready", token); } catch (Exception refused) { emit("denied", token); } }
                } else if (change == AudioManager.AUDIOFOCUS_LOSS) {
                    focused = false; policy.cancel(); emit("lost", token);
                } else if (change == AudioManager.AUDIOFOCUS_LOSS_TRANSIENT || change == AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK) {
                    focused = false; policy.transientLoss(playing); emit("interrupt", token);
                }
            }, MAIN).build();
        wake = ((PowerManager)getSystemService(POWER_SERVICE)).newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "JatayuMediaPlayer:music");
        wake.setReferenceCounted(false);
        session = new MediaSession(this, "Jatayu Media Player");
        session.setCallback(new MediaSession.Callback() {
            @Override public void onPlay() { emit("play", 0); }
            @Override public void onPause() { emit("pause", 0); }
            @Override public void onStop() { emit("stop", 0); finish(); }
            @Override public void onSkipToNext() { if (state.optBoolean("next")) emit("next", 0); }
            @Override public void onSkipToPrevious() { if (state.optBoolean("previous")) emit("previous", 0); }
            @Override public void onSeekTo(long position) { if (state.optBoolean("seekable")) emit("seek", position); }
        }, MAIN);
        session.setSessionActivity(openApp());
        getSystemService(NotificationManager.class).createNotificationChannel(new NotificationChannel(CHANNEL, "Music playback", NotificationManager.IMPORTANCE_LOW));
        IntentFilter noisy = new IntentFilter(AudioManager.ACTION_AUDIO_BECOMING_NOISY);
        if (Build.VERSION.SDK_INT >= 33) registerReceiver(noisyReceiver, noisy, Context.RECEIVER_NOT_EXPORTED); else registerReceiver(noisyReceiver, noisy);
    }
    private final BroadcastReceiver noisyReceiver = new BroadcastReceiver() {
        @Override public void onReceive(Context c, Intent i) { policy.cancel(); emit("pause", 0); }
    };
    @Override public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent == null || !nativeReady) { finish(); return START_NOT_STICKY; }
        String action = intent.getAction();
        if ("prepare".equals(action)) {
            token = intent.getLongExtra("token", 0);
            try { state = new JSONObject(intent.getStringExtra("state")); } catch (Exception ignored) { finish(); return START_NOT_STICKY; }
            music = state.optBoolean("music"); valid = state.optBoolean("valid");
            if (!valid || (!music && !JatayuActivity.isVisible() && !JatayuActivity.inPiP())) { emit("denied", token); finish(); return START_NOT_STICKY; }
            policy.play();
            try {
                if (music || JatayuActivity.inPiP()) promote();
                if (!focused) focused = manager.requestAudioFocus(focus) == AudioManager.AUDIOFOCUS_REQUEST_GRANTED;
                if (focused) { session.setActive(true); emit("ready", token); }
                else { policy.cancel(); emit("denied", token); finish(); }
            } catch (Exception refused) { policy.cancel(); emit("denied", token); finish(); }
        } else if ("stop".equals(action)) { emit("stop", 0); finish(); }
        else if ("play".equals(action) || "next".equals(action) || "previous".equals(action)) {
            // These PendingIntents are user-initiated foreground-service starts.
            if (!valid) { finish(); return START_NOT_STICKY; }
            try { promote(); emit(action, 0); } catch (Exception refused) { emit("denied", token); finish(); }
        } else if ("pause".equals(action)) emit("pause", 0);
        return START_NOT_STICKY;
    }
    private void promote() {
        MAIN.removeCallbacks(demote); MAIN.removeCallbacks(idle);
        if (!music && !JatayuActivity.inPiP()) return;
        Notification notification = notification();
        if (Build.VERSION.SDK_INT >= 29) startForeground(NOTIFICATION, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK);
        else startForeground(NOTIFICATION, notification);
        foreground = true;
    }
    private void update(String json, long generation) {
        try { state = new JSONObject(json); } catch (Exception invalid) { return; }
        token = generation; playing = state.optBoolean("playing"); music = state.optBoolean("music"); valid = state.optBoolean("valid");
        if (!valid) { finish(); return; }
        long actions = PlaybackState.ACTION_PLAY | PlaybackState.ACTION_PAUSE | PlaybackState.ACTION_PLAY_PAUSE | PlaybackState.ACTION_STOP;
        if (state.optBoolean("next")) actions |= PlaybackState.ACTION_SKIP_TO_NEXT;
        if (state.optBoolean("previous")) actions |= PlaybackState.ACTION_SKIP_TO_PREVIOUS;
        if (state.optBoolean("seekable")) actions |= PlaybackState.ACTION_SEEK_TO;
        session.setMetadata(new MediaMetadata.Builder().putString(MediaMetadata.METADATA_KEY_TITLE, state.optString("title", "Jatayu Media Player"))
            .putString(MediaMetadata.METADATA_KEY_ARTIST, state.optString("artist", "Local music"))
            .putLong(MediaMetadata.METADATA_KEY_DURATION, state.optLong("duration")).build());
        session.setPlaybackState(new PlaybackState.Builder().setActions(actions)
            .setState(playing ? PlaybackState.STATE_PLAYING : PlaybackState.STATE_PAUSED,
                state.optLong("position"), playing ? (float)state.optDouble("speed", 1) : 0, SystemClock.elapsedRealtime()).build());
        session.setActive(true);
        MAIN.removeCallbacks(demote); MAIN.removeCallbacks(idle);
        if (playing && (music || JatayuActivity.inPiP())) {
            if (!foreground) { try { promote(); } catch (Exception denied) { emit("denied", token); return; } }
            if (!wake.isHeld()) renewWake.run();
        }
        else {
            MAIN.removeCallbacks(renewWake);
            if (wake.isHeld()) wake.release();
            MAIN.postDelayed(demote, 3000); MAIN.postDelayed(idle, 600000);
        }
        if (!music && !JatayuActivity.inPiP()) { stopForeground(STOP_FOREGROUND_REMOVE); foreground = false; return; }
        String signature = state.optString("title") + "\n" + state.optString("artist") + "\n" + state.optString("playLabel") + playing + state.optBoolean("next") + state.optBoolean("previous");
        if (!signature.equals(lastNotification)) {
            lastNotification = signature;
            getSystemService(NotificationManager.class).notify(NOTIFICATION, notification());
        }
    }
    private PendingIntent openApp() {
        return PendingIntent.getActivity(this, 0, new Intent(this, JatayuActivity.class).setAction(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_LAUNCHER).addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP | Intent.FLAG_ACTIVITY_CLEAR_TOP), PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
    }
    private Notification.Action action(String name, String label, int icon) {
        Intent intent = new Intent(this, JatayuPlaybackService.class).setAction(name);
        boolean starts = name.equals("play") || name.equals("next") || name.equals("previous");
        PendingIntent pending = starts ? PendingIntent.getForegroundService(this, name.hashCode(), intent, PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE)
            : PendingIntent.getService(this, name.hashCode(), intent, PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
        return new Notification.Action.Builder(icon, label, pending).build();
    }
    private Notification notification() {
        Notification.Builder b = new Notification.Builder(this, CHANNEL).setSmallIcon(R.drawable.ic_media_notification)
            .setContentTitle(state.optString("title", "Jatayu Media Player")).setContentText(state.optString("artist", "Local music"))
            .setContentIntent(openApp()).setVisibility(Notification.VISIBILITY_PUBLIC).setOnlyAlertOnce(true).setShowWhen(false)
            .setCategory(Notification.CATEGORY_TRANSPORT).setOngoing(playing);
        int playIndex = 0;
        if (state.optBoolean("previous")) { b.addAction(action("previous", state.optString("previousLabel", "Previous"), android.R.drawable.ic_media_previous)); playIndex++; }
        b.addAction(action(playing ? "pause" : "play", playing ? state.optString("pauseLabel", "Pause") : state.optString("playLabel", "Play"), playing ? android.R.drawable.ic_media_pause : android.R.drawable.ic_media_play));
        if (state.optBoolean("next")) b.addAction(action("next", state.optString("nextLabel", "Next"), android.R.drawable.ic_media_next));
        b.addAction(action("stop", state.optString("stopLabel", "Stop"), android.R.drawable.ic_menu_close_clear_cancel));
        b.setStyle(new Notification.MediaStyle().setMediaSession(session.getSessionToken()).setShowActionsInCompactView(playIndex));
        return b.build();
    }
    private void abandon() { if (manager != null && focus != null) manager.abandonAudioFocusRequest(focus); focused = false; }
    private void finish() { policy.cancel(); abandon(); stopForeground(STOP_FOREGROUND_REMOVE); stopSelf(); }
    @Override public void onTaskRemoved(Intent root) { emit("stop", 0); finish(); }
    @Override public void onDestroy() {
        emit("serviceStopped", token);
        MAIN.removeCallbacks(demote); MAIN.removeCallbacks(idle); MAIN.removeCallbacks(renewWake);
        policy.cancel(); abandon(); if (wake != null && wake.isHeld()) wake.release();
        if (session != null) session.release();
        try { unregisterReceiver(noisyReceiver); } catch (IllegalArgumentException ignored) { }
        if (current == this) current = null;
        super.onDestroy();
    }
    @Override public IBinder onBind(Intent intent) { return null; }
}
