
//  system libraries
#include <limits.h>
#include <math.h>

//  temperature sensor
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

//  project
#include <CalibrationPacket.h>
#include "WeatherReport.h"

/****************************************************************************************************
  utility functions
 ****************************************************************************************************/

//  start serial reporting
static bool serialStarted = false;

static void startSerial() {
  if (!serialStarted) {
   //  start serial monitor connection
    Serial.begin(115200);
    delay(500);
    serialStarted = true;
  }
}

//  safe way to goto deep sleep
static void deepSleep() {

  //  make sure we do not get interupts after deep sleep
  detachInterrupt(digitalPinToInterrupt(WINDSPEED_PIN));

  //  turn LED off
  digitalWrite(LED_PIN, LOW);

  //  send HC12 to power saving mode
  HC12.end();

  if (serialStarted) {
    if (DEBUG) {
      Serial.println("going to deep sleep...");
      Serial.flush();
      delay(1000); // make sure flush is completed before power down
    }
  }

  //  sent ESP32 to deep sleep
  esp_deep_sleep_start();  
}

//  function that prints the reason by which ESP32 has been awaken from sleep
static void printWakeupReason() {
  startSerial();
  
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:
      Serial.println("----------- wakeup caused by external signal using RTC_IO -----------"); 
      break;
    case ESP_SLEEP_WAKEUP_EXT1:
      Serial.println("---------- wakeup caused by external signal using RTC_CNTL ----------"); 
      break;
    case ESP_SLEEP_WAKEUP_TIMER: 
      Serial.println("---------------------- wakeup caused by timer -----------------------"); 
      break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
      Serial.println("--------------------- wakeup caused by touchpad ---------------------"); 
      break;
    case ESP_SLEEP_WAKEUP_ULP:
      Serial.println("-------------------- wakeup caused by ULP program -------------------"); 
      break;
    default: 
      Serial.printf("-------------- wakeup was not caused by deep sleep: %d ---------------\n",wakeup_reason); 
      break;
  }
}

/****************************************************************************************************
  calibration stuff
 ****************************************************************************************************/

//  tricky: while RTC_DATA_ATTR variables should be initialized once and keep state for wake ups
//  after deep sleep, the compiler is calling the constructor every time - including wake ups;
//  to keep RTC state from former run, initializeCalibrationPacketMembers is passed in to allow disabling
//  of member initialization; called with true during startup, it will be false for wake ups from deep
//  sleep (see CalibrationPacket())
RTC_DATA_ATTR bool initializeCalibrationPacketMembers = true;
RTC_DATA_ATTR CalibrationPacket calibrationPacket(initializeCalibrationPacketMembers);

/****************************************************************************************************
  sensor handling functions

  overview:
    - the wind vane uses 8 reed contacts, its state is polled when a report is due; the 8 contacts
      are available multiplexed by three bits 
    - the anemometer uses a single reed contact, speed is sampled when a report is due
    - the rain gauge uses a single reed contact, a change is notified by an ext0 wakeup, a counter
      is increased, and the result is sent when a report is due
    - temperature, barometric pressure, humidity are measure using a Bosch BME280, its state is polled
      when a report is due
    - OPEN: luminescence
    - OPEN: ground humidity (Bodenfeuchte)
  
 ****************************************************************************************************/

//  wind vane / direction
static void propagateWindDirection(WeatherReport &report) {

  if (!report.hasWindDirection()) {

    if (DEBUG) {
      static bool reported = false;
      if (!reported) {
        startSerial();
        Serial.println("retrieving wind vane data...");
        reported = true;
      }
    }

    static struct {
      const char *windDirection;
      uint8_t pattern[2];
    } windDirectionPattern [] = {
      //  single bit, two bit, three and four bit pattern
      //  to cope with different reed and magnet characteristics
      { "N",    { 0b00000001, 0b10000011 } },
      { "NNE",  { 0b00000011, 0b10000111 } },
      { "NE",   { 0b00000010, 0b00000111 } },
      { "ENE",  { 0b00000110, 0b00001111 } },
      { "E",    { 0b00000100, 0b00001110 } },
      { "ESE",  { 0b00001100, 0b00011110 } },
      { "SE",   { 0b00001000, 0b00011100 } },
      { "SSE",  { 0b00011000, 0b00111100 } },
      { "S",    { 0b00010000, 0b00111000 } },
      { "SSW",  { 0b00110000, 0b01111000 } },
      { "SW",   { 0b00100000, 0b01110000 } },
      { "WSW",  { 0b01100000, 0b11110000 } },
      { "W",    { 0b01000000, 0b11100000 } },
      { "WNW",  { 0b11000000, 0b11100001 } },
      { "NW",   { 0b10000000, 0b11000001 } },
      { "NNW",  { 0b10000001, 0b11000011 } }      
    };

    //  collect states
    const char *result = NULL;
    uint8_t directions = 0;

    if (DEBUG)
      Serial.print("active directions: ");

    for (int i = 0; i<8; i++) {
      //  select one of 8 vane contacts
      digitalWrite(WIND_VANE_S0, i&0b00000001?HIGH:LOW);    
      digitalWrite(WIND_VANE_S1, i&0b00000010?HIGH:LOW);    
      digitalWrite(WIND_VANE_S2, i&0b00000100?HIGH:LOW);
      delay(10); // this fixes issues with wrong digitalRead() results below

      //  read and store in directions
      if (digitalRead(WIND_VANE_Z)) {
        directions = directions|(0b1<<i);
        if (DEBUG) {
          Serial.print(windDirectionPattern[i*2].windDirection);
          Serial.print(" ");
        }    
      }
    }

    //  Check for all pin pattern...
    for (int i=0; i<16; i++) {
      for (int j=0; j<2; j++) {
        uint8_t pattern = windDirectionPattern[i].pattern[j];
        if (directions==pattern)
          result = windDirectionPattern[i].windDirection;
      }
    }
    
    if (DEBUG) {
      Serial.print("-> ");
      Serial.println(result);
    }

    if (result)
      report.setWindDirection(result);
    else {
      static bool reported = false;
      if (!reported) {
        startSerial();
        if (!directions)
          Serial.println("no main wind vane found");
        else
          Serial.println("invalid vane pattern found");          
        reported = true;
      }
    }
  }
}

