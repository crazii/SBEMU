//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2008 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
// time,delay functions

#include <conio.h>
#include <time.h>
#if defined(__GNUC__)
#include <sys/time.h>
#endif
#include "mpxplay.h"

#ifdef __DOS__
#include <dos.h>
extern void (__far __interrupt *oldint08_handler)();
extern volatile unsigned long int08counter;
#endif

unsigned long pds_gettimeh(void)
{
 return ((unsigned long)clock()*100/CLOCKS_PER_SEC);
}

mpxp_int64_t pds_gettimem(void)
{
    mpxp_int64_t time_ms;
#ifdef __DOS__
    if(oldint08_handler){
        unsigned long tsc;
        _disable();
        outp(0x43, 0x04);
        tsc = inp(0x40);
        tsc += inp(0x40) << 8;
        _enable();
        if(tsc < INT08_DIVISOR_NEW)
            tsc = INT08_DIVISOR_NEW - tsc;
        else
            tsc = 0;
        time_ms = (mpxp_int64_t)(((float)int08counter+(float)tsc/(float)INT08_DIVISOR_NEW)*1000.0/(float)INT08_CYCLES_NEW);
        //fprintf(stderr,"time_ms:%d \n",time_ms);
    }else
#elif defined(_POSIX_TIMERS) && defined(_POSIX_MONOTONIC_CLOCK)
    struct timespec tss;
    if(clock_gettime(CLOCK_MONOTONIC, &tss) == 0)
    {
        time_ms = (mpxp_int64_t)tss.tv_sec * (mpxp_int64_t)1000 + (mpxp_int64_t)(tss.tv_nsec / 1000000);
    }else
#endif
    time_ms = (mpxp_int64_t)clock() * (mpxp_int64_t)1000 / (mpxp_int64_t)CLOCKS_PER_SEC;

    return time_ms;
}

mpxp_int64_t pds_gettimeu(void)
{
    mpxp_int64_t time_ms;
#ifdef __DOS__
    if(oldint08_handler){
        unsigned long tsc;
        _disable();
        outp(0x43,0x04);
        tsc = inp(0x40);
        tsc += inp(0x40)<<8;
        _enable();
        if(tsc < INT08_DIVISOR_NEW)
            tsc = INT08_DIVISOR_NEW - tsc;
        else
            tsc = 0;
        time_ms = (mpxp_int64_t)(((float)int08counter+(float)tsc/(float)INT08_DIVISOR_NEW)*1000000.0/(float)INT08_CYCLES_NEW);
        //fprintf(stderr,"time_ms:%d \n",(long)time_ms);
    }else
#elif defined(_POSIX_TIMERS) && defined(_POSIX_MONOTONIC_CLOCK)
    struct timespec tss;
    if(clock_gettime(CLOCK_MONOTONIC, &tss) == 0)
    {
        time_ms = (mpxp_int64_t)tss.tv_sec * (mpxp_int64_t)1000000 + (mpxp_int64_t)(tss.tv_nsec / 1000);
    }else
#endif
    time_ms = (mpxp_int64_t)clock() * (mpxp_int64_t)1000000 / (mpxp_int64_t)CLOCKS_PER_SEC;

    return time_ms;
}

unsigned long pds_gettime(void)
{
 unsigned long timeval;
 time_t timer;
 struct tm *t;
 timer=time(NULL);
 t=localtime(&timer);
 timeval=(t)? (t->tm_sec&63)|((t->tm_min&63)<<8)|((t->tm_hour&31)<<16) : 0;
 return timeval; // 0x00HHMMSS
}

unsigned long pds_getdate(void)
{
 unsigned long dateval;
 time_t timer;
 struct tm *t;
 timer=time(NULL);
 t=localtime(&timer);
 dateval=(t)? (t->tm_mday&31)|(((t->tm_mon+1)&15)<<8)|(((t->tm_year+1900)&65535)<<16) : 0;
 return dateval; // 0xYYYYMMDD
}

