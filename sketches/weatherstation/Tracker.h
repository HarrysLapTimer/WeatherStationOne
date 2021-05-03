/* -------------------------------------------------------------------------------- 
 *  Tracker
 *  solar tracker featuring horizontal tracking
 *  Harald Schlangmann, April 2021
 * -------------------------------------------------------------------------------- */

#include "WeatherConfig.h"

#include <AccelStepper.h>
#include <limits.h>

class Tracker
{
  private:

    AccelStepper  *mStepper;
#define UNDEFINEDPOSITION LONG_MIN
    long mPosition; // used to store stepper state during deep sleep and calibration

    void restorePosition() {
      if (mStepper&&mPosition!=UNDEFINEDPOSITION) {
        if (DEBUG) {
          Serial.print("restoring position ");
          Serial.println(mPosition);
        }
        mStepper->moveTo(mPosition);
        mPosition = UNDEFINEDPOSITION;
      }
    }
    
    void setDefaults() {
      mState = UndefinedStatus;
      mPosition = UNDEFINEDPOSITION;
    }

    enum {
      UndefinedStatus,
      CalibratingStatus,
      UnlockingStatus, // tracker has been turned on in calibration position - move away
      StartMinMaxStatus, // go to min, to max, and back to old position
      MinDegreeStatus,
      MaxDegreeStatus,
      RunningStatus
    } mState;

    static long degree2position(float degree) {
      //  position 0 is defined as 180 degree, negative position delta means degree counter clockwise
      long position = (180l-degree)*STEPS_PER_REVOLUTION/360l;
      return position;
    }

    static float position2degree(long position) {
      float degree = 180.0-position*360.0/STEPS_PER_REVOLUTION;
      return degree;
    }
    
  public:

    Tracker(bool &initializeMembers) {
      if (initializeMembers) {
        setDefaults();

        //  initialize only once; working when in RTC memory
        initializeMembers = false;
      }

      //  create a stepper from members
      //  pins entered in sequence IN1-IN3-IN2-IN4 for proper step sequence
      mStepper = new AccelStepper(AccelStepper::FULL4WIRE, 
        STEPPER_IN1_PIN, STEPPER_IN3_PIN, STEPPER_IN2_PIN, STEPPER_IN4_PIN);
      mStepper->setMaxSpeed(1000.0);
      mStepper->setAcceleration(50.0);
      mStepper->setSpeed(200);
      if (mPosition!=UNDEFINEDPOSITION) {
        mStepper->setCurrentPosition(mPosition);
        mPosition = UNDEFINEDPOSITION;
      } else
        //  this means mState will be in UndefinedStatus
        mStepper->setCurrentPosition(degree2position(TRACKER_MAX_DEGREE));

      //  prepate calibration
      pinMode(TRACKER_CALIBRATION_PIN, INPUT_PULLDOWN);
    }

    virtual ~Tracker() {
      deepSleep();
    }

    void deepSleep() {
      if (mStepper) {
        mPosition = mStepper->currentPosition();
        delete mStepper;
        mStepper = 0;
      }     
    }

    //  operations to perform
    void calibrate() {
      //  reset status so the next run() will start calibration
      mState = UndefinedStatus;
      mPosition = mStepper->targetPosition();
    }

    //  returns true in case the stepper is calibrated and can be controlled
    bool canBeControlled() {
      return mState==RunningStatus;
    }

    bool isMoving() {
      return mStepper->distanceToGo() != 0;
    }

