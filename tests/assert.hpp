#ifndef QBUF_ASSERT_HPP
#define QBUF_ASSERT_HPP

// adaptation of assert.h that is unaffected by NDEBUG

#include <features.h>

#ifndef __COLD
#define __COLD // unsupported if not defined
#endif

__BEGIN_DECLS

/* This prints an "Assertion failed" message and aborts.  */
extern void __assert_fail(
    const char* __assertion, const char* __file, unsigned int __line, const char* __function
) __THROW __attribute__((__noreturn__)) __COLD;

/* Likewise, but prints the error text for ERRNUM.  */
extern void __assert_perror_fail(
    int __errnum, const char* __file, unsigned int __line, const char* __function
) __THROW __attribute__((__noreturn__)) __COLD;

/* The following is not at all used here but needed for standard
   compliance.  */
extern void __assert(const char* __assertion, const char* __file, int __line) __THROW
    __attribute__((__noreturn__)) __COLD;

__END_DECLS

/* When possible, define assert so that it does not add extra
   parentheses around EXPR.  Otherwise, those added parentheses would
   suppress warnings we'd expect to be detected by gcc's -Wparentheses.  */
#if defined __cplusplus
#if defined __has_builtin
#if __has_builtin(__builtin_FILE)
#define __ASSERT_FILE __builtin_FILE()
#define __ASSERT_LINE __builtin_LINE()
#endif
#endif
#if !defined __ASSERT_FILE
#define __ASSERT_FILE __FILE__
#define __ASSERT_LINE __LINE__
#endif
#define assert(expr)                                                                               \
    (static_cast<bool>(expr)                                                                       \
         ? void(0)                                                                                 \
         : __assert_fail(#expr, __ASSERT_FILE, __ASSERT_LINE, __ASSERT_FUNCTION))
#elif !defined __GNUC__ || defined __STRICT_ANSI__
#define assert(expr)                                                                               \
    ((expr) ? __ASSERT_VOID_CAST(0) : __assert_fail(#expr, __FILE__, __LINE__, __ASSERT_FUNCTION))
#else
/* The first occurrence of EXPR is not evaluated due to the sizeof,
   but will trigger any pedantic warnings masked by the __extension__
   for the second occurrence.  The ternary operator is required to
   support function pointers and bit fields in this context, and to
   suppress the evaluation of variable length arrays.  */
#define assert(expr)                                                                               \
    ((void)sizeof((expr) ? 1 : 0), __extension__({                                                 \
         if (expr)                                                                                 \
             ; /* empty */                                                                         \
         else                                                                                      \
             __assert_fail(#expr, __FILE__, __LINE__, __ASSERT_FUNCTION);                          \
     }))
#endif

#ifdef __USE_GNU
#define affirm_perror(errnum)                                                                      \
    (!(errnum) ? __ASSERT_VOID_CAST(0)                                                             \
               : __assert_perror_fail((errnum), __FILE__, __LINE__, __ASSERT_FUNCTION))
#endif

/* Version 2.4 and later of GCC define a magical variable `__PRETTY_FUNCTION__'
   which contains the name of the function currently being defined.
   This is broken in G++ before version 2.6.
   C9x has a similar variable called __func__, but prefer the GCC one since
   it demangles C++ function names.  */
#if defined __cplusplus ? __GNUC_PREREQ(2, 6) : __GNUC_PREREQ(2, 4)
#define __ASSERT_FUNCTION __extension__ __PRETTY_FUNCTION__
#else
#if defined __STDC_VERSION__ && __STDC_VERSION__ >= 199901L
#define __ASSERT_FUNCTION __func__
#else
#define __ASSERT_FUNCTION ((const char*)0)
#endif
#endif

#endif // QBUF_ASSERT_HPP
