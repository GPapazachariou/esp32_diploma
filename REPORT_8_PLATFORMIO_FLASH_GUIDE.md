# How to Flash the Gimbal Firmware
### Step-by-step guide using PlatformIO + VS Code
### Written for someone who has never done this before

---

## What Is "Flashing"?

Your ESP32 chip is like a blank computer. It has no program on it until you put one there. **Flashing** means taking the code from this repository, compiling it (turning it into machine instructions the chip understands), and copying it onto the chip over a USB cable.

Think of it like burning a CD — you need the right software, the right cable, and the right settings.

---

## What You Need

| Item | What it is |
|------|-----------|
| A computer | Windows, Mac, or Linux all work |
| A USB cable | **Must be a data cable** — some cheap cables only charge and cannot transfer data. If flashing fails immediately, try a different cable. |
| The ESP32 board | The small green circuit board with the WiFi chip |
| This repository | The folder of code you are reading right now |

---

## Step 1 — Install VS Code

VS Code (Visual Studio Code) is a free text editor made by Microsoft. PlatformIO runs inside it.

1. Go to **https://code.visualstudio.com/**
2. Click the big download button for your operating system
3. Run the installer and follow the prompts — all default options are fine
4. Open VS Code when it finishes

> **What is VS Code?** It is just a program for editing files and running tools. It is not the actual compiler — it is the window you work in.

---

## Step 2 — Install PlatformIO inside VS Code

PlatformIO is the tool that compiles your code and sends it to the ESP32. It lives inside VS Code as an "extension".

1. Inside VS Code, look at the left sidebar — find the icon that looks like **four small squares** (Extensions)
2. Click it
3. In the search box that appears, type: `PlatformIO IDE`
4. The first result should say **"PlatformIO IDE"** by PlatformIO
5. Click the blue **Install** button
6. Wait — this takes **3–10 minutes** the first time because it downloads the ESP32 toolchain (the compiler)
7. When it finishes, VS Code will ask you to reload — click **Reload Now**

After reloading, you will see a new icon in the left sidebar that looks like an **alien head** 👽 — that is PlatformIO.

> **Why PlatformIO and not Arduino IDE?** PlatformIO handles library downloads automatically, has better error messages, and is easier to configure for complex projects like this one.

---

## Step 3 — Open the Project

The firmware code is in the `General_Driver` folder inside this repository.

1. In VS Code, go to the menu: **File → Open Folder**
2. Navigate to where you downloaded this repository
3. Open the **`General_Driver`** folder — not the outer `esp32_diploma` folder, the inner one
4. VS Code will ask "Do you trust the authors of the files in this folder?" — click **Yes, I trust the authors**
5. PlatformIO will automatically detect the `platformio.ini` file and set up the project

> **What is `platformio.ini`?** It is a small configuration file (already included in this repo) that tells PlatformIO:
> - Which chip this is (ESP32)
> - Which libraries to download
> - What baud rate to use for the serial monitor
>
> You do not need to edit it.

---

## Step 4 — Install Libraries Automatically

PlatformIO reads `platformio.ini` and downloads all required libraries by itself.

1. Click the **alien head icon** in the left sidebar to open PlatformIO
2. Under **"Project Tasks"**, find your project and click **"Build"** (or press `Ctrl+Alt+B`)
3. PlatformIO will download the libraries and compile the code
4. Watch the terminal at the bottom — it shows progress

The first build takes **2–5 minutes** because it downloads and compiles everything. Subsequent builds are much faster.

**If the build succeeds** you will see:
```
[SUCCESS] Took X.XX seconds
```

**If the build fails** — see the Troubleshooting section at the end of this guide.

> **What libraries does it download?**
> - **ArduinoJson** — parses the `{"T":133,...}` JSON commands
> - **Adafruit SSD1306** — drives the OLED display
> - **Adafruit GFX** — drawing primitives used by the OLED library
> - **INA219_WE** — reads the battery voltage sensor
>
> The **SCServo** library (for the servo motors) is already included locally in the `SCServo/` folder — it does not need to be downloaded.

---

## Step 5 — Add the SCServo Library

The SCServo library is included in this repository (in the `SCServo/` folder) but PlatformIO needs to know where to find it.

Check your `platformio.ini` file — it should already contain this line:

```ini
lib_deps =
    ...
```

PlatformIO also automatically searches for libraries in the project folder. Since `SCServo/` is inside the `General_Driver/` folder alongside the sketch, it will be found automatically.

