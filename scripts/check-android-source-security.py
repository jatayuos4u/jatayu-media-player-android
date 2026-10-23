#!/usr/bin/env python3
"""Fail-closed checks for Android source security invariants; not a vulnerability scanner."""
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "android/package/AndroidManifest.xml"
ANDROID = "{http://schemas.android.com/apk/res/android}"
errors = []
def require(ok, why):
    if not ok:
        errors.append(why)

manifest = ET.parse(MANIFEST).getroot()
application = manifest.find("application")
require(application is not None, "Missing application")
permissions = {p.get(ANDROID + "name", "") for p in manifest.findall("uses-permission")}
for disallowed in ("android.permission.INTERNET", "android.permission.CAMERA",
                   "android.permission.RECORD_AUDIO", "android.permission.ACCESS_FINE_LOCATION",
                   "android.permission.ACCESS_COARSE_LOCATION", "android.permission.MANAGE_EXTERNAL_STORAGE",
                   "android.permission.REQUEST_INSTALL_PACKAGES"):
    require(disallowed not in permissions, "Unexpected high-risk permission: " + disallowed)
if application is not None:
    require(application.get(ANDROID + "allowBackup") == "false", "Backups must be disabled")
    require(application.get(ANDROID + "usesCleartextTraffic") == "false", "Cleartext must be disabled")
    for kind in ("activity", "service", "receiver", "provider"):
        for component in application.findall(kind):
            name = component.get(ANDROID + "name", "(unnamed)")
            exported = component.get(ANDROID + "exported")
            require(exported in ("true", "false"), f"{kind} {name}: explicit android:exported required")
            if kind in ("service", "provider", "receiver"):
                require(exported == "false", f"{kind} {name}: must not be exported")
            if kind == "activity" and exported == "true":
                filters = component.findall("intent-filter")
                launcher = any(
                    any(a.get(ANDROID + "name") == "android.intent.action.MAIN" for a in fl.findall("action"))
                    and any(c.get(ANDROID + "name") == "android.intent.category.LAUNCHER" for c in fl.findall("category"))
                    for fl in filters
                )
                require(launcher, f"Exported activity {name} must be launcher only")
    require(application.get(ANDROID + "debuggable") != "true", "Debuggable production manifest")

gitignore = (ROOT / ".gitignore").read_text(encoding="utf-8")
for pattern in ("*.jks", "*.keystore", "*.p12", "*.pfx", "local.properties", "*.env"):
    require(pattern in gitignore, "Missing key/secret ignore pattern: " + pattern)

cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
require('QT_ANDROID_PACKAGE_SOURCE_DIR' in cmake, "Expected Android package source not configured")
require('QT_ANDROID_VERSION_CODE 19' in cmake, "Unexpected Android release version code")
require('QT_ANDROID_MIN_SDK_VERSION 28' in cmake, "Unexpected Android minimum SDK")
require('QT_ANDROID_TARGET_SDK_VERSION 36' in cmake, "Unexpected Android target SDK")
for flag in ("-fstack-protector-strong", "-z,relro", "-z,now", "-z,noexecstack"):
    require(flag in cmake, "Missing hardening flag " + flag)

if errors:
    print("Android security source checks FAILED:")
    for item in errors:
        print(" - " + item)
    sys.exit(1)
print("Android security source invariants PASS. This is NOT a secret scan, runtime test, or security certification.")
