// rocket instrument
#include <BMP388_DEV.h>
#include <Wire.h>
#include "SdFat.h"
#include "SparkFun_Qwiic_KX13X.h"

SdFat SD;
File kxFile;
File bmpFile;
QwiicKX134 kxAccel;
outputData myData;
BMP388_DEV bmp388;
float temperature, pressure, altitude;

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  if (!SD.begin(23)) {
    Serial.println("SD initialization failed!");
    while (1);
  }

  kxFile = SD.open("kx.txt", FILE_WRITE);
  if (!kxFile) {
    Serial.println("error opening kx.txt");
    while (1);
  }
  
  bmpFile = SD.open("bmp.txt", FILE_WRITE);  
  if (!bmpFile) {
    Serial.println("error opening bmp.txt");
    while (1);
  }

  Wire = TwoWire(1);
  const byte PIN_QWIIC_POWER = 18;
  pinMode(PIN_QWIIC_POWER, OUTPUT);
  digitalWrite(PIN_QWIIC_POWER, HIGH);
  Wire.begin();
  //Wire.setClock(100000);
  //Wire.setPullups(1);
  delay(100);

  if ( !kxAccel.begin(KX13X_DEFAULT_ADDRESS, Wire) ) {
    Serial.println("Could not communicate with the the KX134.");
    while (1);
  }

  // ============== ACCELEROMETER ==============

  if ( !kxAccel.initialize(DEFAULT_SETTINGS)) { // Loading default settings.
    Serial.println("Could not initialize the KX134.");
    while (1);
  }

  kxAccel.setRange(KX132_RANGE16G);
  // kxAccel.setRange(KX134_RANGE32G); // For a larger range uncomment

  // ============== BAROMETER ==============

  if (!bmp388.begin(BMP388_I2C_ALT_ADDR)) {
    Serial.println("Could not communicate with bmp388.");
    while(1);
  }
  bmp388.setTimeStandby(TIME_STANDBY_320MS);
  bmp388.startNormalConversion();
}

void loop() {
  // ============== ACCELEROMETER ==============
  // goes at about 200hz, will integrate to find speed.
  
  myData = kxAccel.getAccelData();
  uint32_t us = micros();
  Serial.print(us);
  Serial.print(" ");
  Serial.print("X: ");
  Serial.print(myData.xData, 4);
  Serial.print("g ");
  Serial.print(" Y: ");
  Serial.print(myData.yData, 4);
  Serial.print("g ");
  Serial.print(" Z: ");
  Serial.print(myData.zData, 4);
  Serial.println("g ");
  kxFile.print(us);
  kxFile.print(" ");
  kxFile.print("X: ");
  kxFile.print(myData.xData, 4);
  kxFile.print("g ");
  kxFile.print(" Y: ");
  kxFile.print(myData.yData, 4);
  kxFile.print("g ");
  kxFile.print(" Z: ");
  kxFile.print(myData.zData, 4);
  kxFile.println("g ");

  // ============== BAROMETER ==============
  // can go much more slowly, it's just looking for apogee
  
  if (bmp388.getMeasurements(temperature, pressure, altitude)) {
    us = micros();
    Serial.print(us);
    Serial.print(" ");
    Serial.print(temperature);
    Serial.print(F("*C   "));
    Serial.print(pressure);
    Serial.print(F("hPa   "));
    Serial.print(altitude);
    Serial.println(F("m"));

    bmpFile.print(us);
    bmpFile.print(" ");
    bmpFile.print(temperature);
    bmpFile.print(F("*C   "));
    bmpFile.print(pressure);
    bmpFile.print(F("hPa   "));
    bmpFile.print(altitude);
    bmpFile.println(F("m"));
  }
}
