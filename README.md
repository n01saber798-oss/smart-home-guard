# 🛡️ Smart Home Guard

### ESP32-Based Electrical Monitoring and Alert System

Smart Home Guard is an educational IoT project designed to monitor simulated electrical voltage and current, display measurements in real time, and notify the user when predefined thresholds are exceeded.

The project combines ESP32, an OLED display, status LEDs, a buzzer, MQTT communication, Telegram notifications, and a web-based monitoring dashboard.

## ✨ Features

* Real-time voltage and current monitoring.
* Apparent power calculation.
* OLED measurement display.
* Visual system status using LEDs.
* Audible warnings using a buzzer.
* MQTT telemetry publishing.
* Telegram alerts when system status changes.
* Web dashboard with live measurements and history.
* Wokwi simulation for educational testing.

## 🧰 Technologies

* ESP32-S3
* Arduino C++
* HTML, CSS, and JavaScript
* MQTT
* Telegram Bot API
* Wokwi Simulator

## 🏗️ System Architecture

ESP32 Simulation → MQTT Broker → Web Dashboard

The ESP32 firmware reads simulated voltage and current inputs, calculates apparent power, evaluates the configured thresholds, and publishes telemetry through MQTT.

The dashboard subscribes to the telemetry topic and displays incoming measurements.

## ⚙️ Monitoring Thresholds

| Condition                       | System State |
| ------------------------------- | ------------ |
| Voltage below 190 V             | Low Voltage  |
| Voltage above 250 V             | Critical     |
| Current above 16 A              | Critical     |
| Values within configured limits | Normal       |

These thresholds are configurable in the firmware.

## 🚀 Getting Started

1. Open the Wokwi simulation.
2. Start the ESP32 simulation.
3. Open the web dashboard.
4. Connect the dashboard to the configured MQTT broker and topic.
5. Adjust the simulated inputs and observe the measurements.

## 🔐 Security

Do not commit Telegram bot tokens, passwords, or other credentials to a public repository.

Use placeholders for credentials and configure your own secrets locally.

The public MQTT broker used for testing is not intended for private or safety-critical monitoring.

## ⚠️ Disclaimer

This project is an educational prototype using simulated electrical measurements. It is not a certified electrical protection system and must not be used as a substitute for properly rated electrical monitoring or protection equipment.

## 👨‍💻 Contributions

Suggestions, bug reports, and improvements are welcome. Please open an issue or submit a pull request.

## 📄 License

The project license will be specified in the `LICENSE` file.
