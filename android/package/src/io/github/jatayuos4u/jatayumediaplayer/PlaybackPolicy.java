package io.github.jatayuos4u.jatayumediaplayer;

/** Pure transport intent, shared by the service and host transition tests. */
final class PlaybackPolicy {
    private boolean wanted, interrupted;
    void play() { wanted = true; interrupted = false; }
    void cancel() { wanted = false; interrupted = false; }
    void transientLoss(boolean playing) { interrupted = wanted && (interrupted || playing); }
    boolean regain() { boolean resume = wanted && interrupted; interrupted = false; return resume; }
    boolean awaitingFocus() { return wanted && interrupted; }
}
