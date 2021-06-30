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

  const uint8_t PIN_SD_CHIP_SELECT = 23;
  SPI.begin();
  pinMode(PIN_SD_CHIP_SELECT, OUTPUT);
  digitalWrite(PIN_SD_CHIP_SELECT, HIGH); // deselect SD card
  delay(1000);

  // ============== IMU ==============

  enableCIPOpullUp(); // Enable CIPO pull-up on the OLA

  const byte PIN_IMU_CHIP_SELECT = 44;
  const byte PIN_IMU_POWER = 27;

  pinMode(PIN_IMU_CHIP_SELECT, OUTPUT);
  digitalWrite(PIN_IMU_CHIP_SELECT, HIGH); //Be sure IMU is deselected

  //Reset ICM by power cycling it
  pinMode(PIN_IMU_POWER, OUTPUT);
  digitalWrite(PIN_IMU_POWER, LOW);
  delay(10);
  pinMode(PIN_IMU_POWER, OUTPUT);
  digitalWrite(PIN_IMU_POWER, HIGH);

  delay(100); // Wait for the IMU to power up

  bool imu_initialized = false;
  while ( !imu_initialized ) {
    myICM.enableDebugging();
    myICM.begin( PIN_IMU_CHIP_SELECT, SPI, 4000000);

    Serial.print( F("Initialization of the sensor returned: ") );
    Serial.println( myICM.statusString() );
    Serial.print("connected? ");
    Serial.println(myICM.isConnected());
    if ( myICM.status != ICM_20948_Stat_Ok ) {
      Serial.println( "Trying again..." );
      delay(500);
    } else {
      imu_initialized = true;
    }
  }

  //Perform a full startup (not minimal) for non-DMP mode
  ICM_20948_Status_e retval = myICM.startupDefault(false);
  if (retval != ICM_20948_Stat_Ok)
  {
    Serial.println(F("Error: Could not startup the IMU in non-DMP mode!"));
    return;
  }
  //Update the full scale and DLPF settings
  retval = myICM.enableDLPF(ICM_20948_Internal_Acc, false);
  if (retval != ICM_20948_Stat_Ok)
  {
    Serial.println(F("Error: Could not configure the IMU Accelerometer DLPF!"));
    return;
  }
  retval = myICM.enableDLPF(ICM_20948_Internal_Gyr, false);
  if (retval != ICM_20948_Stat_Ok)
  {
    Serial.println(F("Error: Could not configure the IMU Gyro DLPF!"));
    return;
  }
  ICM_20948_dlpcfg_t dlpcfg;
  dlpcfg.a = 7;
  dlpcfg.g = 7;
  retval = myICM.setDLPFcfg((ICM_20948_Internal_Acc | ICM_20948_Internal_Gyr), dlpcfg);
  if (retval != ICM_20948_Stat_Ok)
  {
    Serial.println(F("Error: Could not configure the IMU DLPF BW!"));
    return;
  }
  ICM_20948_fss_t FSS;
  FSS.a = 0;
  FSS.g = 0;
  retval = myICM.setFullScale((ICM_20948_Internal_Acc | ICM_20948_Internal_Gyr), FSS);
  if (retval != ICM_20948_Stat_Ok)
  {
    Serial.println(F("Error: Could not configure the IMU Full Scale!"));
    return;
  }





  // ============== SD ==============

  // SD begin fails a lot but not all the time.  why?
