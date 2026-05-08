# USB Proxy

A hardware USB security proxy that sits between a USB device and a host computer, detecting BadUSB-style attacks in real time. Built on the Raspberry Pi RP2040.

<img width="887" height="538" alt="image" src="https://github.com/user-attachments/assets/bc297e7b-bbff-4c5a-9d2e-3251689e8e1d" />

---

## Overview

USB Proxy intercepts all USB HID (keyboard/mouse) and mass storage traffic before it reaches the host. You declare what device type you expect to plug in, and the proxy enforces that contract, blocking re-enumeration attacks, bot-speed keystroke injection, and device class spoofing.

The repo contains two independent firmware projects:

| Directory | Board | Role |
|---|---|---|
| `Host/` | Adafruit Feather RP2040 USB Host | The proxy — sits between device and PC |
| `BadDevice2/` | Adafruit ItsyBitsy RP2040 | A simulated bad device for testing the proxy |

---

## How It Works

```
[USB Device] ---USB--> [Feather RP2040 Proxy] ---USB--> [Host PC]
```

On startup the OLED display asks you to declare the expected device type (Keyboard, Mouse, or MSC). Once selected, the proxy:

1. **Checks enumeration** if the connected device doesn't match your declared type, it's blocked immediately.
2. **Detects re-enumeration** if a device disconnects and reconnects as a different class within 5 seconds, it's flagged as a spoofing attempt.
3. **Detects bot keypresses** keystroke timing is analyzed over a rolling 4-sample window. If the average interval is under 30 ms with less than 5 ms of spread (inhuman consistency), a strike is recorded. Four strikes triggers a security alert and the proxy stops forwarding.
4. **AFK timeout** if no HID activity is seen for 30 seconds after a device is selected, the proxy resets and asks you to re-declare the device type.

All events and per-keystroke latency samples are exported over USB CDC (serial) as CSV records for offline analysis.

---

## Host Firmware (`Host/`)

### Hardware

<img width="883" height="595" alt="image" src="https://github.com/user-attachments/assets/1e41c397-fe3c-4cd5-b0d4-38fa86139477" />


| Component | Connection |
|---|---|
| Adafruit Feather RP2040 USB Host | — |
| SSD1306 OLED (128×16, 2 lines) | I2C1 — SDA: GP2, SCL: GP3 |
| Scroll button | GP26 (A0), GND on GP27 (A1) |
| Select button | GP29 (A3), GND on GP28 (A2) |
| Built-in LED | GP13 |

The RP2040 uses both USB ports:
- **Native USB (BOARD_TUD_RHPORT)** device-side, presents as a HID keyboard+mouse composite to the PC
- **PIO USB (BOARD_TUH_RHPORT)** host-side, enumerates whatever is plugged in

Core 0 runs the device stack and UI loop; Core 1 runs the host stack.

### Detection details

**Bot detection thresholds** (see `proxy_detection.h`):

| Parameter | Value | Meaning |
|---|---|---|
| `BOT_AVG_THRESHOLD` | 30 ms | Average inter-keystroke interval below this is suspicious |
| `MIN_SPREAD_THRESHOLD` | 5 ms | Spread (mean absolute deviation) below this is suspicious |
| Strike threshold | 4 strikes | Strikes needed before blocking |
| `STRIKE_TIMEOUT_US` | 5 s | Strikes reset after 5 s of no suspicious activity |

**Re-enumeration window** (`REENUM_WINDOW_US`): 5 seconds. A disconnect followed by a reconnect as a *different* device class within this window is flagged.

### CDC metrics format

Connect to the proxy's USB CDC port (any baud rate). Records are emitted as newline-terminated CSV lines:

```
H,fmt=K:seq,mode,total,strk;E:ts,ev,dev,vid,pid   ← header (sent once on connect)
K,<seq>,<mode>,<total_us>,<strike>                 ← keystroke forwarding sample
E,<ts_us>,<event>,<dev>,<vid_hex>,<pid_hex>        ← event record
```

Event codes: `M` mount, `U` unmount, `O` enum OK, `R` enum error, `B` bot detected, `S` strike, `D` device selected.

