//
//  binary packet of weather data
//

#include <Bolbro.h>
#include <Packet.h>

class WeatherPacket : public Packet {

  private:

    //  header
    byte mMagicByte;

  public:

    //  rain gauge
    double mAccumulatedRainMM; // mm

    //  temperature et al
    float mTemperatureDegreeCelsius; // degree Celsius
    float mPressureHPA; // hPa
    float mHumidityPercent; // %

    //  wind vane
    char mWindDirection [4]; // three digits plus \0

    //  anemometer
    float mWindSpeedMpS;

    //  system voltage, usually the battery
    float mBatteryVoltage;

  private:

    //  CRC16 checksum
    uint16_t  mCRC16;

  public:

    WeatherPacket() : Packet() {
      mAccumulatedRainMM = UNDEFINEDVALUE;

      mTemperatureDegreeCelsius = UNDEFINEDVALUE;
      mPressureHPA = UNDEFINEDVALUE;
      mHumidityPercent = UNDEFINEDVALUE;

      mWindDirection[0] = '\0';
      mWindSpeedMpS = UNDEFINEDVALUE;

      mBatteryVoltage = UNDEFINEDVALUE;
    }

    float batteryPercentage() {
      //  derive percentage with cell voltage > 3.87 = 100% and < 3.4 = 0%
      float cellVoltage = mBatteryVoltage/NUMCELLS;
      float batteryPercentage = 100*(cellVoltage-3.4)/(3.87-3.4);

      if (batteryPercentage>100)
        batteryPercentage = 100;
      if (batteryPercentage<0)
        batteryPercentage = 0;

      return batteryPercentage;
    }

    void printSerial() {
      if (mCRC16==crc16()) {
        if (mAccumulatedRainMM!=UNDEFINEDVALUE) {
            Serial.print("rain: ");
            Serial.print(mAccumulatedRainMM, 1);
            Serial.println(" mm overall");
        }

        if (mTemperatureDegreeCelsius!=UNDEFINEDVALUE) {
            Serial.print("temperature: ");
            Serial.print(mTemperatureDegreeCelsius, 1);
            Serial.println(" degree C");

            Serial.print("pressure: ");
            Serial.print(mPressureHPA, 1);
            Serial.println(" hPa");

            Serial.print("humidity: ");
            Serial.print(mHumidityPercent, 0);
            Serial.println(" %rH");
        }

        if (mWindDirection[0]) {
            Serial.print("wind direction: ");
            Serial.println(mWindDirection);
        }

        if (mWindSpeedMpS!=UNDEFINEDVALUE) {
          Serial.print("wind speed: ");
          Serial.print(mWindSpeedMpS, 1);
          Serial.println(" m/s");
        }

        if (mBatteryVoltage!=UNDEFINEDVALUE) {
          Serial.print("battery voltage: ");
          Serial.print(mBatteryVoltage, 2);
          Serial.print(" V, ");
          Serial.print(batteryPercentage(), 0);
          Serial.println("%");
        }
      } else {
        Serial.println("printing of corrupted weather packet failed");
      }
    }

    String json(String linePrefix = "") {

      String json = linePrefix + "{\n";

      if (mAccumulatedRainMM!=UNDEFINEDVALUE)
        json += linePrefix + "\t\"rainoverall\" : " + String(mAccumulatedRainMM, 1) +",\n";
      else
        json += linePrefix + "\t\"rainoverall\" : \"" STRINGNOTINITIALIZED "\",\n";

      if (mTemperatureDegreeCelsius!=UNDEFINEDVALUE) {
        json += linePrefix + "\t\"temperature\" : " + String(mTemperatureDegreeCelsius, 1) +",\n";
        json += linePrefix + "\t\"humidity\" : " + String(mHumidityPercent, 1) +",\n";
        json += linePrefix + "\t\"pressure\" : " + String(mPressureHPA, 1) +",\n";
      } else {
        json += linePrefix + "\t\"temperature\" : \"" STRINGNOTINITIALIZED "\",\n";
        json += linePrefix + "\t\"humidity\" : \"" STRINGNOTINITIALIZED "\",\n";
        json += linePrefix + "\t\"pressure\" : \"" STRINGNOTINITIALIZED "\",\n";
      }

      if (mWindDirection[0])
        json += linePrefix + "\t\"winddirection\" : \"" + String(mWindDirection) +"\",\n";
      else
        json += linePrefix + "\t\"winddirection\" : \"" STRINGNOTINITIALIZED "\",\n";

      if (mWindSpeedMpS!=UNDEFINEDVALUE)
        json += linePrefix + "\t\"windspeed\" : " + String(mWindSpeedMpS, 1) +",\n";
      else
        json += linePrefix + "\t\"windspeed\" : \"" STRINGNOTINITIALIZED "\",\n";

      if (mBatteryVoltage!=UNDEFINEDVALUE) {
        json += linePrefix + "\t\"batteryvoltage\" : " + String(mBatteryVoltage, 2) +",\n";
        json += linePrefix + "\t\"batterypercentage\" : " + String(batteryPercentage(), 1) +"\n";
      } else {
        json += linePrefix + "\t\"batteryvoltage\" : \"" STRINGNOTINITIALIZED "\",\n";
        json += linePrefix + "\t\"batterypercentage\" : \"" STRINGNOTINITIALIZED "\"\n";
      }

      json += linePrefix + "}";

      return json;
    }

  protected:

    byte *magicByteAddr() {
      return &mMagicByte;
    }

    uint16_t *crc16Addr() {
      return &mCRC16;
    }
};