> **If you get errors about `SMS_STS` or `SCServo` not found**, copy the `SCServo` folder into `General_Driver/lib/SCServo/`.

---

## Step 6 — Connect the ESP32

1. Take your USB cable and plug one end into your computer
2. Plug the other end into the **USB port on the ESP32 board** (there is only one USB port on it)
3. Your computer should make a sound and recognise the device

**On Windows:** Open Device Manager (search for it in the Start menu). You should see a new item appear under **"Ports (COM & LPT)"** called something like `Silicon Labs CP210x USB to UART Bridge (COM3)`. The number after `COM` is your port number — remember it.

**On Mac:** Open Terminal and type `ls /dev/cu.*` — you should see something like `/dev/cu.usbserial-0001`.

**On Linux:** Open Terminal and type `ls /dev/ttyUSB*` or `ls /dev/ttyACM*` — you should see `/dev/ttyUSB0` or similar.

> **If nothing appears:** Your USB cable is probably charge-only. Try a different cable. Also try a different USB port on your computer.

> **On Linux:** You may need permission to access the serial port. Run:
> ```bash
> sudo usermod -a -G dialout $USER
> ```
> Then log out and back in.

---

## Step 7 — Flash the Firmware

1. In VS Code with PlatformIO open, click the **alien head icon**
2. Under **"Project Tasks → env:esp32dev"**, click **"Upload"**

   — OR —

   Click the **→ (right arrow) Upload button** in the bottom toolbar of VS Code

3. PlatformIO will:
   - Compile the code (if not already done)
   - Automatically detect the COM port
   - Upload the firmware to the ESP32

Watch the terminal. You will see a progress bar:
```
Writing at 0x00010000... (5%)
Writing at 0x00014000... (10%)
...
Writing at 0x000b4000... (100%)
Hash of data verified.
Leaving...
Hard resetting via RTS pin...
```

When you see **"Hard resetting via RTS pin"**, flashing is complete. The ESP32 will restart automatically and run the new firmware.

> **What is happening?** PlatformIO compiles your `.ino` and `.h` files into a binary file (`.bin`), then uses a tool called `esptool.py` to copy that binary into the ESP32's flash memory over the USB/serial connection.

---

## Step 8 — Upload the Filesystem (WiFi Config)

The firmware stores its WiFi configuration in a separate area of the ESP32's flash called **LittleFS** (a small filesystem, like a tiny hard drive on the chip). You need to upload the config files separately from the firmware.

The config files are in the `General_Driver/data/` folder:
- `wifiConfig.json` — WiFi credentials
- `devConfig.json` — device settings

**Before uploading**, edit `wifiConfig.json` with your own WiFi details:

```json
{
  "wifi_mode_on_boot": 3,
  "sta_ssid": "YourWiFiName",
  "sta_password": "YourWiFiPassword",
  "ap_ssid": "Gimbal",
  "ap_password": "12345678"
}
```

Change `YourWiFiName` and `YourWiFiPassword` to your actual WiFi network name and password.

> **wifi_mode_on_boot values:**
> - `1` = AP mode only (the ESP32 creates its own hotspot — good for first use)
> - `2` = STA mode (connects to your router)
> - `3` = AP+STA (both at the same time — recommended)

**To upload the filesystem:**

1. In PlatformIO (alien head icon), under **"Project Tasks → env:esp32dev → Platform"**, click **"Upload Filesystem Image"**

   — OR —

   Open the PlatformIO terminal (click the terminal icon in the PlatformIO sidebar) and type:
   ```
   pio run --target uploadfs
   ```

2. Wait for it to complete — you will see:
   ```
   Wrote 262144 bytes (XXXX compressed) at 0x00290000 in X.X seconds
   Hash of data verified.
   ```

> **Important:** Upload the filesystem **after** the firmware, not before. The firmware must be on the chip first so the filesystem partition exists.

---

## Step 9 — Open the Serial Monitor

The Serial Monitor lets you see what the ESP32 is printing (debug messages) and send JSON commands to it by typing them.

1. In VS Code, click the **plug icon** at the bottom toolbar — or in PlatformIO click **"Monitor"**
2. The monitor opens at 115200 baud (this is set in `platformio.ini` — no need to change it)

You should see the ESP32 booting:
```
Initialize LittleFS.
Gimbal servo init (Serial1 GPIO18/19).
WiFi init.
/wifiConfig.json load succeed.
...
Server Starts.
XX:XX:XX:XX:XX:XX
```

If you see this, everything worked.

