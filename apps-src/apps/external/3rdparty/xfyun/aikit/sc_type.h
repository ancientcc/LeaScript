#ifndef SC_TYPE_API_H
#define SC_TYPE_API_H

/**
 * SPARKCHAIN API type
 */
#if defined(_MSC_VER)            /* Microsoft Visual C++ */
#   if !defined(SPARKCHAIN_API)
#       if defined(SPARKCHAIN_EXPORT)
#           define SPARKCHAIN_API __declspec(dllexport)
#       else
#           define SPARKCHAIN_API // define SPARKCHAIN_API __declspec(dllimport)
#       endif
#   endif
#elif defined(__BORLANDC__)      /* Borland C++ */
#   if !defined(SPARKCHAIN_API)
#       define SPARKCHAIN_API __stdcall
#   endif
#elif defined(__WATCOMC__)       /* Watcom C++ */
#   if !defined(SPARKCHAIN_API)
#       define SPARKCHAIN_API __stdcall
#   endif
#else                            /* Any other including Unix */
#   if !defined(SPARKCHAIN_API)
#       if defined(SPARKCHAIN_EXPORT)
#           define SPARKCHAIN_API __attribute__ ((visibility("default")))
#       else
#           define SPARKCHAIN_API
#       endif
#   endif
#endif

#if defined(_MSC_VER)            /* Microsoft Visual C++ */
  #pragma pack(push, 8)
#elif defined(__BORLANDC__)      /* Borland C++ */
  #pragma option -a8
#elif defined(__WATCOMC__)       /* Watcom C++ */
  #pragma pack(push, 8)
#else                            /* Any other including Unix */
#endif

typedef enum {
    AGENT_Event_UnKnown  = -1,
    AGENT_Event_Start    = 0,  
    AGENT_Event_Progress = 1,  
    AGENT_Event_End      = 2,  
    AGENT_Event_Timeout  = 3,  

    
    LLM_Event_Null = 10,          
    LLM_Event_Init,               
    LLM_Event_Connecting,         
    LLM_Event_ConnTimeout,        
    LLM_Event_Failed,             
    LLM_Event_Connected,          
    LLM_Event_Error,              
    LLM_Event_Disconnected,       
    LLM_Event_Closing,            
    LLM_Event_Closed,             
    LLM_Event_Responding,         
    LLM_Event_ResponseTimeout,    


    CHAIN_EVENT_UnKnown = 30,

} SPARKCHAIN_EVENT;

/* Reset the structure packing alignments for different compilers. */
#if defined(_MSC_VER)            /* Microsoft Visual C++ */
#pragma pack(pop)
#elif defined(__BORLANDC__)      /* Borland C++ */
#pragma option -a.
#elif defined(__WATCOMC__)       /* Watcom C++ */
#pragma pack(pop)
#endif

#endif