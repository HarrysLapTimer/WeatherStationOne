
#include "Sun.h"

#include "WeatherConfig.h"
#include <Math.h>
#include <Bolbro.h>
#include <time.h>

//  sun calculation code base on:

/*-------------------------------------------------------------------------*/
/*              ECOLE DES MINES DE PARIS               */
/*      CENTRE D'ENERGETIQUE - GROUPE TELEDETECTION & MODELISATION     */
/*             Rue Claude Daunesse, BP 207             */
/*           06904 Sophia Antipolis cedex, FRANCE          */
/*      Tel (+33) 04 93 95 74 49   Fax (+33) 04 93 95 75 35      */
/*             E-mail : (name)@cenerg.cma.fr             */
/*-------------------------------------------------------------------------*/
/*   L. Wald - O. Bauer - February 1997                    */
/*   modified 8 July 2004 L. Wald for geocentric - geographic lat      */
/*-------------------------------------------------------------------------*/

double geogr_to_geoce(double phi_g)
{
 double phi;
 double CC=0.99330552; /* Correction factor for converting geographic */
             /* into geocentric latitude. CC=(Rpole/Requator)**2 */
             /* Rpole=6356.752, Requator=6378.137 */
 if((phi_g>=-(M_PI/2.0-0.0002)) || (phi_g<=(M_PI/2.0-0.0002)))
   phi=atan(tan(phi_g)*CC);
 else
   phi=phi_g;
 return(phi);
}

int make_julian_day(int day_of_month, int month_number, int year_number, int *julian_day)
/* Source : */
/* Inputs :
   day_of_month : day of the month (1..31)
   month_number : month number (1..12)
   year_number  : year number (4 digits) */
/* Outputs :
   julian_day : integer day number or julian day (1..366)
   */
/* The procedure "make_julian_day" converts a day given in day, month and year
   into a julian day. Returns 0 if OK, 1 otherwise. */
{
 static int tab[12]={0,31,59,90,120,151,181,212,243,273,304,334};
 int ier,julien;

 ier = 1;
 if( (day_of_month > 0) && (day_of_month < 32) && (month_number > 0) &&
   (month_number < 13) && (year_number > 0) )
  {
   ier = 0;
   julien = day_of_month + tab[month_number-1];
   if( ( ( ((year_number%4) == 0) && ((year_number%100) != 0) ) ||
   ((year_number%400) == 0) ) && (month_number > 2) )  /* leap year */
   julien = julien + 1;
   *julian_day = julien;
  }
 return(ier);
}

int declination_sun(int year_number, int julian_day, double lambda, double *delta)
/* Sources :
   Bourges, B., 1985. Improvement in solar declination computation. Solar
   Energy, 35 (4), 367-369.
   Carvalho, M.J. and Bourges, B., 1986. Program Eufrad 2.0 - User's Guide.
   Project EUFRAT final scientific report, Contract EN3S-0111-F, Solar Energy
   and Development in the European Community, pp. 12.1-12.74.
   Duffie, J.A. and Beckman, W.A., 1980. Solar Engineering of Thermal
   Processes. Wiley-Interscience, New York. */
/* Inputs :
   year_number : year number (4 digits)
   julian_day  : integer day number or julian day (1..366)
   lambda     : longitude (in radians, positive to East) */
/* Outputs :
   delta : solar declination angle at noon (in radians) */
/* The procedure "declination_sun" computes the solar declination at noon in
   solar time (in radians). A single (average) value per day -at noon- is
   adequate for pratical calculations. The noon declination depends on
   longitude, as noon occurs earlier if longitude is East of Greenwich, and
   later if it is West. The chosen algorithm uses 1957 as base year; it is
   basically a truncated Fourier series with six harmonics. Returns 0 if OK, 1
   otherwise. */
{
 int ier;
 double b1,b2,b3,b4,b5,b6,b7;
 double w0,n0,t1,wt = 0.0;

 b1 =  0.0064979;
 b2 =  0.4059059;
 b3 =  0.0020054;
 b4 = -0.0029880;
 b5 = -0.0132296;
 b6 =  0.0063809;
 b7 =  0.0003508;

 ier = 1;
 /* n0 : spring-equinox time expressed in days from the beginning of the year
  i.e. the time in decimal days elapsing from 00:00 hours Jan 1st to the
  spring equinox at Greenwich in a given year */
 /* t1 : time in days, from the spring equinox */
 /* 0.5 represents the decimal day number at noon on Jan 1st at Greenwich */
 n0 = 78.8946 + 0.2422*(year_number-1957) - (int)(0.25*(year_number-1957));
 t1 = - 0.5 - lambda / (2 * M_PI) - n0;
 w0 = 2 * M_PI / 365.2422;
 if( (julian_day > 0) && (julian_day <= 366) )
  {
   ier = 0;
   wt = w0 * (julian_day + t1);
  }

 *delta = b1 + b2 * sin(wt) + b3 * sin(2 * wt) + b4 * sin(3 * wt)
       + b5 * cos(wt) + b6 * cos(2 * wt) + b7 * cos(3 * wt);
 return(ier);
}

