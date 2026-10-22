#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
: "${QT_ANDROID_ROOT:?Set to Qt 6.11.3 android_arm64_v8a directory}"
: "${QT_HOST_PATH:?Set to matching Qt 6.11.3 gcc_64 directory}"
: "${ANDROID_SDK_ROOT:?Set to Android SDK directory}"
export ANDROID_NDK_ROOT="${ANDROID_NDK_ROOT:-$ANDROID_SDK_ROOT/ndk/27.2.12479018}"
export JAVA_HOME="${JAVA_HOME:-/usr/lib/jvm/java-21-openjdk-amd64}"
export PATH="$JAVA_HOME/bin:$PATH"
build_dir="${BUILD_DIR:-$repo_dir/build/android-arm64}"
"$QT_ANDROID_ROOT/bin/qt-cmake" -S "$repo_dir" -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE:-Release}" -DQT_HOST_PATH="$QT_HOST_PATH" \
    -DANDROID_SDK_ROOT="$ANDROID_SDK_ROOT" -DANDROID_NDK_ROOT="$ANDROID_NDK_ROOT" \
    -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-28 \
    -DQT_ANDROID_COMPILE_SDK_VERSION=36 -DQT_ANDROID_BUILD_TOOLS_REVISION=36.0.0 -DQT_ANDROID_BUILD_ALL_ABIS=OFF
if [[ "${CLEAN_ANDROID_PACKAGE:-0}" == 1 ]]; then
    # Only discard generated Gradle outputs, retaining sources, caches and keys.
    cmake -E rm -rf "$build_dir/android-build/build"
    cmake -E rm -f "$build_dir/android-build/jatayu-media-player-android.apk"
fi
cmake --build "$build_dir" --target apk --parallel "${BUILD_JOBS:-2}"

if [[ "${BUILD_PLAY_BUNDLE:-0}" == 1 ]]; then
    cmake --build "$build_dir" --target aab --parallel "${BUILD_JOBS:-2}"
fi
