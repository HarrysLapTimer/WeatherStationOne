//
//  binary packet of calibration parameters
//

#ifndef _CALIBRATIONPACKET_H_
#define _CALIBRATIONPACKET_H_

#include <Bolbro.h>
#include <Packet.h>
#include <WeatherConfig.h>

class CalibrationPacket : public Packet {

  private:

    //  header
    byte mMagicByte;

  public:

    //  calibration
    float mBucketTriggerVolume;
    float mWindSpeedFactor;
    float mMeasurementHeight;

    //  settings
    unsigned long mSecondsBetweenReports;

  private:

    //  CRC16 checksum
    uint16_t  mCRC16;

  public:

    CalibrationPacket() : Packet() {
			mBucketTriggerVolume = DEFAULT_BUCKET_TRIGGER_VOLUME;
			mWindSpeedFactor = DEFAULT_WINDSPEED_FACTOR;
			mMeasurementHeight = DEFAULT_MEASUREMENT_HEIGHT;
			mSecondsBetweenReports = DEFAULT_SECONDS_BETWEEN_REPORTS;
    }

    CalibrationPacket(bool &initializePacketMembers) : Packet() {
    	//	restore state from preferences
			if (initializePacketMembers) {
	  		mBucketTriggerVolume = DEFAULT_BUCKET_TRIGGER_VOLUME;
  			mWindSpeedFactor = DEFAULT_WINDSPEED_FACTOR;
  			mMeasurementHeight = DEFAULT_MEASUREMENT_HEIGHT;
  			mSecondsBetweenReports = DEFAULT_SECONDS_BETWEEN_REPORTS;

  			//	initialize only once; working when in RTC memory
  			initializePacketMembers = false;
  		}
  	}

		void restore() {
		  mBucketTriggerVolume = Bolbro.prefGetFloat("bucketVol", DEFAULT_BUCKET_TRIGGER_VOLUME);
  		mWindSpeedFactor = Bolbro.prefGetFloat("speedFactor", DEFAULT_WINDSPEED_FACTOR);
  		mMeasurementHeight = Bolbro.prefGetFloat("height", DEFAULT_MEASUREMENT_HEIGHT);
  		mSecondsBetweenReports = Bolbro.prefGetUnsignedLong("reportSecs", DEFAULT_SECONDS_BETWEEN_REPORTS);
		}

  	void save() {
  		//	save state to preferences
  		Bolbro.prefSetFloat("bucketVol", mBucketTriggerVolume);
  		Bolbro.prefSetFloat("speedFactor", mWindSpeedFactor);
  		Bolbro.prefSetFloat("height", mMeasurementHeight);
  	  Bolbro.prefSetUnsignedLong("reportSecs", mSecondsBetweenReports);
  	}

  	void revertToDefaults() {
		  mBucketTriggerVolume = DEFAULT_BUCKET_TRIGGER_VOLUME;
  		mWindSpeedFactor = DEFAULT_WINDSPEED_FACTOR;
  		mMeasurementHeight = DEFAULT_MEASUREMENT_HEIGHT;
  		mSecondsBetweenReports = DEFAULT_SECONDS_BETWEEN_REPORTS;
  		save();
  	}

    void printSerial() {
    	Serial.print("magic byte: ");
			Serial.print(mMagicByte);
			Serial.println(mMagicByte==MAGICBYTE?" correct":" wrong");

    	Serial.print("bucket trigger volume: ");
      Serial.print(mBucketTriggerVolume, 0);
      Serial.print(" mm3 (");
			Serial.print(mBucketTriggerVolume/1000.0f, 1);
			Serial.println(" ml)");

			Serial.print("wind speed factor: ");
			Serial.println(mWindSpeedFactor, 2);

			Serial.print("wind speed measurement height: ");
			Serial.print(mMeasurementHeight, 2);
			Serial.println(" m");

			Serial.print("seconds between reports: ");
			Serial.print(mSecondsBetweenReports);
			Serial.println(" s");

			Serial.print("checksum: ");
			Serial.print(mCRC16);
			Serial.println(mCRC16==crc16()?" correct":" wrong");
    }

		String json(String linePrefix = "") {

      String json = linePrefix + "{\n";

      json += linePrefix + "\t\"bucketVol\" : " + String(mBucketTriggerVolume, 1) +",\n";
      json += linePrefix + "\t\"speedFactor\" : " + String(mWindSpeedFactor, 2) +",\n";
      json += linePrefix + "\t\"height\" : " + String(mMeasurementHeight, 2) +",\n";
      json += linePrefix + "\t\"reportSecs\" : " + String(mSecondsBetweenReports) +"\n";

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

#endif // _CALIBRATIONPACKET_H_