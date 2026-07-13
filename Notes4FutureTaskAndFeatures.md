AI to ignore this file except if prompt asks for new features or ideas. then mention this file and suggestions.
Security.
https?
Cloud/remote access?
web page. read write to sd card.
Send  data to external system device.
intergration to HomeAssistant.
Website set timezone.
Website add/change wifi ssid and password.
Website set host name.
Website change sampling interval and log flush interval, display refresh interval.
on NTP failure estimate time based on log files, if ntp is made have it recalibrate the estimated timestamps for the logs in the buffer sd card.

Graph/history data pagenated??
localhost all libs (uplot)
web pages stored currently in code other options: Flash, SPIFFS, or SD card?

sd Card File manipulation. (delete rename download)

display move the battery % to the 4th line with state reporting
# Completed Milestones

## Milestone 3: OTA, Stability & Calibrations (Completed)
1. **OTA Firmware Updates:** Implemented over-the-air firmware updates.
2. **Signal Stability & Stack Assembly:** Soldered components in stacked design (SD stability fixed).
3. **Battery State Tracking:** Used logged battery voltage history slope to determine `Chg` / `Dis` / `Ful` states.
4. **Solar Charging Study:** Feasibility study completed (calculated buck converter requirement).
5. **Detailed System Event Logging:** Integrated system event logs for boot, NTP failures/successes, and Wi-Fi transitions.
6. **Retroactive Timestamp Correction:** Implemented logic to calibrate estimated logs retroactively upon NTP synchronization.

## Milestone 4: Web settings, File manipulation, OLED line shift (Completed)
1. **Website Configuration Controls:** Persistent JSON configs saved in `/config.json` on SD, dynamic timezone, and interval updates.
2. **SD Card File Explorer:** Unified directory explorer supporting delete/rename/download on any files/folders.
3. **OLED Layout Shift:** Shifted battery percentage and charge state to line 4.
4. **Connection AP Fallback:** Automatic recovery AP mode with display indicator `W-/AP` and IP `192.168.4.1` on Wi-Fi loss.
5. **Latest Events Backward Scan:** Chronologically sorted timeline with full date-time and backward-file scanning.

---

# Milestone 5: Code Review, Refactoring & Memory Optimization

1. **Memory & Sizing Diagnostics:**
   - Break down dynamic versus static RAM allocations.
   - Evaluate sizing of Web Server client handlers, headers structures, and JSON document sizes (e.g. `StaticJsonDocument` vs heap-allocated `DynamicJsonDocument`).
   - Monitor stack depths and avoid stack overflows on ESP8266.
2. **Structural Critique & Decoupling:**
   - Standardize logging calls and error handlers.
   - Address separation of concerns: Decouple HTTP response routing in `WebManager.cpp` from the large PROGMEM-based HTML literal block (potentially hosting compiled templates or compressing with gzip).
   - Evaluate code style, comments, and consistency across components.
