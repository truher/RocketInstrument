// rocket instrument
#include <BMP388_DEV.h>
#include <Wire.h>
#include "SdFat.h"
#include "SparkFun_Qwiic_KX13X.h"

SdFat SD;
File kxFile;
File bmpFile;
QwiicKX134 kxAccel;
BMP388_DEV bmp388;

bool initialized = false;
const uint8_t blinkPin_OLA = 19;
const byte PIN_QWIIC_POWER = 18;
outputData myData;
float temperature, pressure, altitude;
uint32_t counter = 0;
uint32_t us = 0;

void setup() {
  pinMode(blinkPin_OLA, OUTPUT);

  // ============== SD ==============

  if (!SD.begin(23)) return;
  kxFile = SD.open("kx.txt", FILE_WRITE);
  if (!kxFile) return;
  bmpFile = SD.open("bmp.txt", FILE_WRITE);  
  if (!bmpFile) return;

  // ============== I2C ==============

  Wire = TwoWire(1);
  pinMode(PIN_QWIIC_POWER, OUTPUT);
  digitalWrite(PIN_QWIIC_POWER, HIGH);
  Wire.begin();
  delay(100); // essential to wait for the i2c boards to power up

  // ============== ACCELEROMETER ==============

  if (!kxAccel.begin(KX13X_DEFAULT_ADDRESS, Wire) ) return;
  if ( !kxAccel.initialize(DEFAULT_SETTINGS)) return;
  kxAccel.setRange(KX134_RANGE64G);
  
  // ============== BAROMETER ==============

  if (!bmp388.begin(BMP388_I2C_ALT_ADDR)) return;
  bmp388.setTimeStandby(TIME_STANDBY_320MS);
  bmp388.startNormalConversion();
  initialized = true;
}

void loop() {
  if (!initialized) return;
  counter += 1;
  if (counter > 100) {
    digitalWrite(blinkPin_OLA, HIGH);
    counter = 0;
  }
  if (counter > 50) {
    digitalWrite(blinkPin_OLA, LOW);
  }
  // ============== ACCELEROMETER ==============
  // goes at about 200hz, will integrate to find speed.
  
  myData = kxAccel.getAccelData();
  us = micros();
  kxFile.print(us);
  kxFile.print("\t");
  kxFile.print(myData.xData);
  kxFile.print("\t");
  kxFile.print(myData.yData);
  kxFile.print("\t");
  kxFile.println(myData.zData);
  kxFile.sync();

  // ============== BAROMETER ==============
  // can go much more slowly, it's just looking for apogee
  
  if (bmp388.getMeasurements(temperature, pressure, altitude)) {
    us = micros();
    bmpFile.print(us);
    bmpFile.print("\t");
    bmpFile.print(temperature); // degrees C
    bmpFile.print("\t");
    bmpFile.print(pressure); // hectopascals, i.e. millibar
    bmpFile.print("\t");
    bmpFile.println(altitude); // meters
    bmpFile.sync();
  }
}
