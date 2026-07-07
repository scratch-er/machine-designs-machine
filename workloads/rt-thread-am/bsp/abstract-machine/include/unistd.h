#ifndef __UNISTD_H__
#define __UNISTD_H__

#include <sys/unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

int rename(const char *oldpath, const char *newpath);

#ifdef __cplusplus
}
#endif

#endif
