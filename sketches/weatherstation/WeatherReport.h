//  project
#include <WeatherPacket.h>
#include <HC12.h>

#define TEMPERATURELOWPASS 0.1
#define VOLTAGELOWPASS 0.5

//  this is the object all data is collected to
class WeatherReport {

  private:

    WeatherPacket mPacket;

  public:

    WeatherReport() {}

    //  sensor collects an amount of rain in mm
    void setDeltaRain(double rainMM) {
      mPacket.mDeltaRainMM = rainMM;
    }

    bool hasTemperature() {
      return mPacket.mTemperatureDegreeCelsius!=UNDEFINEDVALUE;
    }

    void addTemperature(float temperatureDegreeCelsius,
      float pressureHPA, float humidityPercent) {

      if (!hasTemperature())
        mPacket.mTemperatureDegreeCelsius = temperatureDegreeCelsius;
      else
        mPacket.mTemperatureDegreeCelsius = mPacket.mTemperatureDegreeCelsius*(1-TEMPERATURELOWPASS)+temperatureDegreeCelsius*TEMPERATURELOWPASS;

      if (mPacket.mPressureHPA==UNDEFINEDVALUE)
        mPacket.mPressureHPA = pressureHPA;
      else
        mPacket.mPressureHPA = mPacket.mPressureHPA*(1-TEMPERATURELOWPASS)+pressureHPA*TEMPERATURELOWPASS;

      if (mPacket.mHumidityPercent==UNDEFINEDVALUE)
        mPacket.mHumidityPercent = humidityPercent;
      else
        mPacket.mHumidityPercent = mPacket.mHumidityPercent*(1-TEMPERATURELOWPASS)+humidityPercent*TEMPERATURELOWPASS; 
    }

    void setWindSpeed(float windSpeedMpS) {
      mPacket.mWindSpeedMpS = windSpeedMpS;
    }

    bool hasWindDirection() {
      return *mPacket.mWindDirection!='\0';
    }

    void setWindDirection(const char *windDirection) {
      strcpy(mPacket.mWindDirection, windDirection);
    }

    void addVoltage(float voltage) {
      if (mPacket.mBatteryVoltage==UNDEFINEDVALUE)
        mPacket.mBatteryVoltage = voltage;
      else
        mPacket.mBatteryVoltage = mPacket.mBatteryVoltage*(1-VOLTAGELOWPASS)+voltage*VOLTAGELOWPASS; 
    }

    void send() {
      if (DEBUG)
        Serial.println("starting HC-12 communication...");

      //  send paket data as binary
      uint8_t *packetBinary = mPacket.encodedBytes();
      int packetSize = mPacket.encodedSize();

      if (DEBUG) {
        Serial.println("sending report...");
        mPacket.print(&Serial);
      }

      HC12.write(packetBinary, packetSize);
    }
};
