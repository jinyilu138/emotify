#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_camera.h>
#include <WebServer.h>

// WiFi Configuration
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// Camera Configuration (AI Thinker ESP32-CAM)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

WebServer server(80);

void setupCamera() {
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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 10;
  config.fb_count = 1;
  

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    ESP.restart();
  }
}

void handleJPG() {
  camera_fb_t *fb = NULL;
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    server.send(500, "text/plain", "Camera capture failed");
    return;
  }

  server.setContentLength(fb->len);
  server.send(200, "image/jpeg", "");
  
  WiFiClient client = server.client();
  size_t sent = 0;
  size_t total = fb->len;
  
  // Send the image in smaller chunks to avoid buffer overflows
  uint8_t *fbBuf = fb->buf;
  size_t chunkSize = 1024; // Send in 1KB chunks
  
  while (sent < total) {
    size_t willSend = (total - sent > chunkSize) ? chunkSize : (total - sent);
    size_t actualSent = client.write(&fbBuf[sent], willSend);
    
    if (actualSent == 0) {
      Serial.println("Client disconnected during transfer");
      break;
    }
    
    sent += actualSent;
    yield(); // Allow the ESP to handle system tasks
  }
  
  esp_camera_fb_return(fb);
}

void handleStream() {
  String html = "<html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<style>body{margin:0;padding:0;background:#000;}</style>";
  html += "<script>function refresh() { document.getElementById('stream').src = '/jpg?t=' + Date.now(); setTimeout(refresh, 500); }</script>";
  html += "</head><body onload='refresh()'>";
  html += "<img id='stream' '/jpg?t=' + Date.now() style='display:block;width:100%;height:auto;'>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);
  
  // 1. WiFi Optimization
  WiFi.disconnect(true);
  delay(1000);
  esp_wifi_restore();
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  WiFi.setSleep(false);

  // 2. Connect to WiFi
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 20000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nFailed to connect!");
    ESP.restart();
  }

  Serial.printf("\nConnected! IP: %s\n", WiFi.localIP().toString().c_str());

  // 3. Initialize Camera
  setupCamera();

  // 4. Start Web Server
  server.on("/", HTTP_GET, handleStream);
  server.on("/jpg", HTTP_GET, handleJPG);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
  
  // Maintain connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi lost - restarting...");
    ESP.restart();
  }
  delay(10);
}