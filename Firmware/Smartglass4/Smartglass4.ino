/*
 * Project Name: ESP32 AI Smart Glass
 * Project Brief: ESP32-S3 based smart glass equiped with camera, I2S amp and Memes Microphone. Capable of home automation with QRCode.
 * Author: Jobit Joseph @ https://github.com/jobitjoseph
 * IDE: Arduino IDE 2.x.x
 * Arduino Core: ESP32 Arduino Core V 3.0.5
 * Dependencies : ESP32-audioI2S Library V 3.0.12 @ https://github.com/schreibfaul1/ESP32-audioI2S
 *                Adafruit Neopixel Library V 1.12.3 @ https://github.com/adafruit/Adafruit_NeoPixel
 * Copyright © Jobit Joseph
 * Copyright © Semicon Media Pvt Ltd
 * Copyright © Circuitdigest.com
 * 
 * This code is licensed under the following conditions:
 *
 * 1. Non-Commercial Use:
 * This program is free software: you can redistribute it and/or modify it
 * for personal or educational purposes under the condition that credit is given 
 * to the original author. Attribution is required, and the original author 
 * must be credited in any derivative works or distributions.
 *
 * 2. Commercial Use:
 * For any commercial use of this software, you must obtain a separate license
 * from the original author. Contact the author for permissions or licensing
 * options before using this software for commercial purposes.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE, AND NONINFRINGEMENT. IN NO EVENT SHALL 
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES, OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT, OR OTHERWISE, ARISING 
 * FROM, OUT OF, OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 *
 * Author: Jobit Joseph
 * Date: 18 October 2024
 *
 * For commercial use or licensing requests, please contact [jobitjoseph1@gmail.com].
 */

#include "esp_camera.h"
#include <WiFi.h>
#include <Adafruit_NeoPixel.h>
#include "Audio.h"
#include "FS.h"
#include <LittleFS.h>
#include <HTTPClient.h>
#define I2S_DOUT 9
#define I2S_BCLK 8
#define I2S_LRC 7
#define RGBPIN 6
#define NUMPIXELS 2
#define FORMAT_LITTLEFS_IF_FAILED true
Adafruit_NeoPixel pixels(NUMPIXELS, RGBPIN, NEO_GRB + NEO_KHZ800);
Audio audio;
WiFiClientSecure client;           // Secure client for HTTPS communication
String receivedText = "";          // Buffer to hold the received serial text
#define CAMERA_MODEL_XIAO_ESP32S3  // Has PSRAM
#include "camera_pins.h"

// ===========================
// Enter your WiFi credentials
// ===========================

const char* ssid = "circuitdigest";
const char* password = "circuitdigest123";

String serverName = "www.circuitdigest.cloud";  // Server domain
String serverPath = "/readqrcode";              // API endpoint path
const int serverPort = 443;                     // HTTPS port
String apiKey = "0Sv8M7UgxMtB";                 // Replace "xxx" with your API key

bool shouldReconnect = false;  // Flag to indicate Wi-Fi disconnection
unsigned long LastConFailedTime = 0;
int threshold = 1000;
bool touchdetected = false;
bool touchreleased = false;
bool EQ1 = false;
bool EQ2 = false;
void startCameraServer();
void setupLedFlash(int pin);
void TouchITR() {
  touchdetected = (touchdetected == false) ? true : false;
  touchreleased = (touchdetected == true) ? touchreleased : true;
}
void initWiFi() {
  Serial.print("Connecting to ");
  Serial.println(ssid);
  //audio.connecttoFS(LittleFS, "/Connecting.mp3");
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - startAttemptTime > 15000) {  // Timeout after 10 seconds
      Serial.println("Failed to connect to WiFi");
      //audio.connecttoFS(LittleFS, "/ConnectionFailed.mp3");
      LastConFailedTime = millis();
      return;  // Exit if unable to connect
    }
    delay(250);
    Serial.print(".");
    pixels.setPixelColor(0, pixels.Color(0, 0, 255));
    pixels.show();
    delay(250);
    pixels.clear();
    pixels.show();
    touchreleased = false;
  }
}
void onWifiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.println("");
      Serial.println("Connected to Wi-Fi successfully.");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      //audio.connecttoFS(LittleFS, "/WiFiActive.mp3");
      shouldReconnect = false;  // Reset flag on successful connection
      break;

    case WIFI_EVENT_STA_DISCONNECTED:
      Serial.println("Disconnected from Wi-Fi, will try to reconnect...");
      shouldReconnect = true;  // Set flag to reconnect in the loop
      break;

    default:
      break;
  }
}
void initFS() {
  if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
    Serial.println("LittleFS Mount Failed");
    return;
  }
}

