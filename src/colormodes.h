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

// ***************************************************************************
// Color modes
// ***************************************************************************
//#include "definitions.h"

void listStatusJSON();

extern WebSocketsServer webSocket;

#define DC_OFFSET  0                                         // DC offset in mic signal - if unusure, leave 0
                                                             // I calculated this value by serialprintln lots of mic values
#define NOISE     30                                         // Noise/hum/interference in mic signal and increased value until it went quiet
#define SAMPLES   10                                         // Length of buffer for dynamic level adjustment
#define TOP (NUM_LEDS + 2)                                   // Allow dot to go slightly off scale
#define PEAK_FALL 5                                          // Rate of peak falling dot
#define DAMPING   6
uint8_t
  peak      = 0,                                              // Used for falling dot
  dotCount  = 0,                                              // Frame counter for delaying dot-falling speed
  volCount  = 0;                                              // Frame counter for storing past volume data

int32_t
  vol[SAMPLES],                                               // Collection of prior volume samples
  lvl       = 10,                                             // Current "dampened" audio level
  minLvlAvg = 0,                                              // For dynamic adjustment of graph low & high
  maxLvlAvg = 512;

void soundsense() {

  uint16_t i;
  uint16_t minLvl, maxLvl;
  int32_t      n, height;

  n = analogRead(AUDIO_PIN);                                  // Raw reading from mic
//DBG_OUTPUT_PORT.printf("raw: %04d", n);
  n = abs(n - 2048 - DC_OFFSET);                               // Center on zero
//DBG_OUTPUT_PORT.printf(" center: %04d", n);

  n = (n <= NOISE) ? 0 : (n - NOISE);                         // Remove noise/hum
  lvl = ((lvl * DAMPING) + n) / (DAMPING+1);                  // "Dampened" reading (else looks twitchy)

  // Calculate bar height based on dynamic min/max levels (fixed point):
  height = TOP * (lvl - minLvlAvg) / (int32_t) (maxLvlAvg - minLvlAvg);

  if (height < 0)
    height = 0; // Clip output
  else if (height > TOP)
    height = TOP;

  if (height > peak)
    peak = height; // Keep 'peak' dot at top

  //DBG_OUTPUT_PORT.printf(" height: %04d", height);

  // Color pixels based on rainbow gradient
  for (i = 0; i < NUM_LEDS; i++) {
    if (i >= height)
      leds[i].setRGB(0, 0, 0);
    else
      leds[i] = CHSV(map(i,0,NUM_LEDS-1,30,150), 255, 255);
  }

  // Draw peak dot
  if (peak > 0 && peak <= NUM_LEDS-1) {
//    DBG_OUTPUT_PORT.printf(" peak: %04d\n", peak);
    leds[peak] = CHSV(map(peak,0,NUM_LEDS-1,30,150), 255, 255);
  }

  // Every few frames, make the peak pixel drop by 1:
  if (++dotCount >= PEAK_FALL) {      // fall rate
    if (peak > 0)
      peak--;
    dotCount = 0;
  }

  vol[volCount] = n; // Save sample for dynamic leveling
  if (++volCount >= SAMPLES)
    volCount = 0; // Advance/rollover sample counter

  // Get volume range of prior frames
  minLvl = maxLvl = vol[0];
  for (i=1; i<SAMPLES; i++) {
    if (vol[i] < minLvl)
      minLvl = vol[i];
    else if (vol[i] > maxLvl)
      maxLvl = vol[i];
  }
  // minLvl and maxLvl indicate the volume range over prior frames, used
  // for vertically scaling the output graph (so it looks interesting
  // regardless of volume level).  If they're too close together though
  // (e.g. at very low volume levels) the graph becomes super coarse
  // and 'jumpy'...so keep some minimum distance between them (this
  // also lets the graph go to zero when no sound is playing):
  if ((maxLvl - minLvl) < TOP)
    maxLvl = minLvl + TOP;

  minLvlAvg = (minLvlAvg * 63 + minLvl) >> 6;                 // Dampen min/max levels
  maxLvlAvg = (maxLvlAvg * 63 + maxLvl) >> 6;                 // (fake rolling average)
}

