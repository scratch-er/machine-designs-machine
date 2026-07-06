/* String library implementation.
 *
 * Core functions are adapted from sonnet-libc
 * (https://gitlink.org.cn/foobat/sonnet-libc).
 * strchr/strrchr are klib additions.
 */

#include <klib.h>
#include <limits.h>

size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len]) {
        len += 1;
    }
    return len;
}

char *strcpy(char *dst, const char *src) {
    size_t i;
    for (i = 0; src[i]; ++i) {
        dst[i] = src[i];
    }
    dst[i] = 0;
    return dst;
}

char *strncpy(char *dst, const char *src, size_t n) {
    size_t i = 0;
    while (i < n && src[i]) {
        dst[i] = src[i];
        i += 1;
    }
    while (i < n) {
        dst[i] = 0;
        i += 1;
    }
    return dst;
}

char *strcat(char *dst, const char *src) {
    strcpy(dst + strlen(dst), src);
    return dst;
}

int strcmp(const char *s1, const char *s2) {
    return strncmp(s1, s2, SIZE_MAX);
}

int strncmp(const char *s1, const char *s2, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        if ((unsigned char)s1[i] > (unsigned char)s2[i]) {
            return 1;
        } else if ((unsigned char)s1[i] < (unsigned char)s2[i]) {
            return -1;
        } else if (s1[i] == 0) {
            return 0;
        }
    }
    return 0;
}

void *memset(void *s, int c, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        ((unsigned char *)s)[i] = (unsigned char)c;
    }
    return s;
}

void *memmove(void *dst, const void *src, size_t n) {
    unsigned char *p_src = (unsigned char *)src;
    unsigned char *p_dst = (unsigned char *)dst;
    if (p_src + n <= p_dst || p_dst + n <= p_src) {
        // no overlap
        memcpy(p_dst, p_src, n);
    } else if (p_src < p_dst) {
        size_t overlap_size = p_src + n - p_dst;
        size_t non_overlap_size = p_dst - p_src;
        memcpy(p_dst + non_overlap_size, p_src + non_overlap_size, overlap_size);
        memcpy(p_dst, p_src, non_overlap_size);
    } else if (p_dst < p_src) {
        size_t overlap_size = p_dst + n - p_src;
        size_t non_overlap_size = p_src - p_dst;
        memcpy(p_dst, p_src, overlap_size);
        memcpy(p_dst + non_overlap_size, p_src + non_overlap_size, non_overlap_size);
    }
    return dst;
}

void *memcpy(void *out, const void *in, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        ((unsigned char *)out)[i] = ((unsigned char *)in)[i];
    }
    return out;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    unsigned char *p1 = (unsigned char *)s1;
    unsigned char *p2 = (unsigned char *)s2;
    for (size_t i = 0; i < n; ++i) {
        if (p1[i] > p2[i]) {
            return 1;
        } else if (p1[i] < p2[i]) {
            return -1;
        }
    }
    return 0;
}

char *strchr(const char *s, int c) {
    do {
        if (*s == c) return (char *)s;
        if (*s == '\0') break;
        s++;
    } while (1);
    return NULL;
}

char *strrchr(const char *s, int c) {
    const char *p = s + strlen(s);
    do {
        if (*p == c) return (char *)p;
        if (s == p) break;
        p--;
    } while (1);
    return NULL;
}
