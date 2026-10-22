# Build Jatayu Media Player Android 1.0

## Inputs

- Qt 6.11.3 Android ARM64-v8a plus matching Linux gcc_64 host tools; include base,
  declarative/Quick/Controls/Dialogs, SVG, Multimedia, Shader Tools and translations.
- Android SDK platform 36, build tools 36.0.0 and NDK 27.2.12479018 (r27c).
- JDK 21, CMake 3.21+ and Ninja; Gradle 9.3.1 / Android Gradle Plugin 9.0.0.
- Runtime Maven coordinates and exact source-archive hashes are in the release
  dependency-source package. No system-wide Qt or Linux libmpv is used.

The Qt installer or aqtinstall can install these kits. Newer Android packages use
host all_os, while the build host kit uses linux desktop:

```sh
python3 -m aqt install-qt linux desktop 6.11.3 linux_gcc_64 -m qtmultimedia qtshadertools -O "$HOME/Qt"
python3 -m aqt install-qt all_os android 6.11.3 android_arm64_v8a -m qtmultimedia qtshadertools -O "$HOME/Qt"
```

Install the SDK/NDK through the official Android tools under terms you accept.
Set QT_ANDROID_ROOT, QT_HOST_PATH, ANDROID_SDK_ROOT and JAVA_HOME to your kit paths.
Use matching host tools, preserve executable modes and NDK symlinks, and ensure
Qt's relocatable qt.conf/target_qt.conf point at the installed host and target.

```sh
bash scripts/build-android-arm64.sh
```

The default Release build produces an unsigned, non-debuggable APK. If Java needs
an HTTP proxy, pass its host/port as JVM system properties to Gradle; Java does
not automatically honour HTTPS_PROXY. Do not put proxy credentials in git.
The generated Qt project can also be assembled with Gradle 9.3.1 directly after
androiddeployqt stages the libraries, manifest and assets. Network access is
needed for first-time Gradle/Maven dependencies. The source archive is not an APK.

## Production signing

Keep your keystore outside this checkout, with a private backup. Never upload it
or its password to a release. Use the public certificate fingerprint to verify
identity. Provide JATAYU_RELEASE_KEYSTORE, JATAYU_RELEASE_ALIAS,
JATAYU_RELEASE_STORE_PASS, JATAYU_RELEASE_KEY_PASS and JATAYU_RELEASE_CERT_SHA256
through the local environment without shell tracing or plaintext history.

```sh
bash scripts/sign-android-release-apk.sh UNSIGNED_APK OUTPUT_DIR/Jatayu-Media-Player-Android-1.0.apk
```

The helper rejects the development certificate and validates package, version 1.0,
version code 19, minimum/target API, hardened manifest, ARM64 libraries, ZIP CRCs,
16 KB alignment and the expected certificate. It does not perform phone testing.

## Host checks

Configure the matching host Qt kit with JATAYU_BUILD_TESTS=ON. Build and run ctest
with QT_QPA_PLATFORM=offscreen and QT_QUICK_BACKEND=software. A 1 kHz MP3 fixture
is required for JATAYU_TEST_MP3; JATAYU_TEST_VIDEO must be 1920x1080. Audio-output
and crossfade integration tests additionally require an actual/virtual output,
JATAYU_TEST_AUDIO_OUTPUT and suitable second-track/format fixtures. Do not present
skipped integration tests as coverage of an audio device or Android hardware.

The dependency-source package includes official module sources, runtime source
JARs/POMs, exact binary/source archive inventories, FFmpeg configuration and full
licences. Keep it alongside the matching APK. Qt modules can be rebuilt using
the included source versions and Qt's configure/build instructions; FFmpeg's
upstream source and Qt's FFmpeg stubs/build instructions are included there.
