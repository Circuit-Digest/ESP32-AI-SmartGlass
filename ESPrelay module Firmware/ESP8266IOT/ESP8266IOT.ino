/*
 * Project Name: ESP8266 Realy board for home automation - Part of ESP32 AI Smart Glass
 * Project Brief: ESP8266 Realy board for home automation with auto addressing using mDNS.
 * Author: Jobit Joseph @ https://github.com/jobitjoseph
 * IDE: Arduino IDE 2.x.x
 * Arduino Core: ESP8266 Arduino Core V 3.1.2
 * Dependencies : Nill
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
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
const char* ssid = "circuitdigest";
const char* password = "circuitdigest123";
ESP8266WebServer server(80);  // Web server on port 80
bool shouldReconnect = false;  // Flag to indicate Wi-Fi disconnection
const int controlPin = 4;

void initWiFi() {
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    Serial.print(".");
    delay(500);
    yield();  // Prevent watchdog reset
  }
}

void onWifiEvent(WiFiEvent_t event) {
  switch (event) {
    case WIFI_EVENT_STAMODE_GOT_IP:
      digitalWrite(LED_BUILTIN, LOW);  // Turn off the LED when connected
      Serial.println("");
      Serial.println("Connected to Wi-Fi successfully.");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      shouldReconnect = false;  // Reset flag on successful connection
      break;

    case WIFI_EVENT_STAMODE_DISCONNECTED:
      digitalWrite(LED_BUILTIN, HIGH);
      Serial.println("Disconnected from Wi-Fi, will try to reconnect...");
      shouldReconnect = true;  // Set flag to reconnect in the loop
      break;

    default:
      break;
  }
}
void setup() {
  Serial.begin(115200);
  pinMode(controlPin, OUTPUT);
  digitalWrite(controlPin, LOW);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  // Register Wi-Fi event handler
  WiFi.onEvent(onWifiEvent); 

  initWiFi();
  if (MDNS.begin("esp8266-2")) {  // Set up mDNS responder with hostname "esp8266-1"
    Serial.println("MDNS responder started");
  }
  /*
  server.on("/toggle", []() {
    digitalWrite(controlPin, !digitalRead(controlPin));
    //digitalWrite(LED_BUILTIN, digitalRead(controlPin));
    server.send(200, "text/plain", "Pin state toggled");
  });

  server.on("/state", []() {
    String pinState = digitalRead(controlPin) == HIGH ? "HIGH" : "LOW";
    server.send(200, "text/plain", pinState);
  });
*/
// Add CORS headers for cross-origin requests
  server.on("/toggle", []() {
    digitalWrite(controlPin, !digitalRead(controlPin));
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", "Pin state toggled");
  });

  server.on("/state", []() {
    String pinState = digitalRead(controlPin) == HIGH ? "HIGH" : "LOW";
    server.sendHeader("Access-Control-Allow-Origin", "*");  // Add CORS header here
    server.send(200, "text/plain", pinState);
  });

  // Handle preflight requests (for complex CORS requests like POST)
  server.on("/state", HTTP_OPTIONS, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    server.send(200);
  });
  server.begin();
}

void loop() {
  server.handleClient();
  MDNS.update();
  // Handle Wi-Fi reconnection
  if (shouldReconnect && WiFi.status() != WL_CONNECTED) {
    Serial.println("Reconnecting to Wi-Fi...");
    initWiFi();  // Reattempt connection
  }
}
