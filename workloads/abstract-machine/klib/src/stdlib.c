/* Standard library implementation.
 *
 * Core functions are adapted from sonnet-libc
 * (https://gitlink.org.cn/foobat/sonnet-libc).
 * calloc/realloc are klib additions.
 */

#include <klib.h>

#ifndef RAND_MAX
#define RAND_MAX 0x7fffffff
#endif

static unsigned int _random_seed;

int rand(void) {
    if (_random_seed == 0) {
        _random_seed = 0x7a2d5eed;
    }
    unsigned int feedback = 0;
    feedback ^= (_random_seed >> 31) | 1;
    feedback ^= (_random_seed >> 21) | 1;
    feedback ^= (_random_seed >> 1) | 1;
    feedback ^= (_random_seed >> 0) | 1;
    _random_seed >>= 1;
    _random_seed |= feedback << 31;
    return (_random_seed >> 1) % RAND_MAX;
}

void srand(unsigned int seed) {
    _random_seed = seed;
}

int abs(int x) {
    return (x < 0 ? -x : x);
}

int atoi(const char *nptr) {
    int x = 0;
    bool negative = false;
    while (isspace((unsigned char)*nptr)) {
        nptr++;
    }
    if (*nptr == '+') {
        negative = false;
        ++nptr;
    } else if (*nptr == '-') {
        negative = true;
        ++nptr;
    }
    while (*nptr >= '0' && *nptr <= '9') {
        x = x * 10 + *nptr - '0';
        nptr++;
    }
    if (negative) {
        return -x;
    } else {
        return x;
    }
}

long int atol(const char *nptr) {
    long int x = 0;
    bool negative = false;
    while (isspace((unsigned char)*nptr)) {
        nptr++;
    }
    if (*nptr == '+') {
        negative = false;
        ++nptr;
    } else if (*nptr == '-') {
        negative = true;
        ++nptr;
    }
    while (*nptr >= '0' && *nptr <= '9') {
        x = x * 10 + *nptr - '0';
        nptr++;
    }
    if (negative) {
        return -x;
    } else {
        return x;
    }
}

long long int atoll(const char *nptr) {
    long long int x = 0;
    bool negative = false;
    while (isspace((unsigned char)*nptr)) {
        nptr++;
    }
    if (*nptr == '+') {
        negative = false;
        ++nptr;
    } else if (*nptr == '-') {
        negative = true;
        ++nptr;
    }
    while (*nptr >= '0' && *nptr <= '9') {
        x = x * 10 + *nptr - '0';
        nptr++;
    }
    if (negative) {
        return -x;
    } else {
        return x;
    }
}

long int strtol(const char *str, char **endptr, int base) {
    size_t i = 0;
    bool negative = false;
    long int result = 0;
    // fail if `base` is invalid
    if (base != 0 && (base < 2 || base > 36)) {
        goto finish;
    }
    // skip all whitespaces
    while (isspace((unsigned char)str[i])) {
        ++i;
    }
    // detect sign
    if (str[i] == '+') {
        negative = false;
        ++i;
    } else if (str[i] == '-') {
        negative = true;
        ++i;
    }
    // detect base
    if (base == 0) {
        if (str[i] == '0') {
            ++i;
            if (str[i] == 'x' || str[i] == 'X') {
                base = 16;
                ++i;
            } else {
                base = 8;
            }
        } else {
            base = 10;
        }
    } else if (base == 16) {
        // skip the "0x" prefix for hexadecimal
        if (str[i] == '0') {
            ++i;
        }
        if (str[i] == 'x' || str[i] == 'X') {
            ++i;
        }
    }
    // parse number
    while (true) {
        long int digit;
        if (isdigit((unsigned char)str[i])) {
            digit = str[i] - '0';
        } else if (isupper((unsigned char)str[i])) {
            digit = str[i] - 'A' + 10;
        } else if (islower((unsigned char)str[i])) {
            digit = str[i] - 'a' + 10;
        } else {
            break;
        }
        if (digit >= base) {
            break;
        }
        result *= base;
        result += digit;
        ++i;
    }
finish:
    if (endptr != NULL) {
        *endptr = (char *)str + i;
    }
    if (negative) {
        return -result;
    } else {
        return result;
    }
}

