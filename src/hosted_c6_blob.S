.section .rodata.hosted_c6,"a"
.balign 16
.global hosted_c6_firmware_start
.global hosted_c6_firmware_end
hosted_c6_firmware_start:
.incbin "data/hosted/esp32c6-v2.12.3.bin"
hosted_c6_firmware_end:
.balign 4
