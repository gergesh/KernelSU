# KernelSU on Android Emulator (AVD) Guide

This guide explains how to run an Android emulator with KernelSU for development and testing.

## TL;DR - Quick Start

### Prerequisites
- Android SDK with emulator installed
- An ARM64 Android 15 (API 35) system image

### Steps

```bash
# 1. Download artifacts from GitHub Actions
# The AVD kernel workflow is available on forks that include the AVD build workflow.
# Example: https://github.com/gergesh/KernelSU/actions/workflows/build-kernel-avd-a15.yml
# Download the latest successful build artifact for arm64-v8a

# 2. Extract the artifact
unzip kernel-avd-android15-aarch64-*.zip -d ksu-kernel/

# 3. Create an AVD (if you don't have one)
sdkmanager "system-images;android-35;google_apis_playstore;arm64-v8a"
avdmanager create avd -n KernelSU_AVD -k "system-images;android-35;google_apis_playstore;arm64-v8a"

# 4. Create combined ramdisk
AVD_RAMDISK=~/Library/Android/sdk/system-images/android-35/google_apis_playstore/arm64-v8a/ramdisk.img
cat "$AVD_RAMDISK" ksu-kernel/initramfs.img > /tmp/combined-ramdisk.img

# 5. Launch emulator with KernelSU kernel
emulator @KernelSU_AVD \
  -kernel ksu-kernel/kernel-avd-android15-aarch64 \
  -ramdisk /tmp/combined-ramdisk.img \
  -no-snapshot-load \
  -gpu swiftshader_indirect

# 6. Install KernelSU Manager
curl -L -o /tmp/KernelSU-manager.apk \
  "https://github.com/tiann/KernelSU/releases/latest/download/KernelSU_v1.0.3_12372-release.apk"
adb install /tmp/KernelSU-manager.apk
```

---

## Detailed Guide

### Overview

Running KernelSU on an Android emulator requires:
1. A custom kernel with KernelSU built-in
2. Kernel modules compatible with that kernel (especially `virtio_blk` for disk access)
3. The emulator's ramdisk for boot configuration (fstab, init scripts)

The main challenge is **module compatibility** - the kernel modules must be built from the exact same source as the kernel, or they will fail to load with "disagrees about version of symbol" errors.

### Architecture Support

| Architecture | System Image | Kernel | Status |
|-------------|--------------|--------|--------|
| ARM64 | `android-35/google_apis_playstore/arm64-v8a` | `kernel-avd-android15-aarch64` | Supported |
| x86_64 | `android-35/google_apis_playstore/x86_64` | `kernel-avd-android15-x86_64` | Supported |

### Step 1: Get the Pre-built Kernel

#### Option A: Download from GitHub Actions (Recommended)

> **Note:** The upstream KernelSU repo does not include the AVD kernel workflow. You need to use a fork that has it, or add the workflow to your own fork.

1. Go to your fork's Actions page (e.g., [gergesh/KernelSU](https://github.com/gergesh/KernelSU/actions/workflows/build-kernel-avd-a15.yml))
2. Trigger the workflow manually (workflow_dispatch) and select your architecture
3. Once complete, download the artifact for your architecture:
   - `kernel-avd-android15-aarch64-XXXXX` for ARM64
   - `kernel-avd-android15-x86_64-XXXXX` for x86_64

The artifact contains:
- `kernel-avd-android15-{arch}` - The kernel image with KernelSU
- `initramfs.img` - Ramdisk with compatible kernel modules
- `modules/` - Individual `.ko` module files

#### Option B: Set Up Your Own Fork

To add AVD kernel support to your KernelSU fork, you need these files:

1. **Workflow**: [`.github/workflows/build-kernel-avd-a15.yml`](../.github/workflows/build-kernel-avd-a15.yml)
2. **Manifests**:
   - [`.github/manifests/android-15-avd_aarch64.xml`](../.github/manifests/android-15-avd_aarch64.xml)
   - [`.github/manifests/android-15-avd_x86_64.xml`](../.github/manifests/android-15-avd_x86_64.xml)

