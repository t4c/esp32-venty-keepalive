# ESP32 Venty Keep-Alive

An autonomous background daemon running on an ESP32 that prevents the Storz & Bickel VENTY from shutting down automatically during inhalation sessions.

## Origin & Motivation

The project originated from a medical necessity: written for a medical cannabis patient who takes very gentle, prolonged draws.

The built-in airflow sensor and draw detection of the Venty do not register subtle pressure drops below its minimum threshold. Because of this, the internal auto-shutoff timer continues to count down despite active medication use, frequently turning off the device mid-session.

It is equally useful for any "lazy smoker" who prefers a continuous, uninterrupted session without having to constantly press physical buttons to prove they are still there.

## Technical Deep-Dive

### The Auto-Shutoff Trap

Our reverse engineering revealed that the Venty's internal auto-shutoff watchdog (120s timer) cannot be reset by simply tweaking the target temperature via BLE, nor by writing flags like `BUTTON_CHANGED_FILLING_CHAMBER`. The firmware strictly checks for actual state changes or physical trigger events.

### The Breakthrough: Adaptive State Bouncing

While temperature changes are ignored by the watchdog, heater state shifts reset the timer back to 120s immediately.

Simply toggling Boost mode causes the device to lock into the higher target temperature (+15 °C or 210 °C) without automatically returning. This daemon solves the problem by executing a millisecond-precision state bounce whenever the timer drops to or below 100 seconds:

- **Normal Mode (Mode 1):** The ESP32 triggers Boost (Mode 2) -> immediately interrupts with Heater OFF (Mode 0) for 150 ms -> switches right back to Normal (Mode 1). The timer resets to 120s instantly while preserving the user's base temperature setting (e.g. 180.0 °C).
- **Boost Mode (Mode 2):** When the user actively vapes in Boost, dropping the heater into Mode 0 would cancel it. Instead, the daemon performs an unlatch sequence: Mode 0 (150ms) -> Mode 1 (200ms) -> Mode 2. This guarantees the timer resets while locking the device safely back into Boost (195.0 °C).
- **Superboost Mode (Mode 3):** Executes a similar bounce (Mode 0 -> Mode 1 -> Mode 3) to preserve the 210.0 °C profile without dropping the session.

Because these transitions execute in under 600 ms, the heating chamber experiences zero drop in actual temperature.

## Credits & Acknowledgments

Special credit goes to the [reactive-volcano-app](https://github.com/firsttris/reactive-volcano-app) project.

Their reverse-engineered BLE packet structures, opcode definitions (`protocol.ts`), and command mask mappings (`StatusWriteMask`, `HeaterMode`) were essential in understanding the Venty communication protocol and building this stand-alone ESP32 implementation.

## Hardware & Flashing

### 1. Web Flashing via Browser (Easiest / No Software Needed)

You can flash the ESP32 directly from your browser (Chrome, Edge, Opera) using any standard ESP Web Flasher (e.g. [ESP Web Tools](https://esp.rainmaker.espressif.com/) or [Adafruit ESPTool](https://adafruit.github.io/Adafruit_WebSerial_ESPTool/)):

1. Download the pre-compiled **`esp32-venty-keepalive-factory-merged.bin`** from the latest [GitHub Release](https://github.com/t4c/esp32-venty-keepalive/releases).
2. Connect your ESP32 board to your computer via USB.
3. Open a Web Flasher in Chrome/Edge and click **Connect** (select your ESP32's COM port).
4. Set the flash offset address to **`0x0`** (or `0x0000`).
5. Choose `esp32-venty-keepalive-factory-merged.bin` and click **Program / Flash**.

---

### 2. Manual Flashing via esptool.py (Command Line)

If you prefer flashing individual binary partitions via the command line:

1. Download all assets from the [GitHub Release](https://github.com/t4c/esp32-venty-keepalive/releases):
   - `bootloader.bin`
   - `partitions.bin`
   - `esp32-venty-keepalive-firmware.bin`
2. Run `esptool.py`:
   ```bash
   esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 \
     --before default_reset --after hard_reset write_flash -z \
     --flash_mode dio --flash_freq 40m --flash_size 4MB \
     0x1000 bootloader.bin \
     0x8000 partitions.bin \
     0x10000 esp32-venty-keepalive-firmware.bin
   ```
   *(Note: Replace `/dev/ttyUSB0` with your actual serial port, e.g. `COM3` on Windows).*

---

### 3. Building from Source

#### Requirements
- Any standard ESP32 development board (ESP32-WROOM-32, NodeMCU ESP32, etc.)
- Arduino IDE (easiest) or PlatformIO
- Native ESP32 BLE libraries (`BLEDevice`, `BLEScan`, `BLEClient`)

#### Option A: Using Arduino IDE (Recommended for most users)
1. Clone or download this repository.
2. Open `esp32-venty-keepalive.ino` in the Arduino IDE.
3. Under **Tools > Board**, select **ESP32 Dev Module** (from the `esp32` board package).
4. Under **Tools > Partition Scheme**, select **Huge APP (3MB No OTA/1MB SPIFFS)**.
5. Select your ESP32's COM port under **Tools > Port**.
6. Click the **Upload** button (the arrow icon in the top left).
7. Open the Serial Monitor (**Tools > Serial Monitor**) and set the baud rate to **`115200`**.

#### Option B: Using PlatformIO Core / VS Code
1. Clone this repository:
   ```bash
   git clone https://github.com/t4c/esp32-venty-keepalive.git
   cd esp32-venty-keepalive
   ```
2. Build and flash the firmware directly to your connected ESP32 via CLI:
   ```bash
   pio run --target upload
   ```
   *(Or click the PlatformIO **Upload** arrow in the VS Code bottom status bar).*

---

Once booted, the ESP32 automatically scans for advertising packets starting with `S&B VY`, pairs, negotiates notifications, and operates fully autonomously in the background.

## Disclaimer & Warranty Warning

> **WARNING: USE AT YOUR OWN RISK**

- This software is a third-party modification and is not endorsed, affiliated with, or supported by Storz & Bickel GmbH.
- Operating the heating element continuously by suppressing the built-in auto-shutoff mechanism may cause additional thermal stress on battery cells and electronics.
- Using third-party software to bypass manufacturer safety or convenience timers may void your device warranty.
- The author and contributors assume no responsibility for damaged vaporizers, bricked units, shortened component lifespans, or unexpected side effects.
