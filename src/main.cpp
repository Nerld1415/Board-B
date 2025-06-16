#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ArduinoJson.h>

// ✅ 함수 선언 (앞에 선언 필수)
void callback(char* topic, byte* payload, unsigned int length);
void setRGB(bool r, bool g, bool b);
void reconnect();

// 📡 Wi-Fi 정보
const char* ssid = "Arnoldroom";
const char* password = "Fanta!1600";

// 🛰️ MQTT 브로커 정보
const char* mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;
const char* outTopic = "i2r/warnold2114@gmail.com/out";
const char* sensorTopic = "i2r/sensor";
const char* inTopic = "i2r/warnold2114@gmail.com/in";

// 핀 정의
#define DHTPIN 10
#define DHTTYPE DHT11
#define LIGHT_SENSOR_PIN 2
#define BUZZER_PIN 11
#define RED_PIN 15
#define GREEN_PIN 21
#define BLUE_PIN 16

WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHTPIN, DHTTYPE);

// 전역 상태
bool autoMode = false;
int lastOrder = -1;
unsigned long lastSensorSend = 0;
unsigned long lastStatusSend = 0;

void setup() {
  Serial.begin(115200);
  dht.begin();

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LIGHT_SENSOR_PIN, INPUT);
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  setRGB(0, 0, 0);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi 연결 완료");

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  unsigned long now = millis();

  if (autoMode && now - lastSensorSend > 5000) {
    float temp = dht.readTemperature();
    float hum = dht.readHumidity();
    int light = analogRead(LIGHT_SENSOR_PIN);

    StaticJsonDocument<128> doc;
    doc["temp"] = temp;
    doc["hum"] = hum;
    doc["light"] = light;

    char buffer[128];
    serializeJson(doc, buffer);
    client.publish(sensorTopic, buffer);

    Serial.print("📤 센서 전송: ");
    Serial.println(buffer);

    lastSensorSend = now;
  }

  if (now - lastStatusSend > 5000) {
    StaticJsonDocument<100> doc;
    doc["status"] = "connected";
    doc["id"] = "boardB";

    char buffer[128];
    serializeJson(doc, buffer);
    client.publish(outTopic, buffer);
    lastStatusSend = now;
  }
}

// 📥 MQTT 콜백 함수
void callback(char* topic, byte* payload, unsigned int length) {
  StaticJsonDocument<128> doc;
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err) return;

  if (String(topic) == inTopic) {
    if (doc["mode"].is<const char*>()) {
      const char* modeStr = doc["mode"].as<const char*>();
      autoMode = (strcmp(modeStr, "auto") == 0);
      Serial.println(autoMode ? "🔁 자동 모드 전환됨" : "🎮 수동 모드 전환됨");
    }

    if (doc["order"].is<int>()) {
      lastOrder = doc["order"];
      if (lastOrder == 0) setRGB(1, 0, 0);       // 정지
      else if (lastOrder == 1) setRGB(0, 1, 0);  // 전진
      else if (lastOrder == 2) setRGB(0, 0, 1);  // 후진
      else setRGB(0, 0, 0);                      // 기타
    }
  }
}

// 🔧 RGB 설정
void setRGB(bool r, bool g, bool b) {
  digitalWrite(RED_PIN, r ? HIGH : LOW);
  digitalWrite(GREEN_PIN, g ? HIGH : LOW);
  digitalWrite(BLUE_PIN, b ? HIGH : LOW);
}

// 🔄 MQTT 연결
void reconnect() {
  while (!client.connected()) {
    String clientId = "BoardB-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("🔌 MQTT 연결 성공");
      client.subscribe(inTopic);
    } else {
      Serial.println("❌ MQTT 연결 실패. 재시도...");
      delay(3000);
    }
  }
}
