#include <Arduino.h>

/* WiFi and HTTP Rest API*/
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>

/* IR Send and Receive */
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRrecv.h>
#include <IRutils.h>

// ==================== start of TUNEABLE PARAMETERS ====================

// Create a file called secrets.h and override the settings for you wifi
#include "secrets.h"
#ifndef SSID
#define SSID "wifiName"
#endif
#ifndef PASSWORD
#define PASSWORD "wifiPassword"
#endif

// ========================== IR RemoteESP8266 ==========================

// The GPIO an IR detector/demodulator is connected to. Recommended: 14 (D5)
// Note: GPIO 16 won't work on the ESP8266 as it does not have interrupts.
const uint16_t kRecvPin = 14;

// GPIO to use to control the IR LED circuit. Recommended: 4 (D2).
const uint16_t kIrLedPin = 4;

// The Serial connection baud rate.
// NOTE: Make sure you set your Serial Monitor to the same speed.
const uint32_t kBaudRate = 115200;

// As this program is a special purpose capture/resender, let's use a larger
// than expected buffer so we can handle very large IR messages.
// i.e. Up to 512 bits.
const uint16_t kCaptureBufferSize = 1024;

// kTimeout is the Nr. of milli-Seconds of no-more-data before we consider a
// message ended.
const uint8_t kTimeout = 50;  // Milli-Seconds

// ==================== end of TUNEABLE PARAMETERS ====================

// The REST API Server
ESP8266WebServer http_rest_server(80); //port

// The IR transmitter.
IRsend irsend(kIrLedPin);
// The IR receiver.
IRrecv irrecv(kRecvPin, kCaptureBufferSize, kTimeout, false);
// Somewhere to store the captured message.
decode_results results;

void decodeMultibracketsRestApi()
{
  uint32_t now = millis();
  Serial.printf("%06u.%03u: Received HTTP POST: ", now / 1000, now % 1000);

  // fetch the raw HTTP Post
  String post_body = http_rest_server.arg("plain");
  Serial.println(post_body);

  if (post_body.equals("ok"))
  {
    Serial.print("IR transmission command: ");
    Serial.println(0xd4);
    irsend.sendMultibrackets(0xd4, 8, 1);
  } else if (post_body.equals("right"))
  {
    Serial.print("IR transmission command: ");
    Serial.println(0xfa);
    irsend.sendMultibrackets(0xfa, 8, 1);
  } else if (post_body.equals("left"))
  {
    Serial.print("IR transmission command: ");
    Serial.println(0xf5);
    irsend.send(decode_type_t::MULTIBRACKETS, 0xf5, 8, 1);
  }

  http_rest_server.send(200);
}


void setup() {
  Serial.begin(kBaudRate, SERIAL_8N1);
  while (!Serial)  // Wait for the serial connection to be establised.
    delay(50);

  Serial.println();
  Serial.println("Connecting to WiFi");

  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
  int retries = 0;
  while ((WiFi.status() != WL_CONNECTED) && (retries < 10)) {
    retries++;
    delay(500);
    Serial.print("#");
  }

  // check the status of WiFi connection
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print("Connected to ");
    Serial.print(SSID);
    Serial.print(", assigned IP: ");
    Serial.println(WiFi.localIP());
  }
  else {
    Serial.println();
    Serial.print("Error connecting to: ");
    Serial.println(SSID);
  }

  // configure REST API
  http_rest_server.on("/", HTTP_GET, []() {
      http_rest_server.send(200, "text/html",
          "Welcome to the ESP8266 REST Web Server");
  }); // on a get request, configure the server call an unnamed function that responds with Ok/200 and a simple web-page
  http_rest_server.on("/multibrackets", HTTP_POST, decodeMultibracketsRestApi); // on a POST to the Mutlibrackets API call decode function
  
  // Startup all services
  Serial.println("Starting services:");

  // Start the HTTP Web server
  http_rest_server.begin();
  Serial.println("  HTTP Web Server (REST Api)");
  

  // Start the IR receiver
  irrecv.enableIRIn();
  Serial.print("  IR Receiver, pin: ");
  Serial.println(kRecvPin);

  // Start the IR sender.
  irsend.begin();
  Serial.print("  IR Sender, pin: ");
  Serial.println(kIrLedPin);

  Serial.println("Services started!");
}

void loop() {

  http_rest_server.handleClient();

  if(irrecv.decode(&results)) {
    uint32_t now = millis();

    // Retransmit
    bool success = irsend.send(results.decode_type, results.value, results.bits, 1);

    // Receive the next value
    irrecv.resume();

    Serial.printf(
        "%06u.%03u: A %d-bit %s message was %ssuccessfully retransmitted.\n"
        "            Encoded value: 0x%02llx\n",
        now / 1000, now % 1000, results.bits, typeToString(results.decode_type).c_str(),
        success ? "" : "un",
        results.value);
  }

  yield();

}