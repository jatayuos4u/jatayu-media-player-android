package io.github.jatayuos4u.jatayumediaplayer;
public final class PlaybackPolicyTest {
    private static void check(boolean value) { if (!value) throw new AssertionError("Playback intent transition failed"); }
    public static void main(String[] args) {
        PlaybackPolicy p = new PlaybackPolicy();
        check(!p.regain());
        p.play(); p.transientLoss(true); p.transientLoss(false); check(p.awaitingFocus()); check(p.regain()); check(!p.regain());
        p.play(); p.transientLoss(true); p.cancel(); check(!p.regain());
        p.play(); p.transientLoss(false); check(!p.regain());
        p.play(); p.transientLoss(true); p.cancel(); p.play(); check(!p.regain());
        System.out.println("PASS: transient resume, permanent/user cancellation, paused output and source replacement");
    }
}
