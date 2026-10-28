#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


// WI-FI CONFIGURATION

const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";

// MQTT CONFIGURATION
const char* MQTT_BROKER = "test.mosquitto.org";
const int MQTT_PORT = 1883;

const char* MQTT_TOPIC = "smart-home-guard/setif-esp32/telemetry";

WiFiClient mqttWifiClient;
PubSubClient mqttClient(mqttWifiClient);

unsigned long lastMqttPublish = 0;
const unsigned long MQTT_PUBLISH_INTERVAL = 1000;


// TELEGRAM CONFIGURATION

const char* BOT_TOKEN = "BOT-TOKEN";
const char* CHAT_ID = "8497546585";

// OLED CONFIGURATION

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);

// ESP32-S3 PIN CONFIGURATION

const int BUZZER_PIN = 7;

const int GREEN_LED  = 4;
const int YELLOW_LED = 5;
const int RED_LED    = 6;

const int VOLTAGE_PIN = 1;
const int CURRENT_PIN = 2;

const int OLED_SDA = 8;
const int OLED_SCL = 9;

// SIMULATED MEASUREMENT RANGES

const float MAX_VOLTAGE = 300.0;
const float MAX_CURRENT = 30.0;

// THRESHOLDS

const float MIN_VOLTAGE = 190.0;
const float HIGH_VOLTAGE = 250.0;
const float MAX_ALLOWED_CURRENT = 16.0;

// TIMING

unsigned long lastUpdate = 0;
const unsigned long UPDATE_INTERVAL = 250;

unsigned long lastWiFiAttempt = 0;
const unsigned long WIFI_RETRY_INTERVAL = 10000;

// SYSTEM STATUS

enum SystemStatus {
  NORMAL,
  LOW_VOLTAGE,
  CRITICAL
};

SystemStatus previousStatus = NORMAL;
bool firstReading = true;

// LED CONTROL

void setLEDs(bool green, bool yellow, bool red) {
  digitalWrite(GREEN_LED, green ? HIGH : LOW);
  digitalWrite(YELLOW_LED, yellow ? HIGH : LOW);
  digitalWrite(RED_LED, red ? HIGH : LOW);
}

// STATUS TEXT

const char* getStatusText(SystemStatus status) {
  switch (status) {
    case NORMAL:
      return "NORMAL";

    case LOW_VOLTAGE:
      return "LOW VOLTAGE";

    case CRITICAL:
      return "CRITICAL";

    default:
      return "UNKNOWN";
  }
}

// URL ENCODING

String urlEncode(const String& input) {
  String encoded;
  char hexBuffer[4];

  for (unsigned int i = 0; i < input.length(); i++) {
    unsigned char c = (unsigned char)input[i];

    if ((c >= 'A' && c <= 'Z') ||
        (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') ||
        c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += (char)c;
    } else {
      snprintf(hexBuffer, sizeof(hexBuffer), "%%%02X", c);
      encoded += hexBuffer;
    }
  }

  return encoded;
}


// TELEGRAM NOTIFICATION

bool sendTelegramMessage(const String& message) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Telegram skipped: Wi-Fi disconnected.");
    return false;
  }

  if (String(BOT_TOKEN) == "PASTE_YOUR_NEW_BOT_TOKEN_HERE" ||
      strlen(BOT_TOKEN) == 0) {
    Serial.println("Telegram error: Add your bot token.");
    return false;
  }

  WiFiClientSecure client;

  client.setInsecure();

  HTTPClient https;

  String url = String("https://api.telegram.org/bot") +
               BOT_TOKEN +
               "/sendMessage";

  String payload = "chat_id=" + String(CHAT_ID) +
                   "&text=" + urlEncode(message);

  https.setTimeout(5000);

  if (!https.begin(client, url)) {
    Serial.println("Telegram error: HTTPS initialization failed.");
    return false;
  }

  https.addHeader(
    "Content-Type",
    "application/x-www-form-urlencoded"
  );

  int httpCode = https.POST(payload);
  bool success = (httpCode == HTTP_CODE_OK);

  if (success) {
    Serial.println("Telegram notification sent.");
  } else {
    Serial.print("Telegram HTTP error: ");
    Serial.println(httpCode);

    if (httpCode > 0) {
      Serial.println(https.getString());
    } else {
      Serial.println(https.errorToString(httpCode));
    }
  }

  https.end();
  return success;
}

