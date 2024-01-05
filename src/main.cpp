#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_HTU21DF.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <config.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <PubSubClient.h>

// define static vars
#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT  64
#define SCREEN_ADDRESS 0x3D ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
#define OLED_RESET_PIN -1
#define AN             LOW
#define AUS            HIGH
#define DEBUG          true

// settings-section
//const char* WLAN_SSID = "123";
//const char* WLAN_PASSWORD = "123";
const char* DNS_NAME = "ESP_Dryer2";
const char* OTA_Password = "FilaDry";
//const char* PARAM_STRING = "inputString";
//const char* PARAM_INT = "inputInt";
//const char* PARAM_FLOAT = "inputFloat";
const String config_filename = "/config.json";
String mqttTopic;
String mqttTopcSubs;

String wlanApName = "";
float tempNow         = 0;
float humNow          = 0;
int relay01State      = 0;  // FAN
int relay02State      = 0;  // HEATER
int fanTime           = 0;
int switchTimeOut     = 0;
int tryConnectsToMqtt = 0;
int mqttSendInterval  = 15;
int loopTimer         = 1000;
bool initComplet      = false;
bool initMqttOk       = true;
bool fanActiv         = false;
bool heartBeat        = false;
unsigned long previousMillis = 0;
unsigned long interval = 30000;

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP Setup Form</title>
  <meta name="viewport" content="width=device-width, initial-scale=1" charset="utf-8">
</head>
<body>
  <h2>ESP Filament Dryer - Data</h2>
  <table>
    <tr style="height:50px">
      <th style="width:175px">Beschreibung</th>
      <th>Akt. Wert</th> 
    </tr>
    <tr style="height:30px">
      <td>Temperatur (°C) </td><td>%tempNow%</td>
    </tr>
    <tr style="height:30px">
      <td>Feuchtigkeit (Proz) </td><td>%humNow%</td>
    </tr>
    <tr style="height:30px">
      <td>Lüfterlaufzeit (sek) </td><td>%fan_Timer%</td>
    </tr>
  </table>
  <a href="/start"> <button>Start fan timer</button> </a>
  </br>
  </br>
  <a href="/stop"> <button>Stopp fan timer</button> </a>
  <br>
  <br>
  <a href="/setup">Setup</a>
  <iframe style="display:none" name="hidden-form"></iframe>
</body></html>)rawliteral";

