# Android repository security checklist

This repository is the public Android 1.0 source snapshot. The published signed APK, its signing identity and release artifacts **must remain unchanged** when hardening repository administration.

## GitHub Settings (owner actions required)

1. **Settings → Security → Code security and analysis**: enable secret scanning and push protection where GitHub makes them available. Enable Dependabot alerts; use dependency updates only where the dependency ecosystem is supported.
2. **Settings → Rules → Rulesets** (or Branch protection): protect `main`. Require pull requests, at least one approving review from a distinct reviewer when available, prevent force pushes and branch deletion, and restrict bypasses. A solo maintainer may need a review approach that does not block emergency patches.
3. **Settings → Actions → General**: keep GitHub Actions permissions minimal (read-only by default); permit only trusted actions. No release-signing secrets should be exposed to pull-request workflows. Do not enable workflows unless necessary.
4. **Settings → Collaborators and teams**: remove unused write/admin permissions, use phishing-resistant 2FA/passkeys, and review personal access tokens and GitHub App authorizations.
5. **Settings → Security → Private vulnerability reporting**: enable if offered; coordinate sensitive exploit information privately using SECURITY.md.
6. Audit past commits as well as the current working tree for credentials. Ignore rules only prevent accidental future adds; they do **not** purge already committed secrets. If a secret was ever published, revoke/rotate it and follow a carefully planned history-remediation process.

## Pull-request gate

- No keystores, private keys, passwords, tokens, `local.properties`, or signing credentials in changes, build logs or uploaded artifacts.
- No new Internet/camera/microphone/location permissions without a documented, explicit user-facing reason and review.
- Keep `android:debuggable=false` for production and avoid Android backup/exported-component regressions.
- Ensure source and output hashes, signing certificate identity, version codes and required third-party notices still match the release procedure.
- Confirm any update to JNI, Qt, FFmpeg, or Gradle dependencies against current security advisories and licenses.
- Test against malicious/corrupt media, invalid metadata, revoked storage grants, and unexpected lifecycle transitions.
- Publish a signed update only after independent checks and actual-device smoke tests.

## Release signature

The publicly recorded SHA256 certificate fingerprint is in `SIGNING-CERTIFICATE-SHA256.txt`. It is safe to publish the certificate fingerprint; **never** commit or share the matching private signing key. Back up that key encrypted and independently of the source repository.

This checklist records required actions; it does not claim the account settings above are already enabled, nor that a vulnerability scan has been completed.
