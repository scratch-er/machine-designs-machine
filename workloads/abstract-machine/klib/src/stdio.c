/* Standard I/O implementation.
 *
 * putchar/puts and the printf family are adapted from sonnet-libc
 * (https://gitlink.org.cn/foobat/sonnet-libc).
 * vsscanf/sscanf are klib additions.
 */

#include <klib.h>
#include <limits.h>

int putchar(int c) {
    char ch = c;
    putch(ch);
    return ch;
}

int puts(const char *str) {
    int cnt;
    for (cnt = 0; str[cnt]; ++cnt) {
        putch(str[cnt]);
    }
    putch('\n');
    return cnt;
}

typedef struct {
    char *dest;             // if putch is NULL, write to this memory address
    void (*putch)(char ch); // the function to write a character
    size_t count;           // number of characters written, SIZE_MAX indicates an error
    size_t max_count;       // max number of characters to write
} printf_state_t;

static void printf_backend(printf_state_t *state, const char *fmt, va_list ap);

int printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    printf_state_t state = {
        .dest = NULL,
        .count = 0,
        .max_count = SIZE_MAX,
        .putch = putch,
    };
    printf_backend(&state, fmt, ap);
    va_end(ap);
    return state.count;
}

int vprintf(const char *fmt, va_list ap) {
    printf_state_t state = {
        .dest = NULL,
        .count = 0,
        .max_count = SIZE_MAX,
        .putch = putch,
    };
    printf_backend(&state, fmt, ap);
    return state.count;
}

int vsprintf(char *out, const char *fmt, va_list ap) {
    return vsnprintf(out, SIZE_MAX, fmt, ap);
}

int sprintf(char *out, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int count = vsnprintf(out, SIZE_MAX, fmt, ap);
    va_end(ap);
    return count;
}

int snprintf(char *out, size_t n, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int count = vsnprintf(out, n, fmt, ap);
    va_end(ap);
    return count;
}

int vsnprintf(char *out, size_t n, const char *fmt, va_list ap) {
    printf_state_t state = {
        .dest = out,
        .count = 0,
        .max_count = (n > 0) ? n - 1 : 0,
        .putch = NULL,
    };
    printf_backend(&state, fmt, ap);
    if (n > 0) {
        size_t term = (state.count < n - 1) ? state.count : n - 1;
        out[term] = '\0';
    }
    return state.count;
}

static void printf_putch(printf_state_t *state, char ch) {
    if (state->count < state->max_count) {
        if (state->putch != NULL) {
            state->putch(ch);
        } else {
            state->dest[state->count] = ch;
        }
    }
    state->count += 1;
}

static void printf_pad(printf_state_t *state, char ch, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        printf_putch(state, ch);
    }
}

static void printf_char(printf_state_t *state, char ch, size_t width, bool left_justified) {
    if (left_justified) {
        printf_putch(state, ch);
        if (width > 1) {
            printf_pad(state, ' ', width - 1);
        }
    } else {
        if (width > 1) {
            printf_pad(state, ' ', width - 1);
        }
        printf_putch(state, ch);
    }
}

static void printf_string(printf_state_t *state, const char *str, size_t width, bool left_justified) {
    size_t len = strlen(str);
    if (width > len && !left_justified) {
        printf_pad(state, ' ', width - len);
    }
    for (size_t i = 0; str[i]; ++i) {
        printf_putch(state, str[i]);
    }
    if (width > len && left_justified) {
        printf_pad(state, ' ', width - len);
    }
}

static void printf_uint32(printf_state_t *state, unsigned int n, size_t width, bool left_justified, char padding) {
    unsigned int n_digits = 0;
    char buffer[10] = { '0' };
    if (n == 0) {
        buffer[0] = '0';
        n_digits = 1;
    } else {
        while (n > 0) {
            buffer[n_digits] = n % 10 + '0';
            n_digits += 1;
            n /= 10;
        }
    }
    if (!left_justified && width > n_digits) {
        printf_pad(state, padding, width - n_digits);
    }
    for (int i = n_digits - 1; i >= 0; --i) {
        printf_putch(state, buffer[i]);
    }
    if (left_justified && width > n_digits) {
        printf_pad(state, ' ', width - n_digits);
    }
}

