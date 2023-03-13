#include <stdlib.h>
#include "wcwidth.c"

int wcwidth_auto_default(wchar_t ucs);
int wcswidth_auto_default(const wchar_t *ucs, size_t n);

int (*wcwidth_auto)(wchar_t) = wcwidth_auto_default;
int (*wcswidth_auto)(const wchar_t *, size_t) = wcswidth_auto_default;

int wcwidth_auto_default(wchar_t ucs)
{
  if (getenv("MOSH_CJKWIDTH"))
    return (wcwidth_auto = mk_wcwidth_cjk)(ucs);
  else
    return (wcwidth_auto = mk_wcwidth)(ucs);
}

int wcswidth_auto_default(const wchar_t *ucs, size_t n)
{
  if (getenv("MOSH_CJKWIDTH"))
    return (wcswidth_auto = mk_wcswidth_cjk)(ucs, n);
  else
    return (wcswidth_auto = mk_wcswidth)(ucs, n);
}
