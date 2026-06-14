# Report 3: How to Flash the ESP32 — Complete Beginner's Guide

---

## What Does "Flashing" Mean?

Your robot's brain is an **ESP32 microcontroller chip**. Right now it has no software on it (or old software). "Flashing" means copying the program from your computer onto the chip's internal memory so it runs when powered on.

Think of it like installing an app on a phone — except instead of an app store, you connect a USB cable and use a special tool on your computer to write the program directly.

After a successful flash:
- The robot runs the new software **immediately** on the next power cycle
- The software stays there permanently (even without power) until you flash again

---

## What You Will Need (Hardware)

1. **Your robot** (the board with the ESP32 chip on it)
2. **A USB cable** — must be a **data cable**, not just a charging cable
   - Look for one that came with the robot, or use a cable you know works for data transfer
   - Many cheap cables only charge and cannot transfer data — this is the #1 cause of "my computer doesn't see the board"
3. **A Windows, Mac, or Linux computer**

---

## Understanding the Files in This Repository

Before flashing, you need to know what each file does:

```
General_Driver/
├── General_Driver.ino          ← The main source code file (the program)
├── ugv_config.h                ← Settings (pin numbers, servo IDs, etc.)
├── movtion_module.h            ← Motor control code
├── RoArm-M2_module.h          ← Robot arm code
├── ... (all other .h files)   ← More code modules
│
├── data/                       ← Files stored separately ON the chip's filesystem
│   ├── wifiConfig.json         ← WiFi settings (SSID, password)
│   └── devConfig.json          ← Device configuration
│
└── build/esp32.esp32.esp32/    ← Pre-compiled binaries (ready to flash without compiling)
    ├── General_Driver.ino.bin              ← The main program binary
    ├── General_Driver.ino.bootloader.bin   ← The bootloader (startup code)
    └── General_Driver.ino.partitions.bin   ← Flash memory layout
```

### Two Things That Need to Be Flashed Separately

This is something beginners often miss:

| What | Where it goes | Contains |
|------|--------------|---------|
| **The firmware** | Program flash | All the `.h` and `.ino` code |
| **The filesystem** | LittleFS partition | The `data/` folder (WiFi config, etc.) |

If you only flash the firmware and not the filesystem, the robot will work but:
- It won't find `wifiConfig.json` → starts in default AP mode with SSID `"UGV"` / password `"12345678"`
- The boot mission file won't exist yet (created at first run)

---

## Method 1: Arduino IDE (Recommended for Beginners)

Arduino IDE is a free program that lets you write, compile, and flash code to boards like the ESP32. It has a graphical interface — no command line needed.

### Step 1 — Install Arduino IDE

1. Go to **arduino.cc/en/software** and download **Arduino IDE 2** (the newer version)
2. Install it like any normal program
3. Open it

### Step 2 — Install ESP32 Board Support

Arduino IDE does not know about ESP32 by default. You need to add it:

1. Open Arduino IDE
2. Go to **File → Preferences** (Windows/Linux) or **Arduino IDE → Settings** (Mac)
3. Find the field called **"Additional boards manager URLs"**
4. Paste this URL into that field:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
5. Click **OK**
6. Go to **Tools → Board → Boards Manager**
7. In the search box type `esp32`
8. Find **"esp32 by Espressif Systems"** and click **Install**
9. Wait — this downloads about 300 MB and takes a few minutes

### Step 3 — Install Required Libraries

The code uses many external libraries. You need to install all of them:

1. Go to **Tools → Manage Libraries** (or press `Ctrl+Shift+I`)
2. Search for and install each of these one by one:

| Library Name to Search | Install This One |
|----------------------|-----------------|
| `ArduinoJson` | ArduinoJson by Benoit Blanchon |
| `Adafruit SSD1306` | Adafruit SSD1306 by Adafruit |
| `Adafruit BusIO` | Adafruit BusIO (usually auto-installed) |
| `INA219_WE` | INA219_WE by Wolfgang Ewald |
| `ESP32Encoder` | ESP32Encoder by Kevin Harrington |
| `PID_v2` | PID v2 by Brett Beauregard |
| `SimpleKalmanFilter` | SimpleKalmanFilter by Denys Sene |
| `Adafruit ICM20X` | Adafruit ICM20X |
| `Adafruit Unified Sensor` | Adafruit Unified Sensor |

> ⚠️ **Warning:** When Arduino offers to install dependencies automatically, always click **"Install All"**

### Step 4 — Install the SCServo Library

This library is **included in this repository** (in the `SCServo/` folder), not in the library manager. You need to install it manually:

