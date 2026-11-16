#!/bin/bash
# usage: bash collect_bins.sh 6d    (run from the folder that contains "wled")
D=$1
mkdir -p out
cd wled
NAME="Ashish_Battery_Clock_${D}_WLED_0.14.4"
set -e
cp .pio/build/ashish_bat_${D}_esp8266/firmware.bin "../out/${NAME}_ESP8266_4MB.bin"
cp .pio/build/ashish_bat_${D}_esp32/firmware.bin "../out/${NAME}_ESP32_update.bin"
set +e

# ---- ESP32 first-install file (bootloader + partitions + boot_app0 + firmware in one bin) ----
B=.pio/build/ashish_bat_${D}_esp32
find ~/.platformio/packages -maxdepth 6 -name "bootloader*.bin" | head -5
LOADER=""
[ -f "$B/bootloader.bin" ] && LOADER="$B/bootloader.bin"
[ -z "$LOADER" ] && LOADER=$(find ~/.platformio/packages -name "bootloader_dio_40m.bin" | head -1)
[ -z "$LOADER" ] && LOADER=$(find ~/.platformio/packages -name "bootloader*.bin" | head -1)
PART=""
[ -f "$B/partitions.bin" ] && PART="$B/partitions.bin"
if [ -z "$PART" ]; then
  GEN=$(find ~/.platformio/packages -name gen_esp32part.py | head -1)
  python3 "$GEN" tools/WLED_ESP32_4MB_1MB_FS.csv /tmp/partitions.bin && PART=/tmp/partitions.bin
fi
BOOT=$(find ~/.platformio/packages -name boot_app0.bin | head -1)
echo "bootloader=$LOADER partitions=$PART boot_app0=$BOOT"
if [ -f "$LOADER" ] && [ -f "$PART" ] && [ -f "$BOOT" ]; then
  /tmp/esptoolenv/bin/esptool.py --chip esp32 merge_bin -o "../out/${NAME}_ESP32_first_install.bin" --flash_mode dio --flash_size 4MB 0x1000 "$LOADER" 0x8000 "$PART" 0xe000 "$BOOT" 0x10000 "$B/firmware.bin" || echo "first-install file failed (not needed for updates)"
else
  echo "first-install file skipped: a boot file is missing"
fi
ls -la ../out
exit 0
