//
//  HC12 wrapper with power management and transmission settings support
//

#include <HC12.h>
#include <WeatherConfig.h>

#define SET_RTC_HOLD  0

#if SET_RTC_HOLD
# include <driver/rtc_io.h>
#endif

//  instantiate singleton
HC12Class HC12;

//  constructor
HC12Class::HC12Class() {
  mBeginCalled = false;
}

//  setup HC12 for communication
void HC12Class::begin() {
  if (DEBUG)
    Serial.println("starting up HC-12...");
  //  goto transparent / normal mode
  pinMode(HC12_SET_PIN, OUTPUT);

  //  sequence to wake up HC12
  digitalWrite(HC12_SET_PIN, LOW);
  delay(200);
  digitalWrite(HC12_SET_PIN, HIGH);

  Serial2.begin(9600, SERIAL_8N1, HC12_RXD_PIN, HC12_TXD_PIN);
  mBeginCalled = true;
}

void HC12Class::end() {
  if (mBeginCalled) {
    if (DEBUG)
      Serial.println("flushing HC-12 buffer and sending it to sleep...");
    Serial2.flush();
    delay(200); // allow transmission of pending bytes

    //  sequence to sleep HC-12
    digitalWrite(HC12_SET_PIN, LOW);
    delay(200);
    Serial2.print("AT+SLEEP");
    delay(200);
    digitalWrite(HC12_SET_PIN, HIGH);
#if SET_RTC_HOLD
    rtc_gpio_hold_en((gpio_num_t) HC12_SET_PIN); // make sure HIGH level is kept during deep sleep
#endif

    mBeginCalled = false;
  }
}

//  write a number of bytes to serial2
void HC12Class::write(uint8_t *bytes, int bytesNum) {
  for (int i = 0; i<bytesNum; i++) {
    Serial2.write(bytes[i]);
    if (DEBUG) {
      Serial.print(bytes[i]);
      Serial.print(" ");
    }
  }

  if (DEBUG) {
    Serial.println();
    Serial.print("sent ");
    Serial.print(bytesNum);
    Serial.println(" bytes...");
  }
}

bool HC12Class::available() {
  return Serial2.available();
}

uint8_t HC12Class::read() {
  return Serial2.read();
}
