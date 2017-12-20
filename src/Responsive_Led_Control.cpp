// Copyright (c) 2016 @jake-b, @russp81, @toblum
// Griswold LED Lighting Controller

// Griswold is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation, either version 3 of
// the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

// Griswold is a fork of the LEDLAMP project at
//        https://github.com/russp81/LEDLAMP_FASTLEDs

// The LEDLAMP project is a fork of the McLighting Project at
//        https://github.com/toblum/McLighting


#define ENABLE_TEMPSENSE
#define ENABLE_LIGHTSENSE

// ***************************************************************************
// Load libraries for: WebServer / WiFiManager / WebSockets
// ***************************************************************************
#if defined(ESP32)
#include <WiFi.h>
#ifndef BUILTIN_LED
#define BUILTIN_LED 2
#define KEY_BUTTON 0
#endif
#else
#include <ESP8266WiFi.h>  //https://github.com/esp8266/Arduino
#endif

#include <Wire.h>

// needed for library WiFiManager
#include <DNSServer.h> // ESP32 -> https://github.com/zhouhan0126/DNSServer---esp32.git
#include <ESP8266WebServer.h>
#include <WiFiManager.h>  // https://github.com/bbx10/WiFiManager.git (esp32 branch)

#include <ArduinoOTA.h>
#if defined (ESP32)
#include <ESPmDNS.h>
#else
#include <ESP8266mDNS.h>
#endif
#include <FS.h>
#include <WiFiClient.h>

#include <SimpleTimer.h>  //https://github.com/schinken/SimpleTimer.git
#include "RemoteDebug.h" //https://github.com/JoaoLopesF/RemoteDebug

#include <WebSockets.h>  //https://github.com/Links2004/arduinoWebSockets (esp32 branch)
#include <WebSocketsServer.h>

#ifdef ENABLE_TEMPSENSE
#include <EnvironmentCalculations.h>
#include <BME280I2C.h>
#endif

#ifdef ENABLE_LIGHTSENSE
#include <BH1750.h>
#endif

// ***************************************************************************
// Sub-modules of this application
// ***************************************************************************
#include "definitions.h"
#include "eepromsettings.h"
#include "palettes.h"
#include "colormodes.h"

// ***************************************************************************
// Forward Declarations
// ***************************************************************************
void nextPattern(void);

// ***************************************************************************
// Instanciate HTTP(80) / WebSockets(81) Server
// ***************************************************************************
#if defined(ESP32)
WebServer server(80);
#else
ESP8266WebServer server(80);
#endif
WebSocketsServer webSocket = WebSocketsServer(81);

#ifdef ENABLE_TEMPSENSE
BME280I2C::Settings bme_settings(
   BME280::OSR_X1,
   BME280::OSR_X1,
   BME280::OSR_X1,
   BME280::Mode_Forced,
   BME280::StandbyTime_1000ms,
   BME280::Filter_Off,
   BME280::SpiEnable_False,
   0x76
);
BME280I2C bme(bme_settings);
float temp, hum, baro, alt, dew, sea;
#endif

#ifdef ENABLE_LIGHTSENSE
BH1750 bhl(0x23);
uint16_t lux;
#endif

// ***************************************************************************
// Load library SimpleTimer for blinking status led
// ***************************************************************************
SimpleTimer timer;
int ticker;
bool led_state;

void tick() {
  // toggle state
  led_state = !led_state;
  digitalWrite(BUILTIN_LED, led_state);
}


void Every5Second() {
  DBG_OUTPUT_PORT.printf("FreeHeap: %d\n", ESP.getFreeHeap());

  #ifdef ENABLE_LIGHTSENSE
  lux = bhl.readLightLevel();
  #else
  lux = 0;
  #endif

  #ifdef ENABLE_TEMPSENSE
  bme.read(baro, temp, hum, BME280::TempUnit_Celsius, BME280::PresUnit_hPa); // hPa
  alt = EnvironmentCalculations::Altitude(baro, EnvironmentCalculations::AltitudeUnit_Meters);
  dew = EnvironmentCalculations::DewPoint(temp, hum, EnvironmentCalculations::TempUnit_Celsius);
  sea = EnvironmentCalculations::EquivalentSeaLevelPressure(alt, temp, baro);
  DBG_OUTPUT_PORT.printf("T: %.2f H: %.2f A: %.2f B: %.2f L: %d\n", temp, hum, alt, baro, lux);
  #endif
}

