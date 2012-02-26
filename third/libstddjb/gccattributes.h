/* ISC license. */

#ifndef GCCATTRIBUTES_H
#define GCCATTRIBUTES_H

#ifdef __GNUC__

#define gccattr_noreturn __attribute__((__noreturn__))
#define gccattr_noinline __attribute__((__noinline__))
#define gccattr_inline __attribute__((__always_inline__))
#define gccattr_const __attribute__((__const__))
#define gccattr_unused __attribute__((__unused__))
#define gccattr_used __attribute__((__used__))
#define gccattr_weak __attribute__((__weak__))
#define gccattr_aligned __attribute__((__aligned__))

# if (__GNUC__ >= 3) || ((__GNUC__ == 2) && (__GNUC_MINOR__ >= 96))
#define gccattr_malloc __attribute__((__malloc__))
#define gccattr_pure __attribute__((__pure__))
# else
#define gccattr_malloc
#define gccattr_pure
# endif

# if (__GNUC__ >= 3)
#define gccattr_deprecated __attribute__((__deprecated__))
# else
#define gccattr_deprecated
# endif

#else

#define gccattr_noreturn
#define gccattr_noinline
#define gccattr_inline
#define gccattr_const
#define gccattr_unused
#define gccattr_used
#define gccattr_weak
#define gccattr_aligned
#define gccattr_malloc
#define gccattr_pure
#define gccattr_deprecated

#endif

#ifdef GCCATTR_COMPAT_0_22
#define _a_noreturn gccattr_noreturn
#define _a_noinline gccattr_noinline
#define _a_inline gccattr_inline
#define _a_const gccattr_const
#define _a_unused gccattr_unused
#define _a_used gccattr_used
#define _a_weak gccattr_weak
#define _a_aligned gccattr_aligned
#define _a_malloc gccattr_malloc
#define _a_pure gccattr_pure
#define _a_deprecated gccattr_deprecated
#endif

#endif
