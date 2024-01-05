#include <config.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

#define FORMAT_SPIFFS_IF_FAILED true

Configuration::Configuration(){
}

Configuration::~Configuration(){
}

bool Configuration::initConfiguration() {
    return SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED);
}
bool Configuration::readConfiguration(String file_name) {
  String file_content = readFile(SPIFFS, file_name);

  int config_file_size = file_content.length();
  Serial.println("Config file size: " + String(config_file_size));

  if(config_file_size > 1024) {
    Serial.println("Config file too large");
    return false;
  }
  StaticJsonDocument<1024> doc;
  auto error = deserializeJson(doc, file_content);
  if ( error ) { 
    Serial.println("Error interpreting config file");
    return false;
  }

  const int _maxTemp = doc["maxTemp"];
  const int _relayPin01 = doc["relayPin01"];
  const int _relayPin02 = doc["relayPin02"];
  const int _fan_Timer = doc["fan_Timer"];
  const String _fan_State = doc["fan_State"];
  const String _wlan_SSID = doc["wlan_SSID"];
  const String _wlan_Pass = doc["wlan_Pass"];
  const String _wlan_HostName = doc["wlan_HostName"];
  const String _mqtt_State = doc["mqtt_State"];
  const String _mqtt_Server = doc["mqtt_Server"];
  const String _mqtt_User = doc["mqtt_User"];
  const String _mqtt_Pass = doc["mqtt_Pass"];
  const int _mqtt_Port = doc["mqtt_Port"];
  const int _mqtt_SendInterval = doc["mqtt_SendInterval"];

  TempMax = _maxTemp;
  RelayPin01 = _relayPin01;
  RelayPin02 = _relayPin02;
  Fan_Timer = _fan_Timer;
  Fan_State = _fan_State;
  Wlan_SSID = _wlan_SSID;
  Wlan_Pass = _wlan_Pass;
  Wlan_HostName = _wlan_HostName;
  Mqtt_State = _mqtt_State;
  Mqtt_Server = _mqtt_Server;
  Mqtt_User = _mqtt_User;
  Mqtt_Pass = _mqtt_Pass;
  Mqtt_Port = _mqtt_Port;
  Mqtt_SendInterval = _mqtt_SendInterval;
  return true;
}

bool Configuration::saveConfiguration(String file_name) {
  StaticJsonDocument<1024> doc;

  // write variables to JSON file
  doc["maxTemp"] = TempMax;
  doc["relayPin01"] = RelayPin01;
  doc["relayPin02"] = RelayPin02;
  doc["fan_Timer"] = Fan_Timer;
  doc["fan_State"] = Fan_State;
  doc["wlan_SSID"] = Wlan_SSID;
  doc["wlan_Pass"] = Wlan_Pass;
  doc["wlan_HostName"] = Wlan_HostName;
  doc["mqtt_State"] = Mqtt_State;
  doc["mqtt_Server"] = Mqtt_Server;
  doc["mqtt_User"] = Mqtt_User;
  doc["mqtt_Pass"] = Mqtt_Pass;
  doc["mqtt_Port"] = Mqtt_Port;
  doc["mqtt_SendInterval"] = Mqtt_SendInterval;
  // write config file
  String tmp = "";
  serializeJson(doc, tmp);
  writeFile(SPIFFS, file_name, tmp);
  
  return true;
}

void Configuration::writeFile(fs::FS &fs, String filename, String message){
  Serial.println("writeFile -> Writing file: " + filename);

  File file = fs.open(filename, FILE_WRITE);
  if(!file){
    Serial.println("writeFile -> failed to open file for writing");
    return;
  }
  
  if(file.print(message)){
    Serial.println("writeFile -> file written");
  } else {
    Serial.println("writeFile -> write failed");
  }
  file.close();
}

String Configuration::readFile(fs::FS &fs, String filename){
  Serial.println("readFile -> Reading file: " + filename);

  File file = fs.open(filename);
  if(!file || file.isDirectory()){
    Serial.println("readFile -> failed to open file for reading");
    return "";
  }

  String fileText = "";
  while(file.available()){
    fileText = file.readString();
  }

  file.close();
  return fileText;
}