1. Find the `SCServo` folder inside this project
2. Copy the entire `SCServo` folder
3. Paste it into your Arduino libraries folder:
   - **Windows:** `C:\Users\YourName\Documents\Arduino\libraries\`
   - **Mac:** `~/Documents/Arduino/libraries/`
   - **Linux:** `~/Arduino/libraries/`
4. Restart Arduino IDE

### Step 5 — Open the Project

1. In Arduino IDE go to **File → Open**
2. Navigate to the `General_Driver` folder inside this project
3. Open the file `General_Driver.ino`
4. Arduino IDE will open it along with all the `.h` tabs automatically

### Step 6 — Configure the Board Settings

1. Go to **Tools** menu and set these options:

| Setting | Value |
|---------|-------|
| Board | **ESP32 Dev Module** |
| Upload Speed | **921600** |
| CPU Frequency | **240MHz (WiFi/BT)** |
| Flash Frequency | **80MHz** |
| Flash Mode | **QIO** |
| Flash Size | **4MB (32Mb)** |
| Partition Scheme | **Default 4MB with spiffs** |
| Core Debug Level | **None** |
| PSRAM | **Disabled** |

> ⚠️ **Important:** The partition scheme affects where the LittleFS filesystem lives. If you pick the wrong one, the filesystem upload in Step 8 will fail.

### Step 7 — Select the COM Port

1. Plug in your robot via USB
2. Go to **Tools → Port**
3. You should see a new port appear — something like:
   - Windows: `COM3`, `COM4`, `COM5` (the number varies)
   - Mac: `/dev/cu.usbserial-XXXXX` or `/dev/cu.SLAB_USBtoUART`
   - Linux: `/dev/ttyUSB0` or `/dev/ttyACM0`
4. Select it

> ⚠️ **If no port appears:**
> - Try a different USB cable (data cable, not charge-only)
> - Install the **CP2102** or **CH340** USB driver (search for the chip name on the back of the USB adapter on your board)
> - Try a different USB port on your computer
> - On Windows: open Device Manager and look for "Ports (COM & LPT)" — if you see a yellow warning triangle, right-click → Update Driver

### Step 8 — Compile and Upload the Firmware

1. Click the **→ Upload button** (right-pointing arrow at the top left)
2. Arduino IDE will first **compile** the code (translate it from C++ to machine code) — this takes 1–3 minutes the first time
3. Then it will **upload** to the board

During upload you will see in the bottom console:
```
Connecting........_____
Chip is ESP32-D0WDQ6 (revision 1)
Features: WiFi, BT, Dual Core, 240MHz, VRef calibration in efuse, Coding Scheme None
...
Wrote 1234567 bytes (890123 compressed) at 0x00010000 in 12.3 seconds
Hash of data verified.
Leaving...
Hard resetting via RTS pin...
```

If it gets stuck at `Connecting........_____`, see the troubleshooting section below.

> ✅ When you see **"Done uploading"** — the firmware is on the chip.

### Step 9 — Upload the Filesystem (WiFi Config etc.)

This step uploads the `data/` folder contents (the JSON config files).

First, install the filesystem uploader plugin:

1. Go to **Tools → ESP32 Sketch Data Upload**
   - If this option doesn't exist, you need to install the plugin first:
   - Download **"ESP32FS"** plugin from: `github.com/me-no-dev/arduino-esp32fs-plugin`
   - Unzip it into `Arduino/tools/` folder (create the `tools` folder if it doesn't exist)
   - Restart Arduino IDE
2. Make sure your `data/` folder is inside the `General_Driver/` sketch folder (it already is in this project)
3. Go to **Tools → ESP32 Sketch Data Upload**
4. It will upload all files in the `data/` folder to the LittleFS partition

> ⚠️ **The Serial Monitor must be closed** when uploading the filesystem — Arduino IDE will warn you if it isn't.

> ⚠️ **Edit the WiFi config before uploading** — open `General_Driver/data/wifiConfig.json` in any text editor and change the `sta_ssid` and `sta_password` to your own WiFi network before doing this step. The current file contains the developer's WiFi credentials.

---

## Method 2: PlatformIO (More Powerful, Still Approachable)

PlatformIO is a more professional tool that runs inside **Visual Studio Code** (a free code editor). It handles library management and board configuration automatically via a config file.

### Step 1 — Install Visual Studio Code

1. Download from **code.visualstudio.com**
2. Install it normally

### Step 2 — Install PlatformIO Extension

1. Open VS Code
2. Click the **Extensions icon** on the left sidebar (looks like four squares)
3. Search for `PlatformIO IDE`
4. Click **Install**
5. Wait — it downloads and sets up automatically (takes a few minutes)
6. Restart VS Code when prompted

### Step 3 — Create a PlatformIO Project

PlatformIO uses a file called `platformio.ini` to know what board you have and what libraries to use. This project doesn't have one yet, so you need to create it.

1. Open VS Code
2. Click the **PlatformIO icon** on the left sidebar (the alien head icon)
3. Click **"New Project"**
4. Fill in:
   - **Name:** `General_Driver`
   - **Board:** search for `ESP32 Dev Module` → select it
   - **Framework:** `Arduino`
   - **Location:** uncheck "Use default location" → point it to the `General_Driver` folder of this project
5. Click **Finish**

PlatformIO creates a `platformio.ini` file. Open it and replace its contents with:

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
upload_speed = 921600
monitor_speed = 115200
board_build.filesystem = littlefs
board_build.partitions = default.csv

lib_deps =
    bblanchon/ArduinoJson @ ^6.21.0
    adafruit/Adafruit SSD1306 @ ^2.5.7
    adafruit/Adafruit BusIO @ ^1.14.1
    wollewald/INA219_WE @ ^1.3.1
    madhephaestus/ESP32Encoder @ ^0.10.2
    br3ttb/PID_v2 @ ^0.0.0
    denyssene/SimpleKalmanFilter @ ^0.1.0
    adafruit/Adafruit ICM20X @ ^2.0.5
    adafruit/Adafruit Unified Sensor @ ^1.1.9
```

