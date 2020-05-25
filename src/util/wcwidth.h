#ifndef WCWIDTH_H
#define WCWIDTH_H

#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

int mk_wcwidth(wchar_t);
int mk_wcswidth(const wchar_t *, size_t);
int mk_wcwidth_cjk(wchar_t);
int mk_wcswidth_cjk(const wchar_t *, size_t);

extern int (*wcwidth_auto)(wchar_t);
extern int (*wcswidth_auto)(const wchar_t *, size_t);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