Copy these files to your fork and trigger the workflow via Actions > Build Kernel - Android 15 AVD.

#### Option C: Build Locally

See the [workflow file](../.github/workflows/build-kernel-avd-a15.yml) for the complete build process. Key steps:

```bash
# Clone kernel source with Android 15 AVD manifest
repo init -u https://android.googlesource.com/kernel/manifest
cp KernelSU/.github/manifests/android-15-avd_aarch64.xml .repo/manifests/default.xml
repo sync

# Add KernelSU to kernel
ln -sf /path/to/KernelSU/kernel common/drivers/kernelsu

# Build with virtual device target (includes compatible modules)
tools/bazel run --config=fast --config=stamp --lto=thin \
  //common-modules/virtual-device:virtual_device_aarch64_dist -- --dist_dir=dist
```

### Step 2: Set Up the Android Emulator

#### Install System Image

```bash
# For ARM64 (Apple Silicon Macs, ARM Linux)
sdkmanager "system-images;android-35;google_apis_playstore;arm64-v8a"

# For x86_64 (Intel Macs, x86 Linux/Windows)
sdkmanager "system-images;android-35;google_apis_playstore;x86_64"
```

#### Create AVD

```bash
avdmanager create avd \
  -n KernelSU_AVD \
  -k "system-images;android-35;google_apis_playstore;arm64-v8a" \
  -d pixel_6
```

Or use Android Studio's AVD Manager.

### Step 3: Create Combined Ramdisk

The emulator needs both:
- **AVD's ramdisk**: Contains `init`, fstab configuration, and boot scripts
- **Our initramfs**: Contains kernel modules compatible with our custom kernel

We concatenate them so our modules override the incompatible stock ones:

```bash
# Locate your AVD's ramdisk
AVD_RAMDISK=~/Library/Android/sdk/system-images/android-35/google_apis_playstore/arm64-v8a/ramdisk.img

# For Linux:
# AVD_RAMDISK=~/Android/Sdk/system-images/android-35/google_apis_playstore/arm64-v8a/ramdisk.img

# Concatenate: AVD ramdisk first, then our modules
cat "$AVD_RAMDISK" /path/to/ksu-kernel/initramfs.img > /tmp/combined-ramdisk.img
```

**Why this works:** The Linux kernel unpacks concatenated cpio archives in order. Files from later archives override earlier ones. By putting our `initramfs.img` second, our compatible `virtio_blk.ko` and other modules replace the incompatible ones from the AVD image.

### Step 4: Launch the Emulator

```bash
emulator @KernelSU_AVD \
  -kernel /path/to/ksu-kernel/kernel-avd-android15-aarch64 \
  -ramdisk /tmp/combined-ramdisk.img \
  -no-snapshot-load \
  -gpu swiftshader_indirect
```

**Flags explained:**
- `-kernel`: Use our custom KernelSU kernel instead of the prebuilt one
- `-ramdisk`: Use our combined ramdisk with compatible modules
- `-no-snapshot-load`: Start fresh (required when changing kernel)
- `-gpu swiftshader_indirect`: Software rendering (works on all hosts)

For debugging, add:
- `-show-kernel`: Show kernel boot messages in terminal
- `-verbose`: Show emulator debug output

### Step 5: Install KernelSU Manager

```bash
# Download latest manager APK
curl -L -o /tmp/KernelSU-manager.apk \
  "$(curl -s https://api.github.com/repos/tiann/KernelSU/releases/latest | grep browser_download_url | grep release.apk | head -1 | cut -d'"' -f4)"

# Install
adb install /tmp/KernelSU-manager.apk

# Launch
adb shell am start -n me.weishu.kernelsu/.ui.MainActivity
```

### Step 6: Verify Installation

