#include <rtthread.h>
#include <klib.h>
#include <stdint.h>

__attribute__((weak)) char *strchr(const char *s, int c) {
  const char *p = s;
  while (1) {
    if (*p == (char)c) return (char *)p;
    if (*p == '\0') break;
    p++;
  }
  return NULL;
}

__attribute__((weak)) char *strrchr(const char *s, int c) {
  const char *last = NULL;
  const char *p = s;
  while (1) {
    if (*p == (char)c) last = p;
    if (*p == '\0') break;
    p++;
  }
  return (char *)last;
}

__attribute__((weak)) char *strstr(const char *haystack, const char *needle) {
  return rt_strstr(haystack, needle);
}

__attribute__((weak)) long strtol(const char *restrict nptr,
                                  char **restrict endptr, int base) {
  const char *p = nptr;
  int neg = 0;
  long val = 0;

  while (isspace((unsigned char)*p)) p++;

  if (*p == '-') {
    neg = 1;
    p++;
  } else if (*p == '+') {
    p++;
  }

  if (base == 0) {
    if (*p == '0' && (p[1] == 'x' || p[1] == 'X')) {
      base = 16;
      p += 2;
    } else {
      base = 10;
    }
  }

  while (1) {
    int ch = (unsigned char)*p;
    int digit;
    if (ch >= '0' && ch <= '9') {
      digit = ch - '0';
    } else if (ch >= 'a' && ch <= 'z') {
      digit = ch - 'a' + 10;
    } else if (ch >= 'A' && ch <= 'Z') {
      digit = ch - 'A' + 10;
    } else {
      break;
    }
    if (digit >= base) break;
    val = val * base + digit;
    p++;
  }

  if (endptr) *endptr = (char *)p;
  return neg ? -val : val;
}

__attribute__((weak)) char *strncat(char *restrict dst,
                                    const char *restrict src, size_t sz) {
  size_t dlen = strlen(dst);
  size_t i;
  for (i = 0; i < sz && src[i]; i++) {
    dst[dlen + i] = src[i];
  }
  dst[dlen + i] = '\0';
  return dst;
}

/* Soft-float helper: signed 32-bit integer -> IEEE 754 double.
 * RV32E has no FPU, so the compiler emits a libcall for (double)int. */
double __floatsidf(int32_t a) {
  if (a == 0) return 0.0;

  int sign = a < 0;
  uint32_t u = sign ? -(uint32_t)a : (uint32_t)a;

  /* Count leading zeros without relying on __builtin_clz (which would
   * introduce another libcall on RV32E). */
  int exp = 31;
  uint32_t mask = 1u << 31;
  while ((u & mask) == 0) {
    exp--;
    mask >>= 1;
  }

  uint64_t mant = ((uint64_t)u << (52 - exp)) & ((1ULL << 52) - 1);
  uint64_t bits = ((uint64_t)sign << 63)
                | ((uint64_t)(exp + 1023) << 52)
                | mant;

  union { uint64_t u; double d; } conv;
  conv.u = bits;
  return conv.d;
}
