#include "esp_camera.h"
#include <WiFi.h>
#include <WebSocketsClient.h>

// ===========================
// Select camera model in board_config.h
// ===========================
#include "board_config.h"

// ===========================
// Enter your WiFi credentials
// ===========================
const char *ssid = "OpenWrt";
const char *password = "";

WebSocketsClient webSocket;
const char* websocket_server = "192.168.1.21";
const uint16_t websocket_port = 8080;

void startCameraServer();
void setupLedFlash();

void decode(uint8_t * payload){
  uint8_t *data_right = payload + 7;
  uint8_t *data_left = payload + 7 + 64;
  
  float degrees_right[7];
  float degrees_left[7];
    
  for (int i = 0; i < 7; i++) {
    uint8_t integerPart = data_right[i * 2];
    uint8_t decimalPart = data_right[i * 2 + 1];
    degrees_right[i] = (integerPart + decimalPart / 10.0);
  }
  
  for (int i = 0; i < 7; i++) {
    uint8_t integerPart = data_left[i * 2];
    uint8_t decimalPart = data_left[i * 2 + 1];
    degrees_left[i] = (integerPart + decimalPart / 10.0);
  }
  
   for (int i = 0; i < 7; i++) {
    Serial.print(degrees_right[i], 1);
    Serial.print(" ");
  }
  for (int i = 0; i < 7; i++) {
    Serial.print(degrees_left[i], 1);
    if (i < 6) Serial.print(" ");
  }
  Serial.println();
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("[WS] Disconnected!");
      break;
    case WStype_CONNECTED:
      {
        Serial.println("[WS] Connected to server!");
        Serial.printf("[WS] Connected to URL: %s\n", payload);
        
        String connectMsg = "Camera connected from IP: " + WiFi.localIP().toString();
        webSocket.sendTXT(connectMsg);
      }
      break;
    case WStype_TEXT:
      Serial.printf("[WS] Text message: %.*s\n", length, payload);
      break;
    case WStype_BIN:
      Serial.printf("[WS] Binary message of length %u received\n", (unsigned)length);
      decode(payload);
      // Serial.print("HEX: ");
      // for (size_t i = 0; i < length; i++) {
      //   if (payload[i] < 16) Serial.print('0');
      //   Serial.print(payload[i], HEX);
      //   Serial.print(' ');
      // }
      Serial.println();
      break;
    case WStype_ERROR:
      Serial.printf("[WS] Error: %s\n", payload);
      break;
    case WStype_PING:
      Serial.println("[WS] Ping received");
      break;
    case WStype_PONG:
      Serial.println("[WS] Pong received");
      break;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);
    s->set_brightness(s, 1);
    s->set_saturation(s, -2);
  }
  
  if (config.pixel_format == PIXFORMAT_JPEG) {
    s->set_framesize(s, FRAMESIZE_QVGA);
  }

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

#if defined(CAMERA_MODEL_ESP32S3_EYE)
  s->set_vflip(s, 1);
#endif

#if defined(LED_GPIO_NUM)
  setupLedFlash();
#endif

  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  Serial.print("WiFi connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  // Настройка WebSocket с правильным протоколом
  webSocket.begin(websocket_server, websocket_port, "/", "uart-protocol");
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
  
  // Дополнительные настройки для лучшей совместимости
  webSocket.enableHeartbeat(15000, 3000, 2);

  startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
  Serial.print("WebSocket connecting to: ");
  Serial.print(websocket_server);
  Serial.print(":");
  Serial.println(websocket_port);
}

void loop() {
  webSocket.loop();
  
  // Периодическая отправка статуса
  static unsigned long lastStatus = 0;
  if (millis() - lastStatus > 10000) {
    if (webSocket.isConnected()) {
      String status = "Camera status: OK, Free heap: " + String(esp_get_free_heap_size());
      webSocket.sendTXT(status);
    }
    lastStatus = millis();
  }
  
  delay(10);
}