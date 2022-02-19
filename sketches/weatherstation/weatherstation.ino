
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
#if USE_TRACKER
# include "Tracker.h"

/****************************************************************************************************
  solar tracker
 ****************************************************************************************************/

RTC_DATA_ATTR bool initializeTrackerMembers = true;
RTC_DATA_ATTR Tracker tracker(initializeTrackerMembers);

#endif

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
  rain gauge
 ****************************************************************************************************/

RTC_DATA_ATTR unsigned int numRainBuckets = 0;
RTC_DATA_ATTR bool rainBucketOperational = true; // bucket is *not* horizontal permanently
RTC_DATA_ATTR unsigned int lastNumRainBucketsReported = 0;

/****************************************************************************************************
  deep sleep wakeup status
 ****************************************************************************************************/

RTC_DATA_ATTR int lastWakeupLevel = 0;
RTC_DATA_ATTR int wakeupsSinceLastReport = 0;

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

#if USE_TRACKER
  //  detach stepper
  tracker.deepSleep();
#endif

  //  turn LED off
  digitalWrite(LED_PIN, LOW);

  //  send HC12 to power saving mode
  HC12.end();

  //  set up all rain pin triggered wake up - make sure we register for any change
  //  this works around a station lock down in case the rain gauge short cuts (always HIGH)
  lastWakeupLevel = digitalRead(RAIN_PIN)?0:1;

  if (DEBUG) {
    Serial.print("enabling ext0 wake up for RAIN_PIN going to state ");
    Serial.println(lastWakeupLevel);
  }
  esp_sleep_enable_ext0_wakeup ((gpio_num_t) RAIN_PIN, lastWakeupLevel); // wake up on every change

  //  station reports data on timer wakeups only; set a wakeup time relative to
  //  last reporting time...
  unsigned long secondsToNextReport = calibrationPacket.mSecondsBetweenReports;

  //  reduce wait time by ext0 wake ups to compensate report slow down
