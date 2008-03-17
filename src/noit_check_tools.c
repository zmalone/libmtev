/*
 * Copyright (c) 2007, OmniTI Computer Consulting, Inc.
 * All rights reserved.
 */

#include "noit_defines.h"
#include "noit_check_tools.h"

#include <assert.h>

typedef struct {
  noit_module_t *self;
  noit_check_t *check;
  dispatch_func_t dispatch;
} recur_closure_t;

int
noit_check_interpolate(char *buff, int len, const char *fmt,
                       noit_hash_table *attrs,
                       noit_hash_table *config) {
  char *copy = NULL;
  char closer;
  const char *fmte, *key;
  int replaced_something = 1;
  int iterations = 3;

  while(replaced_something && iterations > 0) {
    char *cp = buff, * const end = buff + len;
    iterations--;
    replaced_something = 0;
    while(*fmt && cp < end) {
      switch(*fmt) {
        case '%':
          if(fmt[1] == '{' || fmt[1] == '[') {
            closer = (fmt[1] == '{') ? '}' : ']';
            fmte = fmt + 2;
            key = fmte;
            while(*fmte && *fmte != closer) fmte++;
            if(*fmte == closer) {
              /* We have a full key here */
              const char *replacement;
              if(!noit_hash_retrieve((closer == '}') ?  config : attrs,
                                     key, fmte - key, (void **)&replacement))
                replacement = "";
              fmt = fmte + 1; /* Format points just after the end of the key */
              strlcpy(cp, replacement, end-cp);
              cp += strlen(cp);
              replaced_something = 1;
              break;
            }
          }
        default:
          *cp++ = *fmt++;
      }
    }
    *cp = '\0';
    if(copy) free(copy);
    if(replaced_something)
      copy = strdup(buff);
    fmt = copy;
  }
  return strlen(buff);
}

static int
noit_check_recur_handler(eventer_t e, int mask, void *closure,
                              struct timeval *now) {
  recur_closure_t *rcl = closure;
  rcl->check->fire_event = NULL; /* This is us, we get free post-return */
  noit_check_schedule_next(rcl->self, &e->whence, rcl->check, now,
                           rcl->dispatch);
  rcl->dispatch(rcl->self, rcl->check);
  free(rcl);
  return 0;
}

int
noit_check_schedule_next(noit_module_t *self,
                         struct timeval *last_check, noit_check_t *check,
                         struct timeval *now, dispatch_func_t dispatch) {
  eventer_t newe;
  struct timeval period, earliest;
  recur_closure_t *rcl;

  assert(check->fire_event == NULL);
  if(check->period == 0) return 0;
  if(NOIT_CHECK_DISABLED(check) || NOIT_CHECK_KILLED(check)) return 0;

  /* If we have an event, we know when we intended it to fire.  This means
   * we should schedule that point + period.
   */
  if(now)
    memcpy(&earliest, now, sizeof(earliest));
  else
    gettimeofday(&earliest, NULL);
  period.tv_sec = check->period / 1000;
  period.tv_usec = (check->period % 1000) * 1000;

  newe = eventer_alloc();
  memcpy(&newe->whence, last_check, sizeof(*last_check));
  add_timeval(newe->whence, period, &newe->whence);
  if(compare_timeval(newe->whence, earliest) < 0)
    memcpy(&newe->whence, &earliest, sizeof(earliest));
  newe->mask = EVENTER_TIMER;
  newe->callback = noit_check_recur_handler;
  rcl = calloc(1, sizeof(*rcl));
  rcl->self = self;
  rcl->check = check;
  rcl->dispatch = dispatch;
  newe->closure = rcl;

  eventer_add(newe);
  check->fire_event = newe;
  return 0;
}

void
noit_check_run_full_asynch(noit_check_t *check, eventer_func_t callback) {
  eventer_t e;
  e = eventer_alloc();
  e->fd = -1;
  e->mask = EVENTER_ASYNCH; 
  memcpy(&e->whence, &__now, sizeof(__now));
  p_int.tv_sec = check->timeout / 1000;
  p_int.tv_usec = (check->timeout % 1000) * 1000;
  add_timeval(e->whence, p_int, &e->whence);
  e->callback = ssh2_connect_complete;
  e->closure =  check->closure;
  eventer_add(e);
}

void
noit_check_make_attrs(noit_check_t *check, noit_hash_table *attrs) {
#define CA_STORE(a,b) noit_hash_store(attrs, a, strlen(a), b)
  CA_STORE("target", check->target);
  CA_STORE("name", check->name);
  CA_STORE("module", check->module);
}
void
noit_check_release_attrs(noit_hash_table *attrs) {
  noit_hash_destroy(attrs, NULL, NULL);
}
