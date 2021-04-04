//
//  binary packet of data
//  besides doing a typed storage, it features encoding / decoding functions
//
//  to encode, use encodedBytes() and encodedSize()
//  to decode, feed bytes into decodeByte(); once it has found a valid packet, true is returned
//

#ifndef _PACKET_H_
#define _PACKET_H_

#include <Bolbro.h>
#include <WeatherConfig.h>

#define UNDEFINEDVALUE  -1.0
#define MAGICBYTE 0xCC

class Packet {

  /**************************************************************************

    for low level transfer, specializations need to implement a continuous
    memory section starting with

      protected:
        //  header
        byte mMagicByte;

      public:
        //  typed data to transfer
        ...

      protected:
        //  CRC16 checksum
        uint16_t  mCRC16;

    in addition, specializations need to implement two methods to access
    the memory area surrounded by mMagicByte and mCRC16

      protected:
        byte *magicByteAddr() { return &mMagicByte; }
        uint16_t *crc16Addr() { return &mCRC16; }

   **************************************************************************/

  private:

    //  internal write status for decodeByte(), not included in encoded packet
    uint8_t mDecodePos;

    uint16_t crc16(uint8_t* data_p, uint16_t length) {
      unsigned char x;
      uint16_t crc = 0xFFFF;

      while (length--){
        x = crc >> 8 ^ *data_p++;
        x ^= x>>4;
        crc = (crc << 8) ^ ((uint16_t)(x << 12)) ^ ((uint16_t)(x <<5)) ^ ((uint16_t)x);
      }

      return crc;
    }

  protected:

    //  CRC16 so it checksums everything starting including mMagicByte and excluding mCRC16 itself
    uint16_t crc16() {
      return crc16(encodedBytes(false), encodedSize()-sizeof(uint16_t));
    }

  public:

    Packet() {
      mDecodePos = 0;
    }

    //  for debugging
    virtual void printSerial() = 0;

    //  see comment above
    virtual byte *magicByteAddr() = 0;
    virtual uint16_t *crc16Addr() = 0;

    uint8_t *encodedBytes(bool setCRC = true) {
      *(magicByteAddr()) = MAGICBYTE;
      if (setCRC) {
        *(crc16Addr()) = crc16();
        if (DEBUG) {
          Serial.print("setting CRC to ");
          Serial.println(*(crc16Addr()));
        }
      }
      return (uint8_t *) magicByteAddr();
    }

    uint16_t encodedSize() {
      return (uint8_t *) crc16Addr() - (uint8_t *) magicByteAddr() + sizeof(uint16_t);
    }

    bool decodeByte(byte b) {
      if (DEBUG) {
        Serial.print(b);
        Serial.print(" ");
      }
      encodedBytes(false)[mDecodePos] = b;
      if (mDecodePos==0&&b!=MAGICBYTE) {
        //  wait for the starting byte and skip otherwise
        if (DEBUG)
          Serial.println("skipping because not magic number");
        return false;
      } else {
        mDecodePos++;
        if (mDecodePos==encodedSize()) {
          mDecodePos = 0;
          //  full list of bytes received, check sum
          if (*(crc16Addr())==crc16()) {
            //  valid packet decoded
            if (DEBUG)
              Serial.println("decoded a valid packet");
            return true;
          } else {
            //  corrupted packet, reset
            if (DEBUG)
              Serial.println("decoded to a corrupted packet, skipping...");
            return false;
          }
        } else
          //  not yet finished
          return false;
      }
    }
};

#endif // _PACKET_H_