#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESP8266HTTPClient.h>

const char* ssid = "LBT";
const char* ssid2 = "FRITZ!WLAN Repeater 450E";
const char* password = "hovawart";

// Domoticz:
const char* HOST = "192.168.178.99";
const int   PORT = 8080;
const int   SENSOR_ID = 2933;
const int   ROBOTER_ID = 2941;

HTTPClient http;
WiFiClient client;

// Create an instance of the server
AsyncWebServer server(80);

#define HIGH 0
#define LOW 1

int torStatus;
int lastTorStatus;

const int TOR_AUF = 0;
const int TOR_ÖFFNET = 1;
const int TOR_SCHLIESST = 2;
const int TOR_ÖFFNET_MANUELL = 3;
const int TOR_SCHLIESST_MANUELL = 4;
const int TOR_ZU = 5;

#define AUF 1
#define ZU 0

// Define GPIO pins for the relays
const int RELAIS_AUF = 5;
const int RELAIS_ZU = 4;

// Define the GPIO for the switches:
const int ENDLAGE_TOR_AUF = 13;
const int ENDLAGE_TOR_ZU = 12;
const int TOR_AUF_ZU = 14;

boolean swTorAuf = HIGH;
boolean swTorZu = HIGH;
boolean swRoboter = LOW;
boolean manualOpen = LOW;

void setup() {
  // Start Serial
  Serial.begin(115200);

  // Initialize the GPIO pins as outputs
  pinMode(RELAIS_AUF, OUTPUT);
  pinMode(RELAIS_ZU, OUTPUT);

  pinMode(TOR_AUF_ZU, INPUT_PULLUP);
  pinMode(ENDLAGE_TOR_AUF, INPUT_PULLUP);
  pinMode(ENDLAGE_TOR_ZU, INPUT_PULLUP);

  // Set initial state of the relays
  digitalWrite(RELAIS_AUF, 1);
  digitalWrite(RELAIS_ZU, 1);
  int wifiTry = 0;
  // Connect to Wi-Fi
  const char * wifi = ssid;
  WiFi.begin(wifi, password);
  while (WiFi.status() != WL_CONNECTED) {
    wifiTry++;
    delay(1000);
    Serial.print("Connecting to WiFi...");
    Serial.println(ssid);
    if(wifiTry > 12){
      break;
    }
  }
  if(WiFi.status() != WL_CONNECTED ){
    wifi = ssid2;
    wifiTry = 0;
    WiFi.begin(wifi, password);
    while (WiFi.status() != WL_CONNECTED) {
      wifiTry++;
      delay(1000);
      Serial.print("Connecting to WiFi...");
      Serial.println(ssid);
      if(wifiTry > 12){
        break;
      }
    }
  }
  Serial.print("Connected to WiFi: ");
  Serial.println(ssid);

  // Print the IP address
  Serial.println(WiFi.localIP());

  // Route for handling the control command
  server.on("/control", HTTP_GET, [](AsyncWebServerRequest *request){
    String cmd = request->getParam("cmd")->value();
    Serial.println("Command received: " + cmd);

    // Parse the command
    int pin, value;
    if (sscanf(cmd.c_str(), "GPIO,%d,%d", &pin, &value) == 2) {
      if (pin == RELAIS_AUF && swTorAuf == LOW) {
        Serial.println("received open command");
        //torStatus = TOR_ÖFFNET_MANUELL;
        if(torStatus != TOR_AUF){
          digitalWrite(RELAIS_AUF, HIGH);
          digitalWrite(RELAIS_ZU, LOW);
          torStatus = TOR_ÖFFNET;
          manualOpen=HIGH;
        } else {
          Serial.println("open command ignored");
        }

        request->send(200, "text/plain", "OK");
      }else if (pin == RELAIS_ZU && swTorZu == LOW) {
        Serial.println("received close command");
        //torStatus = TOR_SCHLIESST_MANUELL;
        if(torStatus != TOR_ZU){
          digitalWrite(RELAIS_ZU, HIGH);
          digitalWrite(RELAIS_AUF, LOW);
          torStatus = TOR_SCHLIESST;
          manualOpen=LOW;
        } else {
          Serial.println("close command ignored");
        }

        request->send(200, "text/plain", "OK");
      } else {
        request->send(400, "text/plain", "Invalid GPIO pin");
      }
    } else {
      request->send(400, "text/plain", "Invalid command format");
    }
  });

  // Start the server
  server.begin();
}