// These functions originally displayed the color using a call to FastLed.show()
// This has been refactored out, theser functions now simply render into the
// leds[] array. The FastLed.show() call happens in the main loop now.
// Furthermore, the 'add glitter' option also refactored out to the main loop.

void addGlitter(fract8 chanceOfGlitter) {
  if (random8() < chanceOfGlitter) {
    leds[random16(settings.num_leds)] +=
        CRGB(settings.glitter_color.red, settings.glitter_color.green,
             settings.glitter_color.blue);
  }
}

void rainbow() {
  // FastLED's built-in rainbow generator
  fill_rainbow(leds, settings.num_leds, gHue, 7);

  // if (settings.glitter_on == true){addGlitter(settings.glitter_density);}
  // frame has been created, now show it
  // FastLED.show();
  // insert a delay to keep the framerate modest
  // FastLED.delay(int(float(500/settings.fps)));
}

void confetti() {
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy(leds, settings.num_leds, settings.ftb_speed);
  for (int x=0; x<settings.confetti_dens; x++) {
    int pos = random16(settings.num_leds);
    leds[pos] += CHSV(gHue + random8(64), 200, settings.effect_brightness);
  }
  // if (settings.glitter_on == true){addGlitter(settings.glitter_density);}
  // frame has been created, now show it
  // FastLED.show();
  // insert a delay to keep the framerate modest
  // FastLED.delay(int(float(1000/settings.fps)));
}

void sinelon() {
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy(leds, settings.num_leds, settings.ftb_speed);
  int pos = beatsin16(13, 0, settings.num_leds);
  leds[pos] += CHSV(gHue, 255, settings.effect_brightness);
  // if (settings.glitter_on == true){addGlitter(settings.glitter_density);}
  // frame has been created, now show it
  // FastLED.show();
  // insert a delay to keep the framerate modest
  // FastLED.delay(int(float(1000/settings.fps)));
}

void bpm() {
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 62;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8(BeatsPerMinute, 64, settings.effect_brightness);
  for (int i = 0; i < settings.num_leds; i++) {  // 9948
    leds[i] = ColorFromPalette(palette, gHue + (i * 2), beat - gHue + (i * 10));
  }
  // if (settings.glitter_on == true){addGlitter(settings.glitter_density);}

  // frame has been created, now show it
  // FastLED.show();
  // insert a delay to keep the framerate modest
  // FastLED.delay(int(float(1000/settings.fps)));
}

void juggle() {
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy(leds, settings.num_leds, settings.ftb_speed);
  int dothue = 0;
  for (int i = 0; i < 8; i++) {
    leds[beatsin16(i + 7, 0, settings.num_leds)] |=
        CHSV(dothue, 200, settings.effect_brightness);
    dothue += 32;
  }
  // if (settings.glitter_on == true){addGlitter(settings.glitter_density);}

  // frame has been created, now show it
  // FastLED.show();
  // insert a delay to keep the framerate modest
  // FastLED.delay(int(float(1000/settings.fps)));
}
//******************************************************************************************
//                     PALETTE ANIMATION FUNCTIONS
//******************************************************************************************
int wipeInProgress = 0;

void FillLEDsFromPaletteColors(CRGBPalette16 palette, uint8_t paletteStartIndex, uint16_t endingLEDIndex=0xFFFF) {
  uint8_t colorIndex = paletteStartIndex;

  for (int i = 0; i < settings.num_leds; i++) {
    if (i > endingLEDIndex) return;  //stop condition

    // leds[i] = ColorFromPalette( currentPalette, colorIndex + sin8(i*16),
    // brightness);
    leds[i] = ColorFromPalette(palette, colorIndex,
                               settings.effect_brightness);
    if (anim_direction == FORWARD) {
      colorIndex += 3;
    }
    if (anim_direction == BACK) {
      colorIndex -= 3;
    }
  }
}