//  rain gauge
RTC_DATA_ATTR unsigned int numRainBuckets = 0;
RTC_DATA_ATTR unsigned int lastNumRainBucketsReported = 0;

const double gaugeDiameter = 106; // mm
const double gaugeArea = M_PI*(gaugeDiameter/2)*(gaugeDiameter/2); // mm2

static void handleRainState() {
  //  called after ext0 wakeup, increment  
  numRainBuckets++;
}

static void propagateRain(WeatherReport &report) {
  //  to calculate mm from buckets
  unsigned int numBuckets = 0;
  
  if (numRainBuckets>lastNumRainBucketsReported)
    // in case a value has been reset...
    numBuckets = numRainBuckets-lastNumRainBucketsReported;
    
  double rainMM = numBuckets*calibrationPacket.mBucketTriggerVolume/gaugeArea;

  //  rainMM is the rain in mm we got since last time propagateRain has been called
  report.addRain(rainMM);

  lastNumRainBucketsReported = numRainBuckets;
}

//  temperature/barometric/humidity sensor
static void propagateTemperatureEtAll(WeatherReport &report) {

  if (!report.hasTemperature()) {  
    if (DEBUG) {
      static bool reported = false;
      if (!reported) {
        startSerial();
        Serial.println("retrieving BME280 data...");
        reported = true;
      }
    }
    Adafruit_BME280 bme;
    if (!bme.begin(0x76)) {
      static bool reported = false;
      if (!reported) {
        startSerial();
        Serial.println("no temperature sensor found...");
        reported = true;
      }
    } else
      report.addTemperature(bme.readTemperature(), bme.readPressure()/100.0f, bme.readHumidity());
  }
}

static void propagateBatteryVoltage(WeatherReport &report) {

  int adcValue = analogRead(VOLTAGE_DIVIDOR_PIN);

  //  implement a calibration value focussed linear correction...
  //
  //  voltage dividor is made up like
  //    GND -> resistorMeasurement -> VOLTAGE_DIVIDOR_PIN -> resistorAdder -> voltage
  //  calibration:
  //    set CALIBRATION to true
  //    measure voltage between GND and VOLTAGE_DIVIDOR_PIN using a multimeter
  //    read out ADC value
  //    meaure precise resistor values resistorMeasurement and resistorAdder
  //    customize resistorMeasurement, resistorAdder, voltageDividorMeasured, and voltageDividorMeasuredADCValue
  
  if (CALIBRATION) {
    Serial.print("ADC value for calibration: ");
    Serial.println(adcValue);
  }

  float voltageDividorMeasured = 2.45; // customize
  int voltageDividorMeasuredADCValue = 2870;// customize

  //  do a linear interpolation towards known end points zero and 3.3V
  float voltage = voltageDividorMeasured;
  
  if (adcValue>voltageDividorMeasuredADCValue)
    //  interpolate between voltageDividorMeasured and 3.3V / 4095 (ADC)
    //  sample: 3482... voltage = 2.45+(3.3-2.45)*(3482-2870)/(4095-2870) = 2.45+0.85*612/1225 = 2.45+0.425 = 2.875V
    voltage = voltageDividorMeasured+(3.3-voltageDividorMeasured)*(adcValue-voltageDividorMeasuredADCValue)/(4095-voltageDividorMeasuredADCValue);
  else if (adcValue<voltageDividorMeasuredADCValue)
    //  interpolate between voltageDividorMeasured and 0V / 0 (ADC)
    //  sample: 1435... voltage = 1435/2870*2.45 = 1.225V
    voltage = (float)adcValue/voltageDividorMeasuredADCValue*voltageDividorMeasured;

  //  make sure we are not getting off realistic results
  if (voltage<0.0)
    voltage = 0.0;
  if (voltage>3.3)
    voltage = 3.3;

  if (CALIBRATION) {
    Serial.print("ADC voltage: ");
    Serial.print(voltage,3);
    Serial.println(" V");
  }
  
  //  we have a voltage devidor made up from two 10k resistors
  const float resistorMeasurement = 9.82;  // customize
  const float resistorAdder = 9.77; // customize
  voltage = voltage*(resistorMeasurement+resistorAdder)/resistorMeasurement;

  if (CALIBRATION) {
    Serial.print("battery voltage: ");
    Serial.print(voltage,3);
    Serial.println(" V");
  }

  report.addVoltage(voltage);
}

