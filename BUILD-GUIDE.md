> 📚 本篇收录两篇指南（可用 TOC 快速跳转）：
> - [① 生成 .config 的经验指南（编译：defconfig + gki_defconfig / clang22）](#guide-config)
> - [② 使用 GitHub Actions 工作流编译内核（免本地环境）](#guide-ci)

<a name="guide-config"></a>

# 生成 .config 的经验指南（Rodin / Xiaomi_Rodin_Kernel_Enhance）

本指南总结在用 `.config` 并基于 clang22 全量编内核时踩过的坑与解法，供以后复用，避免重复踩坑。

适用内核树：`~/rodin-build/kernel-rodin-merge`（Xiaomi Rodin 6.6.x 分支 `bsp-rodin-kernel-oss-bp`）
编译器：Android 预编译 clang22：`$HOME/rodin-build/prebuilts/clang/host/linux-x86/clang-r596125/bin`

### 只能用 gki_defconfig 生成用于编译的 .config ，而不是 defconfig + gki_defconfig 或者其他的组合，否则会生成错误配置！

## 编译命令模板

```bash
export PATH="$HOME/rodin-build/prebuilts/clang/host/linux-x86/clang-r596125/bin:$PATH"
cd ~/rodin-build/kernel-rodin-merge
make ARCH=arm64 LLVM=1 -j8 Image Image.lz4
```

- 产物：`arch/arm64/boot/Image`（未压缩）、`arch/arm64/boot/Image.lz4`（LZ4 legacy 压缩）。
- 验证 lz4 legacy：`xxd arch/arm64/boot/Image.lz4 | head -1` 魔数应为 `0221 4c18`（`0x184C2102`），即 LZ4 Legacy，**不是** `0422 4d18`（标准帧格式）。
- 版本号在 `include/config/kernel.release`。


> 最后更新：2026-09-10

<a name="guide-ci"></a>

# ② 使用 GitHub Actions 工作流编译内核（免本地环境）

**你可以使用工作流编译内核！** 不需要本地 WSL、工具链和几十 GB 源码：仓库自带的
`.github/workflows/build-kernel.yml` 在云端完成 clang-r596125 全量编译与
lz4 legacy 打包，产物下载即用。

## 触发方式

- 网页：仓库 **Actions** → 左侧选 **build-kernel** → **Run workflow** → 分支保持
  `bsp-rodin-v-oss-bp` → 点击运行；
- 命令行：`gh workflow run build-kernel.yml -R omajili-manbu/Xiaomi_Rodin_Kernel_Enhance --ref bsp-rodin-v-oss-bp`

该工作流为 `workflow_dispatch` 手动触发，不会因 push 或 schedule 自动运行。

## 它在云端做什么

1. 下载 AOSP 预编译 clang-r596125（googlesource `mirror-goog-main-llvm-toolchain-source`
   分支 tarball，失败自动回退固定 commit 直链 / sparse clone）；
2. 按指南①的结论生成配置：`make ARCH=arm64 LLVM=1 gki_defconfig`（只用 gki_defconfig），
   然后 `make ARCH=arm64 LLVM=1 -j"$(nproc)" Image Image.lz4`；
3. 校验 `Image.lz4` 魔数为 `02214c18`（LZ4 legacy）；若不是则自动用 `lz4 -l` 重压并复检；
4. 组装 AnyKernel3（WildKernels gki-2.0）：内核 blob 命名为 `Image`（设备只认这个名字），
   内容为 lz4 legacy 压缩的内核，并写入版本号到 kernel.string。

## 产物（run 页底部 1 个 artifact）

| Artifact | 内容与用法 |
|---|---|
| `Rodin-<版本>-AnyKernel3` | 可刷包本体。下载得到的 zip 就是 AK3 包（根目录 `anykernel.sh` / `Image` / `META-INF/`），TWRP 直接选中刷入，**无需先解包** |

## 注意

- 全量 ThinLTO 编译约 35–45 分钟（4 核 runner），任一步失败即中止，日志可直接定位到具体步骤。

> 最后更新：2026-09-13
