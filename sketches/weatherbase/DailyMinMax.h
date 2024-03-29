/* -------------------------------------------------------------------------------- 
 *  DailyMinMax
 *  memorize in-day, min and max values
 *  Harald Schlangmann, April 2021
 * -------------------------------------------------------------------------------- */

#include <Bolbro.h> // use for time functions

class DailyMinMax
{
  public:

    DailyMinMax(const char *name) {
      mName = name;
      mHasSamples = false;
      mStartOfDaySeconds = 0;
    }

    bool hasSamples() {
      return mHasSamples;
    }

    void addSample(float value) {
      //  check if we are on the same day...
      time_t currentTime = time(NULL);
      struct tm *dayDateTime = localtime(&currentTime);

      dayDateTime->tm_sec = 0;
      dayDateTime->tm_min = 0;
      dayDateTime->tm_hour = 0;

      time_t currentBeginOfDay = mktime(dayDateTime);

      if (!mStartOfDaySeconds||currentBeginOfDay!=mStartOfDaySeconds) {
        //  new day, reset
        mHasSamples = false;
        mStartOfDaySeconds = currentBeginOfDay;
        if (DEBUG) {
          LOG->print("reset min max for ");
          LOG->println(mName);
        }
      }
      
      if (mHasSamples) {
        mLast = value;
        if (value<mMin)
          mMin = value;
        if (value>mMax)
          mMax = value;
      } else {
        mMin = mMax = mLast = value;
        mHasSamples = true;
      }

      if (DEBUG) {
        LOG->printf("updated min to %.1f, max to %.1f and last to %.1f for %s\n",
          mMin, mMax, mLast, mName);
      }
    }

    void addDeltaSample(float deltaValue) {
      if (hasSamples())
        addSample(mLast+deltaValue);
      else
        addSample(deltaValue);
    }

    //  call with hasSamples() true only
    float max() {
      return mMax;      
    }

    //  call with hasSamples() true only
    float min() {
      return mMin;      
    }

    //  call with hasSamples() true only
    float range() {
      return max()-min();
    }

  private:

    const char *mName;
    float mMin, mMax, mLast;
    bool mHasSamples;
    time_t mStartOfDaySeconds;
};
