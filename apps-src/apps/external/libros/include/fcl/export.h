
#ifndef FCL_EXPORT_H
#define FCL_EXPORT_H

#ifdef FCL_STATIC_DEFINE
#  define FCL_EXPORT
#  define FCL_NO_EXPORT
#else
#  ifndef FCL_EXPORT
#    ifdef fcl_EXPORTS
        /* We are building this library */
#      define FCL_EXPORT 
#    else
        /* We are using this library */
#      define FCL_EXPORT 
#    endif
#  endif

#  ifndef FCL_NO_EXPORT
#    define FCL_NO_EXPORT 
#  endif
#endif

#ifndef FCL_DEPRECATED
#  if defined(__GNUC__)
#    define FCL_DEPRECATED __attribute__ ((deprecated))
#  elif defined(_MSC_VER)
#    define FCL_DEPRECATED __declspec(deprecated)
#  else
#    define CV_DEPRECATED
#  endif
#endif

#ifndef FCL_DEPRECATED_EXPORT
#  define FCL_DEPRECATED_EXPORT FCL_EXPORT FCL_DEPRECATED
#endif

#ifndef FCL_DEPRECATED_NO_EXPORT
#  define FCL_DEPRECATED_NO_EXPORT FCL_NO_EXPORT FCL_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef FCL_NO_DEPRECATED
#    define FCL_NO_DEPRECATED
#  endif
#endif

#endif /* FCL_EXPORT_H */
