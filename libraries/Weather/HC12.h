//
//  HC12 wrapper with power management and transmission settings support
//

#include <Arduino.h> // other than ino, hpp/cpp do not have this by default

class HC12Class {

  public:

    //  constructor
    HC12Class();

    //  setup HC12 for communication
    void begin();
    void end();
    
    //  write a number of bytes to serial2
    void write(uint8_t *bytes, int bytesNum);

    //  read a byte if available
    bool available();
    uint8_t read();
    
  private:

    bool mBeginCalled;
};

//  provide singleton
extern HC12Class HC12;