void ChangePalettePeriodically(bool forceNow) {
  if (forceNow || millis() - paletteMillis > (settings.show_length * 1000)) {
    paletteMillis = millis();

    targetPaletteIndex = random(0, paletteCount);

    currentPalette = targetPalette;

    anim_direction = (DIRECTION)!anim_direction; // DIRECTION enum allows flipping by boolean not.

    loadPaletteFromFile(targetPaletteIndex, &targetPalette);

    DBG_OUTPUT_PORT.printf("New palette index: %d\n", targetPaletteIndex);

    if (settings.glitter_wipe_on) {
       DBG_OUTPUT_PORT.println("Begin glitter wipe");
       wipeInProgress = true;
    }
  }
}

void colorWipe() {
  static CRGB prevColor = CHSV(gHue, 255, settings.effect_brightness);
  static CRGB currentColor = CHSV(gHue+60, 255, settings.effect_brightness);

  // Wrap around if necessary
  if (wipePos >= settings.num_leds) {
    wipePos = 0;
    prevColor = currentColor;
    gHue += 60;
    currentColor = CHSV(gHue, 255, settings.effect_brightness);
  }

  // Render the first half of the wipe
  for (int x=0; x<wipePos; x++) {
    leds[x] = currentColor;
  }
  // Render the second half
  for (int x=wipePos; x<settings.num_leds; x++) {
    leds[x] = prevColor;
  }

  //Render the glitter at the intersection
  if (settings.glitter_wipe_on) {
    for (int x=0; x < 3; x++) {
      int speckle = wipePos + random(-SPARKLE_SPREAD,SPARKLE_SPREAD);
      if (speckle >= 0 && speckle < settings.num_leds) {
          leds[speckle] +=  CRGB(settings.glitter_color.red, settings.glitter_color.green,
               settings.glitter_color.blue);
      }
    }
  }

  // Advance for next frame
  wipePos+=WIPE_SPEED;
}

void palette_anims() {
  currentBlending = LINEARBLEND;

  if (settings.palette_ndx == -1) ChangePalettePeriodically(false);

  if (!settings.glitter_wipe_on) {
    uint8_t maxChanges = int(float(settings.fps / 2));
    nblendPaletteTowardPalette(currentPalette, targetPalette, maxChanges);

    // Update the current palette if necessary-- and send to any connected clients.
    if (currentPaletteIndex != targetPaletteIndex) {
      currentPaletteIndex = targetPaletteIndex;

      // Send current palette name to the UI.
      String name = getPaletteNameWithIndex(currentPaletteIndex);
      webSocket.broadcastTXT("p"+name);
    }
  }

  static uint8_t startIndex = 0;

  /* motion speed */
  startIndex = startIndex + 3;

  FillLEDsFromPaletteColors(currentPalette,startIndex);

  if (settings.glitter_wipe_on && wipeInProgress) {
    if (wipePos >= settings.num_leds) {
      DBG_OUTPUT_PORT.println("End glitter wipe");
      wipeInProgress = false;
      wipePos = 0;
      currentPalette = targetPalette;
      currentPaletteIndex = targetPaletteIndex;

      // Send current palette name to the UI.
      String name = getPaletteNameWithIndex(currentPaletteIndex);
      webSocket.broadcastTXT("p"+name);
      FillLEDsFromPaletteColors(targetPalette,startIndex);
    } else {
      FillLEDsFromPaletteColors(targetPalette,startIndex, wipePos);
      for (int x=0; x < 3; x++) {
        int speckle = wipePos + random(-SPARKLE_SPREAD,SPARKLE_SPREAD);
        if (speckle >= 0 && speckle < settings.num_leds) {
            leds[speckle] +=  CRGB(settings.glitter_color.red, settings.glitter_color.green,
                 settings.glitter_color.blue);
        }
      }
      wipePos+=WIPE_SPEED;
    }
  }


}

