#ifndef __WCHAR_H__
#define __WCHAR_H__

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t wchar_t;

int wcwidth(wchar_t wc);
int wcswidth(const wchar_t *pwcs, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* __WCHAR_H__ */
