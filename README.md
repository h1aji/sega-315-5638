# Sega Mega Drive 6-Button Controller ASIC Emulator

**ATmega8 / ATmega88 – PlatformIO**

## Overview

This project implements a **hardware-accurate emulator of the original Sega Mega Drive / Genesis 6-button controller ASIC (315-5638)**.

Instead of reading a Sega controller, the AVR **behaves exactly like one**:

* The **console drives the TH (SELECT)** line
* The AVR responds with correct button data on shared data lines
* Full **3-button and 6-button protocols** are supported
* Correct **multiplexed pin behavior** (A/B, C/Start, etc.)
* Accurate timing and state machine behavior for real hardware

This firmware is designed for **real Mega Drive / Genesis consoles**, not emulators.

---

## Protocol References

Primary sources used for implementation:

* **RetroSix Wiki** – Sega Mega Drive Controller Interface  
  [https://www.retrosix.wiki/controller-interface-sega-mega-drive](https://www.retrosix.wiki/controller-interface-sega-mega-drive)
* **Raspberryfield** – 6-Button Controller with oscilloscope traces  
  [https://www.raspberryfield.life/2019/03/25/sega-mega-drive-genesis-6-button-xyz-controller/](https://www.raspberryfield.life/2019/03/25/sega-mega-drive-genesis-6-button-xyz-controller/)
* **Original CMU Documentation** – segasix.txt  
  [http://www.cs.cmu.edu/~chuck/infopg/segasix.txt](http://www.cs.cmu.edu/~chuck/infopg/segasix.txt)

---

## Key Features

* ✔ Accurate **6-button detection sequence**
* ✔ Correct **8-phase TH state machine**
* ✔ Automatic fallback to **3-button mode** after 12ms timeout
* ✔ Proper handling of **shared / multiplexed data lines**
* ✔ Active-LOW Sega signaling
* ✔ Runs on **ATmega8 or ATmega88**
* ✔ Bare-metal **avr-gcc / PlatformIO** implementation

---

## How the Sega 6-Button Protocol Works

### Basic Protocol

1. The console toggles the **TH (SELECT)** line (typically every ~20ms)
2. The controller advances an internal phase counter on **each TH edge**
3. TH pulse width: ~4-7µs LOW, ~7µs HIGH between pulses
4. If TH is idle for >12ms, the controller **resets to 3-button mode**

### 6-Button Detection Sequence

The console pulses TH **4 times** in rapid succession:

| Pulse | TH State | PIN 1 (UP) | PIN 2 (DOWN) | PIN 3 (LEFT) | PIN 4 (RIGHT) | PIN 6 | PIN 9 |
|-------|----------|------------|--------------|--------------|---------------|-------|-------|
| 0 (idle) | HIGH | UP | DOWN | LEFT | RIGHT | B | C |
| 1 | LOW | UP | DOWN | GND* | GND* | A | START |
| 1 | HIGH | UP | DOWN | LEFT | RIGHT | B | C |
| 2 | LOW | UP | DOWN | GND* | GND* | A | START |
| 2 | HIGH | UP | DOWN | LEFT | RIGHT | B | C |
| **3** | **LOW** | **GND*** | **GND*** | **GND*** | **GND*** | A | START |
| **3** | **HIGH** | **Z** | **Y** | **X** | **MODE** | B | C |
| 4 | LOW | UP | DOWN | GND* | GND* | A | START |
| 4 (reset) | HIGH | UP | DOWN | LEFT | RIGHT | B | C |

**Key Points:**
- `*` GND = Pulled LOW for 3-button controller compatibility
- `**` All 4 directional pins LOW on Pulse 3 LOW = **6-button identification signal**
- X/Y/Z/MODE are readable on Pulse 3 HIGH

### Phase Counter

The firmware maintains an 8-phase counter (0-7) that increments on every TH edge:

- **Phase 0**: Pulse 1 LOW
- **Phase 1**: Pulse 1 HIGH
- **Phase 2**: Pulse 2 LOW
- **Phase 3**: Pulse 2 HIGH
- **Phase 4**: Pulse 3 LOW - **IDENTIFICATION** (all 4 directional pins LOW)
- **Phase 5**: Pulse 3 HIGH - **X/Y/Z/MODE readable**
- **Phase 6**: Pulse 4 LOW
- **Phase 7**: Pulse 4 HIGH - Reset to idle

When phase counter reaches 8, it resets to 0 and sets the `isSix` flag, indicating successful 6-button detection.

---

## Supported Microcontrollers

* **ATmega8**
* **ATmega88 / 88P / 88PA**

The code automatically adapts using compile-time detection.

All analog functionality is **explicitly disabled** so every pin operates as **digital I/O only**.

---

## Hardware Requirements

* ATmega8 or ATmega88
* 8 MHz clock (internal RC recommended)
* Sega Mega Drive / Genesis DB9 controller port
* 12 pushbuttons (or other input source)
* 5V power supply

---

## Pin Connections

### Sega DB9 (Console) → AVR Connections

These are the **output signals to the console**.
The AVR must behave exactly like a Sega controller ASIC.

| Sega Function    | DB9 Pin | AVR Pin | Direction | Notes |
|------------------|---------|---------|-----------|-------|
| UP / Z           | 1       | PD2     | Output    | Multiplexed: UP normally, Z in phase 5 |
| DOWN / Y         | 2       | PD3     | Output    | Multiplexed: DOWN normally, Y in phase 5 |
| LEFT / X         | 3       | PD4     | Output    | Multiplexed: LEFT normally, X in phase 5, GND when TH LOW |
| RIGHT / MODE     | 4       | PD5     | Output    | Multiplexed: RIGHT normally, MODE in phase 5, GND when TH LOW |
| +5V              | 5       | VCC     | Power     | — |
| B / A            | 6       | PD6     | Output    | Multiplexed: B when TH HIGH, A when TH LOW |
| TH (SELECT)      | 7       | PB7     | Input     | Console drives this, AVR reads it |
| GND              | 8       | GND     | Ground    | — |
| C / START        | 9       | PD7     | Output    | Multiplexed: C when TH HIGH, START when TH LOW |

**Important:**
* All outputs are **active LOW** (LOW = pressed, HIGH = released)
* Console provides pull-up resistors internally
* TH is driven by the console, read by the AVR

---

### AVR → Local Button Connections

These are **local buttons** connected to the AVR.
The firmware reads these and converts them into Sega protocol responses.

| Button | AVR Pin | Port    | Pull-up |
|--------|---------|---------|---------|
| UP     | PB0     | PINB0   | Enabled |
| RIGHT  | PB1     | PINB1   | Enabled |
| DOWN   | PB2     | PINB2   | Enabled |
| LEFT   | PB3     | PINB3   | Enabled |
| START  | PB4     | PINB4   | Enabled |
| A      | PB5     | PINB5   | Enabled |
| B      | PC0     | PINC0   | Enabled |
| Z      | PC1     | PINC1   | Enabled |
| Y      | PC2     | PINC2   | Enabled |
| X      | PC3     | PINC3   | Enabled |
| C      | PC4     | PINC4   | Enabled |
| MODE   | PC5     | PINC5   | Enabled |

All buttons are **active LOW** with **internal pull-ups enabled**.

---

## Electrical Behavior Notes

* Outputs are driven **LOW for pressed**, HIGH for released
* Matches original Sega controller signaling
* No bus contention with console pull-ups
* Safe for real hardware
* Reset pin should be connected to VCC through 10kΩ resistor

---

## Software Details

* Language: **C (avr-gcc)**
* Clock: **8 MHz**
* Framework: **PlatformIO (bare metal AVR)**
* Arduino framework **not used**
* Analog hardware fully disabled:
  * ADC disabled
  * Analog comparator disabled

### Timer Usage

* **Timer0** generates ~1-2ms ticks
* Used to detect TH inactivity (>12ms)
* Automatically resets protocol state to 3-button mode after timeout

### Main Loop

The main loop continuously:
1. Polls the TH line for edge changes
2. Updates the phase counter on each edge
3. Outputs the appropriate button data based on current phase and TH state

Loop execution is fast enough (~1-2µs) to respond to console pulses (4-7µs).

---

## Build & Flash (PlatformIO)

### Prerequisites

* PlatformIO Core or IDE
* Arduino as ISP programmer (or compatible)

### Build

```bash
pio run
```

### Upload

```bash
pio run --target upload
```

### Fuse Settings

For ATmega8 at 8MHz internal:
```bash
pio run --target fuses
```

---

## Testing

To verify correct operation:

1. Connect to a real Sega Genesis/Mega Drive console
2. Power on with a 6-button compatible game (e.g., Mortal Kombat II, Street Fighter II, Comix Zone)
3. Verify all 12 buttons respond correctly
4. Test with a 3-button only game (e.g., Sonic) to verify fallback mode works

---

## Troubleshooting

**Controller not detected:**
- Verify TH (PB7) connection and direction (should be INPUT)
- Check all output pins (PD2-PD7) are configured as outputs
- Verify 5V power supply is stable

**6-button mode not working:**
- Ensure phase 4 outputs all 4 directional pins LOW (identification signal)
- Check timing: all 4 pulses must occur within 12ms
- Verify phase counter increments correctly on TH edges

**Some buttons don't work:**
- Check button input connections and pull-ups
- Verify correct pin multiplexing in each phase
- Test with oscilloscope to verify timing

---

## Status

✅ Verified protocol logic against multiple sources
✅ Correct 6-button identification sequence (all 4 pins LOW in phase 4)
✅ Proper shared-pin multiplexing behavior
✅ Safe for real consoles
✅ ATmega8 & ATmega88 compatible
✅ Tested timing requirements (4-7µs pulses, 20ms polling)

---

## License

This project is released under the MIT License.

---

## Disclaimer

This project is intended for **educational and hardware preservation purposes**.
Sega, Genesis, and Mega Drive are trademarks of SEGA Corporation.

