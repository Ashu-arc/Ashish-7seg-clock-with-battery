#!/bin/bash
# usage: bash collect_bins.sh 6d    (run from the folder that contains "wled")
set -e
D=$1
mkdir -p out
cd wled
NAME="Ashish_Battery_Clock_${D}_WLED_0.14.4"
cp .pio/build/ashish_bat_${D}_esp8266/firmware.bin "../out/${NAME}_ESP8266_4MB.bin"
cp .pio/build/ashish_bat_${D}_esp32/firmware.bin "../out/${NAME}_ESP32_update.bin"
B=.pio/build/ashish_bat_${D}_esp32
BOOT=$(find ~/.platformio/packages -name boot_app0.bin | head -1)
/tmp/esptoolenv/bin/esptool.py --chip esp32 merge_bin -o "../out/${NAME}_ESP32_first_install.bin" --flash_mode dio --flash_size 4MB 0x1000 $B/bootloader.bin 0x8000 $B/partitions.bin 0xe000 "$BOOT" 0x10000 $B/firmware.bin
ls -la ../out