void togglePinOnESP8266(const char* esp8266_hostname) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String("http://") + esp8266_hostname + ".local/toggle";
    http.begin(url);
    int httpCode = http.GET();
    http.end();
  }
}

int getPinStateFromESP8266(const char* esp8266_hostname) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String("http://") + esp8266_hostname + ".local/state";
    http.begin(url);
    int httpCode = http.GET();
    String payload = http.getString();  // Get the response

    http.end();

    // Convert response to 0 or 1 based on the state
    if (payload == "HIGH") {
      return 1;
    } else if (payload == "LOW") {
      return 0;
    }
  }
  return -1;  // Return -1 if there is an error
}
/*
// Function to extract a JSON array value by key
String extractJsonArrayValue(const String& jsonString, const String& key) {
  int keyIndex = jsonString.indexOf(key);
  if (keyIndex == -1) {
    return "";
  }

  int startIndex = jsonString.indexOf('[', keyIndex);
  int endIndex = jsonString.indexOf(']', startIndex);

  if (startIndex == -1 || endIndex == -1) {
    return "";
  }

  return jsonString.substring(startIndex, endIndex + 1);
}


// Function to extract a JSON array value by key
String extractJsonArrayValue(const String& jsonString, const String& key) {
  int keyIndex = jsonString.indexOf(key);
  if (keyIndex == -1) {
    return "";
  }

  int startIndex = jsonString.indexOf('[', keyIndex);
  int endIndex = jsonString.indexOf(']', startIndex);

  if (startIndex == -1 || endIndex == -1) {
    return "";
  }

  // Extract the string inside the array, remove the square brackets and quotes
  String extractedArray = jsonString.substring(startIndex + 1, endIndex);
  extractedArray.replace("\"", ""); // Remove the double quotes

  return extractedArray;
}
*/
String extractJsonArrayValue(const String& jsonString, const String& key) {
    int keyIndex = jsonString.indexOf(key);
    if (keyIndex == -1) {
        return "";
    }

    int startIndex = jsonString.indexOf('[', keyIndex);
    int endIndex = jsonString.indexOf(']', startIndex);

    if (startIndex == -1 || endIndex == -1) {
        return "";
    }

    // Extract the string inside the array, remove the square brackets and quotes
    String extractedArray = jsonString.substring(startIndex + 1, endIndex);
    extractedArray.replace("\"", ""); // Remove the double quotes

    // Split the extractedArray by commas and return the first element
    int commaIndex = extractedArray.indexOf(',');
    if (commaIndex != -1) {
        return extractedArray.substring(0, commaIndex); // Return the first value before the comma
    }

    // If there is no comma, return the extractedArray (it contains only one element)
    return extractedArray;
}

// Function to extract a JSON string value by key
String extractJsonStringValue(const String& jsonString, const String& key) {
  int keyIndex = jsonString.indexOf(key);
  if (keyIndex == -1) {
    return "";
  }

  int startIndex = jsonString.indexOf(':', keyIndex) + 2;
  int endIndex = jsonString.indexOf('"', startIndex);

  if (startIndex == -1 || endIndex == -1) {
    return "";
  }

  return jsonString.substring(startIndex, endIndex);
}