const char setup_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>ESP Setup Form</title>
  <meta name="viewport" content="width=device-width, initial-scale=1" charset="utf-8">
  <script>
    function submitMessage() {
      alert("Saved value to ESP SPIFFS");
      setTimeout(function(){ document.location.reload(false); }, 500);   
    }
  </script></head><body>
  <form action="/get" target="hidden-form">
    WLAN SSID: <input type="text" name="wlan_SSID" value="%wlan_SSID%">
    <input type="submit" value="Submit" onclick="submitMessage()">
  </form><br>
  <form action="/get" target="hidden-form">
    WLAN Password: <input type="password" name="wlan_Pass" value=%wlan_Pass%>
    <input type="submit" value="Submit" onclick="submitMessage()">
  </form><br>
  <form action="/get" target="hidden-form">
    Max Temperatur: <input type="number " name="tempMax" min="0" max="100" step="5" value=%tempMax%>
    <input type="submit" value="Submit" onclick="submitMessage()">
  </form><br>
  <form action="/get" target="hidden-form">
    FanTimer (sek): <input type="number " name="fan_Timer" value=%fan_Timer%>
    <input type="submit" value="Submit" onclick="submitMessage()">
  </form><br>
  <form action="/get" target="hidden-form">
    RelayPin01: <input type="number " name="relayPin01" value=%relayPin01%>
    <input type="submit" value="Submit" onclick="submitMessage()">
  </form><br>
  <form action="/get" target="hidden-form">
    RelayPin02: <input type="number " name="relayPin02" value=%relayPin02%>
    <input type="submit" value="Submit" onclick="submitMessage()">
  </form><br>
  <form action="/get" target="hidden-form">
    MQTT Server: <input type="text" name="mqtt_Server" value=%mqtt_Server%>
    <input type="submit" value="Submit" onclick="submitMessage()">
  </form><br>
  <form action="/get" target="hidden-form">
    MQTT Username: <input type="text" name="mqtt_User" value=%mqtt_User%>
    <input type="submit" value="Submit" onclick="submitMessage()">
  </form><br>
  <form action="/get" target="hidden-form">
    MQTT Password: &nbsp <input type="password" name="mqtt_Pass" value=%mqtt_Pass%>
    <input type="submit" value="Submit" onclick="submitMessage()">
  </form><br>
  <form action="/get" target="hidden-form">
    MQTT Port: &nbsp &nbsp <input type="number " name="mqtt_Port" value=%mqtt_Port%>
    <input type="submit" value="Submit" onclick="submitMessage()">
  </form><br>
  <form action="/get" target="hidden-form">
    MQTT SendInterval: &nbsp &nbsp <input type="number " name="mqtt_SendInterval" value=%mqtt_SendInterval%>
    <input type="submit" value="Submit" onclick="submitMessage()">
  </form><br>
  <form action="/get" target="hidden-form">
    <fieldset name="mqtt">
      <input type="radio" id="mqqt" name="mqtt_State" value="MQTT_YES" %mqtt_State_Check1%>
      <label for="mqqt">MQTT Aktiv</label><br>
      <input type="radio" id="nomqtt" name="mqtt_State" value="MQTT_NO" %mqtt_State_Check2%>
      <label for="nomqtt">MQTT Deaktiv</label><br>
      <input type="submit" value="Submit" onclick="submitMessage()">
    </fieldset>
  </form><br>
  <form action="/get" target="hidden-form">
    <fieldset name="fan">
      <input type="radio" id="fan" name="fan_State" value="FAN_YES" %fan_State_Check1%>
      <label for="fan">Lüfter Aktiv</label><br>
      <input type="radio" id="nofan" name="fan_State" value="FAN_NO" %fan_State_Check2%>
      <label for="nofan">Lüfter Deaktiv</label><br>
      <input type="submit" value="Submit" onclick="submitMessage()">
    </fieldset>
  </form>
  <br>
  <br>
  <a href="/">Index</a>
  <iframe style="display:none" name="hidden-form"></iframe>
</body></html>)rawliteral";


// init objects, display, sensor, webserver, config
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET_PIN);
Adafruit_HTU21DF htu = Adafruit_HTU21DF();
AsyncWebServer server(80);
Configuration config;
WiFiClient espClient;
PubSubClient client(espClient);

// put function declarations here:
void initOTA();
void initWlan();
void initConfig();
void initDisplay();
void initSensor();
void initRelay();
void initWebserver();
void handlePortal();
void reconnectMqtt();
void WebsiteUnknown(AsyncWebServerRequest *request);
void writeLog(String text);
String GenerateUniqueName();
String processor(const String& var);
bool StringToBoolean(const String& str);
void callback(char* topic, byte* payload, unsigned int length);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  
  Serial.println("Starting ESP, loading Config...");
  initConfig();

  uint64_t chipid=ESP.getEfuseMac();//The chip ID is essentially its MAC address(length: 6 bytes).
  Serial.printf("ESP32 Chip ID = %04X\n",(uint16_t)(chipid>>32));//print High 2 bytes
  uint16_t chip = (uint16_t)(chipid>>32);
  wlanApName = "ESP32AP-" + String(chip);
  //config.Wlan_SSID="FRITZ!Box 7590 HT";
  //config.Wlan_Pass="53319563609913767261";
  initRelay(); 
  writeLog("Init display...");
  initDisplay();
  writeLog("Init temp sensor...");
  initSensor();
  writeLog("Init Wlan...");
  initWlan();
  writeLog("Init OTA...");
  initOTA();
  writeLog("Init Webserver...");
  initWebserver();

  if((config.Mqtt_State != "MQTT_YES")||(config.Mqtt_Server !="")) {
    mqttTopic = "sensor/" + wlanApName;
    mqttTopcSubs = "sensor/" + wlanApName+ "/FanActiv";
    client.setServer(config.Mqtt_Server.c_str(), 1883);
    client.setCallback(callback);
  }
  else {
    Serial.println("No MQTT Server");
  }

  initComplet = true;
  Serial.println("*****   Init complete    *****");
}