#if defined(MPXPLAY_GUI_QT) || defined(MPXPLAY_LINK_ORIGINAL_FFMPEG)
// get date and time in 0xYYYYMMDD00HHMMSS format (not BCD, just byte aligned)
mpxp_uint64_t pds_getdatetime(void)
{
 time_t timerval = time(NULL);
 struct tm *t = localtime(&timerval);
 mpxp_uint64_t localdatetimeval;
 if(t){
  localdatetimeval = ((mpxp_uint64_t)(t->tm_mday&31) << 32) | ((mpxp_uint64_t)((t->tm_mon+1)&15) << 40) | ((mpxp_uint64_t)((t->tm_year+1900)&65535) << 48);
  localdatetimeval |= (mpxp_uint64_t)(t->tm_sec&63) | (mpxp_uint64_t)((t->tm_min&63) << 8) | (mpxp_uint64_t)((t->tm_hour&31) << 16);
 }else
  localdatetimeval = 0;
 return localdatetimeval; // 0xYYYYMMDD00HHMMSS
}

// convert UTC date/time 0xYYYYMMDD00HHMMSS to local 0xYYYYMMDD00HHMMSS (not BCD, just byte aligned)
mpxp_uint64_t pds_utctime_to_localtime(mpxp_uint64_t utc_datetime_val)
{
 mpxp_uint64_t localdatetimeval;
 time_t timerval;
 struct tm t_in, *t_out;
 t_in.tm_isdst = 0; // TODO: ???
 t_in.tm_wday = 0;
 t_in.tm_yday = 0;
 t_in.tm_year = (utc_datetime_val >> 48) - 1900;
 t_in.tm_mon = ((utc_datetime_val >> 40) & 0xFF) - 1;
 t_in.tm_mday = (utc_datetime_val >> 32) & 0xFF;
 t_in.tm_hour = (utc_datetime_val >> 16) & 0xFF;
 t_in.tm_min = (utc_datetime_val >> 8) & 0xFF;
 t_in.tm_sec = (utc_datetime_val & 0xFF);
 timerval = pds_mkgmtime(&t_in);
 t_out = localtime(&timerval);
 if(t_out){
  localdatetimeval = ((mpxp_uint64_t)(t_out->tm_mday&31) << 32) | ((mpxp_uint64_t)((t_out->tm_mon+1)&15) << 40) | ((mpxp_uint64_t)((t_out->tm_year+1900)&65535) << 48);
  localdatetimeval |= (mpxp_uint64_t)(t_out->tm_sec&63) | (mpxp_uint64_t)((t_out->tm_min&63) << 8) | (mpxp_uint64_t)((t_out->tm_hour&31) << 16);
 }else
  localdatetimeval = utc_datetime_val;
 return localdatetimeval;
}

#if 0 // unused
// calculate time_sec diff between two 0xYYYYMMDD00HHMMSS values
mpxp_int32_t pds_datetimeval_difftime(mpxp_uint64_t datetime_val1, mpxp_uint64_t datetime_val0)
{
 time_t timerval_1, timerval_0;
 struct tm t_in1, t_in0;
 double timediff;

 t_in0.tm_isdst = 1; // TODO: ???
 t_in0.tm_wday = 0;
 t_in0.tm_yday = 0;
 t_in0.tm_year = (datetime_val0 >> 48) - 1900;
 t_in0.tm_mon = ((datetime_val0 >> 40) & 0xFF) - 1;
 t_in0.tm_mday = (datetime_val0 >> 32) & 0xFF;
 t_in0.tm_hour = (datetime_val0 >> 16) & 0xFF;
 t_in0.tm_min = (datetime_val0 >> 8) & 0xFF;
 t_in0.tm_sec = (datetime_val0 & 0xFF);
 timerval_0 = mktime(&t_in0);

 t_in1.tm_isdst = 1; // TODO: ???
 t_in1.tm_wday = 0;
 t_in1.tm_yday = 0;
 t_in1.tm_year = (datetime_val1 >> 48) - 1900;
 t_in1.tm_mon = ((datetime_val1 >> 40) & 0xFF) - 1;
 t_in1.tm_mday = (datetime_val1 >> 32) & 0xFF;
 t_in1.tm_hour = (datetime_val1 >> 16) & 0xFF;
 t_in1.tm_min = (datetime_val1 >> 8) & 0xFF;
 t_in1.tm_sec = (datetime_val1 & 0xFF);
 timerval_1 = mktime(&t_in1);

 timediff = difftime(timerval_1, timerval_0);
 return (mpxp_int32_t)timediff;
}