//*****************LED RIPPLE*****************************************************

void one_color_allHSV(int ahue,
                      int abright) {  // SET ALL LEDS TO ONE COLOR (HSV)
  for (int i = 0; i < settings.num_leds; i++) {
    leds[i] = CHSV(ahue, 255, abright);
  }
}

int wrap(int step) {
  if (step < 0) return settings.num_leds + step;
  if (step > settings.num_leds - 1) return step - settings.num_leds;
  return step;
}

void ripple() {
  if (currentBg == nextBg) {
    nextBg = random(256);
  } else if (nextBg > currentBg) {
    currentBg++;
  } else {
    currentBg--;
  }
  for (uint16_t l = 0; l < settings.num_leds; l++) {
    leds[l] = CHSV(currentBg, 255,
                   settings.effect_brightness);  // strip.setPixelColor(l,
                                                 // Wheel(currentBg, 0.1));
  }

  if (step == -1) {
    center = random(settings.num_leds);
    color = random(256);
    step = 0;
  }

  if (step == 0) {
    leds[center] = CHSV(
        color, 255, settings.effect_brightness);  // strip.setPixelColor(center,
                                                  // Wheel(color, 1));
    step++;
  } else {
    if (step < maxSteps) {
      //Serial.println(pow(fadeRate, step));

      leds[wrap(center + step)] =
          CHSV(color, 255,
               pow(fadeRate, step) * 255);  //   strip.setPixelColor(wrap(center
                                            //   + step), Wheel(color,
                                            //   pow(fadeRate, step)));
      leds[wrap(center - step)] =
          CHSV(color, 255,
               pow(fadeRate, step) * 255);  //   strip.setPixelColor(wrap(center
                                            //   - step), Wheel(color,
                                            //   pow(fadeRate, step)));
      if (step > 3) {
        leds[wrap(center + step - 3)] =
            CHSV(color, 255, pow(fadeRate, step - 2) *
                                 255);  //   strip.setPixelColor(wrap(center +
                                        //   step - 3), Wheel(color,
                                        //   pow(fadeRate, step - 2)));
        leds[wrap(center - step + 3)] =
            CHSV(color, 255, pow(fadeRate, step - 2) *
                                 255);  //   strip.setPixelColor(wrap(center -
                                        //   step + 3), Wheel(color,
                                        //   pow(fadeRate, step - 2)));
      }
      step++;
    } else {
      step = -1;
    }
  }
  // if (settings.glitter_on == true){addGlitter(settings.glitter_density);}

  // frame has been created, now show it
  // FastLED.show();
  // insert a delay to keep the framerate modest
  // FastLED.delay(int(float(1000/settings.fps)));
}

//***************************END LED
//RIPPLE*****************************************************

void comet() {
  fadeToBlackBy(leds, settings.num_leds, settings.ftb_speed);
  lead_dot = beatsin16(int(float(settings.fps / 3)), 0, settings.num_leds);
  leds[lead_dot] = CHSV(dothue, 200, 255);
  dothue += 8;
  // if (settings.glitter_on == true){addGlitter(settings.glitter_density);}
  // FastLED.show();
}

// Theatre-style crawling lights.
void theaterChase() {
  static int8_t frame = 0;

  // turn off the previous frame's led
  for (int i = 0; i < settings.num_leds; i = i + 3) {
    if (i + frame < settings.num_leds) {
      leds[i + frame] = CRGB(0, 0, 0);  // turn every third pixel off
    }
  }

  // advance the frame
  frame++;
  if (frame > 2) frame = 0;

  // turn on the current frame's leds
  for (int i = 0; i < settings.num_leds; i = i + 3) {
    if (i + frame < settings.num_leds) {
      leds[i + frame] =
          CRGB(settings.main_color.red, settings.main_color.green,
               settings.main_color.blue);  // turn every third pixel on
    }
  }
}