void writeLog(String text) {
  if(DEBUG)
    Serial.println(text);
}

void loop() {
  if(!initComplet)
    return;
  
  unsigned long currentMillis = millis();
  // if WiFi is down, try reconnecting
  if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >=interval)) {
    Serial.print(millis());
    Serial.println("Reconnecting to WiFi...");
    WiFi.disconnect();
    WiFi.reconnect();
    previousMillis = currentMillis;
  }
  
  tempNow = htu.readTemperature();
  humNow = htu.readHumidity();
  relay01State = digitalRead(config.RelayPin01);
  relay02State = digitalRead(config.RelayPin02);
  
  if(config.Mqtt_State == "MQTT_YES") {
    if((config.Mqtt_Server !="") && (initMqttOk)) {
      if (!client.connected()) {
        Serial.println("reconnect Mqtt...");
        reconnectMqtt();
      }
        Serial.println("mqtt loop...");
      client.loop();
    }
  }
  // FAN - SEKTION
  if((config.Fan_State == "FAN_YES") && (fanActiv))
  {
    if(fanTime-- > 0){
      if(relay01State != LOW) {
        digitalWrite(config.RelayPin01, LOW);
        Serial.println("Set output LOW, Fan START");
      }
    }
    else {
      if(relay01State != HIGH) {
        fanActiv = false;
        fanTime = 0;
        digitalWrite(config.RelayPin01, HIGH);
        Serial.println("Set output HIGH, Fan STOP");
      }
    }
  }
  // HEATER - SEKTION
  
  // MQTT - SEKTION
  if(mqttSendInterval<1){
    if((initMqttOk) && (config.Mqtt_State == "MQTT_YES")) {
      Serial.println("Send Mqtt data ("+String(mqttTopic)+")...");
      client.publish((String(mqttTopic) + ("/Temp")).c_str(), String(tempNow).c_str(), true);
      client.publish((String(mqttTopic) + ("/Hum")).c_str(), String(humNow).c_str(), true);
      client.publish((String(mqttTopic) + ("/FanTime")).c_str(), String(fanTime).c_str(), true);
      client.publish((String(mqttTopic) + ("/FanActivState")).c_str(), String(fanActiv).c_str(), true);
      client.publish((String(mqttTopic) + ("/Relais01State")).c_str(), String(relay01State).c_str(), true);
      client.publish((String(mqttTopic) + ("/Relais02State")).c_str(), String(relay02State).c_str(), true);
    }
    mqttSendInterval=config.Mqtt_SendInterval;
  }
  else {  
    mqttSendInterval--;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println(" Filament dryer v0.2 ");
  display.println("=====================");
  display.println("Temp-Now    : " + String(tempNow) +" C");
  display.println("Humidity    : " + String(humNow) + " %");
  display.println("HeartBeat   : " + String(heartBeat));
  
  // show Fan state
  if(config.Fan_State == "FAN_YES") {
    display.println("Fan-Timer   : " + String(fanTime));
    display.println("Fan-Activ   : " + String(fanActiv));
  }
  else {
    display.println("Fan  : not activ");
  }
  
  // show MQtt state
  if(config.Mqtt_State == "MQTT_YES")
    display.println("MQTT-Update : " + String(mqttSendInterval));
  else  
    display.println("MQTT : not activ");
  display.display();
  // show heartbeat
  heartBeat = !heartBeat;
  // Delay
  delay(loopTimer);
}

void callback(char* topic, byte* payload, unsigned int length) {
  char p[length +1];
  memcpy(p, payload, length);
  p[length] = NULL;
  String message(p);

  if(message == "0") {
      fanActiv = false;
      fanTime = 0;
      digitalWrite(config.RelayPin01, HIGH);
      Serial.println("Set output HIGH, Fan STOP");
  } else {
      fanActiv = true;
      fanTime = config.Fan_Timer;
      digitalWrite(config.RelayPin01, LOW);
      Serial.println("Set output LOW, Fan START");
     Serial.println("Callback != 0");    
  }
  Serial.write(payload, length);
  Serial.println();
}

