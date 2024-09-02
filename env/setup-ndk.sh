#!/bin/bash
set -e
set -o pipefail
echo y | "${ANDROID_HOME}"/cmdline-tools/latest/bin/sdkmanager "build-tools;29.0.2" "ndk;27.0.12077973" "platforms;android-33" >/dev/null
