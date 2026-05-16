#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>

#define DOOR_SWITCH_PIN 15

const char* host = "REPLACE_ME";
const char* ssid = "REPLACE_ME";
const char* password = "REPLACE_ME";
const char* slackWebhook = "REPLACE_ME";
const char* otaUser = "REPLACE_ME";
const char* otaPassword = "REPLACE_ME";

int doorSwitch = HIGH;
int lastReading = HIGH;
int doorState = 0;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 100;
bool authenticated = false;

WebServer server(80);

/*
 * Login page
 */

const char* loginIndex =
  "<form method='POST' action='/login'>"
  "<table width='20%' bgcolor='A09F9F' align='center'>"
  "<tr>"
  "<td colspan=2>"
  "<center><font size=4><b>ESP32 Login Page</b></font></center>"
  "<br>"
  "</td>"
  "</tr>"
  "<tr>"
  "<td>Username:</td>"
  "<td><input type='text' size=25 name='userid'><br></td>"
  "</tr>"
  "<tr>"
  "<td>Password:</td>"
  "<td><input type='Password' size=25 name='pwd'><br></td>"
  "</tr>"
  "<tr>"
  "<td><input type='submit' value='Login'></td>"
  "</tr>"
  "</table>"
  "</form>";

/*
 * Server Index Page
 */

const char* serverIndex =
  "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
  "<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
  "<input type='file' name='update'>"
  "<input type='submit' value='Update'>"
  "</form>"
  "<div id='prg'>progress: 0%</div>"
  "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')"
  "},"
  "error: function (a, b, c) {"
  "}"
  "});"
  "});"
  "</script>";

/*
 * setup function
 */
void setup(void) {
  Serial.begin(115200);

  // Connect to WiFi network
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  /*use mdns for host name resolution*/
  if (!MDNS.begin(host)) {  //http://esp32.local
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");

  /*return index page which is stored in loginIndex */
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", loginIndex);
  });
  /* server-side authentication */
  server.on("/login", HTTP_POST, []() {
    if (server.arg("userid") == otaUser && server.arg("pwd") == otaPassword) {
      authenticated = true;
      server.sendHeader("Connection", "close");
      server.send(200, "text/html", serverIndex);
    } else {
      server.send(401, "text/plain", "Authentication failed");
    }
  });
  server.on("/serverIndex", HTTP_GET, []() {
    if (!authenticated) {
      server.send(401, "text/plain", "Not authenticated");
      return;
    }
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });
  /*handling uploading firmware file */
  server.on(
    "/update", HTTP_POST, []() {
      if (!authenticated) {
        server.send(401, "text/plain", "Not authenticated");
        return;
      }
      server.sendHeader("Connection", "close");
      server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
      ESP.restart();
    },
    []() {
      if (!authenticated) {
        return;
      }
      HTTPUpload& upload = server.upload();
      if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("Update: %s\n", upload.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
          Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
        } else {
          Update.printError(Serial);
        }
      }
    });
  server.begin();
  Serial.println("Server started");

  // initialize pins
  pinMode(DOOR_SWITCH_PIN, INPUT_PULLUP);
  Serial.println("Pins initialized");

  sendSlackMessage("The front door circuit just restarted!");
}

void sendSlackMessage(String message) {
  WiFiClientSecure client;
  client.setInsecure();
  
  HTTPClient http;

  // Prepare message
  DynamicJsonDocument doc(2048);
  doc["text"] = message;

  // Serialize Message
  String json;
  serializeJson(doc, json);

  // Send request
  http.begin(client, slackWebhook);
  http.addHeader("Content-Type", "application/json");
  http.POST(json);

  // Print the response
  Serial.println(http.getString());

  // Disconnect
  http.end();
}

void loop(void) {
  server.handleClient();

  // Reconnect WiFi if disconnected
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, reconnecting...");
    WiFi.reconnect();
    unsigned long reconnectStart = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - reconnectStart < 10000) {
      delay(500);
      Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nReconnected to WiFi");
    } else {
      Serial.println("\nReconnection failed, will retry next loop");
    }
  }

  // read from door switch with debouncing
  int reading = digitalRead(DOOR_SWITCH_PIN);

  if (reading != lastReading) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay && reading != doorSwitch) {
    doorSwitch = reading;

    if (doorSwitch == HIGH && doorState == 0) {
      doorState = 1;
      Serial.println("door was UNLOCKED");
      sendSlackMessage("The front door was unlocked! Come on in!");
    } else if (doorSwitch == LOW && doorState == 1) {
      doorState = 0;
      Serial.println("door was locked");
      sendSlackMessage("The front door was locked, bye everyone!");
    }
  }

  lastReading = reading;

  delay(50);
}
