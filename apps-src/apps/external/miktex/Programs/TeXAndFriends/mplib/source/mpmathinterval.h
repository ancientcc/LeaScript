/*3:*/

#ifndef MPMATHINTERVAL_H
#define  MPMATHINTERVAL_H 1
#include "mplib.h"
#include "mpmp.h" 
#include <gmp/include/gmp.h> 
#include <mpfr/include/mpfr.h> 
#include <mpfi/include/mpfi.h> 
#include <mpfi/include/mpfi_io.h> 

#ifdef HAVE_CONFIG_H_1
#define MP_STR_HELPER(x) #x
#define MP_STR(x) MP_STR_HELPER(x)



#endif

const char*COMPILED_MPFI_VERSION_STRING= MPFI_VERSION_STRING;




/*9:*/

void*mp_initialize_interval_math(MP mp);

/*:9*/
;
#endif

/*:3*/