//  while (!SD.begin(PIN_SD_CHIP_SELECT)) {
  while (!SD.begin(SdSpiConfig(PIN_SD_CHIP_SELECT, SHARED_SPI, SD_SCK_MHZ(24)))) {
    Serial.println("sd file fail, try again");
    digitalWrite(PIN_BLINK, HIGH);
    delay(100);
    digitalWrite(PIN_BLINK, LOW);
    delay(300);
    digitalWrite(PIN_BLINK, HIGH);
    delay(100);
    digitalWrite(PIN_BLINK, LOW);
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
  const char* kxfilename = ("kx." + String(rootFileCount) + ".txt").c_str();
  Serial.println("open " + String(kxfilename));
  if (!kxFile.open(kxfilename, O_CREAT | O_WRITE | O_APPEND)) {
    Serial.println("kx file fail");
    return;
  }
  const char* bmpfilename = ("bmp." + String(rootFileCount) + ".txt").c_str();
  Serial.println("open " + String(bmpfilename));
  if (!bmpFile.open(bmpfilename, O_CREAT | O_WRITE | O_APPEND)) {
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
    digitalWrite(PIN_BLINK, HIGH);
    counter = 0;
    kxFile.flush();
    bmpFile.flush();
  }
  if (counter > 49) {
    digitalWrite(PIN_BLINK, LOW);
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


  // ============== IMU ==============

  Serial.println(myICM.getWhoAmI());
  if ( myICM.dataReady() ) {
    myICM.getAGMT();
    printScaledAGMT( myICM.agmt);
  } //else {
   // Serial.println("no IMU data");
  //}

  // ============== BAROMETER ==============
  if (counter % 5) return; // sample barometer less frequently, ~80hz
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

void printFormattedFloat(float val, uint8_t leading, uint8_t decimals) {
  float aval = abs(val);
  if (val < 0) {
    Serial.print("-");
  } else {
    Serial.print(" ");
  }
  for ( uint8_t indi = 0; indi < leading; indi++ ) {
    uint32_t tenpow = 0;
    if ( indi < (leading - 1) ) {
      tenpow = 1;
    }
    for (uint8_t c = 0; c < (leading - 1 - indi); c++) {
      tenpow *= 10;
    }
    if ( aval < tenpow) {
      Serial.print("0");
    } else {
      break;
    }
  }
  if (val < 0) {
    Serial.print(-val, decimals);
  } else {
    Serial.print(val, decimals);
  }
}

void printScaledAGMT( ICM_20948_AGMT_t agmt) {
  Serial.print("Scaled. Acc (mg) [ ");
  printFormattedFloat( myICM.accX(), 5, 2 );
  Serial.print(", ");
  printFormattedFloat( myICM.accY(), 5, 2 );
  Serial.print(", ");
  printFormattedFloat( myICM.accZ(), 5, 2 );
  Serial.print(" ], Gyr (DPS) [ ");
  printFormattedFloat( myICM.gyrX(), 5, 2 );
  Serial.print(", ");
  printFormattedFloat( myICM.gyrY(), 5, 2 );
  Serial.print(", ");
  printFormattedFloat( myICM.gyrZ(), 5, 2 );
  Serial.print(" ], Mag (uT) [ ");
  printFormattedFloat( myICM.magX(), 5, 2 );
  Serial.print(", ");
  printFormattedFloat( myICM.magY(), 5, 2 );
  Serial.print(", ");
  printFormattedFloat( myICM.magZ(), 5, 2 );
  Serial.print(" ], Tmp (C) [ ");
  printFormattedFloat( myICM.temp(), 5, 2 );
  Serial.print(" ]");
  Serial.println();
}

bool enableCIPOpullUp()
{
  //Add CIPO pull-up
  ap3_err_t retval = AP3_OK;
  am_hal_gpio_pincfg_t cipoPinCfg = AP3_GPIO_DEFAULT_PINCFG;
  cipoPinCfg.uFuncSel = AM_HAL_PIN_6_M0MISO;
  cipoPinCfg.eDriveStrength = AM_HAL_GPIO_PIN_DRIVESTRENGTH_12MA;
  cipoPinCfg.eGPOutcfg = AM_HAL_GPIO_PIN_OUTCFG_PUSHPULL;
  cipoPinCfg.uIOMnum = AP3_SPI_IOM;
  cipoPinCfg.ePullup = AM_HAL_GPIO_PIN_PULLUP_1_5K;
  padMode(MISO, cipoPinCfg, &retval);
  return (retval == AP3_OK);
}