static void printf_int32(printf_state_t *state, int n, size_t width, bool left_justified, char padding, char sign) {
    if (sign && n > 0) {
        printf_putch(state, sign);
        width -= 1;
    } else if (n < 0) {
        printf_putch(state, '-');
        n = -n;
        width -= 1;
    }
    printf_uint32(state, n, width, left_justified, padding);
}

static void printf_uint64(printf_state_t *state, unsigned long long int n, size_t width, bool left_justified, char padding) {
    unsigned int n_digits = 0;
    char buffer[20] = { '0' };
    if (n == 0) {
        buffer[0] = '0';
        n_digits = 1;
    } else {
        while (n > 0) {
            buffer[n_digits] = n % 10 + '0';
            n_digits += 1;
            n /= 10;
        }
    }
    if (!left_justified && width > n_digits) {
        printf_pad(state, padding, width - n_digits);
    }
    for (int i = n_digits - 1; i >= 0; --i) {
        printf_putch(state, buffer[i]);
    }
    if (left_justified && width > n_digits) {
        printf_pad(state, ' ', width - n_digits);
    }
}

static void printf_int64(printf_state_t *state, long long int n, size_t width, bool left_justified, char padding, char sign) {
    if (sign && n > 0) {
        printf_putch(state, sign);
        width -= 1;
    } else if (n < 0) {
        printf_putch(state, '-');
        n = -n;
        width -= 1;
    }
    printf_uint64(state, n, width, left_justified, padding);
}

static void printf_hex32(printf_state_t *state, unsigned int n, size_t width, bool left_justified, char padding, bool upper_case) {
    char buffer[8] = { '0' };
    unsigned int n_digits = 0;
    if (n == 0) {
        buffer[0] = '0';
        n_digits = 1;
    } else {
        while (n > 0) {
            unsigned int rem = n % 16;
            buffer[n_digits++] = (rem < 10) ? (rem + '0') : (rem - 10 + (upper_case ? 'A' : 'a'));
            n /= 16;
        }
    }
    if (!left_justified && width > n_digits) {
        printf_pad(state, padding, width - n_digits);
    }
    for (int i = n_digits - 1; i >= 0; --i) {
        printf_putch(state, buffer[i]);
    }
    if (left_justified && width > n_digits) {
        printf_pad(state, ' ', width - n_digits);
    }
}

static void printf_hex64(printf_state_t *state, unsigned long long int n, size_t width, bool left_justified, char padding, bool upper_case) {
    char buffer[16] = { '0' };
    unsigned int n_digits = 0;
    if (n == 0) {
        buffer[0] = '0';
        n_digits = 1;
    } else {
        while (n > 0) {
            unsigned int rem = n % 16;
            buffer[n_digits++] = (rem < 10) ? (rem + '0') : (rem - 10 + (upper_case ? 'A' : 'a'));
            n /= 16;
        }
    }
    if (!left_justified && width > n_digits) {
        printf_pad(state, padding, width - n_digits);
    }
    for (int i = n_digits - 1; i >= 0; --i) {
        printf_putch(state, buffer[i]);
    }
    if (left_justified && width > n_digits) {
        printf_pad(state, ' ', width - n_digits);
    }
}

