#include "pm.h"      
#include "mproc.h"   

#define NO_RELATION_FOUND 0
#define RELATION_FOUND 1

#define INDEX_NOT_FOUND -1

int check_if_pid_exists(pid_t pid)
{
  /* look for pid idx in mproc */
  for (int i = 0; i < NR_PROCS; ++i) {
    if (mproc[i].mp_pid == pid && mproc[i].mp_flags & IN_USE) {      
        return i;
    }
  }
  return INDEX_NOT_FOUND;
}

int check_if_parent(int current_idx, int potential_parent_idx)
{
  while (current_idx != potential_parent_idx && current_idx != INIT_PROC_NR) {
    current_idx = mproc[current_idx].mp_parent;
  }
  
  if (current_idx == potential_parent_idx) {
    return RELATION_FOUND;
  }
  return NO_RELATION_FOUND;
}

int check_relation(pid_t pid_to_check, uint8_t scale)      
{

  int caller_idx = _ENDPOINT_P(m_in.m_source);
  int to_check_idx = check_if_pid_exists(pid_to_check);
  
  if (to_check_idx == INDEX_NOT_FOUND) {
    return EINVAL;
  }

  if (to_check_idx == caller_idx) {
    return EPERM;
  }

  /* check if pid_to_check is parent*/
  int is_parent = check_if_parent(caller_idx, to_check_idx);
  if (is_parent == RELATION_FOUND) {
    mproc[to_check_idx].mp_distort_type = QUICK_TIME;
    mproc[to_check_idx].mp_distort_scale = scale;
    return(OK);
  }

  /* check if pid_to_check is child*/  
  int is_child = check_if_parent(to_check_idx, caller_idx);
  if (is_child == RELATION_FOUND) {
    mproc[to_check_idx].mp_distort_type = SLOW_TIME;
    mproc[to_check_idx].mp_distort_scale = scale;
    return(OK);
  }

  /* no relationship */
  return EPERM;
}

int do_distort_time(void) {
  pid_t pid_to_check = m_in.m_m1.m1i1;
  uint8_t scale = m_in.m_m1.m1i2;
  
  return check_relation(pid_to_check, scale);
}
