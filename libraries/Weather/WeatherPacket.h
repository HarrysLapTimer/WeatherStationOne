//
//  binary packet of weather data
//

#ifndef _WEATHERPACKET_H_
#define _WEATHERPACKET_H_

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
			float cellVoltage = mBatteryVoltage/NUMCELLS;
			float batteryPercentage = 0;
			//	characteristics for a NCR18650BD Lithium-Ion cell
			if (cellVoltage>2.0f) {
				float remainingCapacity;
				if (cellVoltage<4.3f)
					remainingCapacity = (cellVoltage-2.0)/(4.3-2.0)*3000.0;
				else
					remainingCapacity = 3000.0+(cellVoltage-4.3)/(4.7-4.3)*300.0;

				batteryPercentage = 100*remainingCapacity/3200.0;
			}

			if (batteryPercentage>100.0f)
				batteryPercentage = 100.0f;

			return batteryPercentage;
		}

    void printSerial() {
    	Serial.print("magic byte: ");
			Serial.print(mMagicByte);
			Serial.println(mMagicByte==MAGICBYTE?" correct":" wrong");

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

			Serial.print("checksum: ");
			Serial.print(mCRC16);
			Serial.println(mCRC16==crc16()?" correct":" wrong");
    }

#define STRING_WORKAROUND 1

#if STRING_WORKAROUND
		const char *jsonLine(const char *name, bool valid, float value, int precision, bool closingComma = true) {
			static char buffer[128];

			if (valid)
				snprintf(buffer, 128, "\t\"%s\" : %.*f%s\n", name, precision, value, closingComma?",":"");
			else
				snprintf(buffer, 128, "\t\"%s\" : \"%s\"%s\n", name, STRINGNOTINITIALIZED, closingComma?",":"");

			return buffer;
		}
#endif

    String json(String linePrefix = "") {

      String json = linePrefix + "{\n";

#if STRING_WORKAROUND
			json += linePrefix + jsonLine("rainoverall", mAccumulatedRainMM!=UNDEFINEDVALUE, mAccumulatedRainMM, 1);
			json += linePrefix + jsonLine("temperature", mTemperatureDegreeCelsius!=UNDEFINEDVALUE, mTemperatureDegreeCelsius, 1);
			json += linePrefix + jsonLine("humidity", mHumidityPercent!=UNDEFINEDVALUE, mHumidityPercent, 1);
			json += linePrefix + jsonLine("pressure", mPressureHPA!=UNDEFINEDVALUE, mPressureHPA, 1);

      if (mWindDirection[0])
        json += linePrefix + "\t\"winddirection\" : \"" + String(mWindDirection) +"\",\n";
      else
        json += linePrefix + "\t\"winddirection\" : \"" STRINGNOTINITIALIZED "\",\n";

			json += linePrefix + jsonLine("windspeed", mWindSpeedMpS!=UNDEFINEDVALUE, mWindSpeedMpS, 1);
			json += linePrefix + jsonLine("batteryvoltage", mBatteryVoltage!=UNDEFINEDVALUE, mBatteryVoltage, 2);
			json += linePrefix + jsonLine("batterypercentage", mBatteryVoltage!=UNDEFINEDVALUE, batteryPercentage(), 0, false);
#else
      if (mAccumulatedRainMM!=UNDEFINEDVALUE)
        json += linePrefix + "\t\"rainoverall\" : " + String(mAccumulatedRainMM, 1) +",\n";
      else
        json += linePrefix + "\t\"rainoverall\" : \"" STRINGNOTINITIALIZED "\",\n";

      if (mTemperatureDegreeCelsius!=UNDEFINEDVALUE)
        json += linePrefix + "\t\"temperature\" : " + String(mTemperatureDegreeCelsius, 1) +",\n";
      else
        json += linePrefix + "\t\"temperature\" : \"" STRINGNOTINITIALIZED "\",\n";

      if (mHumidityPercent!=UNDEFINEDVALUE)
        json += linePrefix + "\t\"humidity\" : " + String(mHumidityPercent, 1) +",\n";
      else
        json += linePrefix + "\t\"humidity\" : \"" STRINGNOTINITIALIZED "\",\n";

      if (mPressureHPA!=UNDEFINEDVALUE)
        json += linePrefix + "\t\"pressure\" : " + String(mPressureHPA, 1) +",\n";
      else
        json += linePrefix + "\t\"pressure\" : \"" STRINGNOTINITIALIZED "\",\n";

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
        json += linePrefix + "\t\"batterypercentage\" : " + String(batteryPercentage(), 0) +"\n";
      } else {
        json += linePrefix + "\t\"batteryvoltage\" : \"" STRINGNOTINITIALIZED "\",\n";
        json += linePrefix + "\t\"batterypercentage\" : \"" STRINGNOTINITIALIZED "\"\n";
      }
#endif

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

#endif // _WEATHERPACKET_H_