Mode: `F` = filtering enabled, `N` = filtering disabled.

### Build

Requirements:
- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk) v2.2.0
- ARM GCC toolchain 14.2
- CMake ≥ 3.13
- [Pico-PIO-USB](https://github.com/sekigon-gonnoc/Pico-PIO-USB) (submodule)

```bash
git submodule update --init --recursive

cd Host
mkdir build && cd build
cmake ..
make -j4
```

Flash `usb-proxy.uf2` to the Feather by holding BOOT and pressing RESET.

**Build flags** (set in `CMakeLists.txt`):

| Flag | Default | Effect |
|---|---|---|
| `PROXY_FILTERING` | `1` | Enable enumeration + bot detection. Set to `0` to passthrough only. |
| `PROXY_METRICS` | `1` | Enable CDC metrics export. |

---

## BadDevice2 (`BadDevice2/`)

A deliberately malicious USB device used to test the proxy's detection capabilities. Runs on an **Adafruit ItsyBitsy RP2040** and communicates with a remote operator over Bluetooth (HC-06 module on UART0).

### What it does

- Enumerates as a HID keyboard by default, or as a USB mass storage device, and can switch between them on command (re-enumeration attack).
- Injects a hardcoded keystroke payload on command.
- Fingerprints the host OS by timing the enumeration handshake and checking for Windows-specific descriptor probes, reporting the result over Bluetooth.
- Logs physical macropad keypresses back over Bluetooth when keylogging mode is active.
- Detects AFK/idle and USB suspend/resume, reporting state changes over Bluetooth.

### Bluetooth commands (send to UART)

| Command | Action |
|---|---|
| `1` | Inject payload (or schedule it for when the user returns from AFK) |
| `2` | Re-enumerate (toggle between HID and MSC) |
| `3` | Toggle keylogger on/off |
| `4` | Re-trigger OS fingerprint report |

### Hardware

| Component | Pin |
|---|---|
| HC-06 Bluetooth module | UART0 — TX: GP0, RX: GP1 |
| Built-in LED | GP13 |
| NeoPixel chain | GP12 |
| Macropad keys | See `keymap.h` |

NeoPixel states: dim blue = idle, white burst = key pressed, red = payload running.

### Build

```bash
cd BadDevice2
mkdir build && cd build
cmake ..
make -j4
```

Flash `BadDevice2.uf2` to the ItsyBitsy.

---

## Project Structure

```
usb-proxy/
├── Host/
│   ├── CMakeLists.txt
│   ├── pico_pio_usb/          ← submodule (Pico-PIO-USB)
│   └── src/
│       ├── usb-proxy.c        ← main, GPIO, multicore setup
│       ├── proxy_host.c/h     ← TinyUSB host callbacks (HID + MSC)
│       ├── proxy_device.c/h   ← TinyUSB device callbacks, main loop
│       ├── proxy_detection.c/h← bot detection + enumeration checks
│       ├── proxy_data.c/h     ← shared packet queue between cores
│       ├── proxy_ui.c/h       ← OLED UI state machine
│       ├── proxy_ssd1306.c/h  ← SSD1306 I2C driver
│       ├── proxy_metrics.c/h  ← CDC telemetry export
│       ├── usb_descriptors.c  ← TinyUSB device descriptors
│       └── tusb_config.h
└── BadDevice2/
    ├── CMakeLists.txt
    └── src/
        ├── main.c             ← main loop, UART/BT handling, OS fingerprint
        ├── device_core.c/h    ← HID state machine, payload/re-enum logic
        ├── usb_descriptors.c/h← dual HID+MSC descriptors
        ├── msc_disk.c         ← MSC disk emulation
        ├── neopixel.c/h       ← NeoPixel driver
        ├── keymap.h           ← macropad key-to-HID mapping
        └── ws2812.pio         ← PIO program for WS2812 LEDs
```

---

## Security Notice

`BadDevice2` is a research and testing tool. It implements attack techniques (keystroke injection, re-enumeration spoofing, OS fingerprinting, keylogging) for the purpose of validating the proxy's defenses. Do not deploy it against systems you do not own or have explicit permission to test.
