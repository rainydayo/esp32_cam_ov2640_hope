#include "esp_camera.h"
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#define CAMERA_MODEL_AI_THINKER // Has PSRAM
#include <ESPAsyncWebServer.h>
#include "camera_pins.h"
#include "mbedtls/base64.h"
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

const char *ssid = "Qwerty4826";
const char *password = "oooooooo";

#define API_KEY "AIzaSyCvloRko_s2B1XO_fLEsGHDKerDDr3d7R0"
#define DATABASE_URL "https://hopesmartbin-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define USER_EMAIL "john.demohope@gmail.com"
#define USER_PASSWORD "john_demo66"

//Define Firebase Data object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig Fconfig;

AsyncWebServer server(80);

String base64Image = "";  // To store the Base64 encoded image
bool imageCaptured = false;

#define FLASH_PIN 4

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
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QQVGA;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 2;

  if (psramFound()) {
    config.fb_count = 2;
  } else {
    config.fb_location = CAMERA_FB_IN_DRAM;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return;
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP Address: http://");
  Serial.println(WiFi.localIP());

  // Firebase configuration
  Fconfig.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  Fconfig.database_url = DATABASE_URL;

  Firebase.reconnectNetwork(true);
  fbdo.setBSSLBufferSize(4096, 1024);
  fbdo.setResponseSize(2048);
  Firebase.begin(&Fconfig, &auth);
  Firebase.setDoubleDigits(5);
  Fconfig.timeout.serverResponse = 10 * 1000;

  pinMode(FLASH_PIN, OUTPUT);
  digitalWrite(FLASH_PIN, LOW);

  server.begin();
}

unsigned long lastMillis;

void loop() {
  int ldrValue;
  if (!Firebase.RTDB.getInt(&fbdo, "/ldr/state", &ldrValue)){
    Serial.println(fbdo.errorReason().c_str());
  }

  // Turn on flash if light level is below threshold
  if (ldrValue == 1) {
    digitalWrite(FLASH_PIN, HIGH); // Turn on flash
  } else {
    digitalWrite(FLASH_PIN, LOW);  // Turn off flash
  }

  if (Serial.available() > 0) {
    char command = Serial.read();
    if (command == 'c') {
      Serial.println("Capture command received.");
      captureBase64Image();
    }
  }
  
  if (millis() - lastMillis >= 60*1000UL) 
  {
   lastMillis = millis();
   captureBase64Image();
  }
  delay(500);
}

void captureBase64Image() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    imageCaptured = false;
    return;
  }

  // Encode the image in Base64
  size_t outputLen;
  size_t inputLen = fb->len;
  unsigned char *output = (unsigned char *)malloc(inputLen * 2);  // Allocate memory for Base64 string

  int ret = mbedtls_base64_encode(output, inputLen * 2, &outputLen, fb->buf, inputLen);
  if (ret == 0) {
    base64Image = String((char *)output);
    imageCaptured = true;

    if (!Firebase.RTDB.setString(&fbdo, "/foodCurr/state", base64Image)) {
      Serial.println(fbdo.errorReason().c_str());
    } else {
      Serial.print("Write success");
    }

    Serial.println("----- Base64 Image Captured -----");
    Serial.println(base64Image.substring(0, 100));  // Print only the first 100 characters
    Serial.println("-------- End task --------");
  } else {
    Serial.println("Base64 encoding failed");
    imageCaptured = false;
  }

  free(output);  // Free allocated memory
  esp_camera_fb_return(fb);  // Return the frame buffer back to be reused
}