#include <SPIFFS.h>
#include <ArduinoJson.h>

class Configuration {
    public:
        Configuration();
        ~Configuration();
    
        bool initConfiguration();
        bool saveConfiguration(String file_name);
        bool readConfiguration(String file_name);
        void writeFile(fs::FS &fs, String filename, String message);
        String readFile(fs::FS &fs, String filename);
        
        int TempMax = 0;
        int RelayPin01 = 0;
        int RelayPin02 = 0;
        int Fan_Timer = 0;
        String Fan_State = "";
        String Wlan_SSID = "";
        String Wlan_Pass = "";
        String Wlan_HostName = "";

        String Mqtt_State = "";
        String Mqtt_Server = "";
        String Mqtt_User = "";
        String Mqtt_Pass = "";
        int Mqtt_Port = 1833;
        int Mqtt_SendInterval = 15;
};