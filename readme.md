# Hyperk

## QuinLED theater-lighting development fork

This checkout targets **QuinLED An-Penta-Plus with analog RGB+CCT strips**. Version `0.0.5-quinled.3-alpha1` is an **alpha for on-site testing, not a physically qualified stable release**. The existing build environment name, `esp32-wled-esp32-ota-quinled-rgbw`, is retained for the update workflow; fresh installations now default to analog output and one logical color zone. Existing saved hardware and network settings are retained.

To install the alpha on an existing controller, open Settings, choose **Include Pre-releases** in the firmware update channel, then **Check** and **Install Update**. For manual web updates, use the release asset beginning with `OTA_`; the `factory.bin` image is for initial USB flashing. After reboot, open **Light calibration**. The new color engine activates only when its guide is saved.

Owner clarification (October 30, 2026): calibration will be done **by eye**. The installation uses two An-Penta-Plus controllers; the Dig-Unos are retired. Strip length is estimated at **about 6 feet (1.8 m) per lamp**, pending an on-site check. Treat this length as provisional, not as a verified electrical-load measurement. The guided visual workflow below is the intended calibration method.

Open **Light calibration** from Settings (device path `/analog-calibration.html`). The guide checks the five emitters, balances RGB neutral, compares the white pair against that reference, then tests the combined output and a 16-bit shadow ramp. Adjustments are temporary until **Save calibration & finish**. Stop/discard restores normal operation; incoming UDP color is ignored during a preview, with a 15-second expiry if the page disconnects. Calibration saving changes only the analog profile, leaving Ethernet, Wi-Fi, and standby settings intact.

Saving the guide activates a new analog color engine. It decodes the AnimeJaNai addon's gamma-2.2 RGB16 input once, extracts shared white and applies emitter trims in linear light, and quantizes the final channels to PWM. Playback brightness and per-lamp distance gains remain in the addon. PWM remains about **19.531 kHz at native 12-bit resolution**, with the ESP32's four fractional duty bits providing finer average brightness. This is not native 16-bit PWM. Existing profiles keep the earlier mixing behavior until the guide is saved. White share defaults to zero until the white LEDs have been matched; visual calibration does not establish measured color accuracy or an exact CCT.

The rewrite addresses encoded-domain white mixing (a 50/50 white split at gamma 2.2 previously produced only about 44% of the unsplit duty), inconsistent browser/device previews, and coarse intermediate white weights. It does not replace the network protocols, OTA implementation, or stream-timeout behavior: after 6.5 seconds without a stream, the saved standby color or standby-off setting returns.

Current upstream was checked through `f83984b` (0.0.5, August 31, 2026). Its core now comes from the [proprietary Hyperk-Backend binary releases](https://github.com/awawa-dev/Hyperk-Backend), so this fork keeps its editable base instead of importing that backend. The color design follows [linear-light mixing](https://www.w3.org/TR/css-color/#interpolation-space), [emitter output correction](https://esphome.io/components/light/rgbww/#color-correction), and the [An-Penta-Plus PWM limits](https://quinled.info/quinled-an-penta-plus-capabilities/).

Native regression checks (also in CI):

```sh
g++ -std=c++17 -Wall -Wextra -Werror -Iinclude tests/analog_color_test.cpp -o analog-color-test
./analog-color-test
g++ -std=c++17 -Wall -Wextra -Werror -pthread -Itests/fakes -Iinclude tests/analog_calibration_test.cpp src/analog_calibration.cpp -o analog-calibration-test
./analog-calibration-test
```

These check color math, PWM precision, and the preview lifecycle. They do not verify electrical waveforms, LED spectra, wall color, thermal load, or the actual controllers. The automatic release workflow remains in place and marks alpha versions as prereleases. Physical controller testing is still pending.

## Upstream project reference

The installer and general documentation below describe upstream Hyperk, not this custom calibration build.

Hyperk is a minimalist, high-performance uni-platform WiFi LED driver for ESP8266, ESP32 (S2, S3, C2, C3, C5, C6), Raspberry Pi Pico W (RP2040, RP2350). Designed as a lightweight and streamlined component that avoids unnecessary complexity, it delivers low‑latency performance and integrates smoothly with platforms such as HyperHDR, while offering essential home‑automation capabilities through a clean, modern codebase.

## Installation

The firmware can be flashed directly from your browser:
**[hyperk.hyperhdr.org](https://hyperk.hyperhdr.org)**

> [!TIP]
> Once installed, you can also perform OTA updates directly through the local **Web GUI**.

---

## Supported Hardware

- **Espressif:** ESP8266, ESP32, ESP32-S2, ESP32-S3, ESP32-C2, ESP32-C3, ESP32-C5, ESP32-C6, WT32-ETH01
   - Initial support for custom boards: GLEDOPTO, DOMRAEM, Athom/IoTorero  
   
- **Raspberry Pi Pico W:** RP2040, RP2350

Support for multi-segment (board-dependent) and power-relay control.

## Supported LED Types

- **NeoPixel RGB:** WS2812b and compatible.
- **NeoPixel RGBW:** SK6812 (includes white channel calibration known from HyperSerial).
- **DotStar SPI:** APA102 and high-speed clocked LEDs.

## Manual

👉 [https://wiki.hyperhdr.eu/Hyperk](https://wiki.hyperhdr.eu/Hyperk)

## Integration

- **HyperHDR:** Dedicated Hyperk driver (>v22beta1). Also works via its native `DDP`, `udpraw`, and `WLED` drivers.
- **Home Assistant:** Automatic discovery with support for power on/off, color, and brightness control.

|      HyperHDR      |   Home Assistant   |
|--------------------|--------------------|
| [![1](https://github.com/user-attachments/assets/ab252845-50da-4985-96be-d1da7bfd522d)](https://github.com/user-attachments/assets/ab252845-50da-4985-96be-d1da7bfd522d) | [![2a](https://github.com/user-attachments/assets/b3097b11-249a-4a66-9cff-bcdd70f28e87)](https://github.com/user-attachments/assets/b3097b11-249a-4a66-9cff-bcdd70f28e87) |


## Network Services

| Service | Port | Protocol / Description |
| :--- | :--- | :--- |
| Web GUI | 80 | Device configuration |
| UDP DDP | 4048 | DDP listener |
| UDP RealTime | 21324 | Real-time stream listener |
| UDP Raw RGB | 5568 | Raw color stream listener |
  
<small>This fork restores the saved standby color or standby-off setting 6.5s after stream loss.</small>

---
*Developed for performance. Optimized for HyperHDR. [Privacy & Technical Note](https://awawa-dev.github.io/hyperk/privacy.html)*
