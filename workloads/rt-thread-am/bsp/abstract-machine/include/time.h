#ifndef __TIME_H__
#define __TIME_H__

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CLOCKS_PER_SEC
#define CLOCKS_PER_SEC 100
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

struct tm {
  int tm_sec;
  int tm_min;
  int tm_hour;
  int tm_mday;
  int tm_mon;
  int tm_year;
  int tm_wday;
  int tm_yday;
  int tm_isdst;
};

struct timespec {
  time_t tv_sec;
  long   tv_nsec;
};

time_t time(time_t *t);
clock_t clock(void);
struct tm *gmtime(const time_t *t);
struct tm *gmtime_r(const time_t *t, struct tm *r);
struct tm *localtime(const time_t *t);
struct tm *localtime_r(const time_t *t, struct tm *r);
time_t mktime(struct tm * const t);
char *asctime(const struct tm *timeptr);
char *asctime_r(const struct tm *t, char *buf);
char *ctime(const time_t *tim_p);
char *ctime_r(const time_t *tim_p, char *result);
double difftime(time_t time1, time_t time2);
time_t timegm(struct tm * const t);
int stime(const time_t *t);

#ifdef __cplusplus
}
#endif

#endif /* __TIME_H__ */
