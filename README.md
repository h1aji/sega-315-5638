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

Primary protocol reference:
RetroSix Wiki – Sega Mega Drive Controller Interface
[https://www.retrosix.wiki/controller-interface-sega-mega-drive](https://www.retrosix.wiki/controller-interface-sega-mega-drive)

---

## Key Features

* ✔ Accurate **6-button detection sequence**
* ✔ Correct **8-phase TH state machine**
* ✔ Automatic fallback to **3-button mode**
* ✔ Proper handling of **shared / multiplexed data lines**
* ✔ Active-LOW Sega signaling
* ✔ Runs on **ATmega8 or ATmega88**
* ✔ Bare-metal **avr-gcc / PlatformIO** implementation

---

## How the Sega 6-Button Protocol Works (Summary)

1. The console toggles the **TH (SELECT)** line.
2. The controller advances an internal phase counter on **each TH edge**.
3. Depending on the current phase, the controller outputs:

   * Directions
   * A / B / C / Start
   * X / Y / Z / Mode
4. If TH is idle for ~12 ms, the controller **resets to 3-button mode**.
5. During the 6-button detect phase, the controller pulls **all data lines LOW**.

This firmware faithfully reproduces that behavior.

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
* 12 pushbuttons (or other logic source)
* 5V power supply

---

## Sega DB9 (Console) → AVR Connections

These are the **signals coming from / going to the console**.
The AVR must behave exactly like a Sega controller ASIC.

### Shared / Multiplexed Sega Lines

| Sega Function | DB9 Pin | AVR Pin | AVR Port Bit    |
| ------------- | ------: | ------: | --------------- |
| UP / Z        |       1 |     PB0 | PORTB0          |
| DOWN / Y      |       2 |     PB1 | PORTB1          |
| LEFT / X      |       3 |     PB2 | PORTB2          |
| RIGHT / MODE  |       4 |     PB3 | PORTB3          |
| B / A         |       6 |     PB2 | PORTB2 (shared) |
| C / START     |       9 |     PB3 | PORTB3 (shared) |
| TH (SELECT)   |       7 |     PD2 | PORTD2          |
| +5V           |       5 |     VCC | —               |
| GND           |       8 |     GND | —               |

**Important:**

* Sega uses **shared data pins**
* Meaning depends on **TH phase**
* All lines are **active LOW**
* Console provides pull-ups internally
* Reset pin is connected to VCC through 1-10K resistor

---

## AVR → Local Button Connections

These are **local buttons** connected to the AVR.
The firmware converts these into Sega protocol responses.

| Button | AVR Pin | Port  |
| ------ | ------: | ----- |
| UP     |     PC0 | PINC0 |
| DOWN   |     PC1 | PINC1 |
| LEFT   |     PC2 | PINC2 |
| RIGHT  |     PC3 | PINC3 |
| B      |     PC4 | PINC4 |
| C      |     PC5 | PINC5 |
| A      |     PD3 | PIND3 |
| START  |     PD4 | PIND4 |
| X      |     PD5 | PIND5 |
| Y      |     PD6 | PIND6 |
| Z      |     PD7 | PIND7 |
| MODE   |     PB7 | PINB7 |

All buttons are **active LOW** with **internal pull-ups enabled**.

---

## Electrical Behavior Notes

* Outputs are driven **LOW for pressed**, HIGH otherwise
* Matches original Sega controller signaling
* No bus contention with console pull-ups
* Safe for real hardware

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

* **Timer0** generates ~1 ms ticks
* Used to detect TH inactivity
* Resets protocol state after timeout

---

## Build & Flash (PlatformIO)

### Programmer

This project uses **Arduino as ISP**.

### Build

```bash
pio run
```

### Upload

```bash
pio run --target upload
```

---

## Status

✔ Verified protocol logic
✔ Correct shared-pin behavior
✔ Safe for real consoles
✔ ATmega8 & ATmega88 compatible

---

## Disclaimer

This project is intended for **educational and hardware preservation purposes**.
Sega trademarks remain the property of their respective owners.

