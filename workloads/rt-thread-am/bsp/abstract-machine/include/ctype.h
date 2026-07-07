#ifndef __CTYPE_H__
#define __CTYPE_H__

#ifdef __cplusplus
extern "C" {
#endif

int isspace(int c);
int isprint(int c);
int isdigit(int c);
int isxdigit(int c);
int islower(int c);
int isupper(int c);
int isalpha(int c);
int isalnum(int c);
int ispunct(int c);
int iscntrl(int c);
int isgraph(int c);
int tolower(int c);
int toupper(int c);

#ifdef __cplusplus
}
#endif

#endif /* __CTYPE_H__ */
