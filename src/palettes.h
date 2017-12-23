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

#if defined (ESP32)
#include "FS.h"
#include "SPIFFS.h"
#endif

int paletteCount = 0;

int getPaletteCount() {
  #if defined(ESP32)
  File root = SPIFFS.open("/palettes");
  #else
  Dir dir = SPIFFS.openDir("/palettes");
  #endif
  int palette_count = 0;
  #if defined(ESP32)
  while (root.openNextFile()) {
  #else
  while (dir.next()) {
  #endif
    palette_count++;
  }
  DBG_OUTPUT_PORT.printf("Palette count: %d\n", palette_count);
  return palette_count;
}

bool openPaletteFileWithIndex(int index, File* file) {
  #if defined(ESP32)
  File root = SPIFFS.open("/palettes");
  #else
  Dir dir = SPIFFS.openDir("/palettes");
  #endif

  int palette_count = 0;
  #if defined(ESP32)
  while (*file = root.openNextFile()) {
  #else
  while (dir.next()) {
  #endif
    if (palette_count == index)
      break;
    file->close();
    palette_count++;
  }

  if (palette_count != index) {
    DBG_OUTPUT_PORT.println("Error, unable to open palette");
    return false;
  }
  #if defined(ESP8266)
  *file = dir.openFile("r");
  #endif
  return true; //success
}

bool loadPaletteFromFile(int index, CRGBPalette16* palette) {
  File paletteFile;
  if (!openPaletteFileWithIndex(index, &paletteFile)) {
      DBG_OUTPUT_PORT.printf("Error loading paletteFile at index %d\n", index);
      return false;
  }

  int paletteFileSize = paletteFile.size();
  uint8_t* bytes = new uint8_t[paletteFileSize];
  if (!bytes) {
    DBG_OUTPUT_PORT.println("Unable to allocate memory for palette");
    return false;
  }

  #if defined(ESP32)
  paletteFile.read(bytes, paletteFileSize);
  #else
  paletteFile.readBytes((char*)bytes, paletteFileSize);
  #endif

  String fileName = paletteFile.name();
  DBG_OUTPUT_PORT.printf("Load palette named %s (%d bytes)\n", fileName.c_str(), paletteFileSize);

  palette->loadDynamicGradientPalette(bytes);

  delete[] bytes;
  return true;
}

String getPaletteNameWithIndex(int index) {
  #if defined(ESP32)
  File root = SPIFFS.open("/palettes");
  #else
  Dir dir = SPIFFS.openDir("/palettes");
  #endif

  int ndx = 0;
  #if defined(ESP32)
  File file;
  while (file = root.openNextFile()) {
  #else
  while (dir.next()) {
  #endif
    if (ndx == index)
      #if defined(ESP32)
      return file.name();
      #else
      return dir.fileName();
      #endif
    ndx++;
  }
  return "[unknown]";
}