    void moveTo(float degree) {
      if (canBeControlled()) {
        if (degree<TRACKER_MIN_DEGREE)
          degree = TRACKER_MIN_DEGREE;
        if (degree>TRACKER_MAX_DEGREE)
          degree = TRACKER_MAX_DEGREE;
        long newPosition = degree2position(degree);
        
        if (newPosition>mStepper->targetPosition()) {
          if (DEBUG) {
            Serial.print("detected backward movement from ");
            Serial.print(mStepper->targetPosition());
            Serial.print(" to ");
            Serial.println(newPosition);
          }
          //  we assume the tracker is going from negative to positive usually
          //  in case newPosition is smaller than old one, we are going to south (parking)
          //  or - at end of night - to East; add a calibration here...
          if (DEBUG) {
            Serial.print("triggering calibration and moving to position ");
            Serial.print(newPosition);
            Serial.print(" (");
            Serial.print(degree);
            Serial.println(" degree) afterwards");
          }
          calibrate();
          mPosition = newPosition; // restire this value at end of calibration
        } else {
          if (DEBUG&&newPosition!=mStepper->targetPosition()) {
            Serial.print("moving to position ");
            Serial.print(newPosition);
            Serial.print(" (");
            Serial.print(degree);
            Serial.println(" degree)");
          }          
          //  AccelStepper::moveTo() is a no-op in case the position is unchanged
          mStepper->moveTo(newPosition);
        }
      } else
        Serial.println("error, tracker moved while not calibrated!");
    }

    void minMaxTesting() {
      if (canBeControlled())
        mState = StartMinMaxStatus;
    }

    //  call from loop() regularly
    void run() {
#if 0
      Serial.print("current position ");
      Serial.print(mStepper->currentPosition());
      Serial.print(", target position ");
      Serial.print(mStepper->targetPosition ());
      Serial.print(", distance to go ");
      Serial.print(mStepper->distanceToGo ());
      Serial.print(", home position ");
      Serial.println(digitalRead(TRACKER_CALIBRATION_PIN)?"YES":"NO");
#endif

      switch(mState) {
        case UndefinedStatus:
          if(digitalRead(TRACKER_CALIBRATION_PIN)) {
            if (DEBUG)
              Serial.println("home position reached, switching to UnlockingStatus and moving by 20 degrees");
            mStepper->move(-STEPS_PER_REVOLUTION*20/360); // move 20 degrees clockwise
            mState = UnlockingStatus;
          } else {
            if (DEBUG)
              Serial.println("not in home position, switching to CalibratingStatus");
            mState = CalibratingStatus;
            mStepper->move(STEPS_PER_REVOLUTION);
          }
          break;
        case UnlockingStatus:
          if(mStepper->distanceToGo() == 0) {
            if (DEBUG)
              Serial.println("reached none-home position, switching to CalibratingStatus");
            mState = CalibratingStatus;
            mStepper->move(STEPS_PER_REVOLUTION);
          }
          break;
        case CalibratingStatus:
          if(digitalRead(TRACKER_CALIBRATION_PIN)) {
            // found home position, we are ready with calibration
            if (DEBUG)
              Serial.println("found home position, switching to RunningStatus");
            mStepper->stop();
            mStepper->setCurrentPosition(degree2position(TRACKER_HOME_DEGREE));
            mState = RunningStatus;
            restorePosition();
          } else if(mStepper->distanceToGo() == 0) {
            Serial.println("calibration failed!");
          }
          break;
        case StartMinMaxStatus:
          mPosition = mStepper->targetPosition(); // memorize
          if (DEBUG)
            Serial.println("started min/max testing, switching to MinDegreeStatus");
          mStepper->moveTo(degree2position(TRACKER_MIN_DEGREE));
          mState = MinDegreeStatus;
          break;
        case MinDegreeStatus:
          if (mStepper->distanceToGo() == 0) {
            //  min reached, goto max
            if (DEBUG)
              Serial.println("reached min, switching to MaxDegreeStatus");
            mStepper->moveTo(degree2position(TRACKER_MAX_DEGREE));
            mState = MaxDegreeStatus;
          }
          break;
        case MaxDegreeStatus:
          if (mStepper->distanceToGo() == 0) {
            //  max reached, goto memorized position
            if (DEBUG)
              Serial.println("reached max, switching to RunningStatus");
            restorePosition();
            mState = RunningStatus;
          }
          break;
        case RunningStatus:
          //  power saving...
          if (mStepper->distanceToGo() == 0)
            mStepper->disableOutputs();
          else
            mStepper->enableOutputs(); 
          break;
      }
    
      // Move the motor one step
      mStepper->run();
    }
};
