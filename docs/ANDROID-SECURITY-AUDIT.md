# Android source security checks

Run locally from the repository root:

```bash
python3 scripts/check-android-source-security.py
```

This zero-dependency script parses the source manifest and checks a deliberately conservative permission/export policy, application backup/cleartext/debug settings, key ignore patterns, version settings and selected native hardening flags.

**Limits:** This is not a historical secret scan, SAST/DAST, a compiled APK inspection, dependency-vulnerability analysis, penetration test or runtime verification. Gradle/Qt may merge additional manifest entries into the final APK. Continue to inspect the **actual signed APK** with existing signing and manifest validation tools and perform real-device testing. Version assertions are specific to Android 1.0 and should be reviewed when deliberately releasing a new version.

GitHub account settings (secret scanning, push protection, private vulnerability reporting, branch rulesets, Dependabot alerts) require owner-side confirmation in the GitHub UI. The installed GitHub connection does not offer settings read/write actions to prove or enable them. Do not treat these controls as enabled without confirming.
