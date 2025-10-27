#include <HardwareSerial.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

HardwareSerial LoRa(2);  // Dùng UART2: RX=16, TX=17
WiFiClientSecure espClient;
PubSubClient client(espClient);

#define TFT_CS   5
#define TFT_DC   22
#define TFT_RST  21
#define TFT_MOSI 23
#define TFT_SCK  18
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
#define BUZZER_PIN 13

const char *ssid = "Phong Dien Tu Vien Thong";// tên wifi (p505,11112222)(Nokia1280,1122334455667788)(Phong Dien Tu Vien Thong,dtvtthuyloi)(K1-T3,TLU@2024)
const char *password = "dtvtthuyloi";

const char *mqtt_server = "e55a7426310c457a85053f429ac5f440.s1.eu.hivemq.cloud"; // url server mqtt
const int mqtt_port = 8883;
const char *mqtt_username = "dinhhieu";     // mqtt username
const char *mqtt_password = "Dxh22012004@"; // mqtt password


float temperature = 0;
float humidity = 0;
float CO_ppm = 0;
int fireState = 0;
String nodeID = "";

float fireRisk = 0;  // 0 - 100
String fireLevel = "An toan";

#define TREND_SIZE 15
float lichsufireRisk[TREND_SIZE] = {0};
int xuhuongIndex = 0;
int ruirochay;
String fireTrend = "On dinh";


