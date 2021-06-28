// rocket instrument
#include "BMP388_DEV.h"
#include "SdFat.h"
#include "SparkFun_Qwiic_KX13X.h"
#include "Wire.h"

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
  digitalWrite(blinkPin_OLA, HIGH);
  delay(1000);
  digitalWrite(blinkPin_OLA, LOW);

  Serial.begin(115200);
  Serial.println("setup");

  // ============== I2C ==============

  Wire = TwoWire(1);
  pinMode(PIN_QWIIC_POWER, OUTPUT);
  digitalWrite(PIN_QWIIC_POWER, HIGH);
  Wire.begin();
  Wire.setClock(1000000);  // fast mode plus seems to be the KX max speed
  Wire.setPullups(1); // this is what the OLA code uses
  delay(1000); // essential to wait for the i2c boards to power up
  Serial.println("i2c done");

  // ============== SD ==============

  if (!SD.begin(23)) {
    Serial.println("sd file fail");
    return;
  }
  kxFile = SD.open("kx.txt", FILE_WRITE);
  if (!kxFile) {
    Serial.println("kx file fail");
    return;
  }
  bmpFile = SD.open("bmp.txt", FILE_WRITE);
  if (!bmpFile) {
    Serial.println("bmp file fail");
    return;
  }
  Serial.println("sd done");

  // ============== ACCELEROMETER ==============

  if (!kxAccel.begin(KX13X_DEFAULT_ADDRESS, Wire)) {
    Serial.println("kx begin fail");
    return;
  }
  if (!kxAccel.initialize(DEFAULT_SETTINGS)) {
    Serial.println("kx init fail");
    return;
  }
  if (!kxAccel.setRange(KX134_RANGE64G)) {
    Serial.println("kx range fail");
    return;
  }

  KX13X_STATUS_t returnError;
  returnError = kxAccel.writeRegister(KX13X_CNTL1, 0x7f, 0, 7); // standby mode for this change
  if ( returnError != KX13X_SUCCESS ) {
    Serial.println("standby mode fail");
    return;
  }
  if (!kxAccel.setOutputDataRate(0x0a)) { // 0x08 = 200hz, 0x0a = 800hz
    Serial.println("ODR fail");
    return;
  }
  returnError = kxAccel.writeRegister(KX13X_ODCNTL, 0x7f, 1, 7); // bypass IIR filter
  if ( returnError != KX13X_SUCCESS ) {
    Serial.println("IIR fail");
    return;
  }
  returnError = kxAccel.writeRegister(KX13X_CNTL1, 0x7f, 1, 7); // back to normal mode
  if ( returnError != KX13X_SUCCESS ) {
    Serial.println("normal mode fail");
    return;
  }

  Serial.println("kx done");

  // ============== BAROMETER ==============

  if (!bmp388.begin(BMP388_I2C_ALT_ADDR)) return;
  bmp388.setTempOversampling(OVERSAMPLING_SKIP);
  bmp388.setPresOversampling(OVERSAMPLING_SKIP);
  bmp388.startForcedConversion();
  initialized = true;
  Serial.println("done");
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

  myData = kxAccel.getAccelData();
  us = micros();
  kxFile.print(us);
  kxFile.print("\t");
  kxFile.print(myData.xData, 4);
  kxFile.print("\t");
  kxFile.print(myData.yData, 4);
  kxFile.print("\t");
  kxFile.println(myData.zData, 4);
  kxFile.flush();

  if (counter % 10) return; // more accel samples.

  // ============== BAROMETER ==============

  if (bmp388.getMeasurements(temperature, pressure, altitude)) {
    us = micros();
    bmpFile.print(us);
    bmpFile.print("\t");
    bmpFile.print(temperature, 4); // degrees C
    bmpFile.print("\t");
    bmpFile.print(pressure, 4); // hectopascals, i.e. millibar
    bmpFile.print("\t");
    bmpFile.println(altitude, 4); // meters
    bmpFile.flush();
    bmp388.startForcedConversion();
  }
}
