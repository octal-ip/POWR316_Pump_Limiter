//WiFi can be disabled if this device won't be used where WiFi reception is available.
//Note that this will prevent any OTA updates - code uploads must be done over serial if WiFi is disabled.
#define WIFI

#include <Arduino.h>
#include <CSE7766.h>

#ifdef WIFI
  #include <WiFi.h>
  #include <ESPmDNS.h>
  #include <TelnetStream.h>
  #include <PubSubClient.h>
  #include <ArduinoOTA.h>

  /*Edit this file to include the following details.
    #define SECRET_SSID "your-SSID"
    #define SECRET_PASS "your-password"
  */
  #include <secrets.h>
#endif

const int loadPower = 100; //The power threshold (in watts) that will trigger the load timer to start.
const int loadTime = 900000; //The amount of time in milliseconds the load is allowed to continuously run for.
const char* hostname = "POWR316-Pump-Limiter";

unsigned long lastUpdate = 0;
int failures = 0; //The number of failed WiFi connection attempts. Will automatically restart the ESP if too many failures occurr in a row.
bool toggle = 0;
bool loadRunning = 0;
unsigned long loadStartTime = 0;

CSE7766 SonOffCSE7766;

/* Pin layout for the SonOff POWR316:
  GPIO 0: Button
  GPIO 5: WiFi LED
  GPIO 13: Relay
  GPIO 18: Power LED
  GPIO 16: CSE7766 Rx
*/

void setup()
{
  Serial.begin(115200);

  SonOffCSE7766.setRX(16);
  SonOffCSE7766.begin();

  pinMode(0, INPUT);
  pinMode(5, OUTPUT);
  pinMode(13, OUTPUT);
  pinMode(18, OUTPUT);

  digitalWrite(13, 1); //Turn the relay on.
  digitalWrite(18, 1); //Turn the power light off.

  #ifdef WIFI
    // ****Start ESP32 OTA and Wifi Configuration****
    Serial.println();
    Serial.print("Connecting to "); Serial.println(SECRET_SSID);
    
    WiFi.setHostname(hostname);
    WiFi.mode(WIFI_STA);
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
    WiFi.begin(SECRET_SSID, SECRET_PASS); //Edit include/secrets.h to update this data.

    unsigned long connectTime = millis();
    Serial.print("Waiting for WiFi to connect");
    while (!WiFi.isConnected() && (unsigned long)(millis() - connectTime) < 5000) { //Wait for the wifi to connect for up to 5 seconds.
      delay(100);
      Serial.print(".");
    }
    if (!WiFi.isConnected()) {
      Serial.println();
      Serial.println("WiFi didn't connect, restarting...");
      ESP.restart(); //Restart if the WiFi still hasn't connected.
    }

    Serial.println();
    Serial.print("IP address: "); Serial.println(WiFi.localIP());

    // Port defaults to 3232
    ArduinoOTA.setPort(3232);

    // Hostname defaults to esp3232-[MAC]
    ArduinoOTA.setHostname(hostname);

    // No authentication by default
    // ArduinoOTA.setPassword("admin");

    // Password can be set with it's md5 value as well
    // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
    // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

    ArduinoOTA.onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) {
        type = "sketch";
      } else { // U_SPIFFS
        type = "filesystem";
      }

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    });
    ArduinoOTA.onEnd([]() {
      Serial.println("\nEnd");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) {
        Serial.println("Auth Failed");
      } else if (error == OTA_BEGIN_ERROR) {
        Serial.println("Begin Failed");
      } else if (error == OTA_CONNECT_ERROR) {
        Serial.println("Connect Failed");
      } else if (error == OTA_RECEIVE_ERROR) {
        Serial.println("Receive Failed");
      } else if (error == OTA_END_ERROR) {
        Serial.println("End Failed");
      }
    });
    ArduinoOTA.begin();
    // ****End ESP8266 OTA and Wifi Configuration****

    //Telnet log is accessible at port 23
    TelnetStream.begin();
  #endif
}


void loop()
{
  #ifdef WIFI
    ArduinoOTA.handle();
  
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected. Attempting to reconnect... ");
      failures++;
      WiFi.begin(SECRET_SSID, SECRET_PASS);
      delay(1000);
    }
    else if (failures > 0) failures--;
  #endif

  if ((unsigned long)(millis() - lastUpdate) >= 1000) { //Flash the WiFi LED to show the system is active/alive.
    lastUpdate = millis();
    if (toggle == 0) {
      digitalWrite(5, 0);
      toggle = 1;
    }
    else {
      digitalWrite(5, 1);
      toggle = 0;
    }

    SonOffCSE7766.handle();

    #ifdef WIFI
      TelnetStream.printf("Voltage %.1f V\r\n", SonOffCSE7766.getVoltage());
      TelnetStream.printf("Current %.1f A\r\n", SonOffCSE7766.getCurrent());
      TelnetStream.printf("ActivePower %.1f W\r\n", SonOffCSE7766.getActivePower());
      TelnetStream.printf("ApparentPower %.1f VA\r\n", SonOffCSE7766.getApparentPower());
      TelnetStream.printf("ReactivePower %.1f VAR\r\n", SonOffCSE7766.getReactivePower());
      TelnetStream.printf("PowerFactor %.1f\r\n", SonOffCSE7766.getPowerFactor());
      TelnetStream.printf("Energy %.2f kWh\r\n", (SonOffCSE7766.getEnergy()/3600/1000));
    #endif

    if (SonOffCSE7766.getActivePower() > loadPower && loadRunning == 0) { //Start the timer once the load is turned on.
      loadRunning = 1;
      loadStartTime = millis();
    }
    else if (SonOffCSE7766.getActivePower() < loadPower && loadRunning == 1) { //Stop the timer once the load is turned off.
      loadRunning = 0;
    }

    #ifdef WIFI
      if (loadRunning == 1) TelnetStream.printf("Load has been running for %d seconds.\r\n", ((unsigned long)(millis() - loadStartTime)/1000));
    #endif

    if (loadRunning == 1 && ((unsigned long)(millis() - loadStartTime) > loadTime)) { //If the load has been running for more than 15 minutes
      #ifdef WIFI
        TelnetStream.println("Load has been running too long! Switching off...");
      #endif
      digitalWrite(13, 0);
      digitalWrite(18, 0);
    }
  }

  if (digitalRead(0) == 0) { //Reset timer and switch load on if the button is pushed.
    loadRunning = 0;
    loadStartTime = 0;
    digitalWrite(13, 1);
    digitalWrite(18, 1);
    #ifdef WIFI
      TelnetStream.println("Reset button pressed. Switching on...");
    #endif
  }

  #ifdef WIFI
    if (failures >= 40) {  //Reboot the ESP if there's been too many problems with the WiFi connection.
      Serial.print("Too many failures, rebooting...");
      TelnetStream.print("Failure counter has reached: "); TelnetStream.print(failures); TelnetStream.println(". Rebooting...");
      ESP.restart();
    }
  #endif
}