// returns elapsed time from datetime_val 0xYYYYMMDD00HHMMSS
mpxp_int32_t pds_datetimeval_elapsedtime(mpxp_uint64_t datetime_val)
{
 time_t timerval_in, timerval_curr;
 struct tm t_in;
 double timediff;
 t_in.tm_isdst = 1; // TODO: ???
 t_in.tm_wday = 0;
 t_in.tm_yday = 0;
 t_in.tm_year = (datetime_val >> 48) - 1900;
 t_in.tm_mon = ((datetime_val >> 40) & 0xFF) - 1;
 t_in.tm_mday = (datetime_val >> 32) & 0xFF;
 t_in.tm_hour = (datetime_val >> 16) & 0xFF;
 t_in.tm_min = (datetime_val >> 8) & 0xFF;
 t_in.tm_sec = (datetime_val & 0xFF);
 timerval_in = mktime(&t_in);
 timerval_curr = time(NULL);
 timediff = difftime(timerval_curr, timerval_in);
 return (mpxp_int32_t)timediff;
}
#endif // 0 unused

// convert 0x0000000000HHMMSS to seconds
mpxp_int32_t pds_timeval_to_seconds(mpxp_uint32_t datetime_val)
{
 mpxp_int32_t seconds;
 seconds  = ((datetime_val >> 16) & 0xFF) * 3600;
 seconds += ((datetime_val >>  8) & 0xFF) * 60;
 seconds += (datetime_val & 0xFF);
 return seconds;
}

#if 1
mpxp_int64_t pds_datetimeval_to_seconds(mpxp_uint64_t datetime_val)
{
 mpxp_int64_t seconds;
 struct tm t_in;
 t_in.tm_isdst = 0; // TODO: ???
 t_in.tm_wday = 0;
 t_in.tm_yday = 0;
 t_in.tm_year = (datetime_val >> 48) - 1900;
 t_in.tm_mon = ((datetime_val >> 40) & 0xFF) - 1;
 t_in.tm_mday = (datetime_val >> 32) & 0xFF;
 t_in.tm_hour = (datetime_val >> 16) & 0xFF;
 t_in.tm_min = (datetime_val >> 8) & 0xFF;
 t_in.tm_sec = (datetime_val & 0xFF);
 seconds = (mpxp_int64_t)mktime(&t_in);
 seconds += (mpxp_int64_t)1970 * 146097 * 24 * 3600 / 400;
 return seconds;
}
#else
mpxp_int64_t pds_datetimeval_to_seconds(mpxp_uint64_t datetime_val)
{
 const mpxp_uint8_t days_of_month[13] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
 mpxp_int64_t days = (datetime_val >> 48) * 146097 / 400, seconds;
 mpxp_uint32_t month = (datetime_val >> 40) & 0xFF;
 while(month > 1)
  days += (mpxp_int64_t)days_of_month[--month];
 days += ((datetime_val >> 32) & 0xFF);
 seconds = days * 24 * 3600;
 seconds += ((datetime_val >> 16) & 0xFF) * 3600;
 seconds += ((datetime_val >>  8) & 0xFF) * 60;
 seconds += (datetime_val & 0xFF);
 return seconds;
}
#endif