// WI-FI CONNECTION

void connectWiFi() {
  Serial.println("Connecting to Wi-Fi...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startTime = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - startTime < 15000) {
    delay(250);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Wi-Fi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    sendTelegramMessage(
      "SMART HOME GUARD\n"
      "System online!\n"
      "ESP32-S3 connected to Wi-Fi."
    );
  } else {
    Serial.println("Wi-Fi connection failed.");
    Serial.println("Monitoring will continue locally.");
  }
}

void connectMQTT() {
  Serial.println("[MQTT] Checking connection...");

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[MQTT] WiFi is not connected!");
    return;
  }

  if (mqttClient.connected()) {
    Serial.println("[MQTT] Already connected!");
    return;
  }

  mqttClient.setServer("test.mosquitto.org", 1883);
  mqttClient.setBufferSize(512);
  mqttClient.setKeepAlive(30);

  String clientId = "SmartGuard-";
  clientId += String((uint32_t)ESP.getEfuseMac(), HEX);

  Serial.println("[MQTT] Connecting to test.mosquitto.org:1883");

  if (mqttClient.connect(clientId.c_str())) {
    Serial.println("[MQTT] CONNECTED SUCCESSFULLY!");
  } else {
    Serial.print("[MQTT] Connection failed. Error code: ");
    Serial.println(mqttClient.state());
  }
}

// OLED STARTUP SCREEN

void showStartupScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);

  display.println("SMART HOME GUARD");
  display.println("----------------");
  display.println("ESP32-S3 Starting...");
  display.println("Initializing...");
  display.display();
}

// OLED MEASUREMENT DISPLAY

void updateDisplay(
  float voltage,
  float current,
  float power,
  const char* status
) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);

  display.println("SMART HOME GUARD");
  display.println("----------------");

  display.print("Voltage: ");
  display.print(voltage, 1);
  display.println(" V");

  display.print("Current: ");
  display.print(current, 2);
  display.println(" A");

  display.print("Power: ");
  display.print(power, 0);
  display.println(" VA");

  display.println("----------------");
  display.print("STATUS: ");
  display.println(status);

  display.display();
}

// SERIAL DIAGNOSTICS

void printDiagnostics(
  int voltageRaw,
  int currentRaw,
  float voltage,
  float current,
  float power,
  const char* status
) {
  Serial.print("ADC Voltage: ");
  Serial.print(voltageRaw);

  Serial.print(" | Voltage: ");
  Serial.print(voltage, 1);
  Serial.print(" V");

  Serial.print(" | ADC Current: ");
  Serial.print(currentRaw);

  Serial.print(" | Current: ");
  Serial.print(current, 2);
  Serial.print(" A");

  Serial.print(" | Apparent Power: ");
  Serial.print(power, 0);
  Serial.print(" VA");

  Serial.print(" | Status: ");
  Serial.println(status);
}

// BUILD STATUS NOTIFICATION

String buildStatusMessage(
  SystemStatus status,
  float voltage,
  float current,
  float power
) {
  String message = "SMART HOME GUARD\n";

  if (status == CRITICAL) {
    message += "ALERT: CRITICAL CONDITION!\n";

    if (voltage > HIGH_VOLTAGE) {
      message += "Cause: HIGH VOLTAGE\n";
    }

    if (current > MAX_ALLOWED_CURRENT) {
      message += "Cause: OVERCURRENT\n";
    }
  } else if (status == LOW_VOLTAGE) {
    message += "WARNING: LOW VOLTAGE\n";
  } else {
    message += "System returned to NORMAL.\n";
  }

  message += "Voltage: " + String(voltage, 1) + " V\n";
  message += "Current: " + String(current, 2) + " A\n";
  message += "Apparent power: " + String(power, 0) + " VA";

  return message;
}

// SETUP

