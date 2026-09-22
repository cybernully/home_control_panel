#!/usr/bin/env bash
set -euo pipefail
PIO="${PIO:-$HOME/.platformio/penv/bin/pio}"
"$PIO" run -e jc8012p4a1c_2624
"$PIO" run -e jc8012p4a1c_2635
printf '\n2624: .pio/build/jc8012p4a1c_2624/firmware.bin\n'
printf '2635: .pio/build/jc8012p4a1c_2635/firmware.bin\n'