//***********TV
int dipInterval = 10;
int darkTime = 250;
unsigned long currentDipTime;
unsigned long dipStartTime;
unsigned long currentMillis;
int ledState = LOW;
long previousMillis = 0;
int ledBrightness[NUM_LEDS];
uint16_t ledHue[NUM_LEDS];
int led = 5;
int interval = 2000;
int twitch = 50;
int dipCount = 0;
int analogLevel = 100;
boolean timeToDip = false;

CRGB hsb2rgbAN1(uint16_t index, uint8_t sat, uint8_t bright) {
    // Source: https://blog.adafruit.com/2012/03/14/constant-brightness-hsb-to-rgb-algorithm/
    uint8_t temp[5], n = (index >> 8) % 3;
    temp[0] = temp[3] = (uint8_t)((                                        (sat ^ 255)  * bright) / 255);
    temp[1] = temp[4] = (uint8_t)((((( (index & 255)        * sat) / 255) + (sat ^ 255)) * bright) / 255);
    temp[2] =          (uint8_t)(((((((index & 255) ^ 255) * sat) / 255) + (sat ^ 255)) * bright) / 255);

    return CRGB(temp[n + 2], temp[n + 1], temp[n]);
}

void _tvUpdateLed (int led, int brightness) {
  ledBrightness[led] = brightness;
  for (int i=0; i<settings.num_leds; i++) {
    uint16_t index = (i%3 == 0) ? 400 : random(0,767);
    ledHue[led] = index;
  }
}

// See: http://forum.mysensors.org/topic/85/phoneytv-for-vera-is-here/13
void tv() {
  if (timeToDip == false) {
    currentMillis = millis();
    if (currentMillis-previousMillis > interval)  {
      previousMillis = currentMillis;
      interval = random(750,4001);//Adjusts the interval for more/less frequent random light changes
      twitch = random(40,100);// Twitch provides motion effect but can be a bit much if too high
      dipCount++;
    }
    if (currentMillis-previousMillis<twitch) {
      led=random(0, settings.num_leds-1);
      analogLevel=random(50,255);// set the range of the 3 pwm leds
      ledState = ledState == LOW ? HIGH: LOW; // if the LED is off turn it on and vice-versa:


      _tvUpdateLed(led, (ledState) ? 255 : 0);

      if (dipCount > dipInterval) {
        //DBG_OUTPUT_PORT.println("dip");
        timeToDip = true;
        dipCount = 0;
        dipStartTime = millis();
        darkTime = random(50,150);
        dipInterval = random(5,250);// cycles of flicker
      }
    }
  } else {
    //DBG_OUTPUT_PORT.println("Dip Time");
    currentDipTime = millis();
    if (currentDipTime - dipStartTime < darkTime) {
      for (int i=3; i<settings.num_leds; i++) {
        _tvUpdateLed(i, 0);
      }
    } else {
      timeToDip = false;
    }
  }

  // Render the thing, with a little flicker
  uint8_t flicker = 255;
  int sat = 200;

  EVERY_N_MILLISECONDS(150) {
    flicker = random(220,255);
    sat = random(180, 220);
  }
  
  for (int i=0; i<settings.num_leds; i++) {
    uint16_t index = (i%3 == 0) ? 400 : random(0,767);
    //leds[i] = ((index >> 8) % 3, 200, ledBrightness[i]);

    leds[i] = hsb2rgbAN1(ledHue[i], sat, ledBrightness[i]).nscale8_video(flicker);
  }
}

