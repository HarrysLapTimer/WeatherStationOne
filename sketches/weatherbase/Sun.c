
#include "Sun.h"

#include "WeatherConfig.h"
#include <Math.h>
#include <Bolbro.h>

static double deg2rad (double deg)
{
  return deg*M_PI/180.0;
}

static double rad2deg (double rad)
{
  return rad*180.0/M_PI;
}

bool calcSun (float *azimuthP, float *inclinationP)
{
  bool result = false;

  if (Bolbro.timeConfigured()) {
    double phi_g = deg2rad (LATITUDE);
    double lambda = Deg2Rad (LONGITUDE);
    double delta;
    double omega;
    double gamma;
    double alpha;
    double UT;
    double LAT;
    double theta;
    double day_angle;
    int year_number, month_number, day_of_month;
    int julian_day;
  
    struct tm *dateTime = gmtime(time(NULL));

    year_number = dateTime.year+1900;
    month_number = dateTime.month+1;
    day_of_month = dateTime.day;
  
    //TRACE (TRACEGPSCALC, IndentUnchanged,
    //    "year_number: %hu, month_number: %hu, day_of_month: %hu",
    //    (UInt16) year_number, (UInt16) month_number, (UInt16) day_of_month);
    
    if (make_julian_day (day_of_month, month_number, year_number, &julian_day)==0)
    {
      //TRACE (TRACEGPSCALC, IndentUnchanged, "julian_day: %hu", (UInt16) julian_day);
      
      if (declination_sun (year_number, julian_day, lambda, &delta)==0)
      {
        //TRACE (TRACEGPSCALC, IndentUnchanged,
        //  "delta (solar declination angle): %s (%hd" DEGREESIGNCP1252 ")",
        //  TRACEDOUBLE (delta), (Int16) Rad2Deg (delta));
  
        UT = (double) dateTime->hour+((double) dateTime->minute+(double) dateTime->second/60.0)/60.0;
  
        //TRACE (TRACEGPSCALC, IndentUnchanged, "universal time: %s", TRACEDOUBLE (UT));
  
        if (Day_Angle (julian_day, &day_angle)==0)
        {
          if (UT_to_LAT (UT, day_angle, lambda, &LAT)==0)
          {
            //TRACE (TRACEGPSCALC, IndentUnchanged, "solar time: %s", TRACEDOUBLE (LAT));
            
            if (solar_hour_angle (LAT, &omega)==0)
            {
              //TRACE (TRACEGPSCALC, IndentUnchanged,
              //  "omega (solar hour angle): %s (%hd" DEGREESIGNCP1252 ")",
              //  TRACEDOUBLE (omega), (Int16)Rad2Deg (omega));
              
              if (elevation_zenith_sun (phi_g, delta, omega, &gamma, &theta)==0)
              {
                //TRACE (TRACEGPSCALC, IndentUnchanged,
                //  "gamma (solar altitude or elevation angle): %s (%hd" DEGREESIGNCP1252 ")",
                //  TRACEDOUBLE (gamma), (Int16)Rad2Deg (gamma));
                
                if (gamma>0.0)
                {
                  //  gamma==0.0 when on or behind horizon
                  if (azimuth_sun (phi_g, delta, omega, gamma, &alpha)==0)
                  {
                      if (pos->latitude>0.0)
                        //  Northern hemisphere, alpha is positiv if west of south
                        alpha += M_PI;
  
                      *azimutP = rad2deg (alpha);
                      *inclinationP = rad2deg (gamma);
        
                      //TRACE (TRACEGPSCALC, IndentUnchanged, "azimut: %hu" DEGREESIGNCP1252 ", elevation: %hu" DEGREESIGNCP1252, *azimut, *height);
        
                      result = true;
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  return result;
}