static void printf_octal32(printf_state_t *state, unsigned int n, size_t width, bool left_justified, char padding) {
    char buffer[11] = { '0' };
    unsigned int n_digits = 0;
    if (n == 0) {
        buffer[0] = '0';
        n_digits = 1;
    } else {
        while (n > 0) {
            unsigned int rem = n % 8;
            buffer[n_digits++] = rem + '0';
            n /= 8;
        }
    }
    if (!left_justified && width > n_digits) {
        printf_pad(state, padding, width - n_digits);
    }
    for (int i = n_digits - 1; i >= 0; --i) {
        printf_putch(state, buffer[i]);
    }
    if (left_justified && width > n_digits) {
        printf_pad(state, ' ', width - n_digits);
    }
}

static void printf_octal64(printf_state_t *state, unsigned long long int n, size_t width, bool left_justified, char padding) {
    char buffer[22] = { '0' };
    unsigned int n_digits = 0;
    if (n == 0) {
        buffer[0] = '0';
        n_digits = 1;
    } else {
        while (n > 0) {
            unsigned int rem = n % 8;
            buffer[n_digits++] = rem + '0';
            n /= 8;
        }
    }
    if (!left_justified && width > n_digits) {
        printf_pad(state, padding, width - n_digits);
    }
    for (int i = n_digits - 1; i >= 0; --i) {
        printf_putch(state, buffer[i]);
    }
    if (left_justified && width > n_digits) {
        printf_pad(state, ' ', width - n_digits);
    }
}

static void printf_backend(printf_state_t *state, const char *fmt, va_list ap) {
    const size_t n = strlen(fmt) + 1;
    for (size_t i = 0; i < n; ++i) {
        if (fmt[i] == '%') {
            ++i;
            bool left_justified = false;
            char sign = '\0';
            char padding = ' ';
            while (true) {
                char flag = fmt[i];
                if (flag == '-') {
                    left_justified = true;
                } else if (flag == '+') {
                    sign = '+';
                } else if (flag == ' ') {
                    sign = ' ';
                } else if (flag == '0') {
                    padding = '0';
                } else if (flag == '#') {
                    // not implemented
                } else {
                    break;
                }
                ++i;
            }
            size_t width = 0;
            while (fmt[i] >= '0' && fmt[i] <= '9') {
                width *= 10;
                width += fmt[i] - '0';
                ++i;
            }
            if (fmt[i] == '.') {
                // not implemented, skip only
                ++i;
                while (fmt[i] >= '0' && fmt[i] <= '9') {
                    ++i;
                }
            }
            int length_level = 0;
            while (true) {
                if (fmt[i] == 'l') {
                    length_level += 1;
                } else if (fmt[i] == 'h') {
                    length_level -= 1;
                } else {
                    break;
                }
                ++i;
            }
            switch (fmt[i]) {
                case '%':
                    printf_char(state, '%', width, left_justified);
                    break;
                case 'c': {
                    char ch = (char)va_arg(ap, int);
                    printf_char(state, ch, width, left_justified);
                    break;
                }
                case 's': {
                    char *str = (char *)va_arg(ap, void *);
                    if (str == NULL) str = "(null)";
                    printf_string(state, str, width, left_justified);
                    break;
                }
                case 'd':
                case 'i':
                    if (length_level <= 0) {
                        int n = va_arg(ap, int);
                        printf_int32(state, n, width, left_justified, padding, sign);
                    } else if (length_level == 1 && sizeof(long) * CHAR_BIT == 32) {
                        long int n = va_arg(ap, long int);
                        printf_int32(state, n, width, left_justified, padding, sign);
                    } else if (length_level == 1 && sizeof(long) * CHAR_BIT == 64) {
                        long int n = va_arg(ap, long int);
                        printf_int64(state, n, width, left_justified, padding, sign);
                    } else {
                        long long int n = va_arg(ap, long long int);
                        printf_int64(state, n, width, left_justified, padding, sign);
                    }
                    break;
                case 'u':
                    if (length_level <= 0) {
                        unsigned int n = va_arg(ap, unsigned int);
                        printf_uint32(state, n, width, left_justified, padding);
                    } else if (length_level == 1 && sizeof(long) * CHAR_BIT == 32) {
                        unsigned long int n = va_arg(ap, unsigned long int);
                        printf_uint32(state, n, width, left_justified, padding);
                    } else if (length_level == 1 && sizeof(long) * CHAR_BIT == 64) {
                        unsigned long int n = va_arg(ap, unsigned long int);
                        printf_uint64(state, n, width, left_justified, padding);
                    } else {
                        unsigned long long int n = va_arg(ap, unsigned long long int);
                        printf_uint64(state, n, width, left_justified, padding);
                    }
                    break;
                case 'x':
                case 'X':
                case 'p': {
                    bool upper_case = (fmt[i] == 'X');
                    if (length_level <= 0) {
                        unsigned int n = va_arg(ap, unsigned int);
                        printf_hex32(state, n, width, left_justified, padding, upper_case);
                    } else if (length_level == 1) {
                        unsigned long int n = va_arg(ap, unsigned long int);
                        if (sizeof(unsigned long) * CHAR_BIT == 32) {
                            printf_hex32(state, n, width, left_justified, padding, upper_case);
                        } else {
                            printf_hex64(state, n, width, left_justified, padding, upper_case);
                        }
                    } else {
                        unsigned long long int n = va_arg(ap, unsigned long long int);
                        printf_hex64(state, n, width, left_justified, padding, upper_case);
                    }
                    break;
                }
                case 'o':
                    if (length_level <= 0) {
                        unsigned int n = va_arg(ap, unsigned int);
                        printf_octal32(state, n, width, left_justified, padding);
                    } else if (length_level == 1) {
                        unsigned long int n = va_arg(ap, unsigned long int);
                        if (sizeof(unsigned long) * CHAR_BIT == 32) {
                            printf_octal32(state, n, width, left_justified, padding);
                        } else {
                            printf_octal64(state, n, width, left_justified, padding);
                        }
                    } else {
                        unsigned long long int n = va_arg(ap, unsigned long long int);
                        printf_octal64(state, n, width, left_justified, padding);
                    }
                    break;
                case 'f':
                case 'F':
                case 'e':
                case 'E':
                case 'g':
                case 'G':
                case 'a':
                case 'A':
                    // not implemented, skip only
                    va_arg(ap, double);
                    break;
            }
        } else {
            printf_putch(state, fmt[i]);
        }
    }
}

