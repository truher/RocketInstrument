// rocket instrument
#include "BMP388_DEV.h"
#include "SdFat.h"
#include "SparkFun_Qwiic_KX13X.h"
#include "ICM_20948.h"
#include "SPI.h"
#include "Wire.h"

SdFat SD;
File kxFile;
File bmpFile;
File imuFile;
QwiicKX134 kxAccel;
BMP388_DEV bmp388;
ICM_20948_SPI myICM;

const byte PIN_BLINK = 19;
const byte PIN_IMU_CHIP_SELECT = 44;
const byte PIN_IMU_POWER = 27;
const byte PIN_MICROSD_CHIP_SELECT = 23;
const byte PIN_MICROSD_POWER = 15;
const byte PIN_QWIIC_POWER = 18;
const uint32_t SPI_FREQ = 1000000;

bool initialized = false;

void setup() {
  pinMode(PIN_BLINK, OUTPUT);
  digitalWrite(PIN_BLINK, HIGH);
  delay(1000);
  digitalWrite(PIN_BLINK, LOW);
  delay(1000);
  digitalWrite(PIN_BLINK, HIGH);
  delay(1000);
  digitalWrite(PIN_BLINK, LOW);

  Serial.begin(115200);
  Serial.println("setup");
  Serial.print("CPU speed: ");
  Serial.println(getCpuFreqMHz());

  // ============== SPI ==============

  SPI.begin();

  // ============== I2C ==============

  Wire = TwoWire(1);

  pinMode(PIN_QWIIC_POWER, OUTPUT);
  digitalWrite(PIN_QWIIC_POWER, HIGH);
  Wire.begin();
  Wire.setClock(1000000);  // Fast mode plus (1 MHz) seems to be the KX max speed.
  Wire.setPullups(1); // 1 is what the OLA code uses.
  delay(500); // Wait for the i2c boards to power up.
  Serial.println("i2c done");

  // ============== SD ==============

  pinMode(PIN_MICROSD_CHIP_SELECT, OUTPUT);
  digitalWrite(PIN_MICROSD_CHIP_SELECT, HIGH); // Deselect SD.

  pinMode(PIN_MICROSD_POWER, OUTPUT);
  digitalWrite(PIN_MICROSD_POWER, LOW); // Turn SD on.
  delay(1000);  // Wait for SD to power up, this takes awhile.

  while (!SD.begin(SdSpiConfig(PIN_MICROSD_CHIP_SELECT, SHARED_SPI, SD_SCK_MHZ(24)))) {
    Serial.println("sd file fail, try again");
    digitalWrite(PIN_BLINK, HIGH);
    delay(100);
    digitalWrite(PIN_BLINK, LOW);
    delay(300);
    digitalWrite(PIN_BLINK, HIGH);
    delay(100);
    digitalWrite(PIN_BLINK, LOW);
    delay(1000);
  }
  Serial.println("SD init ok");

  // Adds SPI CIPO pull-up because without peripheral action it floats.
  // Must happen after SD.begin() (why?)
  am_hal_gpio_pincfg_t cipoPinCfg = AP3_GPIO_DEFAULT_PINCFG;
  cipoPinCfg.uFuncSel = AM_HAL_PIN_6_M0MISO;
  cipoPinCfg.eDriveStrength = AM_HAL_GPIO_PIN_DRIVESTRENGTH_12MA;
  cipoPinCfg.eGPOutcfg = AM_HAL_GPIO_PIN_OUTCFG_PUSHPULL;
  cipoPinCfg.uIOMnum = AP3_SPI_IOM;
  cipoPinCfg.ePullup = AM_HAL_GPIO_PIN_PULLUP_1_5K;
  ap3_err_t retval2 = AP3_OK;
  padMode(MISO, cipoPinCfg, &retval2);
  if (retval2 != AP3_OK) {
    Serial.println("MISO pullup fail");
    return;
  }

  int rootFileCount = 0;
  File root;
  if (!root.open("/")) {
    Serial.println("root open fail");
    return;
  }
  File file;
  while (file.openNext(&root, O_RDONLY)) {
    file.printName(&Serial);
    Serial.println();
    rootFileCount++;
    file.close();
  }
  Serial.println("root file count: " + String(rootFileCount));
  String kxfilename = "kx." + String(rootFileCount) + ".txt";
  Serial.println("open " + kxfilename);
  if (!kxFile.open(kxfilename.c_str(), O_CREAT | O_WRITE | O_APPEND)) {
    Serial.println("kx file fail");
    return;
  }
  String bmpfilename = "bmp." + String(rootFileCount) + ".txt";
  Serial.println("open " + bmpfilename);
  if (!bmpFile.open(bmpfilename.c_str(), O_CREAT | O_WRITE | O_APPEND)) {
    Serial.println("bmp file fail");
    return;
  }
  String imufilename = "imu." + String(rootFileCount) + ".txt";
  Serial.println("open " + imufilename);
  if (!imuFile.open(imufilename.c_str(), O_CREAT | O_WRITE | O_APPEND)) {
    Serial.println("imu file fail");
    return;
  }
  Serial.println("sd done");

  digitalWrite(PIN_MICROSD_CHIP_SELECT, HIGH); // Deselect SD.

  // ============== IMU ==============

  pinMode(PIN_IMU_CHIP_SELECT, OUTPUT);
  digitalWrite(PIN_IMU_CHIP_SELECT, HIGH); // Deselect IMU

  pinMode(PIN_IMU_POWER, OUTPUT);
  digitalWrite(PIN_IMU_POWER, HIGH); // Turn IMU on.
  delay(100); // Wait for IMU to power up.

  myICM.enableDebugging();
  myICM.begin(PIN_IMU_CHIP_SELECT, SPI, SPI_FREQ);
  if (myICM.status != ICM_20948_Stat_Ok) {
    Serial.println("beginIMU: first attempt at myICM.begin failed. myICM.status = " + (String)myICM.status + "\r\n");

    //Reset IMU by power cycling it
    digitalWrite(PIN_IMU_POWER, LOW); // Turn IMU off.
    delay(10);
    digitalWrite(PIN_IMU_POWER, HIGH); // Turn IMU on.
    delay(100); // Wait for IMU to power up.

    // begin() defaults include:
    // DLPF (low pass filter) disabled (but configured, which is weird)
    // no sleeping, no lowpower, continuous mode, max speed, max resolution (min range)
    myICM.begin(PIN_IMU_CHIP_SELECT, SPI, SPI_FREQ);
    if (myICM.status != ICM_20948_Stat_Ok) {
      Serial.println("beginIMU: second attempt at myICM.begin failed. myICM.status = " + (String)myICM.status + "\r\n");
      digitalWrite(PIN_IMU_CHIP_SELECT, HIGH); // Deselect IMU.
      Serial.println(F("ICM-20948 failed to init."));
      digitalWrite(PIN_IMU_POWER, LOW); // Turn IMU off.
      return;
    }
  }

  // Set max range.
  ICM_20948_fss_t FSS;
  FSS.a = 3; // +-16g
  FSS.g = 3; // +-2000dps
  ICM_20948_Status_e retval = myICM.setFullScale((ICM_20948_Internal_Acc | ICM_20948_Internal_Gyr), FSS);
  if (retval != ICM_20948_Stat_Ok) {
    Serial.println("Error: Could not configure the IMU Full Scale!");
    return;
  }

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
  if (!kxAccel.setOutputDataRate(0x0b)) { // 0x0b=1600hz, seems like the effective maximum?
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
  bmp388.setIIRFilter(IIR_FILTER_OFF);
  bmp388.setTimeStandby(TIME_STANDBY_5MS); // ~continuous
  bmp388.startNormalConversion();
  initialized = true;
  Serial.println("setup done");
}

uint32_t counter = 0;

void loop() {
  if (!initialized) return;
  counter += 1;
  if (counter > 99) {
    digitalWrite(PIN_BLINK, HIGH);
    counter = 0;
    kxFile.flush();
    bmpFile.flush();
    imuFile.flush();
  }
  if (counter > 49) {
    digitalWrite(PIN_BLINK, LOW);
  }

  if (counter % 20 == 0) {
    recordBMP();
    return;
  }
  if (counter % 5 == 0) {
    recordIMU();
    return;
  }
  recordKX();
}

// first is about 3.5ms, subsequent about 1.2ms.
void recordKX() {
  rawOutputData rawAccelData;
  outputData myData;
  if (kxAccel.getRawAccelData(&rawAccelData) && kxAccel.convAccelData(&myData, &rawAccelData)) {
    kxFile.print(micros());
    kxFile.print("\t");
    kxFile.print(myData.xData, 4);
    kxFile.print("\t");
    kxFile.print(myData.yData, 4);
    kxFile.print("\t");
    kxFile.println(myData.zData, 4);
  }
}

//first is about 5.5ms, subsequent about 3ms
void recordIMU() {
  if (myICM.dataReady()) {
    myICM.getAGMT();
    if (myICM.status == ICM_20948_Stat_Ok) {
      imuFile.print(micros());
      imuFile.print("\t");
      imuFile.print( myICM.accX(), 4); // mg
      imuFile.print("\t");
      imuFile.print( myICM.accY(), 4); // mg
      imuFile.print("\t");
      imuFile.print( myICM.accZ(), 4); // mg
      imuFile.print("\t");
      imuFile.print( myICM.gyrX(), 4); // deg/s
      imuFile.print("\t");
      imuFile.print( myICM.gyrY(), 4); // deg/s
      imuFile.print("\t");
      imuFile.print( myICM.gyrZ(), 4); // deg/s
      imuFile.print("\t");
      imuFile.print( myICM.magX(), 4); // uT
      imuFile.print("\t");
      imuFile.print( myICM.magY(), 4); // uT
      imuFile.print("\t");
      imuFile.print( myICM.magZ(), 4); // uT
      imuFile.print("\t");
      imuFile.print( myICM.temp(), 4); // deg C
      imuFile.println();
    }
  }
}

// in forced mode, first is about 8ms, subsequent about 6ms
// in normal mode, the first one comes sooner since it's running all the time,
// and the rest are in 5ms which is the standby time.
// datasheet says 234us + 392us + 2000us + 313us + 2000us = just about 5ms.
void recordBMP() {
  float temperature, pressure, altitude;
  while (!bmp388.getMeasurements(temperature, pressure, altitude)) {
    // do nothing, it's ok to wait
  }
  bmpFile.print(micros());
  bmpFile.print("\t");
  bmpFile.print(temperature, 4); // degrees C
  bmpFile.print("\t");
  bmpFile.print(pressure, 4); // hectopascals, i.e. millibar
  bmpFile.print("\t");
  bmpFile.println(altitude, 4); // meters
}