void setup() {
  Serial.begin(115200);

  // Configure outputs
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  // Configure analog inputs
  pinMode(VOLTAGE_PIN, INPUT);
  pinMode(CURRENT_PIN, INPUT);

  analogReadResolution(12);
  analogSetPinAttenuation(VOLTAGE_PIN, ADC_11db);
  analogSetPinAttenuation(CURRENT_PIN, ADC_11db);

  // Set safe initial output states
  setLEDs(false, false, false);
  noTone(BUZZER_PIN);

  // Initialize OLED I2C
  Wire.begin(OLED_SDA, OLED_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("ERROR: OLED initialization failed!");

    while (true) {
      delay(100);
    }
  }

  showStartupScreen();

  Serial.println();
  Serial.println("========================");
  Serial.println(" SMART HOME GUARD");
  Serial.println("========================");

  // Initialize Wi-Fi after local hardware
  // so the display is initialized first.
  connectWiFi();

  Serial.println("Monitoring started.");
}

// MAIN LOOP

void loop() {
  // Retry Wi-Fi connection periodically
  if (WiFi.status() != WL_CONNECTED &&
      millis() - lastWiFiAttempt >= WIFI_RETRY_INTERVAL) {
    lastWiFiAttempt = millis();

    Serial.println("Retrying Wi-Fi...");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }

  // Run monitoring at the configured interval
  if (millis() - lastUpdate < UPDATE_INTERVAL) {
    return;
  }

  lastUpdate = millis();

  // Read potentiometers
  int voltageRaw = analogRead(VOLTAGE_PIN);
  int currentRaw = analogRead(CURRENT_PIN);

  // Convert ADC values into simulated measurements
  float voltage = (voltageRaw / 4095.0f) * MAX_VOLTAGE;
  float current = (currentRaw / 4095.0f) * MAX_CURRENT;

  // Calculate apparent power
  float apparentPower = voltage * current;

  // Detect faults
  bool underVoltage = voltage < MIN_VOLTAGE;
  bool overVoltage = voltage > HIGH_VOLTAGE;
  bool overCurrent = current > MAX_ALLOWED_CURRENT;

  unsigned long lastMqttPublish = 0;
  const unsigned long MQTT_PUBLISH_INTERVAL = 1000;

  SystemStatus currentStatus;

  if (overVoltage || overCurrent) {
    currentStatus = CRITICAL;
  } else if (underVoltage) {
    currentStatus = LOW_VOLTAGE;
  } else {
    currentStatus = NORMAL;
  }

  const char* statusText = getStatusText(currentStatus);

  // LED AND BUZZER CONTROL

  if (currentStatus == CRITICAL) {
    setLEDs(false, false, true);
    tone(BUZZER_PIN, 2000);
  } else if (currentStatus == LOW_VOLTAGE) {
    setLEDs(false, true, false);

    if ((millis() / 300) % 2 == 0) {
      tone(BUZZER_PIN, 1000);
    } else {
      noTone(BUZZER_PIN);
    }
  } else {
    setLEDs(true, false, false);
    noTone(BUZZER_PIN);
  }
  
  // OLED UPDATE
  
  updateDisplay(
    voltage,
    current,
    apparentPower,
    statusText
  );

  // SERIAL MONITOR

  printDiagnostics(
    voltageRaw,
    currentRaw,
    voltage,
    current,
    apparentPower,
    statusText
  );

  // TELEGRAM ALERTS ON STATUS CHANGES
  
  if (firstReading) {
    previousStatus = currentStatus;
    firstReading = false;

    // Send an initial status report.
    sendTelegramMessage(
      buildStatusMessage(
        currentStatus,
        voltage,
        current,
        apparentPower
      )
    );
  } else if (currentStatus != previousStatus) {
    String message = buildStatusMessage(
      currentStatus,
      voltage,
      current,
      apparentPower
    );

    sendTelegramMessage(message);
    previousStatus = currentStatus;
  }
  connectMQTT();
  mqttClient.loop();

  if (mqttClient.connected() &&
      millis() - lastMqttPublish >= MQTT_PUBLISH_INTERVAL) {

    lastMqttPublish = millis();

    char payload[160];

    snprintf(
      payload,
      sizeof(payload),
      "{\"voltage\":%.1f,\"current\":%.2f,\"power\":%.1f,\"status\":\"%s\"}",
      voltage,
      current,
      apparentPower,
      stat
    );

    if (mqttClient.publish(
          "smart-home-guard/setif-esp32/telemetry",
          payload
        )) {
      Serial.print("[MQTT] Published: ");
      Serial.println(payload);
    } else {
      Serial.println("[MQTT] Publish failed!");
    }
  }
}
