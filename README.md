# 🎧 AcousticSense
> **An Extreme-Edge Predictive Maintenance Framework for Industrial Motors**

[![C++](https://img.shields.io/badge/C++-17-blue.svg)](https://isocpp.org/)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-IDE-orange.svg)](https://platformio.org/)
[![Edge Impulse](https://img.shields.io/badge/TinyML-Edge_Impulse-red.svg)](https://edgeimpulse.com/)
[![Hardware](https://img.shields.io/badge/Hardware-ESP32--S3-lightgrey.svg)]()

---

## 📖 Overview
This repository contains the complete software and hardware architecture for an advanced Industrial IoT condition monitoring system. Moving beyond traditional, cloud-dependent acoustic monitoring, AcousticSense introduces a **"Fully Localized"** edge AI framework. 

It utilizes an I2S MEMS digital microphone to capture high-fidelity acoustic emissions directly at the sensory edge. The system then mathematically cross-references these signals using a dual-engine machine learning pipeline running locally on an ESP32-S3. By executing a 1D Convolutional Neural Network (CNN) to identify specific faults and a K-Means Anomaly Arbiter to flag unclassified mechanical degradation, AcousticSense guarantees zero-downtime diagnostic reasoning without the latency, bandwidth, or privacy constraints of cloud computing.

---

## 🔗 Dataset & Pre-trained Model
* **Dataset Architecture:** Custom dataset generated via an ESP32 Wi-Fi WAV recorder to ensure zero hardware-mismatch bias. Features classes for normal operation, mechanical friction, loose wiring, and ambient factory noise.
* **DSP & Training Platform:** Features extracted using Mel-Filterbank Energy (MFE) and trained/quantized via Edge Impulse Studio.
* **Trained Edge Model:** The optimized `AcousticSense_v1_inferencing` Arduino library is included in this repository. 
  * *(Note: Place the Edge Impulse C++ library folder directly into the `lib/` directory before compiling the application).*

---

## ✨ Key Technical Features
* **Dual-Engine ML Pipeline:** Combines a custom-trained 1D CNN (32 filters) for distinct fault classification with an unsupervised K-Means algorithm to flag out-of-distribution acoustic anomalies.
* **Ultra-Low Latency Inference:** Clocked at just **7 ms** inference time for real-time monitoring and instantaneous fault reaction.
* **Resource Optimized:** Implements extreme 8-bit INT quantization, reducing the active footprint to **14.7 KB of Peak RAM** and **41.2 KB of Flash** memory.
* **Robust Temporal Smoothing:** Employs a custom C++ sliding window average (debouncer) to logically filter out sudden, transient factory background noise and mitigate false positive alerts.

---

## 🛠️ Technology Stack
| Domain | Technologies Used |
| :--- | :--- |
| **Machine Learning** | TinyML, Edge Impulse, 1D CNN, K-Means Clustering |
| **Digital Signal Processing** | Mel-Filterbank Energy (MFE), Audio Spectrograms |
| **Firmware & Logic** | Embedded C++, Arduino Framework, PlatformIO |
| **Edge Hardware** | ESP32-S3 (Xtensa Dual-Core), INMP441 Microphone, SSD1306 OLED |

---

## 🚀 Local Setup & Installation

**1. Clone the Repository**
```bash
git clone [https://github.com/harshadith/AcousticSense.git](https://github.com/harshadith/AcousticSense.git)
cd AcousticSense

2. Set Up the EnvironmentOpen the cloned directory in VS Code.Ensure the PlatformIO IDE extension is installed. PlatformIO will automatically read the platformio.ini file and download the required ESP32-S3 board dependencies.3. Install the ML Library
Ensure your downloaded Edge Impulse Arduino library is placed inside the lib/ folder.PlaintextAcousticSense/
 ├── lib/
 │   └── AcousticSense_v1_inferencing/  <-- Place the unzipped ML library here
 ├── src/
 │   └── main.cpp
 └── platformio.ini
4. Build and Flash the Firmware
Connect your ESP32-S3 via USB. Use the PlatformIO CLI or the VS Code UI shortcuts:Bashpio run --target upload
🔌 Pinout & Wiring GuideCRITICAL: The INMP441 microphone requires a digital I2S connection to bypass analog electromagnetic interference common on factory floors. Ensure the L/R channel pin is grounded to lock the microphone to the left channel, which matches the software's I2S_CHANNEL_FMT_ONLY_LEFT configuration.ComponentComponent PinESP32-S3 PinNotesSSD1306 OLED (I2C)VCCGNDSCLSDA3V3GNDGPIO 9GPIO 8Continuous 3.3V powerCommon GroundStandard I2C ClockStandard I2C DataINMP441 (Audio)VDDGNDL/R3V3GNDGNDContinuous 3.3V powerCommon GroundCritical: Pull to GND for Left ChannelINMP441 (I2S Data)WSSCKSDGPIO 4GPIO 5GPIO 6Word Select (I2S Clock)Serial Clock (I2S BCLK)Serial Data (I2S Audio Stream)
