#ifndef __FCNTL_H__
#define __FCNTL_H__

#define O_RDONLY    0x0000
#define O_WRONLY    0x0001
#define O_RDWR      0x0002
#define O_ACCMODE   0x0003

#define O_CREAT     0x0100
#define O_EXCL      0x0200
#define O_TRUNC     0x0400
#define O_APPEND    0x0800
#define O_NONBLOCK  0x1000
#define O_DIRECTORY 0x2000

#define F_GETLK     5
#define F_SETLK     6
#define F_SETLKW    7
#define F_GETFL     8
#define F_SETFL     9

#define SEEK_SET    0
#define SEEK_CUR    1
#define SEEK_END    2

#ifdef __cplusplus
extern "C" {
#endif

int open(const char *pathname, int flags, ...);
int creat(const char *pathname, mode_t mode);
int fcntl(int fd, int cmd, ...);

#ifdef __cplusplus
}
#endif

#endif
