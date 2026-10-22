#!/usr/bin/env python3
"""Validate an ARM64 APK and optional release hardening without claiming device coverage."""
import argparse
import hashlib
import os
from pathlib import Path
import struct
import subprocess
import zipfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('apk', type=Path)
parser.add_argument('--expected-version-name', help='Required manifest versionName')
parser.add_argument('--expected-version-code', type=int, help='Required manifest versionCode')
parser.add_argument('--require-release-notices', action='store_true', help='Require bundled full licence assets')
parser.add_argument('--require-16k', action='store_true', help='Fail if native libraries lack 16 KB LOAD alignment')
parser.add_argument('--hardened', action='store_true', help='Require a non-debuggable APK and privacy/native protections')
parser.add_argument('--expected-signing-cert', help='Required public signing certificate SHA256 for this APK')
parser.add_argument('--build-tools', type=Path, default=Path(os.environ.get('ANDROID_SDK_ROOT', '/opt/android-sdk')) / 'build-tools/36.0.0')
args = parser.parse_args()

def run(tool, *arguments):
    result = subprocess.run([str(args.build_tools / tool), *map(str, arguments)], check=True, text=True, capture_output=True)
    print(result.stdout)
    return result.stdout

badging = run('aapt', 'dump', 'badging', args.apk)
assert "package: name='io.github.jatayuos4u.jatayumediaplayer'" in badging
if args.expected_version_name:
    assert f"versionName='{args.expected_version_name}'" in badging, 'Version name mismatch'
if args.expected_version_code is not None:
    assert f"versionCode='{args.expected_version_code}'" in badging, 'Version code mismatch'
assert "application-label:'Jatayu Media Player'" in badging
assert "sdkVersion:'28'" in badging
assert "targetSdkVersion:'36'" in badging
assert "native-code: 'arm64-v8a'" in badging
assert 'launchable-activity:' in badging
manifest = run('aapt', 'dump', 'xmltree', args.apk, 'AndroidManifest.xml')
assert 'org.qtproject.qt.android.bindings.QtApplication' in manifest
assert 'android.app.lib_name' in manifest
assert 'android.intent.category.LEANBACK_LAUNCHER' not in manifest
assert 'android.permission.INTERNET' not in manifest
assert 'android.permission.MANAGE_EXTERNAL_STORAGE' not in manifest
assert 'io.github.jatayuos4u.jatayumediaplayer.JatayuActivity' in manifest
assert 'android.permission.READ_EXTERNAL_STORAGE' in manifest
legacy_permission = manifest.split('android.permission.READ_EXTERNAL_STORAGE', 1)[1].split('E: uses-permission', 1)[0]
assert 'android:maxSdkVersion' in legacy_permission and '0x20' in legacy_permission
assert 'android.permission.FOREGROUND_SERVICE' in manifest
assert 'android.permission.FOREGROUND_SERVICE_MEDIA_PLAYBACK' in manifest
assert 'android.permission.WAKE_LOCK' in manifest
assert 'io.github.jatayuos4u.jatayumediaplayer.JatayuPlaybackService' in manifest
service = manifest.split('E: service', 1)[1].split('E: provider', 1)[0]
assert 'android:exported' in service and '(type 0x12)0x0' in service
assert 'android:foregroundServiceType' in service and '0x2' in service
assert 'android.app.background_running' in manifest
assert 'android.permission.READ_MEDIA_AUDIO' in manifest
assert 'android.permission.READ_MEDIA_VIDEO' in manifest
assert 'android.permission.READ_MEDIA_VISUAL_USER_SELECTED' in manifest
assert 'android.permission.WRITE_EXTERNAL_STORAGE' not in manifest
run('apksigner', 'verify', '--verbose', args.apk)
certificate = run('apksigner', 'verify', '--print-certs', args.apk)
if args.expected_signing_cert:
    assert f'certificate SHA-256 digest: {args.expected_signing_cert.lower()}' in certificate, 'Signing certificate mismatch'
if args.hardened:
    assert 'application-debuggable' not in badging, 'Debuggable APK is not hardened'
    import re
    for attribute in ('allowBackup', 'usesCleartextTraffic'):
        assert re.search(r'android:' + attribute + r'[^\n]*\(type 0x12\)0x0\b', manifest), attribute
    assert 'android:dataExtractionRules' in manifest
    provider = manifest.split('E: provider', 1)[1].split('E: ', 1)[0]
    assert re.search(r'android:exported[^\n]*\(type 0x12\)0x0\b', provider), 'FileProvider must not be exported'
    for permission in ('RECORD_AUDIO', 'CAMERA', 'READ_CONTACTS', 'ACCESS_FINE_LOCATION', 'REQUEST_INSTALL_PACKAGES'):
        assert 'android.permission.' + permission not in manifest
    resources = run('aapt', 'dump', '--values', 'resources', args.apk)
    rule_file = re.search(r'(?m)^\s*resource [^\n]*:xml/data_extraction_rules:[^\n]*\n\s*\(string8\) "([^"]+)"', resources)
    assert rule_file, 'Missing extraction policy resource'
    rules = run('aapt', 'dump', 'xmltree', args.apk, rule_file.group(1))
    assert 'E: cloud-backup' in rules and 'E: device-transfer' in rules