int __am_vsscanf_internal(const char *str, const char **end_pstr, const char *fmt, va_list ap) {
    const char *pstr = str;
    const char *pfmt = fmt;
    int item = -1;
    while (*pfmt) {
        char ch = *pfmt++;
        if (isspace((unsigned char)ch)) {
            for (ch = *pfmt; isspace((unsigned char)ch); ch = *(++pfmt));
            for (ch = *pstr; isspace((unsigned char)ch); ch = *(++pstr));
            item++;
            continue;
        }
        switch (ch) {
            case '%': break;
            default:
                if (*pstr == ch) {
                    pstr++;
                    item++;
                    continue;
                }
                goto end;
        }

        ch = *pfmt++;
        switch (ch) {
            case 'd': {
                char *p;
                *(va_arg(ap, int *)) = strtol(pstr, &p, 10);
                if (p == pstr) goto end;
                pstr = p;
                item++;
                break;
            }
            case 'c':
                *(va_arg(ap, char *)) = *pstr++;
                item++;
                break;
            default:
                printf("Unsupported conversion specifier '%c'\n", ch);
                assert(0);
        }
    }

end:
    if (end_pstr) *end_pstr = pstr;
    return item;
}

int vsscanf(const char *str, const char *fmt, va_list ap) {
    return __am_vsscanf_internal(str, NULL, fmt, ap);
}

int sscanf(const char *str, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vsscanf(str, fmt, ap);
    va_end(ap);
    return r;
}

int __isoc99_sscanf(const char *str, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vsscanf(str, fmt, ap);
    va_end(ap);
    return r;
}
