#include <HardwareSerial.h>
#include <DHT.h>

HardwareSerial LoRa(2);
HardwareSerial camSerial(1); // UART1
String data = "";
int fireState=0;

#define DHT22_PIN 13
DHT dht22(DHT22_PIN, DHT22);

#define MQ7_PIN 34
#define RL 10000.0
#define Vc 3.3
#define R0 11000.0
#define NODE_ID "Node_001"


#define TEMP_DELTA 0.5
#define HUMI_DELTA 1.0
#define CO_DELTA   0.5


#define SLEEP_TIME 30e6  
#define M0_PIN 25
#define M1_PIN 26

float temperature = 0;
float humidity = 0;
float mq7Value = 0;
float voltage = 0;
float RS = 0;
float ratio = 0;
float CO_ppm = 0;

RTC_DATA_ATTR float lastTemp = -100;
RTC_DATA_ATTR float lastHumi = -100;
RTC_DATA_ATTR float lastCO   = -100;
void configLoRa(){
  digitalWrite(M0_PIN, HIGH);
  digitalWrite(M1_PIN, HIGH);
  delay(100);
  Serial.println("⚙️ Đang cấu hình LoRa E32...");
   byte configData[6] = {
    0xC0, // cấu hình vĩnh viễn, ghi vào flash
    0x00, // Địa chỉ cao = 0
    0x00, // Địa chỉ thấp = 0
    0x1A, // SPED: UART 9600, Air rate = 0.3kbps (tối đa tầm xa)
    0x10, // CHAN = 433 + 0x17 = 446 MHz(dài)
    0x07  // OPTION: 20dBm, Công suất tối đa
  };
  LoRa.write(configData, 6);
  delay(100);
  Serial.println("✅ Đã cấu hình xong LoRa.");

  // Chuyển về chế độ hoạt động
  digitalWrite(M0_PIN, LOW);
  digitalWrite(M1_PIN, LOW);
  delay(100);
}

void setup() {
  Serial.begin(9600);
  pinMode(M0_PIN, OUTPUT);
  pinMode(M1_PIN, OUTPUT);
  digitalWrite(M0_PIN, LOW);
  digitalWrite(M1_PIN, LOW);
  delay(50);
  LoRa.begin(9600, SERIAL_8N1, 16, 17);
  camSerial.begin(9600, SERIAL_8N1, 5, 4); // RX=5, TX=4 cho ESP32-CAM
  dht22.begin();
  pinMode(2,OUTPUT);

  Serial.println("=== LoRa Sender (Forest Fire Node) ===");
}

void readDHT22() {
  humidity  = dht22.readHumidity();
  temperature = dht22.readTemperature();

  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("❌ Lỗi đọc DHT22!");
  } else {
    Serial.println("🌡 Nhiệt độ: " + String(temperature) + " °C");
    Serial.println("💧 Độ ẩm   : " + String(humidity) + " %");
  }
}
void readMQ7() {
  mq7Value = analogRead(MQ7_PIN);
  voltage = mq7Value * (Vc / 4095.0);
  RS = RL * ((Vc - voltage) / voltage);
  ratio = RS / R0;
  CO_ppm = 99.042 * pow(ratio, -1.518);

  Serial.print("🛑 CO: ");
  Serial.print(CO_ppm, 1);
  Serial.println(" ppm");
}
void readCam(){
    while (camSerial.available()) {
    char c = camSerial.read();
    if (c == '\n') {
      data.trim();
      if (data == "true") {
        fireState=1;
        Serial.print("Trang thai lua: ");Serial.println(fireState);

      } else if (data == "false") {
        fireState=0;
        Serial.print("Trang thai lua: ");Serial.println(fireState);
      }
      data = "";
    } else {
      data += c;
    }
  }
}
bool shouldSendData() {
  return (fabs(temperature - lastTemp) >= TEMP_DELTA) ||
         (fabs(humidity - lastHumi) >= HUMI_DELTA) ||
         (fabs(CO_ppm - lastCO) >= CO_DELTA) ||
         (fireState == 1);
}
void readLoRa() {
  if (LoRa.available()) {
    String msg = LoRa.readStringUntil('\n');  
    msg.trim();
    if(String(msg)=="2on"){
      Serial.println(msg);
      digitalWrite(2,HIGH);
    }
    else if(String(msg)=="2off"){
      digitalWrite(2,LOW);
      Serial.println(msg);
    }
    else if(String(msg)=="3on"){
      camSerial.println("3on");
      Serial.println(msg);
    }
    else if(String(msg)=="3off"){
      camSerial.println("3off");
      Serial.println(msg);
    }
  }
}

void loop() {
  Serial.println("---📡 Chu kỳ đo mới---");
  Serial.println("ID: " + String(NODE_ID));

  readLoRa();
  readDHT22();
  readMQ7();
  readCam();

  if (shouldSendData()) {
    String payload = "{";
    payload += "\"id\":\"" + String(NODE_ID) + "\",";
    payload += "\"humidity\":" + String(humidity, 1) + ",";
    payload += "\"temperature\":" + String(temperature, 1) + ",";
    payload += "\"CO\":" + String(CO_ppm, 1) + ",";
    payload += "\"fireState\":" + String(fireState);
    payload += "}";



    LoRa.println(payload);

    Serial.println("📤 Gửi dữ liệu LoRa:");
    Serial.println(payload);

    lastTemp = temperature;
    lastHumi = humidity;
    lastCO = CO_ppm;

  } else {
    Serial.println("📭 Không có thay đổi đáng kể, bỏ qua gửi để tiết kiệm năng lượng.");
  }

  //Serial.println("💤 ESP32 ngủ tiết kiệm năng lượng 30 giây...");
  delay(2000);
  //esp_sleep_enable_timer_wakeup(SLEEP_TIME);
  //esp_deep_sleep_start();
}