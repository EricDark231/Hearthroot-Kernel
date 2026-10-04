<h1 align="center">Hearthroot Kernel</h1>

<p align="center">
  <b>A carefully tended Android kernel for the POCO X7 Pro / Redmi Turbo 4 (rodin).</b>
</p>

<p align="center">
  Built from Xiaomi's open-source kernel, rooted in Android Common Kernel,
  and maintained with a focus on stability, compatibility and sensible improvements.
</p>

---

## About Hearthroot

**Hearthroot** is a custom Android kernel for `rodin`, the platform shared by the
POCO X7 Pro and Redmi Turbo 4.

The project began as a fork of
[Xiaomi Rodin Kernel Enhance](https://github.com/omajili-manbu/Xiaomi_Rodin_Kernel_Enhance)
and has since grown into its own maintained kernel tree.

Rather than chasing version numbers or collecting aggressive performance tweaks,
Hearthroot follows a more conservative approach: maintain a reliable base,
integrate useful features carefully, preserve upstream work, and test changes
before they become part of a release.

Think of it as a tended branch rather than a race to the top of the tree.

---

## Features

### Root & SUSFS

- ReSukiSU integrated directly into the kernel
- SUSFS kernel support
- Kernel-level root implementation
- Support for SUSFS hiding functionality
- ReSukiSU Manager distributed alongside official Hearthroot releases
- Standard and spoofed Manager variants when available

Hearthroot keeps the kernel driver and recommended Manager version paired whenever
possible to prevent API and version mismatches.

### Performance & Kernel Improvements

- Cortex-A725 compiler optimizations
- ThinLTO
- AutoFDO support
- BBRv3 TCP congestion control
- `fq` queueing discipline
- ZRAM support
- LZ4 compression support
- ZSTD 1.5.7
- Selected upstream fixes and improvements
- Optimizations inherited from the rodin kernel base

Hearthroot does not use aggressive frequency, scheduler or thermal modifications
simply to produce higher benchmark numbers.

Changes are intended to preserve a sensible balance between performance, battery
life and everyday stability.

### Security & Protection

- Baseband-guard (BBG) LSM
- Protection against unauthorized writes to critical partitions and device nodes
- rodin-specific allowlist adjustments
- SELinux compatibility

---

## Kernel Information

| Component | Information |
| --- | --- |
| Devices | POCO X7 Pro / Redmi Turbo 4 |
| Codename | `rodin` |
| SoC | MediaTek Dimensity 8400 |
| Architecture | ARM64 |
| Kernel | Linux 6.6.144 |
| Kernel base | Android Common Kernel / Xiaomi |
| Android | Android 16 / Android 17 |
| Root implementation | ReSukiSU |
| SUSFS | 2.3.0 |
| Primary development device | POCO X7 Pro |

---

## Why Linux 6.6.144?

Hearthroot intentionally follows its tested Android, Xiaomi and MediaTek kernel
base rather than merging newer Linux stable versions solely to increase the
reported kernel version.

Android kernels contain significant changes from Android Common Kernel, device
vendors and SoC manufacturers. Newer Linux stable releases may therefore conflict
with Android, MediaTek or Xiaomi-specific implementations even when they belong
to the same Linux LTS series.

Kernel updates are evaluated before adoption rather than merged automatically.

**Stability comes before version chasing.**

---

## Supported Devices

| Device | Codename | Status |
| --- | --- | --- |
| POCO X7 Pro | `rodin` | Tested |
| Redmi Turbo 4 | `rodin` | Compatible |

The **POCO X7 Pro** is the primary development and physical testing device for
Hearthroot.

Because both devices share the `rodin` platform, Redmi Turbo 4 compatibility is
expected from the common kernel base. However, POCO X7 Pro remains the device on
which Hearthroot releases are primarily validated.

---

## Branches

### `hearthroot-dev`

Main Hearthroot development branch.

Kernel integrations, fixes, experiments and other changes are developed here
before being considered for a stable release.

A successful build from this branch should not automatically be considered a
stable Hearthroot release.

### `bsp-rodin-v-oss-upstream`

Upstream/base branch inherited from the original rodin kernel project.

It is retained as part of Hearthroot's upstream history and as a reference for
the kernel tree on which the project was built.

---

## Releases

Stable Hearthroot builds are published through:

**[Hearthroot Releases](https://github.com/EricDark231/Hearthroot-Kernel/releases)**

Official releases may contain:

- Hearthroot flashable ZIP
- Recommended ReSukiSU Manager
- Spoofed ReSukiSU Manager variant when available
- Source archives
- SHA-256 checksums for distributed binaries

### Hearthroot v1.0.0

`v1.0.0` represents the first tested public Hearthroot baseline.

The release was physically tested on the POCO X7 Pro with:

- Linux 6.6.144
- ReSukiSU kernel integration
- ReSukiSU Manager
- SUSFS 2.3.0
- SELinux Enforcing
- Android 16

Development builds and commits should not be treated as stable releases simply
because they compile successfully.

---

## Installation

> [!WARNING]
> Flashing a custom kernel always carries risk. Keep a copy of your working
> boot/kernel images and make sure you know how to restore your device before
> installing Hearthroot.

1. Download the latest Hearthroot ZIP from the
   [Releases](https://github.com/EricDark231/Hearthroot-Kernel/releases) page.

2. Keep a backup of the kernel/boot components currently working on your device.

3. Flash the Hearthroot package using a compatible kernel flashing environment.

4. Reboot the device.

5. Install the recommended ReSukiSU Manager distributed with the corresponding
   Hearthroot release when required.

6. Verify the kernel version, root status and SUSFS status after booting.

Do not mix kernel components or ReSukiSU Manager builds from unrelated releases
unless you know they are API-compatible.

---

## Development Philosophy

Hearthroot follows a small set of principles that guide development.

### Stability before novelty

A newer kernel patch level or a longer feature list does not automatically make
a better Android kernel.

Changes should be evaluated in the context of the device and its Android kernel
base.

### Changes should have a reason

Features and patches should solve a problem, improve compatibility, provide a
useful capability or offer a measurable benefit.

Hearthroot avoids collecting arbitrary tweaks simply to make the feature list
longer.

### Preserve upstream work

Hearthroot stands on the work of Xiaomi, MediaTek, Google, Linux contributors
and several independent kernel and root projects.

Whenever possible, original authorship and commit history should remain
traceable.

### Test before release

A successful CI build proves that the source can compile.

It does not prove that the kernel boots, that every subsystem works correctly,
or that the result is suitable for a stable release.

Physical testing remains part of the Hearthroot release process.

---

## Development Roadmap

Development following the first stable Hearthroot release is focused on
investigating areas where meaningful improvements may be possible.

Current areas of interest include:

- Scheduler and EAS behaviour on the MediaTek Dimensity 8400
- CPU scheduling and task placement
- Memory management
- ZRAM configuration and compression
- F2FS and storage I/O behaviour
- BBRv3 and networking configuration
- Power and efficiency behaviour
- Future ReSukiSU updates
- Future SUSFS updates
- Appropriate Android Common Kernel fixes
- Appropriate upstream Linux fixes
- Additional rodin-specific improvements

Items listed here are **areas of investigation, not promises for a future
release**.

Changes will only be integrated when they make sense for Hearthroot's kernel
base and can be tested appropriately.

---

## Building Hearthroot

Hearthroot can be compiled using the build configuration included in this
repository.

Official builds use the Android Clang toolchain and the configuration maintained
for the Hearthroot kernel tree.

For development, use:

```text
hearthroot-dev