```bash
# Check kernel version
adb shell cat /proc/version
# Should show: 6.6.17-android15-1-g... with KernelSU

# Check KernelSU in kernel log (from emulator terminal with -show-kernel)
# Look for: "KernelSU: feature: feature management initialized"
```

In the KernelSU Manager app, you should see:
- Working status (green checkmark)
- Kernel version: 12XXX (matches your build)
- Ability to grant root to apps

---

## Troubleshooting

### "virtio_blk: disagrees about version of symbol module_layout"

**Cause:** Module version mismatch between kernel and modules.

**Solution:** Use the combined ramdisk approach with `initramfs.img` from the same build as your kernel.

### "Failed to create FirstStageMount: failed to read default fstab"

**Cause:** Using only `initramfs.img` as ramdisk, which lacks the fstab configuration.

**Solution:** Create combined ramdisk with AVD's ramdisk first, then initramfs.img.

### Emulator stuck at "Run /init as init process"

**Cause:** Init cannot load virtio_blk module to access virtual disk.

**Solution:** Ensure combined ramdisk was created correctly with modules second.

### "adb devices" shows "offline"

**Cause:** Emulator still booting or boot failed.

**Solution:** Wait longer, or check `-show-kernel` output for errors.

### KernelSU Manager shows "Unsupported"

**Cause:** KernelSU not compiled into kernel or version mismatch.

**Solution:** Ensure you're using the kernel from the GitHub Actions build, not the stock emulator kernel.

---

## Creating a Launch Script

Save this as `launch-ksu-emulator.sh`:

```bash
#!/bin/bash
set -e

# Configuration
AVD_NAME="KernelSU_AVD"
KERNEL_DIR="$HOME/ksu-kernel"  # Adjust to your extracted artifact location
SDK_ROOT="${ANDROID_SDK_ROOT:-$HOME/Library/Android/sdk}"

# Paths
KERNEL="$KERNEL_DIR/kernel-avd-android15-aarch64"
INITRAMFS="$KERNEL_DIR/initramfs.img"
AVD_RAMDISK="$SDK_ROOT/system-images/android-35/google_apis_playstore/arm64-v8a/ramdisk.img"
COMBINED_RAMDISK="/tmp/combined-ramdisk-ksu.img"

# Create combined ramdisk
echo "Creating combined ramdisk..."
cat "$AVD_RAMDISK" "$INITRAMFS" > "$COMBINED_RAMDISK"

# Launch emulator
echo "Launching emulator with KernelSU..."
"$SDK_ROOT/emulator/emulator" @"$AVD_NAME" \
  -kernel "$KERNEL" \
  -ramdisk "$COMBINED_RAMDISK" \
  -no-snapshot-load \
  -gpu swiftshader_indirect \
  "$@"
```

Usage:
```bash
chmod +x launch-ksu-emulator.sh
./launch-ksu-emulator.sh              # Normal launch
./launch-ksu-emulator.sh -show-kernel # With kernel logs
```

---

## Required Files

To use the AVD emulator workflow, your fork needs these files:

| File | Description |
|------|-------------|
| [`.github/workflows/build-kernel-avd-a15.yml`](../.github/workflows/build-kernel-avd-a15.yml) | GitHub Actions workflow |
| [`.github/manifests/android-15-avd_aarch64.xml`](../.github/manifests/android-15-avd_aarch64.xml) | ARM64 kernel source manifest |
| [`.github/manifests/android-15-avd_x86_64.xml`](../.github/manifests/android-15-avd_x86_64.xml) | x86_64 kernel source manifest |

## References

- [KernelSU GitHub](https://github.com/tiann/KernelSU)
- [KernelSU Manager Releases](https://github.com/tiann/KernelSU/releases)
- [Android Kernel Build Documentation](https://source.android.com/docs/setup/build/building-kernels)

## Credits

- KernelSU by [tiann](https://github.com/tiann)
- Installation guides by [5ec1cff](https://blog.5ec1cff.cc/)
- AVD workflow and combined ramdisk approach documented here
