//
//  binary packet of calibration parameters
//

#include <Bolbro.h>
#include <Packet.h>

class CalibrationPacket : public Packet {

  private:

    //  header
    byte mMagicByte;

  public:

    //  calibration
    float mBucketTriggerVolume;
    float mWindSpeedFactor;

    //  settings
    unsigned long mSecondsBetweenReports;

  private:

    //  CRC16 checksum
    uint16_t  mCRC16;

  public:

    CalibrationPacket() : Packet() {
      mBucketTriggerVolume = DEFAULT_BUCKET_TRIGGER_VOLUME;
      mWindSpeedFactor = DEFAULT_WINDSPEED_FACTOR;
      mSecondsBetweenReports = DEFAULT_SECONDS_BETWEEN_REPORTS;
    }

    void printSerial() {
      if (mCRC16==crc16()) {
        Serial.print("bucket trigger volume: ");
        Serial.print(mBucketTriggerVolume, 1);
        Serial.println(" ml");

        Serial.print("wind speed factor: ");
        Serial.println(mWindSpeedFactor, 2);

        Serial.print("seconds between reports: ");
        Serial.print(mSecondsBetweenReports);
        Serial.println(" s");

      } else {
        Serial.println("printing of corrupted calibration packet failed");
      }
    }

  protected:

    byte *magicByteAddr() {
      return &mMagicByte;
    }

    uint16_t *crc16Addr() {
      return &mCRC16;
    }
};