void reconnectMqtt() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Try connecting MQTT...");
    tryConnectsToMqtt++;
    // Attempt to connect
    if (client.connect(DNS_NAME, config.Mqtt_User.c_str(), config.Mqtt_Pass.c_str())) {
      Serial.println("connected");
      client.subscribe(mqttTopcSubs.c_str());
      initMqttOk = true;
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      //
      if(tryConnectsToMqtt>6) {
        tryConnectsToMqtt = 0;
        Serial.println("Cancel connect to Mqtt-Server");
        display.println("MQTT: No Conn");
        display.display();
        delay(2500);
        initMqttOk = false;
        return;
      }
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void initWebserver() {

  // Send web page with current data
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });
  // Send web page with input fields to config this esp
  server.on("/setup", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", setup_html, processor);
  });
  // start the fan
  server.on("/start", HTTP_GET, [](AsyncWebServerRequest *request){
    fanActiv = true;
    fanTime = config.Fan_Timer;
    request->send_P(200, "text/html", index_html, processor);
  });
  // stop the fan
  server.on("/stop", HTTP_GET, [](AsyncWebServerRequest *request){
    fanTime = 0;
    request->send_P(200, "text/html", index_html, processor);
  });

  // Send a GET request to <ESP_IP>/get?inputString=<inputMessage>
  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage;
    // GET inputString value on <ESP_IP>/get?inputString=<inputMessage>
    if (request->hasParam("wlan_SSID")) {
      inputMessage = request->getParam("wlan_SSID")->value();
      config.Wlan_SSID = inputMessage;
    }
    else if (request->hasParam("wlan_Pass")) {
      inputMessage = request->getParam("wlan_Pass")->value();
      config.Wlan_Pass = inputMessage;
    }
    else if (request->hasParam("tempMax")) {
      inputMessage = request->getParam("tempMax")->value();
      config.TempMax = atoi(inputMessage.c_str());
    }
    else if (request->hasParam("relayPin01")) {
      inputMessage = request->getParam("relayPin01")->value();
      config.RelayPin01 = atoi(inputMessage.c_str());
      initRelay();  // reinit PIN
    }
    else if (request->hasParam("relayPin02")) {
      inputMessage = request->getParam("relayPin02")->value();
      config.RelayPin02 = atoi(inputMessage.c_str());
      initRelay();  // reinit PIN
    }
    else if (request->hasParam("fan_Timer")) {
      inputMessage = request->getParam("fan_Timer")->value();
      config.Fan_Timer = atoi(inputMessage.c_str());
    }
    else if (request->hasParam("fan_State")) {
      inputMessage = request->getParam("fan_State")->value();
      config.Fan_State = inputMessage;
    }
    else if (request->hasParam("mqtt_State")) {
      inputMessage = request->getParam("mqtt_State")->value();
      config.Mqtt_State = inputMessage;
    }
    else if (request->hasParam("mqtt_Server")) {
      inputMessage = request->getParam("mqtt_Server")->value();
      config.Mqtt_Server = inputMessage;
    }
    else if (request->hasParam("mqtt_User")) {
      inputMessage = request->getParam("mqtt_User")->value();
      config.Mqtt_User = inputMessage;
    }
    else if (request->hasParam("mqtt_Pass")) {
      inputMessage = request->getParam("mqtt_Pass")->value();
      config.Mqtt_Pass = inputMessage;
    }
    else if (request->hasParam("mqtt_Port")) {
      inputMessage = request->getParam("mqtt_Port")->value();
      config.Mqtt_Port = atoi(inputMessage.c_str());
    }
    else if (request->hasParam("mqtt_SendInterval")) {
      inputMessage = request->getParam("mqtt_SendInterval")->value();
      config.Mqtt_SendInterval = atoi(inputMessage.c_str());
    }
    else {
      inputMessage = "No message sent";
    }

    config.saveConfiguration(config_filename);

    Serial.println("inputMessage: " + inputMessage);
    request->send(200, "text/text", inputMessage);
  });
  server.onNotFound(WebsiteUnknown);
  server.begin();
}

