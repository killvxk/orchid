#!/bin/bash
set -e
set -o pipefail
echo y | "${ANDROID_HOME}"/cmdline-tools/latest/bin/sdkmanager \
    "build-tools;30.0.3" \
    "ndk;27.0.12077973" \
    "platforms;android-34" \
>/dev/null