int Day_Angle(int julian_day, double *day_angle)
/* Source : */
/* Inputs :
   julian_day : integer day number or julian day (1..366) */
/* Outputs :
   day_angle : day angle (in radians) */
/* The procedure "Day_Angle" expresses the integer day number as an angle (in
   radians) from 12:00 hours on the day 31st December. A year length of
   365.2422 days is used. Returns 0 if OK, 1 otherwise. */
{
 int ier;

 ier = 1;
 if( (julian_day > 0) && (julian_day <= 366) )
  {
   ier = 0;
   *day_angle = (double)julian_day * 2.0 * M_PI / 365.2422;
  }
 return(ier);
}


int UT_to_LAT(double UT, double day_angle, double lambda, double *LAT)
/* Source : */
/* Inputs :
   UT       : Universal Time (in decimal hours)
   day_angle   : day angle (in radians)
   lambda    : longitude of the site (in radians, positive to East) */
/* Outputs :
   LAT : local apparent time or solar time or true solar time (TST) (in decimal
     hours) */
/* The procedure "UT_to_LAT computes the conversion of the UT (Universal time)
   into the LAT (local apparent time) systems at solar noon (in decimal hours).
   First, the equation of time, ET, is computed (in decimal hours), wich allows
   for perturbations in the rotational and angular orbital speed of the Earth.
   Returns 0 if OK, 1 otherwise. */
{
 const double deg_rad = (M_PI/180.0); /* converts decimal degrees into radians*/
 int ier;
 double a1,a2,a3,a4,ET;

 ier = 1;
 a1 = -0.128;
 a2 = -0.165;
 a3 =  2.80 * deg_rad;
 a4 = 19.70 * deg_rad;
 if( (UT >= 0.0) && (UT <= 24.0) && (day_angle > 0.0) &&
   (day_angle < (2.0*M_PI*1.0021)) && (fabs(lambda) <= M_PI) )
  {
   ier = 0;
   ET = a1 * sin(day_angle - a3) + a2 * sin(2.0 * day_angle + a4);
  *LAT = UT + ET + (lambda * 12.0 / M_PI);
  if(*LAT<0) *LAT+=24.0;
  if(*LAT>24.0) *LAT-=24.0;
  }

 return(ier);
}

int solar_hour_angle(double t, double *omega)
/* Source : */
/* Inputs :
   t : solar time i.e. LAT (0..24 decimal hours) */
/* Outputs :
   omega : solar hour angle (in radians) */
/* The procedure "solar_hour_angle" supplies the solar hour angle (in radians).
   By convention the hour angle is negative before noon and positive after noon
   Returns 0 if OK, 1 otherwise. */
{
 int ier;

 ier = 1;
 if((t >= 0.0) && (t <= 24.0))
  {
   ier = 0;
   *omega = (t - 12.0) * M_PI / 12.0;
  }
 return(ier);
}

int sunrise_hour_angle(double phi_g, double delta, double gamma_riset, double *omega_sr, double *omega_ss)
/* Source : */
/* Inputs :
   phi_g     : latitude of site (in radians, positive to North)
   delta     : solar declination angle (in radians)
   gamma_riset : solar elevation near sunrise/sunset:
         - set to  0.0 for astronomical sunrise/sunset
       - set to -1.0 for refraction corrected sunrise/sunset. */
/* Outputs :
   omega_sr : sunrise solar hour angle (in radians)
   omega_ss : sunset solar hour angle (in radians) */
/* The procedure "sunrise_hour_angle" supplies the sunrise and sunset hour
   angles (in radians). Due to the dimension of the solar disk and the effect
   of the atmospheric refraction, the edge of the solar disk will just appear
   (disappear) at the horizon at sunrise (at sunset) when the calculated
   astronomical elevation is 50'. Returns 0 if OK, 1 otherwise. */
{
 const double deg_rad = (M_PI/180.0); /* converts decimal degrees into radians*/
 int ier;
 double horizon,max_delta,cos_omegas = 0.0,omegas = 0.0;
 double phi;

 ier = 1;
 if( (gamma_riset == 0.0) || (gamma_riset == -1.0) )
   ier = 0;
 horizon = (-50.0 / 60.0) * deg_rad;  /* horizon, -50' in radians */
 if(gamma_riset >= horizon)
   horizon = gamma_riset;

 phi=geogr_to_geoce(phi_g);
 max_delta = 23.45 * deg_rad;
 if( (fabs(phi) < (M_PI/2.0)) && (fabs(delta) <= max_delta) && (ier == 0) )
  {
   cos_omegas = (sin(horizon) - (sin(phi) * sin(delta))) /
        (cos(phi) * cos(delta));
   ier = 0;
  }
 else
   ier = 1;

 if(fabs(cos_omegas) < 1.0)
   omegas = acos(cos_omegas);
 if(cos_omegas >= 1.0)  /* the sun is always below the horizon : polar night */
   omegas = 0.0;
 if(cos_omegas <= -1.0)  /* the sun is always above the horizon : polar day */
   omegas = M_PI;

 *omega_sr = -omegas;
 *omega_ss =  omegas;
 return(ier);
}

