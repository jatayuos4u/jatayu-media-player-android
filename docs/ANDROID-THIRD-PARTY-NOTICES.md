# Jatayu Media Player Android 1.0 — credits and notices

Created by **Anup Kumar Yadav** · A **JatayuOS** project · https://jatayuos.in/.
Original application source is **GPL-3.0-or-later**; see [LICENSE](../LICENSE).
Third-party components retain their own copyright and licence terms.

## Components and source

- **Qt 6.11.3**: Core, GUI, QML/Quick, Controls/Dialogs/Layouts, Multimedia, SVG,
  Shader Tools, translations and their distributed plugins/dependencies.
  Qt module-specific GPL/LGPL alternatives and third-party terms are retained.
  Exact official build archives: 6.11.3-0-202610220312; source inventory accompanies
  the release. https://code.qt.io/cgit/qt/ and https://doc.qt.io/qt-6/licensing.html.
- **FFmpeg 7.1.5**: avcodec, avformat, avutil, swresample and swscale supplied
  by the Qt Android kit, LGPL-2.1-or-later. Exact binary configuration and upstream
  source archive are included in the dependency-source package. Qt Multimedia
  supplies the FFmpeg symbol stubs/build instructions. The crypto/ssl stubs are
  not an OpenSSL implementation. https://ffmpeg.org/legal.html.
- **AndroidX, Kotlin/coroutines, JetBrains annotations, Guava ListenableFuture
  and JSpecify**: exact resolved runtime versions, upstream POM licences and
  matching source JARs accompany the release. The metadata-only coroutines BOM
  contains no runtime source JAR. See [component list](THIRD-PARTY-NOTICES.txt).
- **Android NDK r27c libc++/libc++abi**: LLVM/Apache-2.0 exceptions and the full
  accompanying NDK/toolchain notices are preserved.

## Full licence texts

[android/package/assets/THIRD-PARTY-LICENCES.zip](../android/package/assets/THIRD-PARTY-LICENCES.zip)
contains full upstream licence, copyright and attribution files; the same file
is included in the APK and as a release download. It conservatively includes
optional/example/tooling notices without claiming all that code ships in the APK.
The release also provides reviewed application source, build instructions,
dependency source package, public certificate and SHA256SUMS.

The bird artwork is shared with Jatayu Media Player's Linux edition. Original
project notices describe it as artwork generated specifically for the project.
No exclusive trademark clearance or rights to unrelated marks are asserted.
No third-party font/icon pack, sample songs or sample videos are bundled.

## Privacy

No ads, accounts, in-app analytics or Internet permission. Media discovery uses
read permissions; the system picker can grant access to specific files. Local
library state, playback progress and notes stay in app storage; user-requested
exports go to their chosen destination. Help/source links open an external browser.
See [privacy policy](PRIVACY-POLICY.md).
