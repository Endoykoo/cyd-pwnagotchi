# leviagotchi

A Pwnagotchi-style WiFi monitor running on an ESP32 CYD (Cheap Yellow Display). Passively sniffs 802.11 frames, detects WPA handshakes, tracks nearby access points and clients, and reacts with animated facial expressions. Also runs a captive portal so anyone who joins the "leviagotchi friendly" network gets a surprise.

---

## What it does

- **WiFi monitor** — runs in promiscuous mode, no connection required. Listens to raw 802.11 frames on the 2.4GHz band
- **AP detection** — logs every unique BSSID/SSID it sees
- **Client tracking** — logs unique client MAC addresses sending data frames
- **Handshake detection** — detects EAPOL frames (WPA2 4-way handshake). Goes happy face when one is caught
- **Captive portal** — broadcasts an open AP called "leviagotchi friendly". Anyone who connects gets redirected to a page that says "I made a new friend". Friends counter increments on screen
- **Animated face** — reacts to real events. Bored when quiet, excited when busy, happy on handshakes/friends, sleeps after long idle periods
- **Live stats** — APs seen, clients, handshakes, friends, epochs, activity count

---

## Hardware

- ESP32 CYD (Cheap Yellow Display) — ST7789 320x240 screen, XPT2046 touch
- USB-C cable for flashing

Tested on the `ESP32-2432S028`. Should work on any CYD with the same pinout.

---

## Setup

**1. Clone**
```bash
git clone https://github.com/yourusername/leviagotchi.git
cd leviagotchi
```

**2. Install [PlatformIO](https://platformio.org/install/ide?install=vscode)**

**3. Edit config at the top of `src/main.cpp`**

```cpp
#define PORTAL_SSID  "leviagotchi friendly"   // fake AP name
#define FIXED_CHAN    6                         // set to your router's channel
```

To find your channel: Android — tap your network in WiFi settings. Windows — run `netsh wlan show all` in cmd.

**4. Flash**
```bash
pio run --target upload
```

**5. Open Serial Monitor at 115200** to see live output:
```
[AP] MyRouter  CH:6  RSSI:-52
[CLIENT] AA:BB:CC:DD:EE:FF
[EAPOL] Handshake #1 from AA:BB:CC:DD:EE:FF
[PORTAL] New friend #1
```

---

## Testing handshake capture

Forget your WiFi on your phone and reconnect. Make sure `FIXED_CHAN` matches your router's channel — the ESP32 monitors one channel at a time when the portal AP is running.

---

## Expressions

| Face | Trigger |
|---|---|
| Awake `( ●_● )` | Default |
| Happy | Handshake captured or new portal friend |
| Excited | High frame activity (>200 frames/epoch) |
| Grateful | 10+ APs and active |
| Look L/R | Moderate activity |
| Bored | 3+ idle epochs (90s of nothing) |
| Sleep | 8+ idle epochs (4min of nothing) |

An epoch is 30 seconds. Fewer than 5 frames = idle epoch.

---

## How the captive portal works

The ESP32 broadcasts an open network. When a device connects, the OS checks for internet by hitting a known URL. The ESP32 intercepts all DNS queries, points everything at itself, and serves the portal page. The browser popup appears automatically on iOS, Android and Windows.

---

## Project structure

```
leviagotchi/
├── src/
│   └── main.cpp
├── platformio.ini
└── README.md
```

---

## Disclaimer

For educational and research purposes only. Only use on networks you own or have permission to test. Inspired by [Pwnagotchi](https://pwnagotchi.ai) by evilsocket.

## Built with

- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) by Bodmer
- ESP-IDF WiFi stack via Arduino framework