long long int strtoll(const char *str, char **endptr, int base) {
    size_t i = 0;
    bool negative = false;
    long long int result = 0;
    if (base != 0 && (base < 2 || base > 36)) {
        goto finish;
    }
    while (isspace((unsigned char)str[i])) {
        ++i;
    }
    if (str[i] == '+') {
        negative = false;
        ++i;
    } else if (str[i] == '-') {
        negative = true;
        ++i;
    }
    if (base == 0) {
        if (str[i] == '0') {
            ++i;
            if (str[i] == 'x' || str[i] == 'X') {
                base = 16;
                ++i;
            } else {
                base = 8;
            }
        } else {
            base = 10;
        }
    } else if (base == 16) {
        if (str[i] == '0') {
            ++i;
        }
        if (str[i] == 'x' || str[i] == 'X') {
            ++i;
        }
    }
    while (true) {
        long int digit;
        if (isdigit((unsigned char)str[i])) {
            digit = str[i] - '0';
        } else if (isupper((unsigned char)str[i])) {
            digit = str[i] - 'A' + 10;
        } else if (islower((unsigned char)str[i])) {
            digit = str[i] - 'a' + 10;
        } else {
            break;
        }
        if (digit >= base) {
            break;
        }
        result *= base;
        result += digit;
        ++i;
    }
finish:
    if (endptr != NULL) {
        *endptr = (char *)str + i;
    }
    if (negative) {
        return -result;
    } else {
        return result;
    }
}

unsigned long int strtoul(const char *str, char **endptr, int base) {
    size_t i = 0;
    unsigned long int result = 0;
    if (base != 0 && (base < 2 || base > 36)) {
        goto finish;
    }
    while (isspace((unsigned char)str[i])) {
        ++i;
    }
    if (base == 0) {
        if (str[i] == '0') {
            ++i;
            if (str[i] == 'x' || str[i] == 'X') {
                base = 16;
                ++i;
            } else {
                base = 8;
            }
        } else {
            base = 10;
        }
    } else if (base == 16) {
        if (str[i] == '0') {
            ++i;
        }
        if (str[i] == 'x' || str[i] == 'X') {
            ++i;
        }
    }
    while (true) {
        long int digit;
        if (isdigit((unsigned char)str[i])) {
            digit = str[i] - '0';
        } else if (isupper((unsigned char)str[i])) {
            digit = str[i] - 'A' + 10;
        } else if (islower((unsigned char)str[i])) {
            digit = str[i] - 'a' + 10;
        } else {
            break;
        }
        if (digit >= base) {
            break;
        }
        result *= base;
        result += digit;
        ++i;
    }
finish:
    if (endptr != NULL) {
        *endptr = (char *)str + i;
    }
    return result;
}

unsigned long long int strtoull(const char *str, char **endptr, int base) {
    size_t i = 0;
    unsigned long long int result = 0;
    if (base != 0 && (base < 2 || base > 36)) {
        goto finish;
    }
    while (isspace((unsigned char)str[i])) {
        ++i;
    }
    if (base == 0) {
        if (str[i] == '0') {
            ++i;
            if (str[i] == 'x' || str[i] == 'X') {
                base = 16;
                ++i;
            } else {
                base = 8;
            }
        } else {
            base = 10;
        }
    } else if (base == 16) {
        if (str[i] == '0') {
            ++i;
        }
        if (str[i] == 'x' || str[i] == 'X') {
            ++i;
        }
    }
    while (true) {
        long int digit;
        if (isdigit((unsigned char)str[i])) {
            digit = str[i] - '0';
        } else if (isupper((unsigned char)str[i])) {
            digit = str[i] - 'A' + 10;
        } else if (islower((unsigned char)str[i])) {
            digit = str[i] - 'a' + 10;
        } else {
            break;
        }
        if (digit >= base) {
            break;
        }
        result *= base;
        result += digit;
        ++i;
    }
finish:
    if (endptr != NULL) {
        *endptr = (char *)str + i;
    }
    return result;
}

void *malloc(size_t size) {
    static char *ptr = NULL;
    if (ptr == NULL) {
        ptr = (char *)heap.end;
    }
    size = (size + 7) & ~7;
    if (ptr - size < (char *)heap.start) {
        return NULL;
    }
    ptr -= size;
    return ptr;
}

void free(void *ptr) {
    (void)ptr;
}

void *calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void *ret = malloc(total);
    if (ret != NULL) {
        memset(ret, 0, total);
    }
    return ret;
}

void *realloc(void *ptr, size_t size) {
    if (ptr == NULL) return malloc(size);
    if (size == 0) { free(ptr); return NULL; }
    void *ret = malloc(size);
    if (ret != NULL) {
        memcpy(ret, ptr, size);
    }
    free(ptr);
    return ret;
}

__attribute__((__noreturn__))
void exit(int code) {
    halt(code);
}

__attribute__((__noreturn__))
void abort(void) {
    halt(-1);
}