String processor(const String& var){
  Serial.println(var);
  if(var == "wlan_SSID"){
    return config.Wlan_SSID;
  }
  else if(var == "wlan_Pass"){
    return config.Wlan_Pass;
  }
  else if(var == "tempMax"){
    return String(config.TempMax);
  }
  else if(var == "tempNow"){
    return String(tempNow);
  }
  else if(var == "humNow"){
    return String(humNow);
  }
  else if(var == "relayPin01"){
    return String(config.RelayPin01);
  }
  else if(var == "relayPin02"){
    return String(config.RelayPin02);
  }
  else if(var == "fan_Timer"){
    return String(config.Fan_Timer);
  }
  else if(var == "fan_State"){
    return config.Fan_State;
  }
  else if(var == "mqtt_State"){
    return config.Mqtt_State;
  }
  else if(var == "mqtt_Server"){
    return config.Mqtt_Server;
  }
  else if(var == "mqtt_User"){
    return config.Mqtt_User;
  }
  else if(var == "mqtt_Pass"){
    return config.Mqtt_Pass;
  }
  else if(var == "mqtt_Port"){
    return String(config.Mqtt_Port);
  }
  else if(var == "mqtt_SendInterval"){
    return String(config.Mqtt_SendInterval);
  }
  else if(var == "mqtt_State_Check1"){
    if(config.Mqtt_State == "MQTT_YES") {
      return "checked";
    }
    return "";
  }
  else if(var == "mqtt_State_Check2"){
    if(config.Mqtt_State == "MQTT_NO") {
      return "checked";
    }
    return "";
  }
  else if(var == "fan_State_Check1"){
    if(config.Fan_State == "FAN_YES") {
      return "checked";
    }
    return "";
  }
  else if(var == "fan_State_Check2"){
    if(config.Fan_State == "FAN_NO") {
      return "checked";
    }
    return "";
  }
  return String();
}

void WebsiteUnknown(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

void initRelay() {
  if(config.RelayPin01>0) {
    pinMode(config.RelayPin01, OUTPUT);
      digitalWrite(config.RelayPin01, HIGH);
  }
  if(config.RelayPin02>0) {
    pinMode(config.RelayPin02, OUTPUT);
      digitalWrite(config.RelayPin02, HIGH);
  }
}

void initSensor() {
  if (!htu.begin()) {
    Serial.println("Check circuit. HTU21D not found!");
  }
}

void initDisplay() {
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Display SSD1306 allocation failed");
    return;
  }
  display.clearDisplay();     // Clear the buffer
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println(" Filament dryer v0.2 ");
  display.println("*********************");
  display.println("   by Simon Grimm    ");
  display.display();
  delay(2000);                // Pause for 2 seconds
}

void initConfig() {
  if(!config.initConfiguration())  {
    Serial.println("Config -> SPIFFS Mount Failed");
  }
  else
  {
    Serial.println("Config -> SPIFFS mounted successfully");
    if(!config.readConfiguration(config_filename))    {
      Serial.println("Config -> Could not read Config file -> initializing new file");
      // if not possible -> save new config file
      if (config.saveConfiguration(config_filename))      {
        Serial.println("Config -> Config file saved");
      }   
    }
  }
}

void initWlan() {
  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.print(config.Wlan_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.Wlan_SSID, config.Wlan_Pass);

  byte triesToConnectWLAN = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    if(triesToConnectWLAN++ >30) {
      Serial.println("");
      Serial.println("Unable to connect WLAN, create AP ("+wlanApName+")");
      WiFi.mode(WIFI_AP);
      WiFi.softAP(wlanApName, wlanApName);
      
      display.println("WLAN-AP Mode");
      display.println("Name :" + wlanApName);
      display.println("PWD  :" + wlanApName);
      display.display();
      break;      
    }
  }
  Serial.println("");
  Serial.print("WiFi connected, IP address: ");
  Serial.println(WiFi.localIP());
  
  display.println("Device  :" + wlanApName);
  display.display();
  delay(1500);
  
  if(MDNS.begin(DNS_NAME)) {
    Serial.println("DNS started!");
  }
}

void initOTA() {
  ArduinoOTA.setHostname(DNS_NAME);
  ArduinoOTA.setPassword(OTA_Password);
  ArduinoOTA.begin();
}