void loop() {
  // Nothing to do here - everything is handled by the server
  swTorAuf = digitalRead(ENDLAGE_TOR_AUF);
  swTorZu = digitalRead(ENDLAGE_TOR_ZU);
  swRoboter = digitalRead(TOR_AUF_ZU);
  Serial.print("Endlage Tor auf:");
  Serial.print(!swTorAuf);
  Serial.print(" -- Endlage Tor zu:");
  Serial.print(!swTorZu);
  Serial.print(" -- Roboter da:");
  Serial.print(!swRoboter);
  Serial.print(" -- Tor Status:");
  Serial.print(torStatus);
  Serial.print(" -- Tor LAST Status:");
  Serial.println(lastTorStatus);

    if(manualOpen==LOW){
      if(swRoboter == LOW){ // Roboter hat sich entfernt
        if(swTorAuf == LOW){
          Serial.println("öffne Tor");
          digitalWrite(RELAIS_AUF, HIGH);
          digitalWrite(RELAIS_ZU, LOW);
          torStatus = TOR_ÖFFNET;
        }
      }

      if(swRoboter == HIGH){ // Roboter auf Ladestation
        if(swTorZu == LOW){
          Serial.println("schließe Tor");
          digitalWrite(RELAIS_ZU, HIGH);
          digitalWrite(RELAIS_AUF, LOW);
          torStatus = TOR_SCHLIESST;
        }
      }
    }

  


  if(torStatus == TOR_ÖFFNET && swTorAuf == HIGH ){
    // Tor geöffnet
      Serial.println("Tor geöffnet");
      digitalWrite(RELAIS_ZU, LOW);
      digitalWrite(RELAIS_AUF, LOW);
      torStatus = TOR_AUF;
  }

  if(torStatus == TOR_SCHLIESST && swTorZu == HIGH ){
    // Tor geschlossen
      Serial.println("Tor geschlossen");
      digitalWrite(RELAIS_ZU, LOW);
      digitalWrite(RELAIS_AUF, LOW);
      torStatus = TOR_ZU;
  }

  if(torStatus != lastTorStatus){
    if(torStatus == TOR_AUF){
      send2Domoticz(false);
      sendRoboterStatus("Roboter%20unterwegs");
    } else if(torStatus == TOR_ZU){
      send2Domoticz(true);
      sendRoboterStatus("Roboter%20zu%20Hause");
    }
    lastTorStatus = torStatus;
  }
  delay(100);
  
}

void send2Domoticz(boolean state){
  Serial.print("connecting to ");
  Serial.println(HOST);

// http://192.168.178.99:8080/json.htm?type=command&param=switchlight&idx=2933&switchcmd=Off
  String url = "/json.htm?type=command&param=switchlight&idx=";
  url += String(SENSOR_ID);
  url += "&switchcmd=";
  if(state){
    url += "On"; 
  }else {
    url += "Off"; 
  }
  
  http.begin(client, HOST,PORT,url);
  int httpCode = http.GET();
    if (httpCode) {
      if (httpCode == 200) {
        String payload = http.getString();
        Serial.println("Domoticz response "); 
        Serial.println(payload);
      }
    }
  Serial.println("closing connection");
  http.end();
  delay(3000);
}

void sendRoboterStatus(String status){
  
  Serial.print("connecting to ");
  Serial.println(HOST);

// http://192.168.178.99:8080/json.htm?type=command&param=udevice&idx=2941&nvalue=0&svalue=Roboter%20zu%20Hause
  String url = "/json.htm?type=command&param=udevice&idx=";
  url += String(ROBOTER_ID);
  url += "&nvalue=0&svalue=";
  url += status; 

  
  http.begin(client, HOST,PORT,url);
  int httpCode = http.GET();
    if (httpCode) {
      if (httpCode == 200) {
        String payload = http.getString();
        Serial.println("Domoticz response "); 
        Serial.println(payload);
      }
    }
  Serial.println("closing connection");
  http.end();
  
}