// Function to capture and send photo to the server
int sendPhoto() {
  camera_fb_t* fb = NULL;
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    touchreleased = false;
    pixels.setPixelColor(0, pixels.Color(255, 0, 0));
    pixels.show();
    delay(250);
    pixels.clear();
    pixels.show();
    return -1;
  }

  // Display success message
  Serial.println("Image Capture Success");
  // Connect to server
  Serial.println("Connecting to server:" + serverName);
  client.setInsecure();  // Skip certificate validation for simplicity

  pixels.setPixelColor(0, pixels.Color(0, 255, 0));
  pixels.show();

  if (client.connect(serverName.c_str(), serverPort)) {
    Serial.println("Connection successful!");
    String filename = apiKey + ".jpeg";

    // Prepare HTTP POST request
    String head = "--CircuitDigest\r\nContent-Disposition: form-data; name=\"imageFile\"; filename=\"" + filename + "\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--CircuitDigest--\r\n";
    uint32_t imageLen = fb->len;
    uint32_t extraLen = head.length() + tail.length();
    uint32_t totalLen = imageLen + extraLen;

    client.println("POST " + serverPath + " HTTP/1.1");
    client.println("Host: " + serverName);
    client.println("Content-Length: " + String(totalLen));
    client.println("Content-Type: multipart/form-data; boundary=CircuitDigest");
    client.println("Authorization:" + apiKey);
    client.println();
    client.print(head);

    // Send image data in chunks
    uint8_t* fbBuf = fb->buf;
    size_t fbLen = fb->len;
    for (size_t n = 0; n < fbLen; n += 1024) {
      if (n + 1024 < fbLen) {
        client.write(fbBuf, 1024);
        fbBuf += 1024;
      } else {
        size_t remainder = fbLen % 1024;
        client.write(fbBuf, remainder);
      }
    }

    client.print(tail);

    // Clean up
    esp_camera_fb_return(fb);

    // Wait for server response
    String response;
    long startTime = millis();
    while (client.connected() && millis() - startTime < 10000) {
      if (client.available()) {
        char c = client.read();
        response += c;
      }
    }
    Serial.println("Raw JSON Data: " + response);
    // Extract and display QR code data from response
    String qrCodeData = extractJsonArrayValue(response, "\"QR_code\"");
    String imageLink = extractJsonStringValue(response, "\"view_image\"");

    Serial.print("QR Data: ");
    Serial.println(qrCodeData);
    if (qrCodeData == "light") {
      EQ1 = true;
    } else if (qrCodeData == "fan") {
      EQ2 = true;
    } else {
      audio.connecttoFS(LittleFS, "/InvalidQR.mp3");
    }

    Serial.print("ImageLink: ");
    Serial.println(imageLink);

    // Serial.print("Response Json String: ");
    // Serial.println(response);


    //displayText("QR Code Data:\n\n" + qrCodeData);

    client.stop();
    touchreleased = false;
    pixels.clear();
    pixels.show();
    return 0;
  } else {
    Serial.println("Connection to server failed");
    touchreleased = false;
    pixels.setPixelColor(0, pixels.Color(255, 0, 0));
    pixels.show();
    delay(200);
    pixels.clear();
    pixels.show();
    pixels.setPixelColor(0, pixels.Color(255, 0, 0));
    pixels.show();
    delay(200);
    pixels.clear();
    pixels.show();
    return -2;
  }
}
void EQControl() {
  int pinstate;
  if (EQ1) {
    EQ1 = false;
    togglePinOnESP8266("esp8266-1");
    pinstate = getPinStateFromESP8266("esp8266-1");
    if (pinstate == 0) {
      audio.connecttoFS(LittleFS, "/LightOff.mp3");
    } else if (pinstate == 1) {
      audio.connecttoFS(LittleFS, "/LightOn.mp3");
    }
  } else if (EQ2) {
    EQ2 = false;
    togglePinOnESP8266("esp8266-2");
    pinstate = getPinStateFromESP8266("esp8266-2");
    if (pinstate == 0) {
      audio.connecttoFS(LittleFS, "/FanOff.mp3");
    } else if (pinstate == 1) {
      audio.connecttoFS(LittleFS, "/FanOn.mp3");
    }
  }
  touchreleased = false;
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
  config.pixel_format = PIXFORMAT_JPEG;  // for streaming
  //config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  //config.fb_location = CAMERA_FB_IN_DRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    // Best option for face detection/recognition
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 1;
#endif
  }



  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t* s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);        // flip it back
    s->set_brightness(s, 1);   // up the brightness just a bit
    s->set_saturation(s, -2);  // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  if (config.pixel_format == PIXFORMAT_JPEG) {
    s->set_framesize(s, FRAMESIZE_QVGA);
  }
