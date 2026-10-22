# Android 1.0 validation — 22 October 2026

Version name 1.0, code 19; ARM64; min API 28; target/compile API 36.
Based on the tested 0.10.2 playback implementation. Release-only changes set the
version and link About/help/licences to this public repository; no new playback
features are introduced. Full third-party licence files are packaged as assets.

The native C++/QML Android application compiled, and Gradle assembleRelease
succeeded. All 13 host suites passed with generated 1 kHz MP3 and 1920x1080 video
fixtures in 24.29 seconds before the external About-link update. That update's
UI smoke check then passed in 19.67 seconds. Cases needing an audio output/crossfade
fixtures are skipped on this headless host; they are not hardware validation.

The standalone DSP suite also passed: Flat transparency, EQ response/headroom,
stereo lookahead limiting at five sample rates, Original alignment, segmentation
and preset continuity. Prior device testing of the development app was reported
by the creator. No phone or tablet is connected for independent testing of this
production-signed package; Bluetooth/headsets, PiP/permissions/provider access and
crossfade/Studio behaviour remain dependent on real-device verification.

Final signature, manifest, CRC, ARM64 payload and 16 KB alignment results are
recorded in the release's APK-VALIDATION.txt. The exact APK checksum is in
SHA256SUMS. Build/structure checks are not a security certification.
