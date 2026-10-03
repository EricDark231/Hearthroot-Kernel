### AnyKernel3 Ramdisk Mod Script
## Hearthroot Kernel
## Developer: EricDark231
## Device: POCO X7 Pro (rodin)
## Based on AnyKernel3 by osm0sis

### AnyKernel setup
properties() { '
kernel.string=Hearthroot @KERNEL_RELEASE@ for rodin (ReSukiSU)
do.devicecheck=1
do.modules=0
do.systemless=0
do.cleanup=1
do.cleanuponabort=0
do.check_boot_version=0
device.name1=rodin
device.name2=
device.name3=
device.name4=
device.name5=
supported.versions=
supported.patchlevels=
supported.vendorpatchlevels=
keycheck.timeout=10
'; }

### Boot configuration
block=boot
is_slot_device=auto
ramdisk_compression=auto
patch_vbmeta_flag=auto
no_magisk_check=1

### Import AnyKernel3
. tools/ak3-core.sh

### Hearthroot Installer

ui_print " "
ui_print "========================================"
ui_print " "
ui_print "             /\\       /\\"
ui_print "            /  \\_____/  \\"
ui_print "           /             \\"
ui_print "          /      /\\       \\"
ui_print "                 ||"
ui_print "                 ||"
ui_print "              ___||___"
ui_print "            _/   ||   \\_"
ui_print "          _/    /  \\    \\_"
ui_print " "
ui_print "        H E A R T H R O O T"
ui_print " "
ui_print "========================================"
ui_print " "
ui_print " Kernel : Hearthroot"
ui_print " Device : POCO X7 Pro (rodin)"
ui_print " Dev    : EricDark231"
ui_print " Root   : ReSukiSU"
ui_print " "
ui_print "  Adaptive by nature. Rooted in balance."
ui_print " "
ui_print "========================================"
ui_print " "

### GKI compatibility check
kernel_version=$(cat /proc/version | awk -F '-' '{print $1}' | awk '{print $3}')

case "$kernel_version" in
    5.10*|5.15*|6.1*|6.6*|6.12*)
        ksu_supported=true
        ;;
    *)
        ksu_supported=false
        ;;
esac

ui_print "[+] Current kernel: $kernel_version"

$ksu_supported || abort "[!] Unsupported non-GKI kernel."

ui_print "[+] Device verified: rodin"
ui_print "[+] Preparing boot image..."
ui_print "[+] Planting Hearthroot..."
ui_print " "

### Boot installation
split_boot

if [ -f "$SPLITIMG/ramdisk.cpio" ]; then
    ui_print "[+] Ramdisk detected."
    ui_print "[+] Repacking boot image..."
    unpack_ramdisk
    write_boot
else
    ui_print "[+] No ramdisk detected."
    ui_print "[+] Flashing kernel..."
    flash_boot
fi

ui_print " "
ui_print "========================================"
ui_print " "
ui_print "      Hearthroot installation complete"
ui_print " "
ui_print "      Developer: EricDark231"
ui_print " "
ui_print "             Ready to grow."
ui_print " "
ui_print "========================================"
ui_print " "