void WifiSetup()
{
  Serial.println();
  Serial.print("Đang kết nối wifi ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.print("");
  Serial.println("Kết nối Wifi thành công! ");
  Serial.println("Địa chỉ IP: ");
  Serial.print(WiFi.localIP());
}
void callback(char *topic, byte *message, unsigned int length)
{
  String msg;
  for (int i = 0; i < length; i++)
  {
    msg += (char)message[i]; // quét từng ký tự trong tin nhắn
  }
  Serial.print("Nhận tin nhắn từ topic [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(msg);

  if(String(msg)=="1on"){
    digitalWrite(2, HIGH);
  }
  else if(String(msg)=="1off"){
    digitalWrite(2, LOW);
  }
  else if(String(topic)=="rev/led"&&String(msg)=="2on"){
    LoRa.println("2on");
  }
  else if(String(topic)=="rev/led"&&String(msg)=="2off"){
    LoRa.println("2off");
  }
  else if(String(topic)=="rev/led"&&String(msg)=="3on"){
    LoRa.println("3on");
  }
  else if(String(topic)=="rev/led"&&String(msg)=="3off"){
    LoRa.println("3off");
  }
}
void reconnect()
{
  while (!client.connected())
  {
    Serial.println();
    Serial.print("Đang kết nối MQTT...");
    if (client.connect("ESP32Client", mqtt_username, mqtt_password))
    {
      Serial.println("thành công!");
    }
    else
    {
      Serial.print("lỗi, rc=");
      Serial.print(client.state());
      Serial.println(" thử lại sau 5s");
      delay(5000);
    }
    client.subscribe("rev/led");
  }
}

void setup() {
  Serial.begin(9600);  
  LoRa.begin(9600, SERIAL_8N1, 16, 17);
  WifiSetup();
  espClient.setInsecure();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  Serial.println("=== LoRa Receiver Ready ===");

  tft.begin();
  tft.setRotation(2);
  pinMode(2,OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
}
void readLoRa() {
  if (LoRa.available()) {
    String msg = LoRa.readStringUntil('\n');  
    msg.trim();

    Serial.println("📥 Dữ liệu JSON nhận: " + msg);

    // Tạo vùng nhớ cho JSON (200 byte là đủ cho payload nhỏ)
    StaticJsonDocument<200> doc;

    // Giải mã JSON
    DeserializationError error = deserializeJson(doc, msg);

    // Kiểm tra lỗi
    if (error) {
      Serial.print("❌ Lỗi parse JSON: ");
      Serial.println(error.f_str());
      return;
    }

    // Đọc các trường JSON
    nodeID      = doc["id"].as<String>();
    humidity    = doc["humidity"].as<float>();
    temperature = doc["temperature"].as<float>();
    CO_ppm      = doc["CO"].as<float>();
    fireState   = doc["fireState"].as<int>();

    // In ra kết quả
    Serial.println("=== 📡 Dữ liệu nhận được ===");
    Serial.println("📌 ID         : " + nodeID);
    Serial.println("🌡 Nhiệt độ   : " + String(temperature, 1) + " °C");
    Serial.println("💧 Độ ẩm      : " + String(humidity, 1) + " %");
    Serial.println("🛑 CO         : " + String(CO_ppm, 1) + " ppm");
    Serial.println("🔥 Trạng thái : " + String(fireState));
    Serial.println("=============================");
  }
}
void mucbaodong(){
  // diem dong gop
  fireRisk = 0;
  fireRisk += map(CO_ppm, 0, 50, 0, 40);// CO tu 0 -> 50ppm -> dong gop 0 ->40 diem (toi da 40 diem)
  fireRisk += map(temperature, 20, 50, 0, 30);// Nhiệt 20-50 băt dau dong gop
  fireRisk += map(100 - humidity, 0, 100, 0, 30); // Độ ẩm thấp điểm càng cao

  if (fireRisk > 100) fireRisk = 100;

  // Phân loại mức độ
  if (fireRisk >= 71) fireLevel = "Cao - Co chay rung";
  else if (fireRisk >= 51) fireLevel = "Trung binh";
  else if (fireRisk >= 31) fireLevel = "Thap";
  else fireLevel = "An toan";
  /*CO: 20 ppM (20*40/50)=16
  T: 31 C [(31-20)*(30-0)/(50-20)+0=11]
  F: 50% [(100-50)*30/100=15
  fireRisk= 16+11+15=42]*/
}
void chiso(){
  lichsufireRisk[xuhuongIndex] = ruirochay;
  xuhuongIndex = (xuhuongIndex + 1) % TREND_SIZE;  // cap nhap gia tri vao mang moi
  // Lay gia tri moi va cu de danh gia xu huong
  float first = lichsufireRisk[xuhuongIndex]; // gia tri cu
  float last = lichsufireRisk[(xuhuongIndex + TREND_SIZE - 1)% TREND_SIZE];
  // Xác định xu hướng và lưu vào biến toàn cục
  if (last - first > 5) fireTrend = "Tang";
  else if (last - first < -5) fireTrend = "Giam";
  else fireTrend = "On dinh";                    
}

void readTFT() {
  tft.fillScreen(ILI9341_BLACK);    
  tft.setTextColor(ILI9341_YELLOW);    
  tft.setTextSize(2);
  
  tft.drawRect(0, 0, 320, 240, ILI9341_RED);
  tft.drawRect(53, 5, 227, 29, ILI9341_BLUE);
  tft.drawRect(4, 52, 155, 184,  ILI9341_RED);
  tft.drawRect(163, 52, 152, 184,  ILI9341_RED);

  tft.setCursor(72, 12);
  tft.println("--DU lieu nhan--");
  tft.setCursor(6, 56);
  tft.println("ID:" + nodeID);
  tft.setCursor(6, 75);
  tft.println("Nhietdo:");
  tft.setCursor(6, 94);
  tft.println(String(temperature) + " C");
  tft.setCursor(6, 113);
  tft.println("Do am:");
  tft.setCursor(6, 132);
  tft.println(String(humidity) + " %");
  tft.setCursor(6, 153);
  tft.println("CO:");
  tft.setCursor(6, 172);
  if (CO_ppm >= 15.0){
  tft.setTextColor(ILI9341_RED);
  tft.println(String(CO_ppm) + " ppm");
  } else {
    tft.setTextColor(ILI9341_GREEN);
    tft.println(String(CO_ppm) + " ppm");
  }
  tft.setCursor(6, 193);
  if(fireLevel == "Cao - Co chay rung") tft.setTextColor(ILI9341_RED);
  else if(fireLevel == "Trung binh") tft.setTextColor(ILI9341_YELLOW);
  else if(fireLevel == "Thap") tft.setTextColor(ILI9341_ORANGE);
  else if(fireLevel == "An toan") tft.setTextColor(ILI9341_GREEN);
  tft.println(fireLevel);
  tft.setCursor(6,210 );
  tft.setTextColor(ILI9341_CYAN);
  tft.println(fireTrend);

  // Man 2
  tft.setTextColor(ILI9341_BLUE);    
  tft.setCursor(166, 56);
  tft.println("ID:" + nodeID);
  tft.setCursor(166, 75);
  tft.println("Nhietdo:");
  tft.setCursor(166, 94);
  tft.println(String(temperature) + " C");
  tft.setCursor(166, 113);
  tft.println("Do am:");
  tft.setCursor(166, 132);
  tft.println(String(humidity) + " %");
  tft.setCursor(166, 153);
  tft.println("CO:");
  tft.setCursor(166, 172);
  tft.println(String(CO_ppm) + " ppm");
}
void coiCanhBao(){
  for (int i=0; i<5; i++){
    digitalWrite(BUZZER_PIN,HIGH);
    delay(300);
    digitalWrite(BUZZER_PIN,LOW);
    delay(300);
  }
}
void publishData() {        
  String payload = "{";
  payload += "\"id\":\"" + nodeID + "\",";
  payload += "\"temperature\":" + String(temperature, 1) + ",";
  payload += "\"humidity\":" + String(humidity, 1) + ",";
  payload += "\"CO\":" + String(CO_ppm, 1) + ",";
  payload += "\"fireState\":" + String(fireState);
  payload += "}";

  client.publish("rev/data", payload.c_str());
  Serial.println("📤 MQTT gửi: " + payload);
}

void loop() {
    if (!client.connected())
  {
    reconnect();
  }
  readLoRa();
  publishData();
  readTFT();
  mucbaodong();
  chiso();
  if (fireState == 1||fireLevel == "Cao - Co chay rung") {
    coiCanhBao();
  }
  delay(1000);
  client.loop(); 
}