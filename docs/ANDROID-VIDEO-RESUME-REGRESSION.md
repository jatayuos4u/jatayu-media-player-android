# Android video resume regression checks

Related: #1. This branch is a proposed fix, **not a tested APK or released update**.

1. In Settings > Playback, enable **Resume where I left off**.
2. Open a locally stored 10-minute video. Pause at 02:15, navigate to the library, and reopen it. Playback should start near 02:15.
3. Repeat with Android Home/background and foreground; then with app relaunch.
4. Repeat using the same file from Continue Watching, playlists, a scanned library entry, and Android document picker (content URI).
5. Pause, wait 12 seconds, reopen; verify resume is accurate.
6. Pause near the beginning; verify it doesn't reset to zero during source teardown.
7. Finish the video normally; opening it again should start at zero rather than the end.
8. Disable Resume in settings; opening should start at zero.
9. Verify pause/play and next/previous queue, music background playback, Picture-in-Picture and subtitle controls still behave correctly.
10. Verify restored positions survive restarts without deleting user playlists, notes or library entries.

Before distribution: compile Android ARM64 from this branch, run host and physical-device tests, increment Android versionCode above 19, sign with the existing production key (kept offline), validate resulting APK and SHA256, then publish as a new patch release. **Never overwrite Android 1.0 binary with an unverified build.**
