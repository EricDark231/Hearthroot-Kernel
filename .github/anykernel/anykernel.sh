### AnyKernel3 Ramdisk Mod Script
## Framework: osm0sis
## Hearthroot No Root - POCO X7 Pro (rodin)

properties() { '
kernel.string=Hearthroot No Root
do.devicecheck=0
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

block=boot
is_slot_device=auto
ramdisk_compression=auto
patch_vbmeta_flag=0
no_magisk_check=1

. tools/ak3-core.sh

# Explicit device verification.
device="$(getprop ro.product.device)"
vendor_device="$(getprop ro.product.vendor.device)"

if [ "$device" != "rodin" ] && [ "$vendor_device" != "rodin" ]; then
    abort "Unsupported device: ${device:-unknown}. Expected rodin."
fi

ui_print " "
ui_print "Hearthroot No Root"
ui_print "Device: POCO X7 Pro (rodin)"
ui_print "Target: boot partition"
ui_print " "

split_boot
flash_boot

ui_print " "
ui_print "Hearthroot No Root installation completed."
ui_print " "
