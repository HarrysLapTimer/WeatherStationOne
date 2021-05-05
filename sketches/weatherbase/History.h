/* -------------------------------------------------------------------------------- 
 *  History
 *  store a time series and allow aggregation
 *  memory management not super efficient, do not use for excessive time and samples
 *  Harald Schlangmann, April 2021
 * -------------------------------------------------------------------------------- */

#include "WeatherConfig.h"

class History
{
  public:

    History(const char *name, int seconds) {
      mSeconds = seconds;
      mName = name;
      mSamples = NULL;

      mAggregatedVoid = true;
      mMax = mMin = mAvg = 0;
      
      mCapacity = mCount = 0;
    }

    virtual ~History() {
      if (mCount>0)
        free(mSamples);
    }

    bool hasSamples() {
      return mCount>0;
    }

    void addSample(float value) {
      expire();
      
      if (mCount>=mCapacity) {
        //  request more space
#define CAPACITY_INCREASE 50
        mCapacity += CAPACITY_INCREASE;
        struct Sample *newSamplesP = (struct Sample *) malloc(sizeof(struct Sample)*mCapacity);

        if (mSamples) {
          //  copy existing samples
          memcpy(newSamplesP, mSamples, sizeof(struct Sample)*mCount);

          //  clean up
          free(mSamples);
        }
        
        mSamples = newSamplesP;

        if (DEBUG) {
          Serial.print("history ");
          Serial.print(mName);
          Serial.print(" increased capacity to ");
          Serial.println(mCapacity);
        }
      }

      //  add value
      mSamples[mCount].time = millis();
      mSamples[mCount++].value = value;

      if (DEBUG) {
        Serial.print("history ");
        Serial.print(mName);
        Serial.print(" added sample ");
        Serial.println(value);
      }
        
      if (!mAggregatedVoid) {
        //  reflect in aggregates
        if (value>mMax)
          mMax = value;
        if (value<mMin)
          mMin = value;
        if (mCount==1)
          mAvg = value;
        else if (mCount==2)
          mAvg = (mSamples[0].value + mSamples[1].value)/2.0;
        else { // mCount > 2
          if (mSamples[mCount-1].time-mSamples[0].time)
            mAvg = (mAvg*(mSamples[mCount-2].time-mSamples[0].time)
                    +(mSamples[mCount-2].value+value)/2*(mSamples[mCount-1].time-mSamples[mCount-2].time))
                    /(mSamples[mCount-1].time-mSamples[0].time); // time weighted
          else
            mAvg = (mAvg*(mCount-1)+value)/mCount; // not time weighted
        }
      }

      if (DEBUG)
        printSerial();
    }

    void addDeltaSample(float deltaValue) {
      if (hasSamples())
        addSample(mSamples[mCount-1].value+deltaValue);
      else
        addSample(deltaValue);
    }

    //  call with hasSamples() true only
    float avg() {
      if (mAggregatedVoid)
        aggregate();
      return mAvg;
    }

    //  call with hasSamples() true only
    float max() {
      if (mAggregatedVoid)
        aggregate();
      return mMax;      
    }

    //  call with hasSamples() true only
    float min() {
      if (mAggregatedVoid)
        aggregate();
      return mMin;      
    }

    //  call with hasSamples() true only
    float range() {
      return max()-min();
    }

    float change() {
      if (mCount>=2)
        return mSamples[mCount-1].value-mSamples[0].value;
      else
        return 0;
    }

    void printSerial() {
      Serial.print("history ");
      Serial.print(mName);
      Serial.println(":");
      Serial.print(" ");
      Serial.print(mCount);
      Serial.print(" samples: ");

      for (int i = 0; i<mCount; i++) {
        Serial.print(mSamples[i].value);
        Serial.print("/");
        Serial.print(mSamples[i].time);
        Serial.print(" ");
      }
      Serial.println();

      if (mAggregatedVoid)
        Serial.println(" aggregated void");
      else {
        Serial.print(" min: ");
        Serial.println(mMin);
        Serial.print(" max: ");
        Serial.println(mMax);
        Serial.print(" avg: ");
        Serial.println(mAvg);
      }
    }

  private:

    //  history property
    int mSeconds;
    const char *mName;

    //  aggregated values
    float mAvg;
    float mMax, mMin;
    bool mAggregatedVoid;

    //  content
    struct Sample {
      unsigned long time;
      float value;
    } *mSamples;
    int mCapacity;
    int mCount;

    //  drop samples older than mSeconds, returns true in case items have been removed
    bool expire() {
      if (mCount) {
        int numToExpire = 0;
        unsigned long currentMillis = millis();
  
        for (int i = 0; i<mCount; i++)  {
          unsigned long millisPassed = currentMillis-mSamples[i].time;
          
          if (millisPassed>mSeconds*1000ul)
            numToExpire++;
          else
            break;
        }
  
        if (numToExpire) {
          memmove(mSamples, mSamples+numToExpire, sizeof(struct Sample)*(mCount-numToExpire));
          mCount -= numToExpire;
          mAggregatedVoid = true;

          if (DEBUG) {
            Serial.print("history ");
            Serial.print(mName);
            Serial.print(" expired ");
            Serial.print(numToExpire);
            Serial.println(" samples");
          }

          return true;
        }
      }

      return false;
    }

    void aggregate() {
      if (mAggregatedVoid&&mCount) {
        mMax = mMin = mAvg = mSamples[0].value;
        for (int i = 1; i<mCount; i++) {
          if (mSamples[i].value>mMax)
            mMax = mSamples[i].value;
          if (mSamples[i].value<mMin)
            mMin = mSamples[i].value;
          if (i==1)
            mAvg = (mAvg+mSamples[i].value)/2;
          else
            if (mSamples[i].time-mSamples[0].time)
              mAvg = (mAvg*(mSamples[i-1].time-mSamples[0].time)
                    +(mSamples[i-1].value+mSamples[i].value)/2*(mSamples[i].time-mSamples[i-1].time))
                    /(mSamples[i].time-mSamples[0].time); // time weighted
            else
              mAvg = (mAvg*i+mSamples[i].value)/(i+1); // not time weighted
        }
        mAggregatedVoid = false;
        
        if (DEBUG) {
          Serial.print("history ");
          Serial.print(mName);
          Serial.print(" fully aggregated: max = ");
          Serial.print(mMax);
          Serial.print(" , min = ");
          Serial.print(mMin);
          Serial.print(" , avg = ");
          Serial.println(mAvg);
        }
      }
    }
};
