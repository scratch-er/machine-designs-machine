#ifndef __SYS_TYPES_H__
#define __SYS_TYPES_H__

#include <stdint.h>
#include <stddef.h>

typedef int32_t          clockid_t;
typedef int32_t          key_t;
typedef int              pid_t;
typedef unsigned short   uid_t;
typedef unsigned short   gid_t;
typedef signed long      off_t;
typedef int              mode_t;
typedef signed long      ssize_t;
typedef unsigned long    __timer_t;
typedef __timer_t        timer_t;
typedef long             time_t;
typedef long             clock_t;
typedef long             suseconds_t;
typedef unsigned long    useconds_t;
typedef unsigned long    dev_t;

typedef unsigned int     u_int;
typedef unsigned char    u_char;
typedef unsigned long    u_long;

#endif /* __SYS_TYPES_H__ */
