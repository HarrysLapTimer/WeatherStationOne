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
          Serial.print("reset min max for ");
          Serial.println(mName);
        }
      }
      
      if (mHasSamples) {
        if (value<mMin)
          mMin = value;
        if (value>mMax)
          mMax = value;
      } else {
        mMin = mMax = value;
        mHasSamples = true;
      }
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
    float mMin, mMax;
    bool mHasSamples;
    time_t mStartOfDaySeconds;
};