int elevation_zenith_sun(double phi_g, double delta, double omega, double *gamma, double *theta)
/* Source : */
/* Inputs :
   phi_g : latitude of site (in radians, positive to North)
   delta : solar declination angle (in radians)
   omega : solar hour angle (in radians) */
/* Outputs :
   gamma : solar altitude angle (in radians)
   theta : solar zenithal angle (in radians) */
/* The procedure "elevation_zenith_sun" computes the solar elevation (or
   altitude) angle and the solar zenithal (or incidence) angle. These two
   angles are complementary. Returns 0 if OK, 1 otherwise. */
{
 int ier;
 double omega_sr,omega_ss;
 double phi;

 ier = 1;
 phi=geogr_to_geoce(phi_g);
 ier = sunrise_hour_angle(phi_g,delta,0.0,&omega_sr,&omega_ss);
 if(ier != 0) return(ier);
 if((omega < omega_sr) || (omega > omega_ss))
   *gamma = 0.0;
 else
   *gamma = asin( sin(phi) * sin(delta) + cos(phi) * cos(delta) * cos(omega) );

 if(*gamma < 0.0)
   *gamma = 0.0;

 *theta = (M_PI / 2.0) - *gamma;

 return(ier);
}

int azimuth_sun(double phi_g, double delta, double omega, double gamma, double *alpha)
/* Source : */
/* Inputs :
   phi_g : latitude of site (in radians, positive to North)
   delta : solar declination angle (in radians)
   omega : solar hour angle (in radians)
   gamma : solar altitude angle (in radians) */
/* Outputs :
   alpha : solar azimuthal angle (in radians) */
/* The procedure "azimuth_sun" computes the solar azimuth angle in the Northern
   hemisphere. The azimuth angle has a positive value when the sun is to the
   west of South, i.e. during the afternoon in solar time. For the Southern
   hemisphere, the azimuth angle is measured from North. Returns 0 if OK, 1
   otherwise. */
{
 int ier;
 double cos_as,sin_as,x;
 double phi;

 ier = 0;
 phi=geogr_to_geoce(phi_g);
 cos_as = (sin(phi) * sin(gamma) - sin(delta)) / (cos(phi) * cos(gamma));
 if(phi < 0.0) cos_as = -cos_as;   /* Southern hemisphere */
 sin_as = cos(delta) * sin(omega) / cos(gamma);
 x = acos(cos_as);
 if(fabs(x) > M_PI) ier = 1;
 if(sin_as >= 0.0)
   *alpha =   x;
 else
   *alpha = -x;

 return(ier);
}

//  wrapper functions

static double deg2rad (double deg) {
  return deg*M_PI/180.0;
}

static double rad2deg (double rad) {
  return rad*180.0/M_PI;
}

bool calcSun (float *azimuthP, float *inclinationP)
{
  bool result = false;

  if (Bolbro.timeConfigured()) {
    double phi_g = deg2rad (LATITUDE);
    double lambda = deg2rad (LONGITUDE);
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

    time_t t = time(NULL);
    struct tm *dateTime = gmtime(&t);

    year_number = dateTime->tm_year+1900;
    month_number = dateTime->tm_mon+1;
    day_of_month = dateTime->tm_mday;
  
    if (make_julian_day (day_of_month, month_number, year_number, &julian_day)==0)
    {      
      if (declination_sun (year_number, julian_day, lambda, &delta)==0)
      {  
        UT = (double) dateTime->tm_hour+((double) dateTime->tm_min+(double) dateTime->tm_sec/60.0)/60.0;
    
        if (Day_Angle (julian_day, &day_angle)==0)
        {
          if (UT_to_LAT (UT, day_angle, lambda, &LAT)==0)
          {            
            if (solar_hour_angle (LAT, &omega)==0)
            {              
              if (elevation_zenith_sun (phi_g, delta, omega, &gamma, &theta)==0)
              {
                if (gamma>0.0)
                {
                  //  gamma==0.0 when on or behind horizon
                  if (azimuth_sun (phi_g, delta, omega, gamma, &alpha)==0)
                  {
                      if (LATITUDE>0.0)
                        //  Northern hemisphere, alpha is positiv if west of south
                        alpha += M_PI;
  
                      if (azimuthP)
                        *azimuthP = rad2deg (alpha);
                      if (inclinationP)
                        *inclinationP = rad2deg (gamma);
                
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
