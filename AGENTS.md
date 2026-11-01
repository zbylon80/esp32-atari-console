# Project instructions

- Target: classic ESP32 WROVER ECO3, 16 MB flash, PSRAM; pinned Bluepad32 4.1.0.
- Start with README.md and docs/getting-started.md. Public sources must build
  without game files and without the original Atari ROM. MIT covers own code;
  preserve every dependency's notices and license. No bundled/downloaded games.
- Never commit local/, .tools/, .build/, credentials, compiled firmware, ROMs,
  games, backups, device identifiers or machine-specific absolute paths.
- Build one firmware at a time. Use tools/build.py; preserve partition and
  NVS guards, 11 renderer IRAM checks and flash placement of SD-only SPI HAL.
- Initial installation is a separate explicit action for a new board only.
  Never initialize/repartition/erase a working console during normal updates.
- Preserve native game options, current menu, View/Menu meanings, A/LT/RT,
  d-pad/stick duplication and persistent player identities. No auto-game probes.
- SD reads occur at scan/game loading only. Detach all core references,
  including in-progress XEX loader FILE, before freeing a prior game cache.
- Keep filenames English ASCII. Polish UI and beginner documentation are welcome.
