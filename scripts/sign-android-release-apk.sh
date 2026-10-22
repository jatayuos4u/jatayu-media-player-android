#!/usr/bin/env bash
# Production signing only: use an owner-controlled key kept outside the checkout.
set -euo pipefail
umask 077
repo_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
: "${ANDROID_SDK_ROOT:?Set Android SDK path}"
: "${JATAYU_RELEASE_KEYSTORE:?Set owner-controlled production keystore path}"
: "${JATAYU_RELEASE_ALIAS:?Set production key alias}"
: "${JATAYU_RELEASE_STORE_PASS:?Supply through environment, not command arguments}"
: "${JATAYU_RELEASE_KEY_PASS:?Supply through environment, not command arguments}"
: "${JATAYU_RELEASE_CERT_SHA256:?Set independently recorded production certificate SHA256}"
input="$(realpath -- "${1:?Unsigned 1.0 release APK path}")"
output="${2:?Output path ending in Jatayu-Media-Player-Android-1.0.apk}"
[[ -f "$input" && -f "$JATAYU_RELEASE_KEYSTORE" ]] || { echo 'APK or keystore missing' >&2; exit 1; }
[[ ! -e "$output" ]] || { echo 'Refusing to overwrite an existing release artifact' >&2; exit 1; }
[[ "$(basename -- "$output")" == Jatayu-Media-Player-Android-1.0.apk ]] || { echo 'Unexpected release filename' >&2; exit 1; }
[[ "$JATAYU_RELEASE_ALIAS" != androiddebugkey ]] || { echo 'Development alias is not allowed' >&2; exit 1; }
cert="${JATAYU_RELEASE_CERT_SHA256,,}"
cert="${cert//:/}"
[[ "$cert" =~ ^[0-9a-f]{64}$ ]] || { echo 'Expected certificate must be a SHA256 digest' >&2; exit 1; }
[[ "$cert" != 39957d25539e01a203083b5aaaeee99ff7e1d6075602e9e392c19d57e29db03d ]] || { echo 'Known development certificate is not allowed' >&2; exit 1; }
tools="$ANDROID_SDK_ROOT/build-tools/36.0.0"
for tool in zipalign apksigner aapt; do
    [[ -x "$tools/$tool" ]] || { echo "Missing Android build tool: $tool" >&2; exit 1; }
done
output_dir="$(dirname -- "$output")"
[[ -d "$output_dir" ]] || { echo 'Output directory must already exist' >&2; exit 1; }
stage="$(mktemp -d "$output_dir/.jatayu-release.XXXXXXXX")"
trap 'rm -rf -- "$stage"' EXIT
"$tools/zipalign" -f -P 16 4 "$input" "$stage/aligned.apk"
"$tools/apksigner" sign --ks "$JATAYU_RELEASE_KEYSTORE" --ks-key-alias "$JATAYU_RELEASE_ALIAS" \
    --ks-pass env:JATAYU_RELEASE_STORE_PASS --key-pass env:JATAYU_RELEASE_KEY_PASS \
    --out "$stage/signed.apk" "$stage/aligned.apk"
python3 "$repo_dir/scripts/validate-android-apk.py" "$stage/signed.apk" \
    --build-tools "$tools" --hardened --require-16k --require-release-notices \
    --expected-version-name 1.0 --expected-version-code 19 --expected-signing-cert "$cert"
# Atomic no-clobber publication on the same filesystem, after validation.
ln -- "$stage/signed.apk" "$output"
sha256sum -- "$output"