// ***************************************************************************
// Callback for WiFiManager library when config mode is entered
// ***************************************************************************
// gets called when WiFiManager enters configuration mode
void configModeCallback(WiFiManager *myWiFiManager) {
  DBG_OUTPUT_PORT.println("Entered config mode");
  DBG_OUTPUT_PORT.println(WiFi.softAPIP());
  // if you used auto generated SSID, print it
  DBG_OUTPUT_PORT.println(myWiFiManager->getConfigPortalSSID());
  // entered config mode, make led toggle faster
  ticker = timer.setInterval(200, tick);

  // Show USER that module can't connect to stored WiFi
  uint16_t i;
  for (i = 0; i < NUM_LEDS; i++) {
    leds[i].setRGB(0, 0, 50);
  }
  FastLED.show();
}

// ***************************************************************************
// Include: Webserver & Request Handlers
// ***************************************************************************
#include "spiffs_webserver.h"    // must be included after the 'server' object
#include "request_handlers.h"    // is declared.

// ***************************************************************************
// MAIN
// ***************************************************************************
void setup() {

  // Generate a pseduo-unique hostname
  char hostname[strlen(HOSTNAME_PREFIX)+6];
  #if defined (ESP32)
  uint64_t chipid = ESP.getEfuseMac();
  sprintf(hostname, "%s-%04x",HOSTNAME_PREFIX, (uint16_t)(chipid>>32));
  #else
  uint16_t chipid = ESP.getChipId() & 0xFFFF;
  sprintf(hostname, "%s-%04x",HOSTNAME_PREFIX, chipid);
  #endif
#ifdef REMOTE_DEBUG
  Debug.begin(hostname);  // Initiaze the telnet server - hostname is the used
                          // in MDNS.begin
  Debug.setResetCmdEnabled(true);  // Enable the reset command
#endif

  // ***************************************************************************
  // Setup: EEPROM
  // ***************************************************************************
  initSettings();  // setting loaded from EEPROM or defaults if fail
  printSettings();

  ///*** Random Seed***
  randomSeed(analogRead(0));

  //********color palette setup stuff****************
  currentPalette = RainbowColors_p;
  loadPaletteFromFile(settings.palette_ndx, &targetPalette);
  currentBlending = LINEARBLEND;
  //**************************************************

#ifndef REMOTE_DEBUG
  DBG_OUTPUT_PORT.begin(115200);
#endif
  #if defined(ESP32)
  DBG_OUTPUT_PORT.printf("system_get_cpu_freq: %d\n", ESP.getCpuFreqMHz());
  #else
  DBG_OUTPUT_PORT.printf("system_get_cpu_freq: %d\n", system_get_cpu_freq());
  #endif


  #if defined(ESP32) || defined(ESP8266)
  Wire.begin();
  Wire.setClock(100000); // choose 100 kHz I2C rate
  #endif
  if(bme.begin() == false) {
    Serial.println(F("FAIL!"));
  }
  switch(bme.chipModel())
  {
    case BME280::ChipModel_BME280:
      Serial.println("Found BME280 sensor! Success.");
      break;
    case BME280::ChipModel_BMP280:
      Serial.println("Found BMP280 sensor! No Humidity available.");
      break;
    default:
      Serial.println("Found UNKNOWN sensor! Error!");
  }

  bhl.begin(BH1750_CONTINUOUS_HIGH_RES_MODE);

  // set builtin led pin as output
  pinMode(BUILTIN_LED, OUTPUT);
  pinMode(KEY_BUTTON, INPUT);
  // start timer with 500ms because we start in AP mode and try to connect
  ticker = timer.setInterval(500, tick);
 // ***************************************************************************
  // Setup: FASTLED
  // ***************************************************************************
  delay(500);  // 500ms delay for recovery

  // limit my draw to 2.1A at 5v of power draw
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MAX_CURRENT);

  // maximum refresh rate
  FastLED.setMaxRefreshRate(FASTLED_HZ);

  // tell FastLED about the LED strip configuration
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS)
      .setCorrection(TypicalLEDStrip);

  // FastLED.addLeds<LED_TYPE,DATA_PIN,CLK_PIN,COLOR_ORDER>(leds,
  // NUM_LEDS).setCorrection(TypicalLEDStrip);
  // set master brightness control
  FastLED.setBrightness(settings.overall_brightness);

  fill_solid(leds, NUM_LEDS, CRGB(0,0,0));

  FastLED.show();

  // ***************************************************************************
  // Setup: WiFiManager
  // ***************************************************************************
  // Local intialization. Once its business is done, there is no need to keep it
  // around
  WiFiManager wifiManager;
  // reset settings - for testing
  // wifiManager.resetSettings();

  digitalWrite(BUILTIN_LED, HIGH);
  delay(1000);  // delay for checking WIFI reset

  if (!digitalRead(KEY_BUTTON)) {
    DBG_OUTPUT_PORT.println("Key is pressed, reset WIFI config...");
    wifiManager.resetSettings();
    ESP.restart();
  }

  // set callback that gets called when connecting to previous WiFi fails, and
  // enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);

  // fetches ssid and pass and tries to connect
  // if it does not connect it starts an access point with the specified name
  // here  "AutoConnectAP"
  // and goes into a blocking loop awaiting configuration


  if (!wifiManager.autoConnect(hostname)) {
    DBG_OUTPUT_PORT.println("failed to connect and hit timeout");
    // reset and try again, or maybe put it to deep sleep
    #if defined(ESP32)
    ESP.restart();
    #else
    ESP.reset();
    #endif
    delay(1000);
  }

  // if you get here you have connected to the WiFi
  DBG_OUTPUT_PORT.println("connected...yeey :)");
  timer.disable(ticker);
  // keep LED on
  digitalWrite(BUILTIN_LED, LOW);

  // ***************************************************************************
  // Setup: ArduinoOTA
  // ***************************************************************************
  ArduinoOTA.setHostname(hostname);
  ArduinoOTA.onStart([]() {

    // Kill LEDs...
    fill_solid(leds, NUM_LEDS, CRGB(0,0,0));
    FastLED.show();

    String type;
//    if (ArduinoOTA.getCommand() == U_FLASH)
//      type = "sketch";
//    else
//      type = "filesystem";
//
    SPIFFS.end(); // unmount SPIFFS for update.
    // DBG_OUTPUT_PORT.println("Start updating " + type);
    DBG_OUTPUT_PORT.println("Start updating ");
  });
  ArduinoOTA.onEnd([]() {
    DBG_OUTPUT_PORT.println("\nEnd... remounting SPIFFS");
    SPIFFS.begin();
    paletteCount = getPaletteCount();
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    DBG_OUTPUT_PORT.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    DBG_OUTPUT_PORT.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)
      DBG_OUTPUT_PORT.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR)
      DBG_OUTPUT_PORT.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR)
      DBG_OUTPUT_PORT.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR)
      DBG_OUTPUT_PORT.println("Receive Failed");
    else if (error == OTA_END_ERROR)
      DBG_OUTPUT_PORT.println("End Failed");
  });

  ArduinoOTA.begin();
  DBG_OUTPUT_PORT.println("OTA Ready");
  DBG_OUTPUT_PORT.print("IP address: ");
  DBG_OUTPUT_PORT.println(WiFi.localIP());


  // ***************************************************************************
  // Setup: MDNS responder
  // ***************************************************************************
  MDNS.begin(hostname);
  DBG_OUTPUT_PORT.print("Open http://");
  DBG_OUTPUT_PORT.print(hostname);
  DBG_OUTPUT_PORT.println(".local/edit to see the file browser");

  // ***************************************************************************
  // Setup: WebSocket server
  // ***************************************************************************
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  // ***************************************************************************
  // Setup: SPIFFS
  // ***************************************************************************
  #if defined (ESP32)
  if(!SPIFFS.begin(true)){
      DBG_OUTPUT_PORT.println("SPIFFS Mount Failed");
  }
  #else
  SPIFFS.begin();
  #endif
  {
    #if defined (ESP32)
    File root = SPIFFS.open("/");
    File file;
    #else
    Dir dir = SPIFFS.openDir("/");
    #endif
    #if defined (ESP32)
    while (file = root.openNextFile()) {
      String fileName = file.name();
      size_t fileSize = file.size();
      file.close();
    #else
    while (dir.next()) {
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
    #endif
      DBG_OUTPUT_PORT.printf("FS File: %s, size: %s\n", fileName.c_str(),
                             formatBytes(fileSize).c_str());
    }
    DBG_OUTPUT_PORT.printf("\n");
  }

  // ***************************************************************************
  // Setup: SPIFFS Webserver handler
  // ***************************************************************************

  // list directory
  server.on("/list", HTTP_GET, handleFileList);

  // load editor
  server.on("/edit", HTTP_GET, []() {
    if (!handleFileRead("/edit.htm"))
      server.send(404, "text/plain", "FileNotFound");
  });

  // create file
  server.on("/edit", HTTP_PUT, handleFileCreate);

  // delete file
  server.on("/edit", HTTP_DELETE, handleFileDelete);

  // first callback is called after the request has ended with all parsed
  // arguments
  // second callback handles file uploads at that location
  server.on("/edit", HTTP_POST, []() { server.send(200, "text/plain", ""); },
            handleFileUpload);

  // get heap status, analog input value and all GPIO statuses in one json call
  server.on("/esp_status", HTTP_GET, []() {
    String json = "{";
    json += "\"heap\":" + String(ESP.getFreeHeap());
    json += ", \"analog\":" + String(analogRead(A0));
    json += ", \"gpio\":" +
            #if defined(ESP32)
            String((uint32_t)(((GPIO_IN_REG | GPIO_OUT_REG) & 0xFFFF) | ((GPIO_IN1_REG | GPIO_OUT1_REG) << 16)));
            #else
            String((uint32_t)(((GPI | GPO) & 0xFFFF) | ((GP16I & 0x01) << 16)));
            #endif
    json += "}";
    server.send(200, "text/json", json);
    json = String();
  });

  // called when the url is not defined here
  // use it to load content from SPIFFS
  server.onNotFound([]() {
    if (!handleFileRead(server.uri())) handleNotFound();
  });

  server.on("/upload", handleMinimalUpload);

  server.on("/restart", []() {
    DBG_OUTPUT_PORT.printf("/restart:\n");
    server.send(200, "text/plain", "restarting...");
    ESP.restart();
  });

  server.on("/reset_wlan", []() {
    DBG_OUTPUT_PORT.printf("/reset_wlan:\n");
    server.send(200, "text/plain", "Resetting WLAN and restarting...");
    WiFiManager wifiManager;
    wifiManager.resetSettings();
    ESP.restart();
  });

  // ***************************************************************************
  // Setup: SPIFFS Webserver handler
  // ***************************************************************************
  server.on("/set_brightness", []() {
    if (server.arg("c").toInt() > 0) {
      settings.overall_brightness = (int)server.arg("c").toInt() * 2.55;
    } else {
      settings.overall_brightness = server.arg("p").toInt();
    }
    if (settings.overall_brightness > 255) {
      settings.overall_brightness = 255;
    }
    if (settings.overall_brightness < 0) {
      settings.overall_brightness = 0;
    }
    FastLED.setBrightness(settings.overall_brightness);

    if (settings.mode == HOLD) {
      settings.mode = ALL;
    }

    getStatusJSON();
  });

  server.on("/get_brightness", []() {
    String str_brightness = String((int)(settings.overall_brightness / 2.55));
    server.send(200, "text/plain", str_brightness);
    DBG_OUTPUT_PORT.print("/get_brightness: ");
    DBG_OUTPUT_PORT.println(str_brightness);
  });

  server.on("/get_switch", []() {
    server.send(200, "text/plain", (settings.mode == OFF) ? "0" : "1");
    DBG_OUTPUT_PORT.printf("/get_switch: %s\n",
                           (settings.mode == OFF) ? "0" : "1");
  });

  server.on("/get_color", []() {
    String rgbcolor = String(settings.main_color.red, HEX) +
                      String(settings.main_color.green, HEX) +
                      String(settings.main_color.blue, HEX);
    server.send(200, "text/plain", rgbcolor);
    DBG_OUTPUT_PORT.print("/get_color: ");
    DBG_OUTPUT_PORT.println(rgbcolor);
  });

  server.on("/status", []() { getStatusJSON(); });

  server.on("/off", []() {
    //exit_func = true;
    settings.mode = OFF;
    getArgs();
    getStatusJSON();
  });

  server.on("/all", []() {
    //exit_func = true;
    settings.mode = ALL;
    getArgs();
    getStatusJSON();
  });

  server.on("/rainbow", []() {
    //exit_func = true;
    settings.mode = RAINBOW;
    getArgs();
    getStatusJSON();
  });

  server.on("/confetti", []() {
    //exit_func = true;
    settings.mode = CONFETTI;
    getArgs();
    getStatusJSON();
  });

  server.on("/sinelon", []() {
    //exit_func = true;
    settings.mode = SINELON;
    getArgs();
    getStatusJSON();
  });

  server.on("/juggle", []() {
    //exit_func = true;
    settings.mode = JUGGLE;
    getArgs();
    getStatusJSON();
  });

  server.on("/bpm", []() {
    //exit_func = true;
    settings.mode = BPM;
    getArgs();
    getStatusJSON();
  });

  server.on("/ripple", []() {
    //exit_func = true;
    settings.mode = RIPPLE;
    getArgs();
    getStatusJSON();
  });

  server.on("/comet", []() {
    //exit_func = true;
    settings.mode = COMET;
    getArgs();
    getStatusJSON();
  });

  server.on("/wipe", []() {
    settings.mode = WIPE;
    getArgs();
    getStatusJSON();
  });

  server.on("/tv", []() {
    settings.mode = TV;
    getArgs();
    getStatusJSON();
  });

server.on("/fire", []() {
    //exit_func = true;
    settings.mode = FIRE;
    getArgs();
    getStatusJSON();
  });

  server.on("/frainbow", []() {
    //exit_func = true;
    settings.mode = FIRE_RAINBOW;
    getArgs();
    getStatusJSON();
  });

  server.on("/fworks", []() {
    //exit_func = true;
    settings.mode = FIREWORKS;
    getArgs();
    getStatusJSON();
  });

  server.on("/fwsingle", []() {
    //exit_func = true;
    settings.mode = FIREWORKS_SINGLE;
    getArgs();
    getStatusJSON();
  });

  server.on("/fwrainbow", []() {
    //exit_func = true;
    settings.mode = FIREWORKS_RAINBOW;
    getArgs();
    getStatusJSON();
  });

  server.on("/palette_anims", []() {
    settings.mode = PALETTE_ANIMS;
    if (server.arg("p") != "") {
      uint8_t pal = (uint8_t) strtol(server.arg("p").c_str(), NULL, 10);
      if (pal > paletteCount)
        pal = paletteCount;

      settings.palette_ndx = pal;
      loadPaletteFromFile(settings.palette_ndx, &targetPalette);
      currentPalette = targetPalette; //PaletteCollection[settings.palette_ndx];
      DBG_OUTPUT_PORT.printf("Palette is: %d", pal);
    }
    getStatusJSON();
  });

  server.begin();

  paletteCount = getPaletteCount();

  timer.setInterval(5000, Every5Second);

}

