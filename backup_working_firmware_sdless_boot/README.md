# Backup Firmware Binaries (Artillery Genius Pro / MKS_GD_TFT28_V1_2_4)

1. **`MKSTFT28EVO_working_sdless_boot.bin` (Latest Working Production Version)**
   - Size: 211,400 bytes
   - Boots reliably with or without an SD card inserted (`SD_CD_PIN` hardware detect floating fix).
   - Thumbnail streaming desync & race condition fix (pixel 4900 trailer wait, slot tagging `M987 P<slot>`, strict 8-byte header validation).
   - Long filename (`M20 L`) full support.
   - Print popup shows 70x70 model thumbnail and multiline filename.

2. **`MKSTFT28EVO_working_sdless_boot_old.bin` (Previous Working Milestone)**
   - Size: 211,368 bytes
   - Initial verified SD-less boot version prior to slot-tagging and thumbnail trailer race condition patch.
