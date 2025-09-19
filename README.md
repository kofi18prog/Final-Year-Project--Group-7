# Use the .ino files (Those not in a folder)
# ESP8266 + SIM800L + PIR + mmWave + COOLIX IR + SMS Control

## 📖 Overview

This project lets you control an Air Conditioner using an ESP8266, a SIM800L GSM module, and IR remote signals.
It supports three modes of control:

1. **Manual console commands** (`start` / `stop` via Serial Monitor).
2. **SMS commands** (`ON` / `OFF`) from an **authorized phone number only**.
3. **Automatic presence detection** using a **PIR sensor** and a **mmWave radar sensor**.

When presence is detected, the AC is turned **ON**.
When absence is detected, the AC is turned **OFF**.
All actions are confirmed via SMS.

---

## 🛠 Hardware Components

* **ESP8266 (NodeMCU / Wemos D1 Mini)**
* **SIM800L GSM Module** (powered from stable 4V supply)
* **PIR Sensor** (motion detection, digital output)
* **mmWave Radar Sensor** (presence detection, digital output)
* **IR LED on GPIO15 (D8)** (sends COOLIX IR codes to AC)
* **5V–4V step-down regulator** (for SIM800L power, very important!)

---

## 📡 Pin Connections

| Component    | ESP8266 Pin | Notes                              |
| ------------ | ----------- | ---------------------------------- |
| SIM800L TX   | GPIO3 (RX)  | Hardware UART                      |
| SIM800L RX   | GPIO1 (TX)  | Hardware UART                      |
| SIM800L VCC  | 4.0V supply | Needs stable 2A peak               |
| SIM800L GND  | GND         | Common ground                      |
| PIR Sensor   | D7 (GPIO13) | Digital input                      |
| mmWave Radar | D6 (GPIO12) | Digital input                      |
| IR LED       | D8 (GPIO15) | Connected via transistor for drive |

---

## ⚙️ Software Features

* **Authorized SMS control**: Only `AUTHORIZED_NUMBER` can send commands.
* **Commands**:

  * SMS `"ON"` or `"O"` → IR ON + confirmation SMS
  * SMS `"OFF"` or `"F"` → IR OFF + confirmation SMS
* **Console input**:

  * Type `"start"` in Serial Monitor → Manual ON
  * Type `"stop"` in Serial Monitor → Manual OFF
* **Auto mode (sensors)**:

  * PIR or mmWave detects presence → Wait 10s → Confirm → Turn **ON**
  * No presence detected → Wait 10s → Confirm → Turn **OFF**
* **IR sending**:

  * Sends **COOLIX hex command**
  * Also sends **raw timing array** (backup for reliability)
  * Each command sent `IR_REPEAT` times

---

## 🔑 Key Configuration

Inside the code:

```cpp
const char AUTHORIZED_NUMBER[] = "+233xxxxxxxxx"; // Who can control system
const char ALERT_NUMBER[] = "+233xxxxxxxxx";      // Who receives confirmations
const uint8_t IR_REPEAT = 10;                     // Number of times IR signal repeats
```

Update `AUTHORIZED_NUMBER` to your phone number before uploading.

---

## 🚀 Setup Instructions

1. Connect hardware as per table.
2. Ensure **SIM800L has a valid SIM card with SMS enabled**.
3. Power SIM800L from a regulated **4.0V supply** capable of 2A bursts.
4. Flash the code to ESP8266 using Arduino IDE.
5. Open Serial Monitor at **9600 baud**.

---

## 📲 Example Usage

* **Via SMS**:
  Send `"ON"` → AC turns ON → Confirmation SMS `"Remote ON executed"`
  Send `"OFF"` → AC turns OFF → Confirmation SMS `"Remote OFF executed"`

* **Via Console**:
  Type `"start"` → AC turns ON
  Type `"stop"` → AC turns OFF

* **Via Sensors**:
  PIR or mmWave detect movement → AC ON
  No movement for 10s → AC OFF

---

## ⚠️ Notes & Warnings

* **SIM800L is very sensitive to power drops** – always use a stable regulator.
* If IR is weak, use a **transistor driver circuit** for the IR LED.
* Raw IR arrays can be updated with captured codes.