static unsigned long startSampling; // initialized  in setup()
static int windSpeedCounts = 0;
static void handleWindSpeed() {
  windSpeedCounts++;
  if (DEBUG) {
    Serial.print("increased wind speed count to ");
    Serial.println(windSpeedCounts);
  }
} 

static void propagateWindSpeed(WeatherReport &report) {
  unsigned long speedSampleTime = millis(); 
  float secondsPassed = (speedSampleTime-startSampling)/1000.0f;

  if (secondsPassed>=1.0f) {
    //  at least one second sampled, derive wind speed

    //  derive wind speed measured from number of rotations: https://www.ncbi.nlm.nih.gov/pmc/articles/PMC5948875/
    float windSpeedMpS = calibrationPacket.mWindSpeedFactor*windSpeedCounts/NUM_COUNTS_PER_TURN/secondsPassed;

    Serial.print("wind speed measured at ");
    Serial.print(calibrationPacket.mMeasurementHeight, 1);
    Serial.print(" m: ");
    Serial.print(windSpeedMpS, 1);
    Serial.println(" m/s");

    windSpeedCounts = 0; // reset

    //  measurements made on a height (reference) different to height 10m need a compensation:
    //    v(h) = vref/ln(href/z0)*ln(h/z0)
    //  with
    //    vref = windSpeedMpS
    //    href = calibrationPacket.mMeasurementHeight
    //    z0 = 
    //    h = 10.0

    const float z0 = 0.1; // https://www.igwindkraft.at/kinder/windkurs/windpowerweb/de/stat/unitsw.htm#roughness
    windSpeedMpS = windSpeedMpS/log(calibrationPacket.mMeasurementHeight/z0)*log(10.0/z0);

    Serial.print("wind speed at 10 meter height: ");
    Serial.print(windSpeedMpS, 1);
    Serial.println(" m/s");

    report.setWindSpeed(windSpeedMpS);
  } else
    report.setWindSpeed(0);
}

/****************************************************************************************************
  main functions  
 ****************************************************************************************************/

WeatherReport report;

void setup() {

  //  set up all triggers to wake up
  esp_sleep_enable_ext0_wakeup ((gpio_num_t) RAIN_PIN, 1);
  esp_sleep_enable_timer_wakeup (calibrationPacket.mSecondsBetweenReports*uS2S_FACTOR);

  if (DEBUG) {
    printWakeupReason();
  }
  
  //  configure signaling LEDs
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // high until deep sleep

  //  handle wakup cause
  switch(esp_sleep_get_wakeup_cause()) {
    case ESP_SLEEP_WAKEUP_EXT0:
      handleRainState();
      //  goto deep sleep instantly
      deepSleep();
      break;
    case ESP_SLEEP_WAKEUP_TIMER:
      //  proceed to loop() for data collection and reporting
      break;
    default:
      //  all other wake ups, goto deep sleep again
      deepSleep();
      break;
  }

  //  going to loop(), we will send reports... bring up HC-12
  //  communication early
  HC12.begin();

  //  setup wind vane
  pinMode(WIND_VANE_S0, OUTPUT);
  pinMode(WIND_VANE_S1, OUTPUT);
  pinMode(WIND_VANE_S2, OUTPUT);
  pinMode(WIND_VANE_Z, INPUT_PULLDOWN);

  //  setup anemometer
  pinMode(WINDSPEED_PIN, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(WINDSPEED_PIN), handleWindSpeed, RISING);

  if (DEBUG)
    Serial.println("finished setup, continuing to loop()");

  //  set starting millis for sampling-loop
  startSampling = millis();
}

void loop() {
  //  check whether the time passed since start exceeds sampling time
  if (millis()-startSampling>SECONDS_SAMPLING*MS2S_FACTOR) {
    //  sensors propagating once
    propagateWindSpeed(report);
    propagateWindDirection(report);
    propagateTemperatureEtAll(report);
    propagateRain(report);
    
    //  send report...
    report.send();
    digitalWrite(LED_PIN, LOW); //  turn LED off
    
    //  ...wait for a calibration update...
    unsigned long waitStartedMillis = millis();
    CalibrationPacket newCalibrationPacket;
    while (millis()-waitStartedMillis<2000) {
      if (HC12.available()) {
        digitalWrite(LED_PIN, HIGH); // high when data is received
        if (newCalibrationPacket.decodeByte(HC12.read())) {
            calibrationPacket = newCalibrationPacket; // sound packet
            calibrationPacket.printSerial();
            digitalWrite(LED_PIN, LOW); // ready
            break;
        }
      }
    }
    
    //  ...and goto sleep afterwards 
    deepSleep();
  }

  //  sensors collecting data for a longer time
  propagateBatteryVoltage(report);
  delay(100);
}
