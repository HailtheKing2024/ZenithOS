#!/bin/bash
export ZENITH_APT_SIGNING_KEY="86DB09CA6DAE6823"
cd /mnt/c/Users/krish/ZenithOS/OS
make clean 2>/dev/null || true
make workstation-rootfs
BUILD_STATUS=$?
if [ $BUILD_STATUS -eq 0 ]; then
    echo "✅ Rootfs built successfully!"
    make workstation-seed-iso
    if [ $? -eq 0 ]; then
        echo "🎉 ISO BUILD COMPLETE!"
        ls -lh build/workstation/zenithos-seed.iso
    else
        echo "❌ ISO build failed"
        exit 1
    fi
else
    echo "❌ Rootfs build failed"
    exit 1
fi