#endif // defined(MPXPLAY_GUI_QT) || defined(MPXPLAY_LINK_ORIGINAL_FFMPEG)

// "hh:mm:ss" to 0x00hhmmss
unsigned long pds_strtime_to_hextime(char *timestr,unsigned int houralign)
{
 unsigned long hextime=0,i=0;
 char tmp[300];

 if(!pds_strncpy(tmp,timestr,sizeof(tmp)-4))
  return 0;
 tmp[sizeof(tmp) - 4] = 0;

 timestr=&tmp[0];
 do{
  char *p=pds_strchr(timestr,':');
  if(p)
   *p++=0;
  hextime<<=8;
  hextime|=pds_atol(timestr)&0xff;
  timestr=p;
 }while(timestr && (++i<3));

 if(houralign){
  if(i<2)
   hextime<<=8*(2-i);
 }

 return hextime;
}

int pds_hextime_to_strtime(unsigned long hextime, char *timestr, unsigned int buflen)
{
 return snprintf(timestr, buflen, "%2.2d:%2.2d:%2.2d", ((hextime) >> 16), ((hextime >> 8) & 0xff), (hextime & 0xff));
}

int pds_gettimestampstr(char *timestampstr, unsigned int buflen)
{
 mpxp_uint32_t date_val = pds_getdate();
 mpxp_uint32_t time_val = pds_gettime();
 return snprintf(timestampstr, buflen, "%4.4d.%2.2d.%2.2d_%2.2d-%2.2d-%2.2d",
                   (date_val >> 16), ((date_val >> 8) & 0xff), (date_val & 0xff),
                 (time_val >> 16), ((time_val >> 8) & 0xff), (time_val & 0xff));
}

// "hh:mm:ss.nn" to 0xhhmmssnn
unsigned long pds_strtime_to_hexhtime(char *timestr)
{
 static char separators[4]="::.";
 unsigned long hextime=0,i=0,val;
 char *next,tmp[300];

 if(!pds_strncpy(tmp,timestr,sizeof(tmp)-4))
  return 0;
 tmp[sizeof(tmp) - 4] = 0;

 next=timestr=&tmp[0];
 do{
  char *p=pds_strchr(timestr,separators[i]);
  if(p)
   *p++=0;
  if(next){
   hextime<<=8;
   val=pds_atol(timestr)&0xff;
   if(i==3 && val<10 && timestr[0]!='0')
    val*=10;
   hextime|=val;
  }else{
   if(i==3)
    hextime<<=8;
  }
  next=p;
  if(p)
   timestr=p;
 }while((++i)<4);

 return hextime;
}

void pds_delay_10us(unsigned int ticks) //each tick is 10us
{
#ifdef __DOS__
 unsigned int divisor=(oldint08_handler)? INT08_DIVISOR_NEW:INT08_DIVISOR_DEFAULT; // ???
 unsigned int i,oldtsc, tsctemp, tscdif;

 for(i=0;i<ticks;i++){
  _disable();
  outp(0x43,0x04);
  oldtsc=inp(0x40);
  oldtsc+=inp(0x40)<<8;
  _enable();

  do{
   _disable();
   outp(0x43,0x04);
   tsctemp=inp(0x40);
   tsctemp+=inp(0x40)<<8;
   _enable();
   if(tsctemp<=oldtsc)
    tscdif=oldtsc-tsctemp; // handle overflow
   else
    tscdif=divisor+oldtsc-tsctemp;
  }while(tscdif<12); //wait for 10us  (12/(65536*18) sec)
 }
#else
 pds_mdelay((ticks+99)/100);
 //unsigned int oldclock=clock();
 //while(oldclock==clock()){} // 1ms not 0.01ms (10us)
#endif
}

void pds_mdelay(unsigned long msec)
{
#ifdef __DOS__
 delay(msec);
#else
 pds_threads_sleep(msec);
#endif
}
