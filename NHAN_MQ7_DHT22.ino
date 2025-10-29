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
#define M0_PIN 25      
#define M1_PIN 26

const char *ssid = "Mesh_1023";// tên wifi (p505,11112222)(Nokia1280,1122334455667788)(Phong Dien Tu Vien Thong,dtvtthuyloi)(K1-T3,TLU@2024)
const char *password = "thichchepphat";

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
// BIỂU ĐỒ 
#define GRAPH_X 162
#define GRAPH_Y 50
#define GRAPH_W 153
#define GRAPH_H 166

#define MAX_POINTS (GRAPH_W - 10)

float dataTemp[MAX_POINTS] = {0};
float dataHumi[MAX_POINTS] = {0};
float dataCO[MAX_POINTS]   = {0};
int pointIndex = 0;

// --- Vẽ trục Y (0-100) và các đường mức ---
void drawGraphGrid() {
  int baseY = GRAPH_Y + GRAPH_H - 1;
  int topY = GRAPH_Y + 1;

  // Dịch nhẹ để số 0–100 không bị cắt khung
  int offsetBottom = 6;  // nâng 0 lên chút
  int offsetTop = 6;     // hạ 100 xuống chút

  tft.setTextSize(1);
  tft.setTextColor(ILI9341_WHITE);

  // Hiển thị các mốc 0,20,40,60,80,100
  for (int val = 0; val <= 100; val += 20) {
    int y = map(val, 0, 100, baseY - offsetBottom, topY + offsetTop);

    // Vẽ đường ngang (không đè lên chữ)
    if (val != 100) { // bỏ qua 100 để vẽ riêng
      if (y > GRAPH_Y + 10 && y < GRAPH_Y + GRAPH_H - 10)
        tft.drawFastHLine(GRAPH_X + 25, y, GRAPH_W - 27, ILI9341_DARKGREY);
    }

    // In giá trị trục Y
    int textY = y - 4;
    if (val == 0) textY -= 3;
    if (val == 100) textY += 2;
    tft.setCursor(GRAPH_X + 5, textY);
    tft.print(val);
  }

  // Vẽ thêm 1 đường ngang trắng tại 100, không đè chữ
  int y100 = map(100, 0, 100, baseY - offsetBottom, topY + offsetTop);
  tft.drawFastHLine(GRAPH_X + 25, y100 + 3, GRAPH_W - 27, ILI9341_WHITE); // +3 để không đè chữ

  // Trục X (sát đáy khung)
  tft.drawFastHLine(GRAPH_X + 25, baseY - offsetBottom, GRAPH_W - 27, ILI9341_WHITE);
}