run('zipalign', '-c', '4', args.apk)
with zipfile.ZipFile(args.apk) as archive:
    assert archive.testzip() is None, 'APK CRC failure'
    if args.require_release_notices:
        import io
        for name in ('assets/LICENSE.txt', 'assets/THIRD-PARTY-NOTICES.txt', 'assets/THIRD-PARTY-LICENCES.zip'):
            assert name in archive.namelist(), f'Missing release licence asset: {name}'
        with zipfile.ZipFile(io.BytesIO(archive.read('assets/THIRD-PARTY-LICENCES.zip'))) as notices:
            assert notices.testzip() is None, 'Licence ZIP CRC failure'
            assert 'LICENSE.txt' in notices.namelist() and 'runtime-source-inventory.json' in notices.namelist()
    dex = b''.join(archive.read(name) for name in archive.namelist() if name.endswith('.dex'))
    assert b'Lio/github/jatayuos4u/jatayumediaplayer/JatayuActivity;' in dex, 'Custom activity missing from DEX'
    assert b'nativeScanFinished' in dex and b'requestMediaScan' in dex, 'Android discovery bridge missing'
    assert b'identifyMedia' in dex, 'Android media identity bridge missing'
    assert b'openMediaDescriptor' in dex, 'Android playback descriptor bridge missing'
    libs = [n for n in archive.namelist() if n.startswith('lib/') and n.endswith('.so')]
    assert libs, 'No native libraries'
    assert all(n.startswith('lib/arm64-v8a/') for n in libs), 'Unexpected native ABI'
    page_4k = []
    for name in libs:
        data = archive.read(name)
        assert data[:4] == b'\x7fELF' and data[4] == 2 and data[5] == 1, name
        assert struct.unpack_from('<H', data, 18)[0] == 183, name
        phoff = struct.unpack_from('<Q', data, 32)[0]
        phsize, phnum = struct.unpack_from('<HH', data, 54)
        if args.hardened:
            headers = [struct.unpack_from('<IIQQQQQQ', data, phoff + i * phsize) for i in range(phnum)]
            assert not any(kind == 1 and flags & 3 == 3 for kind, flags, *_ in headers), f'Writable executable segment: {name}'
            stack = [flags for kind, flags, *_ in headers if kind == 0x6474e551]
            assert stack and all(flags & 1 == 0 for flags in stack), f'Executable or unspecified stack: {name}'
            assert any(kind == 0x6474e552 for kind, *_ in headers), f'No RELRO: {name}'
            if 'libjatayu-media-player-android' in name:
                # Eager binding is requested for the application's native module.
                dynamic = next(header for header in headers if header[0] == 2)
                tags = [struct.unpack_from('<qQ', data, offset) for offset in range(dynamic[2], dynamic[2] + dynamic[5], 16)]
                assert any(tag == 24 or (tag == 30 and value & 8) or (tag == 0x6ffffffb and value & 1) for tag, value in tags), 'Application lacks BIND_NOW'
        if any(struct.unpack_from('<I', data, phoff + i * phsize)[0] == 1 and
               struct.unpack_from('<Q', data, phoff + i * phsize + 48)[0] < 16384
               for i in range(phnum)):
            page_4k.append(name)
    assert any('jatayu-media-player-android' in n for n in libs)
    assert any('Multimedia' in n for n in libs)
    print(f'Checked {len(libs)} AArch64 ELF libraries; ZIP CRCs passed')
    if page_4k:
        assert not args.require_16k, f'{len(page_4k)} libraries lack 16 KB LOAD alignment'
        print(f'LIMITATION: {len(page_4k)} libraries have LOAD alignment below 16 KB; use a 4 KB page-size device for this preview')
        print('A newer Qt/NDK toolchain and rebuilt dependencies are required before claiming 16 KB device support')
    else:
        run('zipalign', '-c', '-P', '16', '4', args.apk)
        print('All ELF LOAD segments and uncompressed native ZIP entries meet 16 KB alignment; device compatibility is not established by alignment alone')
print('SHA256', hashlib.sha256(args.apk.read_bytes()).hexdigest())
print('PASS: package, launcher, ARM64 payload, manifest, signature and ZIP alignment')
print('NOT TESTED: installation, rendering, picker access, codecs and playback on devices')