#define SECONDS_PER_EXT0_WAKEUP 1
#define MINIMAL_WAIT_SECONDS 2
  if (secondsToNextReport>wakeupsSinceLastReport*SECONDS_PER_EXT0_WAKEUP+MINIMAL_WAIT_SECONDS)
    secondsToNextReport = secondsToNextReport-wakeupsSinceLastReport*SECONDS_PER_EXT0_WAKEUP;
  else
    secondsToNextReport = MINIMAL_WAIT_SECONDS;
  
  if (DEBUG) {
    Serial.print("enabling timer wake up in ");
    Serial.print(secondsToNextReport);
    Serial.println(" s");
    if (wakeupsSinceLastReport>0) {
      Serial.print("with time reduced by ");
      Serial.print(wakeupsSinceLastReport*SECONDS_PER_EXT0_WAKEUP);
      Serial.println(" seconds due to rain wake ups");
    }
  }
  esp_sleep_enable_timer_wakeup (secondsToNextReport*uS2S_FACTOR);
  
  Serial.println("going to deep sleep...");

  if (serialStarted) {
    if (DEBUG) {
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
#if USE_AS5600
static void propagateWindDirection(WeatherReport &report) {

  if (TESTING||!report.hasWindDirection()) {

    if (DEBUG) {
#if TESTING
      Serial.println("retrieving wind vane data...");
#else
      static bool reported = false;
      if (!reported) {
        startSerial();
        Serial.println("retrieving wind vane data...");
        reported = true;
      }
#endif
    }

    int rawValue = analogRead(WIND_VANE_PIN);

    if (TESTING&&DEBUG)
      Serial.printf("raw A2D value is %d\n", rawValue);

    //  while the AS5600 allows read outs in degrees using the I2C or
    //  PWM interfaces, we use the analog plus A2D interface. It is
    //  the default set for the chip and allows us to use the bigger
    //  soldering points; the mapping is good enough to derive one of
    //  the 16 directions
  
    //  map 22.5 degree segments starting with "N" to raw values
    //  this mapping works around the non-linearity of A2D conversion
    //  in addition, it minimized the "blind" spot between 4095 / 0
    //  the best way possible
    static int raw4direction[] = 
      {
        4095, 0, 197, 460, 704, 951, 1223, 1484,
        1743, 1936, 2186, 2410, 2732, 3020, 3363, 3744 
      };
  
    //  find best match according to raw value
    int best_i = 0;
    int best_diff = 4096;
    for (int i = 0; i<16; i++) {
      int diff = 2048 - abs(abs(raw4direction[i]-rawValue)%4096 - 2048);
      if (diff<best_diff) {
        best_diff = diff;
        best_i = i;
      }
    }

    //  set result
    static const char *directions[] = 
      {
        "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
        "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW" 
      };

    if (TESTING&&DEBUG)
      Serial.printf("direction measured: %s\n", directions[best_i]);
      
    report.setWindDirection(directions[best_i]);  
  }
}
#else
static void propagateWindDirection(WeatherReport &report) {

  if (!report.hasWindDirection()) {

    if (DEBUG) {
#if TESTING
      Serial.println("retrieving wind vane data...");
#else      
      static bool reported = false;
      if (!reported) {
        startSerial();
        Serial.println("retrieving wind vane data...");
        reported = true;
      }
#endif
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
#endif

const double gaugeDiameter = 106; // mm
const double gaugeArea = M_PI*(gaugeDiameter/2)*(gaugeDiameter/2); // mm2

static void handleRainState() {
  //  called after ext0 wakeup, increment
  if (rainBucketOperational) {
    numRainBuckets++;
    if (DEBUG) {
      Serial.print("increased number of rain buckets to ");
      Serial.println(numRainBuckets);
    }
  } else
      Serial.println("skipped increasing number of rain buckets");    
}

static void propagateRain(WeatherReport &report) {

  //  sanity check - the rain bucket pin should be LOW here; in case it is not
  //  the bucket is probably in horizontal position
  rainBucketOperational = digitalRead(RAIN_PIN)==LOW;

  if (DEBUG)
    if (rainBucketOperational)
      Serial.println("bucket o.k., pulse collection operational");
    else
      Serial.println("bucket in horizontal position, disabling pulse collection");

  //  to calculate mm from buckets
  unsigned int numDeltaBuckets = 0;
  
  if (numRainBuckets>lastNumRainBucketsReported)
    // in case a value has been reset...
    numDeltaBuckets = numRainBuckets-lastNumRainBucketsReported;

  double deltaRainMM = numDeltaBuckets*calibrationPacket.mBucketTriggerVolume/gaugeArea;

  if (DEBUG&&numDeltaBuckets>0) {
    Serial.print("adding ");
    Serial.print(numDeltaBuckets);
    Serial.print(" buckets equaling ");
    Serial.print(deltaRainMM, 2);
    Serial.println("mm rain");
  }

  //  rainMM is the rain in mm we got since last time propagateRain has been called
  report.setDeltaRain(deltaRainMM);

  //  memorize current rain buckets to allow a delta calculation for the next call
  lastNumRainBucketsReported = numRainBuckets;
}

//  temperature/barometric/humidity sensor
static void propagateTemperatureEtAll(WeatherReport &report) {

  if (!report.hasTemperature()) {  
    if (DEBUG) {
#if TESTING
      Serial.println("retrieving BME280 data...");
#else      
      static bool reported = false;
      if (!reported) {
        startSerial();
        Serial.println("retrieving BME280 data...");
        reported = true;
      }
#endif
    }
    Adafruit_BME280 bme;
    if (!bme.begin(0x76)) {
#if TESTING
      Serial.println("no temperature sensor found...");
#else            
      static bool reported = false;
      if (!reported) {
        startSerial();
        Serial.println("no temperature sensor found...");
        reported = true;
      }
#endif
    } else {
      int numRetries = 10;

      do {      
        float temperature = bme.readTemperature();
        float pressure = bme.readPressure();
        float humidity = bme.readHumidity();

        if (temperature!=NAN && pressure!=NAN && humidity!=NAN) {
          report.addTemperature(temperature, pressure/100.0f, humidity);
          break;      
        }

        numRetries--;
      } while (numRetries);
    }
  }
}

static void propagateBatteryVoltage(WeatherReport &report) {

  int adcValue = analogRead(VOLTAGE_DIVIDOR_PIN);

  //  implement a calibration value focussed linear correction...
  //
  //  voltage dividor is made up like
  //    GND -> resistorMeasurement -> VOLTAGE_DIVIDOR_PIN -> resistorAdder -> voltage
  //  calibration:
  //    set TESTBATTERY to 1
  //    measure voltage between GND and VOLTAGE_DIVIDOR_PIN using a multimeter
  //    read out ADC value
  //    meaure precise resistor values resistorMeasurement and resistorAdder
  //    customize resistorMeasurement, resistorAdder, voltageDividorMeasured, and voltageDividorMeasuredADCValue

#if TESTBATTERY
  Serial.print("ADC value for calibration: ");
  Serial.println(adcValue);
#endif

  float voltageDividorMeasured = 2.45; // customize
  int voltageDividorMeasuredADCValue = 2870; // customize, must not be 4095

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

#if TESTBATTERY
  Serial.print("ADC voltage: ");
  Serial.print(voltage,3);
  Serial.println(" V");
#endif
  
  //  we have a voltage devidor made up from two 10k resistors
  const float resistorMeasurement = 9.82;  // customize
  const float resistorAdder = 9.77; // customize
  voltage = voltage*(resistorMeasurement+resistorAdder)/resistorMeasurement;

#if TESTBATTERY
  Serial.print("battery voltage: ");
  Serial.print(voltage,3);
  Serial.println(" V");
#endif

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

#if !TESTING
  if (DEBUG) {
    printWakeupReason();
  }
#endif
  
  //  configure signaling LEDs
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // high until deep sleep

  //  configure rain PIN
  pinMode(RAIN_PIN, INPUT);
  delay(10); // make sure digitalRead() is ready

#if !TESTING
  //  handle wakup cause
  switch(esp_sleep_get_wakeup_cause()) {
    case ESP_SLEEP_WAKEUP_EXT0:
      //  wakeup has been set either for state 1 or 0
      //  increase count only in case we are in 1
      if (lastWakeupLevel) 
        handleRainState();
      wakeupsSinceLastReport++;
      //  goto deep sleep instantly
      deepSleep();
      break;
    case ESP_SLEEP_WAKEUP_TIMER:
      //  proceed to loop() for data collection and reporting
      wakeupsSinceLastReport = 0;
      break;
    default:
      //  all other wake ups, goto deep sleep again
      deepSleep();
      break;
  }

  //  going to loop(), we will send reports... bring up HC-12
  //  communication early
  HC12.begin();
#endif

  //  setup wind vane
 #if !USE_AS5600
  pinMode(WIND_VANE_S0, OUTPUT);
  pinMode(WIND_VANE_S1, OUTPUT);
  pinMode(WIND_VANE_S2, OUTPUT);
  pinMode(WIND_VANE_Z, INPUT_PULLDOWN);
#endif

  //  setup anemometer
  pinMode(WINDSPEED_PIN, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(WINDSPEED_PIN), handleWindSpeed, RISING);

  if (DEBUG) {
    startSerial();  
    Serial.println("finished setup, continuing to loop()");
  }
  
  //  set starting millis for sampling-loop
  startSampling = millis();

#if USE_TRACKER
  //  maintain tracker status
  if (tracker.canBeControlled())
    tracker.moveTo(calibrationPacket.mAzimuth);
#endif
}

void loop() {

  //  check whether the time passed since start exceeds sampling time
  if (millis()-startSampling>SECONDS_SAMPLING*MS2S_FACTOR) {
    //  sensors propagating once
#if !TESTING||TESTWIND
    propagateWindSpeed(report);
    propagateWindDirection(report);
#endif
#if !TESTING||TESTTEMPERATURE
    propagateTemperatureEtAll(report);
#endif
#if !TESTING||TESTRAIN
    propagateRain(report);
#endif

#if TESTING
    digitalWrite(LED_PIN, LOW); //  turn LED off

    delay(5000); // wait a bit for next cycle
    
    //  set starting millis for sampling-loop
    startSampling = millis();
#else
    //  send report...
    report.send();

    digitalWrite(LED_PIN, LOW); //  turn LED off
    
    //  ...wait for a calibration update...
    unsigned long waitStartedMillis = millis();
    CalibrationPacket newCalibrationPacket;
    while (millis()-waitStartedMillis<2000) {
      if (HC12.available()) {
        digitalWrite(LED_PIN, HIGH); // high when sound data is received
        if (newCalibrationPacket.decodeByte(HC12.read())) {
            calibrationPacket = newCalibrationPacket; // sound packet
            calibrationPacket.print(&Serial);
            
            //  Check if we have received a command...
            if (calibrationPacket.mCommand != CalibrationPacket::Command::NoCommand) {
              switch (calibrationPacket.mCommand) {
#if USE_TRACKER
                case CalibrationPacket::Command::CalibrateSolarTracker:
                  tracker.calibrate();
                  while (!tracker.canBeControlled())
                    tracker.run();
                  break;
                case CalibrationPacket::Command::TestSolarTracker:
                  tracker.minMaxTesting();
                  while (!tracker.canBeControlled())
                    tracker.run();
                  break;          
#endif        
              }
            }
            
            digitalWrite(LED_PIN, LOW); // ready
            break;
        }
      }
    }
    
    //  ...and goto sleep afterwards 
    deepSleep();
#endif
  }

#if USE_TRACKER
  //  run tracker if position has changed
  tracker.run();
#endif

  //  sensors collecting data for a longer time
  propagateBatteryVoltage(report);
  delay(100);
}
