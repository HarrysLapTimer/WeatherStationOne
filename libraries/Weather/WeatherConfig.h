/****************************************************************************************************
  development settings
 ****************************************************************************************************/
#define DEBUG 1 // customize, more text output
#define USEREMOTEDEBUG 1 // customize, use remote debug (weatherbase)
#define TESTING 0 // customize, enable continuous testing vs. deep sleep cycle

//	make implementation decisions
#define USE_TRACKER 0 // customize, enabled code for the optional solar tracker (work in progress)
#define USE_WIND_REED 0 // customize, enable code for anemometer and wind vane using reeds/HC4051 muxer
#define USE_WIND_AS5600 0 // customize, enable code for anemometer and wind vane using reed/AS5600
#define USE_TEMPERATURE 0 // customize, enable code for temperature at al
#define USE_RAIN 1 // customize, enable code for rain gauge
#define USE_BATTERY 0 // customize, enable code for battery level

/****************************************************************************************************
  configuration
 ****************************************************************************************************/

#define ADMINPASSWORD "admin123" // customize

#define DEFAULT_SECONDS_BETWEEN_REPORTS 20 // raise to reduce battery drain; default value, overriden by CalibrationPacket
#define SECONDS_SAMPLING 3

#define NUM_DIRECTIONS_PER_PIN 4

//	for sun position calculation and weather forecast
#define LATITUDE (54.0+49.361/60.0) // customize
#define LONGITUDE (9.0+36.987/60.0) // customize

//  wind vane and anemometer
#if USE_WIND_AS5600
#	define WIND_VANE_PIN 32
#	define WINDSPEED_PIN 5
#endif // USE_WIND_AS5600

#if USE_WIND_REED
#	define WIND_VANE_S0 15 // address direction
#	define WIND_VANE_S1 2
#	define WIND_VANE_S2 18
#	define WIND_VANE_Z 5 // read direction
#	define WINDSPEED_PIN 32
#endif // USE_WIND_REED

#if USE_WIND_REED||USE_WIND_AS5600
#define NUM_COUNTS_PER_TURN 1 // depends on magnet / reed position

//	speed calibration: assuming the cup centers will move at wind speed, we have
//		speed = (rotations * circumference) / time
//	with
//		rotations a number
//		circumference the length of the circle circumference of the cup's movements
//		time the time rotations have been measured
//	in addition, there is a factor for loss of cup speed compared to wind:
//		ANEMOMETER_RADIUS

#define ANEMOMETER_RADIUS 0.08 // 80mm = 0.08m
#define ANEMOMETER_LOSS 1.18
#define DEFAULT_WINDSPEED_FACTOR (2*M_PI*ANEMOMETER_RADIUS*ANEMOMETER_LOSS)

#else

#define DEFAULT_WINDSPEED_FACTOR 2.7

#endif // USE_WIND_REED||USE_WIND_AS5600
#define DEFAULT_MEASUREMENT_HEIGHT 10.0 // no compensation

//  rain gauge
#if USE_RAIN
#define RAIN_PIN 27
#endif // USE_RAIN
// 	8 pulses = 25ml, one bucket = 3.125ml
#define DEFAULT_BUCKET_TRIGGER_VOLUME 3125.0f // 3.125ml = 3125m3;

//  temperature et al
#define SDA_PIN 21 // documentation only
#define SCL_PIN 22 // documentation only

#if USE_TRACKER
//	solar tracker
#	define TRACKER_HOME_DEGREE 66.0 // degree at which the magnet triggers the reed contact
#	define TRACKER_MIN_DEGREE 60.0
#	define TRACKER_MAX_DEGREE (180.0+(180.0-TRACKER_MIN_DEGREE))
#	define STEPS_PER_REVOLUTION (360/11.25*63.68395) // 28BYJ-48 with 11.25° per step and a 1:63.68395 gearbox

#	define STEPPER_IN1_PIN 19 // 15 35
#	define STEPPER_IN2_PIN 25 // 2 25
#	define STEPPER_IN3_PIN 26 // 0 26
#	define STEPPER_IN4_PIN 13  // 4 13
#	define TRACKER_CALIBRATION_PIN 23
#endif

//  HC-12
#define HC12_RXD_PIN 16 // Serial2 RX
#define HC12_TXD_PIN 17 // Serial2 TX
#define HC12_SET_PIN 33

//  others
#define LED_PIN 4

#if USE_BATTERY
#define VOLTAGE_DIVIDOR_PIN 34
#endif // USE_BATTERY
#define NUMCELLS 1

//  other constants
#define MS2S_FACTOR 1000ul // conversion factor for milli seconds to seconds
#define uS2S_FACTOR ((uint64_t)1000000ul) // conversion factor for micro seconds to seconds
