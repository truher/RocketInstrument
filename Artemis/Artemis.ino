// rocket instrument
#include "BMP388_DEV.h"
#include "SdFat.h"
#include "SparkFun_Qwiic_KX13X.h"
#include "SPI.h"
#include "Wire.h"

SdFat SD;
File kxFile;
File bmpFile;
QwiicKX134 kxAccel;
BMP388_DEV bmp388;

bool initialized = false;
const uint8_t blinkPin_OLA = 19;

void setup() {
  pinMode(blinkPin_OLA, OUTPUT);

  digitalWrite(blinkPin_OLA, HIGH);
  delay(1000);
  digitalWrite(blinkPin_OLA, LOW);
  delay(1000);
  digitalWrite(blinkPin_OLA, HIGH);
  delay(1000);
  digitalWrite(blinkPin_OLA, LOW);

  Serial.begin(115200);
  Serial.println("setup");
  Serial.print("CPU speed: ");
  Serial.println(getCpuFreqMHz());

  // ============== I2C ==============

  Wire = TwoWire(1);
  const byte PIN_QWIIC_POWER = 18;
  pinMode(PIN_QWIIC_POWER, OUTPUT);
  digitalWrite(PIN_QWIIC_POWER, HIGH);
  Wire.begin();
  Wire.setClock(1000000);  // fast mode plus seems to be the KX max speed
  Wire.setPullups(1); // 1 is what the OLA code uses
  delay(500); // essential to wait for the i2c boards to power up
  Serial.println("i2c done");

  // ============== SPI ==============

  const uint8_t CS_PIN = 23;
  SPI.begin();
  pinMode(CS_PIN, OUTPUT);
  delay(1000);
  
  // ============== SD ==============

  // SD begin fails a lot but not all the time.  why?
  while (!SD.begin(CS_PIN)) {
    Serial.println("sd file fail, try again");
    digitalWrite(blinkPin_OLA, HIGH);
    delay(100);
    digitalWrite(blinkPin_OLA, LOW);
    delay(300);
    digitalWrite(blinkPin_OLA, HIGH);
    delay(100);
    digitalWrite(blinkPin_OLA, LOW);
    delay(500);
  }

  int rootFileCount = 0;
  File root;
  if (!root.open("/")) {
    Serial.println("root open fail");
    return;
  }
  File file;
  while (file.openNext(&root, O_RDONLY)) {
    rootFileCount++;
    file.close();
  }
  Serial.println("root file count: " + String(rootFileCount));

  kxFile = SD.open("kx."+String(rootFileCount)+".txt", O_CREAT | O_WRITE | O_APPEND);
  if (!kxFile) {
    Serial.println("kx file fail");
    return;
  }
  bmpFile = SD.open("bmp."+String(rootFileCount)+".txt", O_CREAT | O_WRITE | O_APPEND);
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
  if (!kxAccel.setOutputDataRate(0x0b)) { // 0x08=200hz, 0x0a=800hz, 0x0b=1600hz, 0x0c=3200hz
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

uint32_t counter = 0;

void loop() {
  if (!initialized) return;
  counter += 1;
  if (counter > 99) {
    digitalWrite(blinkPin_OLA, HIGH);
    counter = 0;
    kxFile.flush();
    bmpFile.flush();
  }
  if (counter > 49) {
    digitalWrite(blinkPin_OLA, LOW);
  }

  // ============== ACCELEROMETER ==============
  // samples between 200 and 800 hz
  outputData myData = kxAccel.getAccelData(); // takes ~1-3ms
  uint32_t us = micros();
  kxFile.print(us);
  kxFile.print("\t");
  kxFile.print(myData.xData, 4);
  kxFile.print("\t");
  kxFile.print(myData.yData, 4);
  kxFile.print("\t");
  kxFile.println(myData.zData, 4);

  if (counter % 5) return; // sample barometer less frequently

  // ============== BAROMETER ==============
  // samples around 80hz
  float temperature, pressure, altitude;
  if (bmp388.getMeasurements(temperature, pressure, altitude)) { // takes 1ms
    us = micros();
    bmpFile.print(us);
    bmpFile.print("\t");
    bmpFile.print(temperature, 4); // degrees C
    bmpFile.print("\t");
    bmpFile.print(pressure, 4); // hectopascals, i.e. millibar
    bmpFile.print("\t");
    bmpFile.println(altitude, 4); // meters
    bmp388.startForcedConversion();  // takes ~3ms!
  }
}
