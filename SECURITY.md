# Security policy

This policy covers Jatayu Media Player for Android smartphones and tablets. A passing build
or security check is not a security certification or a penetration test.

## Reporting a concern

Contact the creator through <https://jatayuos.in/support/>. Include the Android
and app versions, the APK SHA256, steps to reproduce, and whether a sample file
is needed. Do not post private media, personal library data, passwords or
signing keys in a public issue. Coordinate handling of exploit samples before
sharing them. No response-time guarantee is made for this independent project.

## Boundaries

- Local media playback only; no Internet, microphone, camera, contacts or
  location permissions. Website links open an external browser on user action.
- Android media discovery needs user-granted read access. Selected document
  providers can themselves use cloud services; selecting their files is not
  a guarantee that the provider works offline.
- Library/history/notes use app-local storage, without additional app-level
  encryption. This is not a private vault. User-requested note exports leave
  the app's private storage. Artwork cache can remain after clearing history.
- Android sandboxing does not protect against a compromised/rooted operating
  system. Decoder vulnerabilities remain possible despite input limits.

## Release and update verification

Use a non-debuggable build and an owner-controlled production signing key,
kept outside source control with a verified encrypted backup. Never use the
development signing helper for public releases. Signatures and updates must
be planned before switching keys; existing test installations cannot normally
accept an APK signed with an unrelated new key.

Review current Qt, FFmpeg, AndroidX and toolchain advisories; retain exact
dependency versions and source/licence notices. Run the hardened APK checks
with the expected signing-certificate SHA256, then perform independent static
and dynamic review, permission-revocation tests, device/codec tests and
malformed-file/fuzz testing. Small regression corpora do not replace fuzzing.
Keep release signing, publication and source review under the project owner’s control.