/*
#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif
*/

// Setup LED FLash if LED pin is defined in camera_pins.h
#if defined(LED_GPIO_NUM)
  setupLedFlash(LED_GPIO_NUM);
#endif

  initFS();
  pixels.begin();
  pixels.show();
  pixels.setBrightness(64);
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(21);
  audio.connecttoFS(LittleFS, "/WelcomeTone.mp3");
  // Register Wi-Fi event handler
  WiFi.onEvent(onWifiEvent);
  //audio.connecttoFS(LittleFS, "/Connecting.mp3");
  initWiFi();
  startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");

  touchAttachInterrupt(T1, TouchITR, threshold);
}

void loop() {
  audio.loop();  // Required to keep audio running
  if (WiFi.status() != WL_CONNECTED) {
    if (shouldReconnect || (millis() - LastConFailedTime > 10000)) {
      Serial.println("Reconnecting to Wi-Fi...");
      //audio.connecttoFS(LittleFS, "/Reconnecting.mp3");
      initWiFi();  // Reattempt connection
    }
    if (touchreleased) {
      touchreleased = false;
      audio.connecttoFS(LittleFS, "/NotAvailable.mp3");
    }
  } else {
    SerialControl();
    if (touchreleased) {
      touchreleased = false;
      //audio.connecttospeech("Touch detected", "en");
      int status = sendPhoto();
      EQControl();
      if (status == -1) {
        audio.connecttoFS(LittleFS, "/ICFailed.mp3");
      } else if (status == -2) {
        audio.connecttoFS(LittleFS, "/SeverFailed.mp3");
      }
    }
  }
}

void SerialControl() {
  int pinstate;
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');  // Read serial input until newline character
    command.trim();                                 // Remove any leading/trailing whitespace

    if (isValidCommand(command)) {
      if (command == "esp8266-1") {
        togglePinOnESP8266("esp8266-1");
        pinstate = getPinStateFromESP8266("esp8266-1");
        if (pinstate == 0) {
          audio.connecttoFS(LittleFS, "/LightOff.mp3");
        } else if (pinstate == 1) {
          audio.connecttoFS(LittleFS, "/LightOn.mp3");
        }
      } else {
        togglePinOnESP8266("esp8266-2");
        pinstate = getPinStateFromESP8266("esp8266-2");
        if (pinstate == 0) {
          audio.connecttoFS(LittleFS, "/FanOff.mp3");
        } else if (pinstate == 1) {
          audio.connecttoFS(LittleFS, "/FanOn.mp3");
        }
      }
    } else {
      Serial.println("Invalid command.");
    }
  }
}
// Function to validate if the command is a valid or not
bool isValidCommand(String cmd) {
  return cmd == "esp8266-1" || cmd == "esp8266-2";
}
/*
void SerialPlay() {
  // Check if there's data available on the serial port
  while (Serial.available() > 0) {
    char receivedChar = Serial.read();
    // Append each character to the receivedText string
    if (receivedChar == '\n') {
      // When a new line is detected, play the received text
      receivedText.trim();  // Remove any leading/trailing whitespace
      if (receivedText.length() > 0) {
        Serial.print("Playing: ");
        Serial.println(receivedText);
        audio.connecttospeech(receivedText.c_str(), "en");  // Play received text
      }
      receivedText = "";  // Clear the buffer for the next input
    } else {
      receivedText += receivedChar;  // Add character to the buffer
    }
  }
}

void audio_info(const char* info) {
  Serial.print("audio_info: ");
  Serial.println(info);
}
*/