// Fire2012 by Mark Kriegsman, July 2012
// as part of "Five Elements" shown here: http://youtu.be/knWiGsmgycY
////
// This basic one-dimensional 'fire' simulation works roughly as follows:
// There's a underlying array of 'heat' cells, that model the temperature
// at each point along the line.  Every cycle through the simulation,
// four steps are performed:
//  1) All cells cool down a little bit, losing heat to the air
//  2) The heat from each cell drifts 'up' and diffuses a little
//  3) Sometimes randomly new 'sparks' of heat are added at the bottom
//  4) The heat from each cell is rendered as a color into the leds array
//     The heat-to-color mapping uses a black-body radiation approximation.
//
// Temperature is in arbitrary units from 0 (cold black) to 255 (white hot).
//
// This simulation scales it self a bit depending on settings.num_leds; it should look
// "OK" on anywhere from 20 to 100 LEDs without too much tweaking. 
//
// I recommend running this simulation at anywhere from 30-100 frames per second,
// meaning an interframe delay of about 10-35 milliseconds.
//
// Looks best on a high-density LED setup (60+ pixels/meter).
//
//
// There are two main parameters you can play with to control the look and
// feel of your fire: COOLING (used in step 1 above), and SPARKING (used
// in step 3 above).
//
// COOLING: How much does the air cool as it rises?
// Less cooling = taller flames.  More cooling = shorter flames.
// Default 55, suggested range 20-100
#define COOLING  80

// SPARKING: What chance (out of 255) is there that a new spark will be lit?
// Higher chance = more roaring fire.  Lower chance = more flickery fire.
// Default 120, suggested range 50-200.
#define SPARKING 60

bool gReverseDirection = false;

boolean _firerainbow = false; // used for rainbow mode


void fire2012()
{

// Array of temperature readings at each simulation cell
  static byte heat[NUM_LEDS];
  static byte heat2[NUM_LEDS];

  // Step 1.  Cool down every cell a little
    for( int i = 0; i < settings.num_leds; i++) {
      //heat[i] = qsub8( heat[i],  random8(0, ((COOLING * 10) / settings.num_leds) + 2)); // original with COOLING
      heat[i] = qsub8( heat[i],  random8(0, ((settings.ftb_speed * 20) / settings.num_leds) + 2)); // modified with FTBspeed
      heat2[i] = qsub8( heat2[i],  random8(0, ((settings.ftb_speed * 20) / settings.num_leds) + 2)); // modified with FTBspeed
    }

    // Step 2.  Heat from each cell drifts 'up' and diffuses a little
    for( int k= settings.num_leds - 1; k >= 2; k--) {
      
      heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2] ) / 3;
      heat2[k] = (heat2[k - 1] + heat2[k - 2] + heat2[k - 2] ) / 3;

    }

    // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
    // if( random8() < SPARKING ) { // Original with SPARKING
    if( random8() < settings.show_length ) { // Modified with show_length
      int y = random8(7);
      int z = random8(7);
      heat[y] = qadd8( heat[y], random8(160,255) );
      heat2[z] = qadd8( heat2[z], random8(160,255) );
    }

    // Step 4.  Map from heat cells to LED colors
    for( int j = 0; j < (settings.num_leds / 2); j++) {
      CRGB color = HeatColor( heat[j]);
      int pixelnumber;
      if( gReverseDirection ) {
        pixelnumber = (settings.num_leds-1) - j;
      } else {
        pixelnumber = j;
      }
      leds[pixelnumber] = color;
    }

    // Step 4.  Map from heat cells to LED colors
    for( int j = 0; j < (settings.num_leds / 2); j++) {
      CRGB color = HeatColor( heat2[j]);
      int pixelnumber;
      pixelnumber = (settings.num_leds-1) - j;
      leds[pixelnumber] = color;
    }

  if (_firerainbow) {
    for( int j = 0; j < (settings.num_leds); j++) {
      leds[j] += CHSV(gHue, 255, settings.effect_brightness); // set effect brightness
    }
    _firerainbow = false;
  }

////frame has been created, now show it
//  FastLED.show();
//  // insert a delay to keep the framerate modest
//  FastLED.delay(int(float(1000/FPS)));

}

