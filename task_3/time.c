/* This file takes care of those system calls that deal with time.
 *
 * The entry points into this file are
 *   do_getres:   perform the CLOCK_GETRES system call
 *   do_gettime:  perform the CLOCK_GETTIME system call
 *   do_settime:  perform the CLOCK_SETTIME system call
 *   do_time:   perform the GETTIMEOFDAY system call
 *   do_stime:    perform the STIME system call
 */

#include "pm.h"
#include <minix/callnr.h>
#include <minix/com.h>
#include <signal.h>
#include <sys/time.h>
#include "mproc.h"

/*===========================================================================*
 *        do_gettime             *
 *===========================================================================*/
int do_gettime()
{
  clock_t ticks, realtime, clock;
  time_t boottime;
  int s;

  if ( (s=getuptime(&ticks, &realtime, &boottime)) != OK)
    panic("do_time couldn't get uptime: %d", s);

  switch (m_in.m_lc_pm_time.clk_id) {
  case CLOCK_REALTIME:
    clock = realtime;
    break;
  case CLOCK_MONOTONIC:
    clock = ticks;
    break;
  default:
    return EINVAL; /* invalid/unsupported clock_id */
  }

  mp->mp_reply.m_pm_lc_time.sec = boottime + (clock / system_hz);
  mp->mp_reply.m_pm_lc_time.nsec =
  (uint32_t) ((clock % system_hz) * 1000000000ULL / system_hz);

  return(OK);
}

/*===========================================================================*
 *        do_getres            *
 *===========================================================================*/
int do_getres()
{
  switch (m_in.m_lc_pm_time.clk_id) {
  case CLOCK_REALTIME:
  case CLOCK_MONOTONIC:
    /* tv_sec is always 0 since system_hz is an int */
    mp->mp_reply.m_pm_lc_time.sec = 0;
    mp->mp_reply.m_pm_lc_time.nsec = 1000000000 / system_hz;
    return(OK);
  default:
    return EINVAL; /* invalid/unsupported clock_id */
  }
}

/*===========================================================================*
 *        do_settime             *
 *===========================================================================*/
int do_settime()
{
  int s;

  if (mp->mp_effuid != SUPER_USER) { 
      return(EPERM);
  }

  switch (m_in.m_lc_pm_time.clk_id) {
  case CLOCK_REALTIME:
    s = sys_settime(m_in.m_lc_pm_time.now, m_in.m_lc_pm_time.clk_id,
      m_in.m_lc_pm_time.sec, m_in.m_lc_pm_time.nsec);

    /* reset time baseline for all processes*/
    for (int i = 0; i < NR_PROCS; ++i) {
      mproc[i].mp_time_valid = TIME_NOT_VALID;
    }
    return(s);
  case CLOCK_MONOTONIC: /* monotonic cannot be changed */
  default:
    return EINVAL; /* invalid/unsupported clock_id */
  }
}

/*===========================================================================*
 *        do_time              *
 *===========================================================================*/
int do_time()
{
/* Perform the time(tp) system call. This returns the time in seconds since 
 * 1.1.1970.  MINIX is an astrophysically naive system that assumes the earth 
 * rotates at a constant rate and that such things as leap seconds do not 
 * exist.
 */
  clock_t ticks, realtime;
  time_t boottime;
  int s;

  if ( (s=getuptime(&ticks, &realtime, &boottime)) != OK)
    panic("do_time couldn't get uptime: %d", s);

  mp->mp_reply.m_pm_lc_time.sec = boottime + (realtime / system_hz);
  mp->mp_reply.m_pm_lc_time.nsec =
  (uint32_t) ((realtime % system_hz) * 1000000000ULL / system_hz);

  if (mp->mp_time_valid == TIME_NOT_VALID) {
    mp->mp_time_baseline.tv_sec = mp->mp_reply.m_pm_lc_time.sec;
    mp->mp_time_baseline.tv_nsec = mp->mp_reply.m_pm_lc_time.nsec;
    if (mp->mp_distort_type != NO_TIME_CHANGE) {
      mp->mp_time_valid = TIME_VALID;      
    }
  } else if (mp->mp_time_valid == TIME_VALID){
    /* calculate difference between current time and baseline */
    struct timespec time_diff;
    time_diff.tv_sec = mp->mp_reply.m_pm_lc_time.sec - mp->mp_time_baseline.tv_sec;
    time_diff.tv_nsec = mp->mp_reply.m_pm_lc_time.nsec - mp->mp_time_baseline.tv_nsec;

    /* save baseline */
    mp->mp_reply.m_pm_lc_time.nsec = mp->mp_time_baseline.tv_nsec;  
    mp->mp_reply.m_pm_lc_time.sec = mp->mp_time_baseline.tv_sec;
    
    if (mp->mp_distort_scale != 0) {
      /* scale difference */
      if (mp->mp_distort_type == SLOW_TIME) {
        mp->mp_reply.m_pm_lc_time.nsec =
        (mp->mp_time_baseline.tv_nsec + time_diff.tv_nsec * mp->mp_distort_scale) % 1000000000ULL;  
        mp->mp_reply.m_pm_lc_time.sec +=
        (mp->mp_time_baseline.tv_nsec + time_diff.tv_nsec * mp->mp_distort_scale) / 1000000000ULL
        + time_diff.tv_sec * mp->mp_distort_scale;
      
      } else if (mp->mp_distort_type == QUICK_TIME) {
        mp->mp_reply.m_pm_lc_time.sec += time_diff.tv_sec / mp->mp_distort_scale;
        mp->mp_reply.m_pm_lc_time.nsec +=
        (time_diff.tv_sec % mp->mp_distort_scale) * 1000000000ULL / mp->mp_distort_scale
        + time_diff.tv_nsec / mp->mp_distort_scale; 
      
      } else if (mp->mp_distort_type == NO_TIME_CHANGE){
        mp->mp_reply.m_pm_lc_time.nsec =
        (mp->mp_reply.m_pm_lc_time.nsec + time_diff.tv_nsec) % 1000000000ULL;  
        mp->mp_reply.m_pm_lc_time.sec +=
        time_diff.tv_sec + (mp->mp_reply.m_pm_lc_time.nsec + time_diff.tv_nsec) / 1000000000ULL;
      }
    }
  }

  return(OK);
}

/*===========================================================================*
 *        do_stime             *
 *===========================================================================*/
int do_stime()
{
/* Perform the stime(tp) system call. Retrieve the system's uptime (ticks 
 * since boot) and pass the new time in seconds at system boot to the kernel.
 */
  clock_t uptime, realtime;
  time_t boottime;
  int s;

  if (mp->mp_effuid != SUPER_USER) { 
      return(EPERM);
  }
  if ( (s=getuptime(&uptime, &realtime, &boottime)) != OK) 
      panic("do_stime couldn't get uptime: %d", s);
  boottime = m_in.m_lc_pm_time.sec - (realtime/system_hz);

  s= sys_stime(boottime);   /* Tell kernel about boottime */
  if (s != OK)
  panic("pm: sys_stime failed: %d", s);

  return(OK);
}
