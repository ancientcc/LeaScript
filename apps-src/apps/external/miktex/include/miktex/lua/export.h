
#ifndef MIKTEX_LUA_EXPORT_H
#define MIKTEX_LUA_EXPORT_H

#define MIKTEX_LUA_STATIC

#ifdef MIKTEX_LUA_STATIC
#  define MIKTEX_LUA_EXPORT
#  define MIKTEX_LUA_NO_EXPORT
#else
#  ifndef MIKTEX_LUA_EXPORT
#    ifdef MiKTeX251000_lua53_EXPORTS
        /* We are building this library */
#      define MIKTEX_LUA_EXPORT __declspec(dllexport)
#    else
        /* We are using this library */
#      define MIKTEX_LUA_EXPORT __declspec(dllimport)
#    endif
#  endif

#  ifndef MIKTEX_LUA_NO_EXPORT
#    define MIKTEX_LUA_NO_EXPORT 
#  endif
#endif

#ifndef MIKTEX_LUA_DEPRECATED
#  define MIKTEX_LUA_DEPRECATED __declspec(deprecated)
#endif

#ifndef MIKTEX_LUA_DEPRECATED_EXPORT
#  define MIKTEX_LUA_DEPRECATED_EXPORT MIKTEX_LUA_EXPORT MIKTEX_LUA_DEPRECATED
#endif

#ifndef MIKTEX_LUA_DEPRECATED_NO_EXPORT
#  define MIKTEX_LUA_DEPRECATED_NO_EXPORT MIKTEX_LUA_NO_EXPORT MIKTEX_LUA_DEPRECATED
#endif

/* NOLINTNEXTLINE(readability-avoid-unconditional-preprocessor-if) */
#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef MIKTEX_LUA_NO_DEPRECATED
#    define MIKTEX_LUA_NO_DEPRECATED
#  endif
#endif

#endif /* MIKTEX_LUA_EXPORT_H */