void fire_rainbow() {
  _firerainbow = true;
  fire2012();

}


// Fireworks from WS2812FX

boolean _singlecolor = false; // used for single color mode
boolean _rainbow = false; // used for rainbow mode

void fireworks() {



// fadeToBlackBy( leds, settings.num_leds, ftb_speed);


  uint32_t px_rgb = 0;
  byte px_r = 0;
  byte px_g = 0;
  byte px_b = 0;
  byte px_boost = 200;

  for(uint16_t i=0; i < settings.num_leds; i++) {



    //leds[i] /= 2; // fade out (divide by 2)
    leds[i].nscale8(130 - int(float(settings.ftb_speed*0.5)));
    //leds[i].fadeToBlackBy(ftb_speed);

  }

  // first LED has only one neighbour
  leds[0].r = (leds[1].r >> 1) + leds[0].r;
  leds[0].g = (leds[1].g >> 1) + leds[0].g;
  leds[0].b = (leds[1].b >> 1) + leds[0].b;
  //leds[0].setRGB(px_r, px_g, px_b);

// set brightness(i) = ((brightness(i-1)/2 + brightness(i+1)) / 2) + brightness(i)
    for(uint16_t i=1; i < settings.num_leds-1; i++) {
    leds[i].r = ((
            (leds[i-1].r  >> 1) +
            leds[i+1].r ) >> 1) +
            leds[i].r;

    leds[i].g = ((
            (leds[i-1].g >> 1) +
            leds[i+1].g ) >> 1) +
            leds[i].g;

    leds[i].b = ((
            (leds[i-1].b   >> 1) +
            leds[i+1].b ) >> 1) +
            leds[i].b;
  }

  // last LED has only one neighbour
  leds[settings.num_leds-1].r = ((leds[settings.num_leds-2].r >> 2) + leds[settings.num_leds-1].r);
  leds[settings.num_leds-1].g = ((leds[settings.num_leds-2].g >> 2) + leds[settings.num_leds-1].g);
  leds[settings.num_leds-1].b = ((leds[settings.num_leds-2].b >> 2) + leds[settings.num_leds-1].b);

px_r = random8();
px_g = random8();
px_b = random8();


  
    for(uint16_t i=0; i<_max(1,settings.num_leds/20); i++) {
      if(random8(settings.show_length + 4) == 0) {
        //Adafruit_NeoPixel::setPixelColor(random(_led_count), _mode_color);
        byte pixel = random(settings.num_leds);
        if(_singlecolor){
          leds[pixel] = CRGB(settings.main_color.red,settings.main_color.green,settings.main_color.blue); // tails are in single color from set color on web interface
        } else if(_rainbow) {
          leds[pixel] = CHSV( gHue, 255, settings.effect_brightness); // Rainbow cycling color
        } else if(!_singlecolor && !_rainbow) {
          leds[pixel].setRGB(px_r, px_g, px_b); // Multicolored tale

        }
        leds[pixel].maximizeBrightness();



      }
    }

    _singlecolor = false;
    _rainbow = false;

//  if (GLITTER_ON == true){addGlitter(glitter_density);}
// //frame has been created, now show it
  FastLED.show();
  // insert a delay to keep the framerate modest
//  FastLED.delay(int(float(1000/FPS)));
}



void fw_single() {
  _singlecolor = true;
  fireworks();
}

void fw_rainbow() {
  _rainbow = true;
  fireworks();
}

// END


//*******************************ARRAY OF SHOW ANIMATIONS FOR MIXED SHOW
//MODE***********************
typedef void (*SimplePatternList[])();
SimplePatternList gPatterns = {
//  soundsense,
  rainbow,
  confetti,
  sinelon,
  juggle,
  bpm,
  fire2012,
  fireworks,
  theaterChase,
  palette_anims,
  ripple,
  colorWipe,
  tv,
  comet
  };
//**************************************************************************************************