> ⚠️ **The SCServo library** is not in the PlatformIO registry. Copy the `SCServo` folder into the `lib/` folder that PlatformIO created inside your project.

### Step 4 — Place the Source Files

PlatformIO expects source code in a `src/` folder. But this project uses Arduino's flat structure. The easiest fix:

1. Copy **all** `.ino` and `.h` files from `General_Driver/` into the `src/` folder PlatformIO created
2. Rename `General_Driver.ino` to `main.cpp`
3. Add this line at the very top of `main.cpp`:
   ```cpp
   #include <Arduino.h>
   ```

### Step 5 — Build and Upload

1. Plug in the robot via USB
2. At the bottom of VS Code you'll see a toolbar — click the **→ Upload button** (right arrow)
3. PlatformIO compiles and flashes automatically

### Step 6 — Upload the Filesystem

1. Put the `data/` folder inside your PlatformIO project folder (same level as `src/`)
2. In VS Code, open the PlatformIO sidebar → **Project Tasks → env:esp32dev → Platform → Upload Filesystem Image**
3. Click it — PlatformIO builds the LittleFS image and uploads it

---

## Method 3: Flash Pre-Built Binaries (No Compilation Needed)

This project already has compiled binaries in:
```
General_Driver/build/esp32.esp32.esp32/
├── General_Driver.ino.bin              ← main app
├── General_Driver.ino.bootloader.bin   ← bootloader
└── General_Driver.ino.partitions.bin   ← partition table
```

If you just want to run the existing code without installing Arduino/PlatformIO, you can flash these directly using **esptool** — a command-line tool from Espressif.

### Step 1 — Install Python

esptool requires Python 3.

1. Download from **python.org/downloads**
2. During installation on Windows, **tick "Add Python to PATH"** — critical
3. Verify: open a terminal and type `python --version`

### Step 2 — Install esptool

Open a terminal (Command Prompt on Windows, Terminal on Mac/Linux) and type:

```bash
pip install esptool
```

Verify it worked:
```bash
esptool.py version
```

### Step 3 — Find Your COM Port

- **Windows:** Open Device Manager → Ports (COM & LPT) → look for your board (e.g., `COM4`)
- **Mac:** `ls /dev/cu.*` in terminal → look for something like `/dev/cu.usbserial-0001`
- **Linux:** `ls /dev/ttyUSB*` or `ls /dev/ttyACM*`

### Step 4 — Flash All Three Binaries

Run this command (replace `COM4` with your actual port):

**Windows:**
```bash
esptool.py --chip esp32 --port COM4 --baud 921600 write_flash ^
  0x1000  General_Driver/build/esp32.esp32.esp32/General_Driver.ino.bootloader.bin ^
  0x8000  General_Driver/build/esp32.esp32.esp32/General_Driver.ino.partitions.bin ^
  0x10000 General_Driver/build/esp32.esp32.esp32/General_Driver.ino.bin
```

**Mac/Linux:**
```bash
esptool.py --chip esp32 --port /dev/cu.usbserial-0001 --baud 921600 write_flash \
  0x1000  General_Driver/build/esp32.esp32.esp32/General_Driver.ino.bootloader.bin \
  0x8000  General_Driver/build/esp32.esp32.esp32/General_Driver.ino.partitions.bin \
  0x10000 General_Driver/build/esp32.esp32.esp32/General_Driver.ino.bin
```

The three numbers (`0x1000`, `0x8000`, `0x10000`) are **memory addresses** — where each binary gets placed in flash. Getting these wrong will brick the board (but can always be fixed by flashing again correctly).

> ✅ You will see a progress bar and finally:
> ```
> Hash of data verified.
> Leaving...
> Hard resetting via RTS pin...
> ```

### Step 5 — Flash the Filesystem with esptool

