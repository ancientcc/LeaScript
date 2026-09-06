// In file included from jni/../../../../external/miktex/Libraries/3rd/gmp/source/assert.c:37:
// jni/../../../../external/miktex/Libraries/3rd/gmp/source/gmp-impl.h:4723:9: error: call to undeclared function
//      '__gmpn_add_1'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]
//  4723 |   co += mpn_add_1 (rp, rp, n, ci);

#ifndef GMP_COMPAT_H
#define GMP_COMPAT_H

// #ifndef __GMP_WITHIN_GMP
// #define __GMP_WITHIN_GMP 1
// #endif

#include "gmp.h"

// #if __GMP_WITHIN_GMP
/* lower mpn api */
mp_limb_t __gmpn_add (mp_ptr, mp_srcptr, mp_size_t, mp_srcptr, mp_size_t);
mp_limb_t __gmpn_add_1 (mp_ptr, mp_srcptr, mp_size_t, mp_limb_t);
mp_limb_t __gmpn_add_n (mp_ptr, mp_srcptr, mp_srcptr, mp_size_t);
mp_limb_t __gmpn_sub (mp_ptr, mp_srcptr, mp_size_t, mp_srcptr, mp_size_t);
mp_limb_t __gmpn_sub_1 (mp_ptr, mp_srcptr, mp_size_t, mp_limb_t);
mp_limb_t __gmpn_sub_n (mp_ptr, mp_srcptr, mp_srcptr, mp_size_t);
mp_limb_t __gmpn_mul_1 (mp_ptr, mp_srcptr, mp_size_t, mp_limb_t);
mp_limb_t __gmpn_addmul_1 (mp_ptr, mp_srcptr, mp_size_t, mp_limb_t);
mp_limb_t __gmpn_submul_1 (mp_ptr, mp_srcptr, mp_size_t, mp_limb_t);
mp_limb_t __gmpn_lshift (mp_ptr, mp_srcptr, mp_size_t, unsigned int);
mp_limb_t __gmpn_rshift (mp_ptr, mp_srcptr, mp_size_t, unsigned int);
mp_limb_t __gmpn_neg (mp_ptr, mp_srcptr, mp_size_t);
int __gmpn_cmp (mp_srcptr, mp_srcptr, mp_size_t);
int __gmpn_zero_p (mp_srcptr, mp_size_t);

// for mpfi/xxx
unsigned long int __gmpz_get_ui (mpz_srcptr);
void __gmpz_neg (mpz_ptr, mpz_srcptr);
size_t __gmpz_size (mpz_srcptr);
void __gmpz_abs (mpz_ptr, mpz_srcptr);
// #endif

// #endif /* GMP_COMPAT_H */

#endif /* __GMP_IMPL_H__ */
