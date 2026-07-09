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
# Milestone 3 Tasks and Ideas

1. **OTA Firmware Updates:** Implement over-the-air firmware updates for the WeMos D1 mini Pro.
2. **Signal Stability & Stack Assembly:** Solder components together in a stacked shield design to fix mechanical jumper signal issues.
3. **Battery State Tracking:** Use logged battery voltage history slope to identify charging/discharging states.
4. **Solar Charging Study:** Evaluate if a 4-6 hour window of solar panel charge can support active operations.
5. **Detailed System Event Logging to Serial:**
   - Log API server requests.
   - Trace NTP sync failures, retries, and successes.
   - Trace SD card queue flushing successes and failures.
   - Trace Wi-Fi connectivity events (connect, disconnect, IP changes).
6. **Retroactive Timestamp Correction:**
   - Correct estimated boot timestamps in memory buffers and SD logs once NTP synchronization is successfully completed.
7. **Event file**
   - Log boot time.
   - Ntp, Sd card, wifi events and errors.
   - allow correction tobe made to all logs if ntp sync is later successful.