**Try sending a command** — type this in the serial monitor input box and press Enter:
```json
{"T":405}
```
The ESP32 should reply with its current WiFi status:
```json
{"ip":"192.168.1.105","rssi":-62,"wifi_mode_on_boot":3,...}
```

---

## Step 10 — Test the Gimbal

With the servos connected and power on, try moving the gimbal:

**Move to centre:**
```json
{"T":133,"X":0,"Y":0,"SPD":200,"ACC":0}
```

**Pan right 45°:**
```json
{"T":133,"X":45,"Y":0,"SPD":200,"ACC":0}
```

**Tilt up 30°:**
```json
{"T":133,"X":0,"Y":30,"SPD":200,"ACC":0}
```

**Stop and hold current position:**
```json
{"T":135}
```

If the servos move, everything is working correctly.

---

## Using WiFi Instead of USB

Once the ESP32 is on your network, you can send commands from any device on the same WiFi without a USB cable.

**Find the IP address:** Look at the serial monitor output for a line like `ST:192.168.1.105`. That is the IP.

**From a browser:** Go to `http://192.168.1.105/` — a control web page will load.

**From Python:**
```python
import requests
requests.get('http://192.168.1.105/js', params={'json': '{"T":133,"X":45,"Y":0,"SPD":300,"ACC":0}'})
```

**From curl (terminal):**
```bash
curl "http://192.168.1.105/js?json=%7B%22T%22%3A133%2C%22X%22%3A45%2C%22Y%22%3A0%2C%22SPD%22%3A300%2C%22ACC%22%3A0%7D"
```

---

## Troubleshooting

### "Port not found" or "No such file or directory"
PlatformIO could not find the ESP32. Check:
- Is the USB cable plugged in firmly on both ends?
- Is it a data cable (not charge-only)?
- On Linux: did you add yourself to the `dialout` group?
- Try a different USB port on your computer

### Build fails: `fatal error: ArduinoJson.h: No such file or directory`
Libraries did not download. Click Build again — sometimes the first download fails on slow connections.

### Build fails: `SMS_STS.h: No such file or directory`
The SCServo library was not found. Copy the `SCServo/` folder into `General_Driver/lib/`:
```
General_Driver/
└── lib/
    └── SCServo/
        ├── SCServo.h
        ├── SMS_STS.h
        └── ...
```

### Upload fails: `A fatal error occurred: Failed to connect to ESP32`
The chip is not entering bootloader mode. Try:
1. Hold the **BOOT button** on the ESP32 board while clicking Upload
2. Release the BOOT button once the upload starts (when you see "Connecting...")

Some ESP32 boards need this manual boot trigger. If your board has a BOOT button (usually labelled `IO0` or `BOOT`), hold it during upload.

### Serial monitor shows garbled characters
The baud rate is wrong. Make sure it is set to **115200** in the PlatformIO monitor settings.

### OLED shows nothing
The OLED uses I2C address `0x3C`. Check the wiring: SDA to GPIO 32, SCL to GPIO 33, VCC to 3.3V, GND to GND.

### Servos do not respond to commands
- Check that 12V power is connected to the servo bus (servos need separate power, not just USB)
- Check that GPIO 18 (RX) and GPIO 19 (TX) are wired to the servo bus half-duplex adapter
- Send `{"T":605,"cmd":1}` to enable debug output — error messages will appear in the serial monitor

---

## Quick Reference: PlatformIO Buttons

| Button location | What it does |
|----------------|-------------|
| ✓ (tick) in bottom toolbar | **Build** — compile code, check for errors |
| → (arrow) in bottom toolbar | **Upload** — build + flash to ESP32 |
| 🔌 (plug) in bottom toolbar | **Monitor** — open serial monitor |
| 🗑️ (bin) in bottom toolbar | **Clean** — delete compiled files and start fresh |
| Alien head icon in sidebar | **Open PlatformIO home** — access all tasks |

---

## Summary Checklist

- [ ] VS Code installed
- [ ] PlatformIO extension installed and loaded
- [ ] `General_Driver` folder opened in VS Code
- [ ] First build succeeded (no red errors)
- [ ] ESP32 connected via USB data cable
- [ ] Firmware uploaded (Upload button)
- [ ] `data/wifiConfig.json` edited with your WiFi details
- [ ] Filesystem uploaded (Upload Filesystem Image)
- [ ] Serial monitor opened — boot messages visible
- [ ] Sent `{"T":405}` — received WiFi status reply
- [ ] Servos powered and responded to `{"T":133,"X":0,"Y":0,"SPD":200,"ACC":0}`
