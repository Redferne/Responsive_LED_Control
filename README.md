# doctormord's Responsive Led Control (for ESP8266/ESP32)
doctormord mixed the work of [McLighting](https://github.com/toblum/McLighting), [Russell](https://github.com/russp81/LEDLAMP_FASTLEDs) and [Jake's "Grisworld"](https://github.com/jake-b/Griswold-LED-Controller) with [FastLED](https://github.com/FastLED/FastLED) (FastLED library 3.1.3 as of this writing), the colorjs colorpicker, color spectrums created via FastLED Palette Knife, and some additional strip animations (included in the Arduino Sketch above).

Credit goes to the following pioneers (including but not limited to the required libraries):

* McLighting library:
https://github.com/toblum/McLighting
* Russel's implementation:
https://github.com/russp81/LEDLAMP_FASTLEDs
* Jakes's "Grisworld" Led Controller
https://github.com/jake-b/Griswold-LED-Controller
* jscolor Color Picker:
http://jscolor.com/
* FastLED Palette Knife:
http://fastled.io/tools/paletteknife/

These libraries are required for doctormord's Responsive Led Control.
Please make sure they are installed in the Arduino libraries folder.

* FastLED 3.001.006 library
https://github.com/FastLED/FastLED
For ESP32 RMT support, pull this https://github.com/FastLED/FastLED/pull/522 (unless it's already merged)
* RemoteDebug
https://github.com/JoaoLopesF/RemoteDebug
* SimpleTimer
https://github.com/schinken/SimpleTimer.git
* WifiManager
https://github.com/bbx10/WiFiManager.git (esp32 branch if needed)
* DNSServer (ESP32)
https://github.com/zhouhan0126/DNSServer---esp32.git
* Arduino Websockets
https://github.com/Links2004/arduinoWebSockets (esp32 branch if needed)

Setup your ESP, see the readme on McLighting's git. Or there are many guides available online.

In short:

1.  Configure the Arduino IDE / PlatformIO to communicate with the ESP
2.  Build & Upload the code (from this repo) The default setup is for a 948! pixel WS2813 GRB LED Strip.   
    (change the applicable options in "definitions.h" to suit the specific implementation first!)
3.  Patch FastLED Library

```arduino
// A FastLED patch is needed in order to use this.
// Saves more than 3k given the palettes
//
// Simply edit <fastled_progmem.h> and update the include (Line ~29):

#if FASTLED_INCLUDE_PGMSPACE == 1
#if (defined(__AVR__))
#include <avr\pgmspace.h>
#else
#include <pgmspace.h>
#endif
#endif
```

4.  On first launch, the ESP will advertise it's own WiFi network (AP Mode) with the prefixed hostname in definitions.h *Please note that during first boot on ESP32 the SPIFFS filesystem is created and formatted, this can take a few minutes* Connect to this WiFi network and click here -> http://192.168.4.1 to access the ESP WifiManager. Configure the ESP to login to the nearest available WiFi network. 
5.  Once the ESP (as Station) is connected to the WiFi network, upload the required files for the web interface by typing the in IP address of the ESP followed by "/edit" (i.e. http://192.168.1.20/edit).  Then upload the files from the folder **upload_to_ESP_SPIFFS_or_SD**. The palettes directory is created by the entering "/palettes" and clicking on **Create**
6.  Now the ESP is ready for action. Enter it's IP the browser (i.e. http://192.168.1.20) and configure away ;-)

Forked from Russel, Adafruit Neopixel references and library calls are removed.

# Improvements/changes so far:

* new effect: Fire (from WS2812FX)
* new effect: RainbowFire
* new effect: Fireworks [single color, rainbow, random] (from McLightning, ported to used FastLED instead off Adafruit Neopixel)
* new settings for effects in webinterface *.htm

* speedup the UI alot by pulling the materialize stuff (.css/.js) from server and using .gz compressed files for the rest
* made the UI more responsive with grouped sections and buttons
* added some more palettes
* integrated Arduino OTA
* ESP32 support, with buttery smooth animations using Sam Guyer and Thomas Basler RMT implementation for FastLED
Large Screen (Desktop)

![Large Screen](https://github.com/doctormord/Responsive_LED_Control/raw/master/documentation/large50.png)

Small Screen (Mobile)

![Small Screen](https://github.com/doctormord/Responsive_LED_Control/raw/master/documentation/small50.png)

For reference, interrupts issue:  https://github.com/FastLED/FastLED/issues/306

ESP32 RMT implementation: https://github.com/FastLED/FastLED/issues/504
# License

As per the original [McLighting](https://github.com/toblum/McLighting) and [Jake's "Grisworld"](https://github.com/jake-b/Griswold-LED-Controller) project, this project is released under the GNU LESSER GENERAL PUBLIC LICENSE Version 3, 29 June 2007.

	Griswold is free software: you can redistribute it and/or modify
	it under the terms of the GNU Lesser General Public License as 
	published by the Free Software Foundation, either version 3 of 
	the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU Lesser General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.


# Portions of @jake-b "Griswold" LED controller original README

I bought 1000 WS2811 nodes for my outdoor Christmas light installation this year.
Based on the "Russell's FASTLEDs" project by @russp81, which is in turn based on the "McLighting" project by @toblum

It seemed necessary to name the thing after Clark Griswold, but really just to differentiate this fork from the originals.

@russp81 mixed the work of @toblum with the @FastLED (FastLED library 3.1.3 as of this writing), the colorjs colorpicker, color spectrums created via FastLED Palette Knife, and some additional strip animations.

# Improvements

- Palettes stored as binary files on SPIFFS.  See below for more information on this.
- Display name of the current palette file in the web interface.
- Added ArduinoOTA support so I can update the firmware over WiFi, which will be important when its installed outside.
- Added the ability to store the settings in EEPROM and restore on boot.
- Merged the jscolor interface into the original McLighting interface
- Updated the McLighting interface to retrieve the current settings from the device, and update the UI with the current settings, rather than always default to the defaults.
- General code formatting clean-up.
- Added “RemoteDebug” library for serial console over telnet. (Optional #define)
- Fixed divide-by-zero error that occurs when fps=0 by preventing fps=0 from the UI.
- Updates to the “animated palette” function, now you can select a single palette, or the existing randomized palette after time delay.
- Rearchitected things a bit, now the colormodes.h functions render one single frame, and do not block the main thread.
- Added back the wipe and tv animations from the original McLighting project (removed in LEDLAMP_FASTLEDs)
- Modified TV animation to add some flicker (I like it better)
- Added “effect brightness” setting to allow you to dim the main effect independently of glitter.

# Palettes on SPIFFS

Normally, you use [PaletteKnife](http://fastled.io/tools/paletteknife/) to generate arrays with the palette info.  You then compile this data into your project.  I wanted to be able to update the palettes without recompiling, so I moved them to files in SPIFFS (/palettes directory).  There is a little python program that basically takes the logic from PaletteKnife and outputs a binary file with the palette data instead.  Load these binary files to SPIFFS using the [Arduino ESP8266 filesystem uploader](https://github.com/esp8266/arduino-esp8266fs-plugin) or manually.
