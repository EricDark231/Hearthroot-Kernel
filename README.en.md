# Xiaomi Rodin Kernel Enhance

[![Kernel](https://img.shields.io/badge/kernel-6.6.x_LTS-blue)](https://kernel.org)
[![Base](https://img.shields.io/badge/base-MiCode%20bsp--rodin--v--oss-orange)](https://github.com/MiCode/Xiaomi_Kernel_OpenSource/tree/bsp-rodin-v-oss)
[![Root](https://img.shields.io/badge/Root-SukiSU--Ultra-green)](https://github.com/SukiSU-Ultra/SukiSU-Ultra)
[![SuSFS](https://img.shields.io/badge/Hiding-SuSFS-green)](https://gitlab.com/simonpunk/susfs4ksu)
[![License](https://img.shields.io/badge/license-GPL--2.0-lightgrey)](LICENSES)

English | [简体中文](README.md)

An enhanced Android kernel for Xiaomi **rodin** devices, built on top of Xiaomi's released [`bsp-rodin-v-oss`](https://github.com/MiCode/Xiaomi_Kernel_OpenSource/tree/bsp-rodin-v-oss) kernel source (AOSP `android15-6.6` GKI / MediaTek) and kept in sync with the latest **6.6 LTS**.

## Highlights

### Root & Hiding
- **SukiSU-Ultra** built in (`drivers/kernelsu`), auto-tracked to upstream stable release tags by CI (see "Automation" below)
- **SuSFS** built-in, integrated from [`omajili-manbu/susfs4ksu` (`gki-android15-6.6-mod`)](https://github.com/omajili-manbu/susfs4ksu/tree/gki-android15-6.6-mod)
- Able to hide suspicious SELinux contexts/rules from apps, tied to the SuSFS AVC log spoofing switch

### Brick Protection
- **Baseband-guard (BBG)** LSM: blocks unauthorized writes to critical partitions/device nodes at the kernel level ([vc-teahouse/Baseband-guard](https://github.com/vc-teahouse/Baseband-guard), allowlist adjusted for rodin)

### Performance
- Cortex-A725 compiler tuning (clang 19+)
- **ThinLTO** link-time optimization
- **AutoFDO** link-time optimization guided by real-world profiles (clang 17+)
- **BBRv3** as the default TCP congestion control, with **fq** as the companion queueing discipline
- **ZSTD** upgraded to v1.5.7
- ZRAM built-in with a full compression algorithm set, default LZ4

### Stability & Fixes
- Fixed probabilistic boot hang and restored vendor module compatibility
- Backported upstream fixes on top of Xiaomi's official kernel

The tuning philosophy is **balancing performance and battery life** — all gains come from compile-time optimizations, an up-to-date kernel, updated algorithms, and bug fixes on top of Xiaomi's official kernel, with no aggressive tweaks biased toward either side.

## Automation

- Daily merge of the AOSP `android15-6.6-lts` upstream into the tracked BSP branches (matrix over `bsp-rodin-v-upstream` and `bsp-rodin-v-upstream-noroot`); conflicts open an issue automatically
- Daily tracking of [SukiSU-Ultra](https://github.com/SukiSU-Ultra/SukiSU-Ultra) stable release tags, auto-merged into `drivers/kernelsu`; unclean merges abort the push and open an issue

## Supported Devices

| Device | Codename | OS |
|---|---|---|
| Redmi Turbo 4 | rodin | HyperOS 3 (Android 16) |
| POCO X7 Pro | rodin | HyperOS 3 (Android 16) |

> **Note:** Untested on POCO hardware, but it should work — Xiaomi ships the same kernel source for the POCO and Redmi variants. If you run into any issues, feel free to open an issue with logs attached, and I'll try to fix it.

## Branches

- [`bsp-rodin-v-upstream`](https://github.com/omajili-manbu/Xiaomi_Rodin_Kernel_Enhance/tree/bsp-rodin-v-upstream) — this branch, the main build branch: Xiaomi's official rodin source with additional backports and enhancements (SukiSU-Ultra and SuSFS built in); prebuilt images are built from it
- [`bsp-rodin-v-upstream-noroot`](https://github.com/omajili-manbu/Xiaomi_Rodin_Kernel_Enhance/tree/bsp-rodin-v-upstream-noroot) — the no-root variant of this branch: SukiSU-Ultra and SuSFS removed, upstream semantics restored; builds ship the AnyKernel3 flashable zip only
- [`bsp-rodin-c-rebase`](https://github.com/omajili-manbu/Xiaomi_Rodin_Kernel_Enhance/tree/bsp-rodin-c-rebase) — 6.18 kernel baseline rebuild (in progress; see its own README)
- [`bsp-rodin-c-port`](https://github.com/omajili-manbu/Xiaomi_Rodin_Kernel_Enhance/tree/bsp-rodin-c-port) — running 6.6 vendor modules on the 6.18 kernel (in progress)
- [`c-rebase-16k`](https://github.com/omajili-manbu/Xiaomi_Rodin_Kernel_Enhance/tree/c-rebase-16k) — 16K page size experiment on the 6.18 tree (frozen)
- [`rodin-device-special`](https://github.com/omajili-manbu/Xiaomi_Rodin_Kernel_Enhance/tree/rodin-device-special) — rodin device customization files
- [`rodin-lz4-v1.10.0-backport`](https://github.com/omajili-manbu/Xiaomi_Rodin_Kernel_Enhance/tree/rodin-lz4-v1.10.0-backport) — LZ4 v1.10.0 backport
- [`dash-w-oss`](https://github.com/omajili-manbu/Xiaomi_Rodin_Kernel_Enhance/tree/dash-w-oss) — REDMI Turbo 5 Max (Android W) kernel

## Roadmap

- HyperOS 4 support is planned

## Downloads & Support

- [Some build guides (Chinese)](BUILD-GUIDE.md)
- Prebuilt images: [Releases](https://github.com/omajili-manbu/Xiaomi_Rodin_Kernel_Enhance/releases)
- Release tag versioning: routine updates bump the third version digit, feature improvements bump the second (e.g. v1.2.6 » v1.3)
- Bug reports: open an [issue](https://github.com/omajili-manbu/Xiaomi_Rodin_Kernel_Enhance/issues) with kernel logs attached
- If you like this project, please consider giving it a Star to support me!

## Acknowledgements

- [MiCode/Xiaomi_Kernel_OpenSource](https://github.com/MiCode/Xiaomi_Kernel_OpenSource) — official rodin kernel source (`bsp-rodin-v-oss`)
- [SukiSU-Ultra](https://github.com/SukiSU-Ultra/SukiSU-Ultra) / [KernelSU](https://github.com/tiann/KernelSU) — root solution
- [SuSFS](https://gitlab.com/simonpunk/susfs4ksu) — root hiding
- [Baseband-guard](https://github.com/vc-teahouse/Baseband-guard) — brick protection
- [Linux-Patches](https://gitlab.com/xanmod/linux-patches) — BBRv3 patches
- AOSP `android15-6.6` / upstream Linux 6.6 LTS

## License

This repository is licensed under **GPL-2.0**, following the Linux kernel and the upstream sources it is based on. See [LICENSES](LICENSES) for details.