// --- Cập nhật biểu đồ ---
void updateGraph(float temp, float humi, float co) {
  // Giới hạn dữ liệu
  temp = constrain(temp, 0, 100);
  humi = constrain(humi, 0, 100);
  co   = constrain(co,   0, 100);

  // Lưu dữ liệu vào mảng
  if (pointIndex < MAX_POINTS) {
    dataTemp[pointIndex] = temp;
    dataHumi[pointIndex] = humi;
    dataCO[pointIndex]   = co;
    pointIndex++;
  } else {
    for (int i = 0; i < MAX_POINTS - 1; i++) {
      dataTemp[i] = dataTemp[i + 1];
      dataHumi[i] = dataHumi[i + 1];
      dataCO[i]   = dataCO[i + 1];
    }
    dataTemp[MAX_POINTS - 1] = temp;
    dataHumi[MAX_POINTS - 1] = humi;
    dataCO[MAX_POINTS - 1]   = co;
  }

  // Xóa vùng trong khung (không đụng viền đỏ)
  tft.fillRect(GRAPH_X + 1, GRAPH_Y + 1, GRAPH_W - 2, GRAPH_H - 2, ILI9341_BLACK);

  // Vẽ lại trục & lưới
  drawGraphGrid();

  // Toạ độ cơ bản
  int baseY = GRAPH_Y + GRAPH_H - 7;  // tránh đè trục X
  float scaleY = (float)(GRAPH_H - 20) / 100.0; // tỷ lệ cho 0–100
  int startX = GRAPH_X + 25;  // bắt đầu sau chữ trục Y

  // ⚡ Nếu mới có 1 điểm dữ liệu → vẽ từ (0,0)
  if (pointIndex == 1) {
    int x1 = startX;
    int x2 = startX + 1; // dịch 1px
    int y0 = baseY - (int)(0 * scaleY);  // (0,0)
    int y_t = baseY - (int)(dataTemp[0] * scaleY);
    int y_h = baseY - (int)(dataHumi[0] * scaleY);
    int y_c = baseY - (int)(dataCO[0] * scaleY);

    tft.drawLine(x1, y0, x2, y_t, ILI9341_BLUE);   // nhiệt độ
    tft.drawLine(x1, y0, x2, y_h, ILI9341_RED);    // độ ẩm
    tft.drawLine(x1, y0, x2, y_c, ILI9341_GREEN);  // CO
    return;
  }

  // ⚡ Nếu có nhiều điểm hơn → vẽ liên tục
  for (int i = 1; i < pointIndex; i++) {
    int x1 = startX + i - 1;
    int x2 = startX + i;
    if (x2 >= GRAPH_X + GRAPH_W - 2) break;

    int y1_t = baseY - (int)(dataTemp[i - 1] * scaleY);
    int y2_t = baseY - (int)(dataTemp[i] * scaleY);
    tft.drawLine(x1, y1_t, x2, y2_t, ILI9341_BLUE);

    int y1_h = baseY - (int)(dataHumi[i - 1] * scaleY);
    int y2_h = baseY - (int)(dataHumi[i] * scaleY);
    tft.drawLine(x1, y1_h, x2, y2_h, ILI9341_RED);

    int y1_c = baseY - (int)(dataCO[i - 1] * scaleY);
    int y2_c = baseY - (int)(dataCO[i] * scaleY);
    tft.drawLine(x1, y1_c, x2, y2_c, ILI9341_GREEN);
  }
}
void configLoRa(){
  pinMode(M0_PIN, OUTPUT);
  pinMode(M1_PIN, OUTPUT);
  digitalWrite(M0_PIN, HIGH);
  digitalWrite(M1_PIN, HIGH);
  delay(100);
  Serial.println("⚙️ Đang cấu hình LoRa E32...");

  byte configData[6] = {
    0xC0, // Ghi vĩnh viễn
    0x00, // ADDH
    0x00, // ADDL
    0x1A, // SPED: UART 9600, Air rate 0.3kbps
    0x10, // CHAN = 433 + 0x17 = 446 MHz
    0x07  // OPTION: 20dBm, Transparent mode
  };

  LoRa.write(configData, 6);
  delay(100);
  Serial.println("✅ Đã cấu hình xong LoRa.");

  // Chuyển về chế độ hoạt động
  digitalWrite(M0_PIN, LOW);
  digitalWrite(M1_PIN, LOW);
  delay(100);
}

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
      client.subscribe("rev/led");
    }
    else
    {
      Serial.print("lỗi, rc=");
      Serial.print(client.state());
      Serial.println(" thử lại sau 5s");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(9600);  
  pinMode(M0_PIN, OUTPUT);
  pinMode(M1_PIN, OUTPUT);
  digitalWrite(M0_PIN, LOW);
  digitalWrite(M1_PIN, LOW);
  delay(50);
  LoRa.begin(9600, SERIAL_8N1, 16, 17);
  WifiSetup();
  espClient.setInsecure();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  Serial.println("=== LoRa Receiver Ready ===");

  tft.begin();
  tft.setRotation(2);
  drawGraphGrid();//
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
  tft.drawRect(3, 50, 155, 144,  ILI9341_RED);
  //tft.drawRect(163, 50, 152, 144,  ILI9341_RED);
  tft.drawRect(162, 50, 153, 166,  ILI9341_RED);

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
  tft.setCursor(6, 221);
  if(fireLevel == "Cao - Co chay rung") tft.setTextColor(ILI9341_RED);
  else if(fireLevel == "Trung binh") tft.setTextColor(ILI9341_YELLOW);
  else if(fireLevel == "Thap") tft.setTextColor(ILI9341_ORANGE);
  else if(fireLevel == "An toan") tft.setTextColor(ILI9341_GREEN);
  tft.println(fireLevel);
  tft.setCursor(6,193 );
  tft.setTextColor(ILI9341_CYAN);
  tft.println(fireTrend);

  // Man 2
  /*tft.setTextColor(ILI9341_BLUE);    
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
  tft.println(String(CO_ppm) + " ppm");*/
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
  updateGraph(temperature, humidity, CO_ppm);
  if (fireState == 1||fireLevel == "Cao - Co chay rung") {
    coiCanhBao();
  }
  //updateGraph(temperature, humidity, CO_ppm);
  delay(2000);
  client.loop(); 
}