For this you need to build the LittleFS image first. The easiest way is to use the `mklittlefs` tool:

1. Download `mklittlefs` from: `github.com/earlephilhower/mklittlefs/releases`
2. Run:
```bash
mklittlefs -c General_Driver/data -s 0x150000 littlefs.bin
```
3. Flash it (the address `0x290000` is where LittleFS lives in the default partition scheme):
```bash
esptool.py --chip esp32 --port COM4 --baud 921600 write_flash 0x290000 littlefs.bin
```

> ⚠️ The filesystem address depends on the partition scheme. If you used a different partition scheme when compiling, this address will be different. Stick with `default.csv` / "Default 4MB with spiffs" and `0x290000` will be correct.

---

## What Happens If Something Goes Wrong

### "Connecting........_____ " — upload keeps failing

The ESP32 needs to enter **bootloader mode** to accept a flash. This happens automatically via the RTS/DTR pins on most boards. If it doesn't:

**Try 1:** Hold the **BOOT button** on the board while clicking Upload, release it once you see "Connecting..." in the console.

**Try 2:** Manually enter bootloader:
1. Hold **BOOT** button
2. Press and release **EN/RST** button
3. Release **BOOT** button
4. Now start the upload

**Try 3:** Lower the baud rate — change upload speed from `921600` to `115200`

**Try 4:** Try a different USB cable

### "Wrong port" / No COM port appears

- Install the USB-to-serial driver:
  - If the chip on your board says **CP2102** → install Silicon Labs CP210x driver
  - If it says **CH340** or **CH341** → install CH340 driver
  - Search for the chip name + "driver download"
- On Mac, you may need to allow the driver in **System Preferences → Security & Privacy**
- On Linux, add your user to the `dialout` group: `sudo usermod -a -G dialout $USER` then log out and back in

### "Sketch too large"

The compiled code is bigger than the space allocated for it. Fix: go to **Tools → Partition Scheme** and select **"Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)"** or **"No OTA (2MB APP/2MB SPIFFS)"**.

### Board appears to be bricked / not booting after flash

Don't panic — the ESP32 almost never gets permanently bricked. You can always erase it and start over:

```bash
esptool.py --chip esp32 --port COM4 erase_flash
```

Then flash everything again from scratch.

### "LittleFS mount failed" in the serial monitor after flashing

You flashed the firmware but forgot to flash the filesystem. Run the filesystem upload step again (Step 9 in Arduino IDE method, or Step 6 in PlatformIO method).

### The robot boots but hangs after "WAVE ROVER / starting..."

This is **Bug #3** from the bug report — `while(!Serial) {}` in `General_Driver.ino:103`. It waits for a USB serial connection that never comes when powered from battery.

**Quick fix:** open `General_Driver.ino`, find line 103, and delete or comment out that line:
```cpp
// while(!Serial) {}   ← comment this out
```
Then re-flash.

### Serial monitor shows garbage / random characters

Your serial monitor baud rate is wrong. Set it to **115200** — that is what `Serial.begin(115200)` sets in `setup()`.

---

## Verifying It Worked

After flashing, open the serial monitor:
- Arduino IDE: **Tools → Serial Monitor** (set baud to 115200)
- PlatformIO: click the **plug icon** at the bottom toolbar

You should see output like:
```
WAVE ROVER
version: 0.95
starting...
Initialize LittleFS for Flash files ctrl.
LittleFS mount succeed.
...
/wifiConfig.json load succeed.
wifi mode on boot: AP+STA
AP mode starts...
SSID: UGV
...
UGV started.
Server Starts.
```

If you see this — the flash worked. The robot is running.

---

## Editing the WiFi Config Before Flashing

The `data/wifiConfig.json` file currently contains the **developer's** WiFi credentials:
```json
{
  "wifi_mode_on_boot": 3,
  "sta_ssid": "JSBZY-2.4G",
  "sta_password": "waveshare0755",
  "ap_ssid": "RoArm",
  "ap_password": "12345678"
}
```

Before uploading the filesystem, open this file in any text editor (Notepad, TextEdit, etc.) and change it to your own network. Otherwise the robot will try to connect to a network that doesn't exist near you and fall back to AP mode.

You can also leave it as-is and configure WiFi later by sending a JSON command over USB serial:
```json
{"T":403,"ssid":"YourWiFiName","password":"YourPassword"}
```

---

## Quick Reference — Which Method to Choose

| Situation | Best Method |
|-----------|-------------|
| Complete beginner, want a GUI | **Arduino IDE** |
| Developer, want fast builds and autocomplete | **PlatformIO** |
| Just want to run existing code, no changes | **esptool + pre-built bins** |
| Want to update only WiFi config | **esptool filesystem upload** |
| Something went wrong and board won't boot | **esptool erase_flash** then re-flash |

---

*End of Report 3.*
