# leviagotchi

so i got one of those cheap ESP32 displays (the CYD) and wanted to do something actually cool with it. ended up building something inspired by pwnagotchi — it sits on my desk, watches all the wifi traffic around it, and makes faces depending on whats happening.

it also broadcasts its own open wifi network. if you connect to it you get a little surprise page instead of internet lol

---

## what it does

- passively listens to all 802.11 wifi frames around it without connecting to anything
- tracks every router (AP) and device (client) it sees
- detects WPA2 handshakes — the 4-way exchange that happens when a device connects to a network
- makes different faces depending on whats going on (bored when quiet, happy when it catches something, falls asleep if nothing happens for a while)
- broadcasts a fake open network called "leviagotchi friendly" — connect to it and you get redirected to a page that says i made a new friend
- shows live stats on the right side of the screen (APs seen, clients, handshakes, friends, etc)

---

## hardware

just an ESP32 CYD (Cheap Yellow Display). the specific one i used is the ESP32-2432S028 which has a 320x240 ST7789 screen built in. you can get them for like $10 on aliexpress.

nothing else needed, no extra wiring.

---

## flashing it

you need [PlatformIO](https://platformio.org/install/ide?install=vscode) installed in VS Code.

```bash
git clone https://github.com/Endoykoo/leviagotchi.git
cd leviagotchi
```

open `src/main.cpp` and change these two lines at the top:

```cpp
#define PORTAL_SSID  "leviagotchi friendly"   // whatever you want the fake network called
#define FIXED_CHAN    6                         // set this to your router's wifi channel
```

to find your channel on windows open cmd and run `netsh wlan show all` and find your network. most routers use channel 1, 6 or 11.

then just hit upload in PlatformIO. open the serial monitor at 115200 to see whats happening:

```
[AP] SomeRouter  CH:6  RSSI:-58
[CLIENT] AA:BB:CC:DD:EE:FF
[EAPOL] Handshake #1 from AA:BB:CC:DD:EE:FF
[PORTAL] New friend #1
```

---

## testing if it works

the easiest way to trigger a handshake is to forget your wifi on your phone and reconnect. make sure FIXED_CHAN matches your actual router channel or it wont see anything.

---

## the faces

| face | when |
|---|---|
| `( ●_● )` awake | just sitting there scanning |
| happy | caught a handshake or someone joined the portal |
| excited | lots of wifi traffic flying around |
| look left/right | some activity, scanning |
| bored | nothing happened for 90 seconds |
| sleeping | nothing happened for 4 minutes |

---

## the captive portal

when someone connects to the fake network, their phone/laptop automatically opens a browser popup (same thing that happens at hotels or cafes). thats because the OS checks for internet by hitting a known URL, the ESP32 intercepts the DNS request, points it at itself, and serves the page. works on iOS, Android and Windows automatically.

---

## disclaimer

built this for fun and to learn about 802.11. only use it on your own network or where you have permission. the captive portal is the exact same tech airports and coffee shops use. i'm not responsible for how you use this.

inspired by [pwnagotchi](https://pwnagotchi.ai) — go check that out if you want the real thing running on a raspberry pi.

---

## license

MIT — do whatever you want with it, just give credit.

---

## built with

- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) for the display
- ESP-IDF wifi stack through the Arduino framework
- PlatformIO
