# Jatayu Media Player Android 1.0.2

Stable maintenance update for Android 9+ ARM64 phones and tablets.

## Fixed
- Videos with **Resume where I left off** enabled could reopen at the beginning instead of the saved timestamp. Playback now waits for the media decoder to confirm the restored seek position.
- Prevent transient zero-position events from replacing the saved checkpoint.

## Version and verification
- Version name: 1.0.2; version code: 21
- Application ID: `io.github.jatayuos4u.jatayumediaplayer`
- Filename: `Jatayu-Media-Player-Android-1.0.2.apk`
- SHA256 supplied by the maintainer for the locally signed APK:
  `90f121d3cfe7c73f9038b4c716a1fc53ba6cd3dfad0b4c637803d19085a62b35`
- Production signing certificate SHA256:
  `62fa39e2bd37a94fe4386370b37c12664df3f0a035ca75c9f9951d9dac1fc2ac`
- APK signature scheme v3 verified with Android Build Tools 36.0.0 according to the maintainer's terminal output.
- The maintainer reports the resume bug resolved on their Android handset.

## Updating
Install the production-signed APK over the existing Android 1.0 installation without uninstalling it, to preserve local app data. Back up study notes and other important local information before upgrading. This APK is distributed separately from Linux and Android TV.

## Sources and licensing
Reviewed source is available in this repository. Original app code is GPL-3.0-or-later; third-party components retain their own terms.

**Release gate:** These notes record the verified local candidate. The matching APK must be uploaded and its downloaded digest checked before marking the GitHub Release published.