static long int avg_time = 0;
static uint32_t avg_cnt = 0;

void loop() {
  EVERY_N_MILLISECONDS(int(float(1000 / settings.fps))) {
    gHue++;  // slowly cycle the "base color" through the rainbow
  }

  timer.run();

  // Simple statemachine that handles the different modes
  switch (settings.mode) {
    default:
    case OFF:
      fill_solid(leds, NUM_LEDS, CRGB(0,0,0));
      break;

    case ALL:
      fill_solid(leds, NUM_LEDS,  CRGB(settings.main_color.red, settings.main_color.green,
                         settings.main_color.blue));
      break;

    case MIXEDSHOW:
      {
        gPatterns[gCurrentPatternNumber]();

        // send the 'leds' array out to the actual LED strip
        int showlength_Millis = settings.show_length * 1000;

        // DBG_OUTPUT_PORT.println("showlengthmillis = " +
        // String(showlength_Millis));
        if (((millis()) - (lastMillis)) >= showlength_Millis) {
          nextPattern();
          DBG_OUTPUT_PORT.println( "void nextPattern was called at " + String(millis()) +
            " and the current show length set to " + String(showlength_Millis));
        }
      }
      break;

    case RAINBOW:
      rainbow();
      break;

    case CONFETTI:
      confetti();
      break;

    case SINELON:
      sinelon();
      break;

    case JUGGLE:
      juggle();
      break;

    case BPM:
      bpm();
      break;

    case PALETTE_ANIMS:
      palette_anims();
      break;

    case RIPPLE:
      ripple();
      break;

    case COMET:
      comet();
      break;

    case THEATERCHASE:
      theaterChase();
      break;

    case WIPE:
      colorWipe();
      break;

    case TV:
      tv();
      break;

    case FIRE:
      fire2012();
      break;

      case FIRE_RAINBOW:
      fire_rainbow();
      break;

    case FIREWORKS:
      fireworks();
      break;

    case FIREWORKS_SINGLE:
      fw_single();
      break;

    case FIREWORKS_RAINBOW:
      fw_rainbow();
      break;
  }

  // Add glitter if necessary
  if (settings.glitter_on == true) {
    addGlitter(settings.glitter_density);
  }

  // Get the current time
  unsigned long continueTime = millis() + int(float(1000 / settings.fps));
  // Do our main loop functions, until we hit our wait time

  do {
    long int now = micros();
    FastLED.show();         // Display whats rendered.
    long int later = micros();
    avg_time += (later-now);
    if (avg_cnt++ >= 500) {
      DBG_OUTPUT_PORT.printf("Average render-time is %ld us\n", avg_time/avg_cnt);
      avg_time = avg_cnt = 0;
    }
    server.handleClient();  // Handle requests to the web server
    webSocket.loop();       // Handle websocket traffic
    ArduinoOTA.handle();    // Handle OTA requests.
#ifdef REMOTE_DEBUG
    Debug.handle();         // Handle telnet server
#endif
    yield();                // Yield for ESP8266 stuff

 if (WiFi.status() != WL_CONNECTED) {
      // Blink the LED quickly to indicate WiFi connection lost.
      ticker = timer.setInterval(100, tick);

      //EVERY_N_MILLISECONDS(1000) {
      //  int state = digitalRead(BUILTIN_LED);  // get the current state of GPIO1 pin
      //  digitalWrite(BUILTIN_LED, !state);
      // }
    } else {
      timer.disable(ticker);
      // Light on-steady indicating WiFi is connected.
      //digitalWrite(BUILTIN_LED, false);
    }

  } while (millis() < continueTime);
}

void nextPattern() {
  // add one to the current pattern number, and wrap around at the end
  //  gCurrentPatternNumber = (gCurrentPatternNumber + random(0,
  //  ARRAY_SIZE(gPatterns))) % ARRAY_SIZE( gPatterns);
  gCurrentPatternNumber = random(0, ARRAY_SIZE(gPatterns));
  lastMillis = millis();
}
