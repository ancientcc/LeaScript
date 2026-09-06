#ifndef __AIKIT_TYPE_H__
#define __AIKIT_TYPE_H__

#include <stddef.h>
#include <stdint.h>
#include "aikit_common.h"

#if defined(_MSC_VER)            /* Microsoft Visual C++ */
  #pragma pack(push, 8)
#elif defined(__BORLANDC__)      /* Borland C++ */
  #pragma option -a8
#elif defined(__WATCOMC__)       /* Watcom C++ */
  #pragma pack(push, 8)
#else                            /* Any other including Unix */
#endif

typedef enum _AIKIT_VarType {
    AIKIT_VarTypeString   =   0,      
    AIKIT_VarTypeInt      =   1,      
    AIKIT_VarTypeDouble   =   2,      
    AIKIT_VarTypeBool     =   3,      
    AIKIT_VarTypeParamPtr =   4,      
    AIKIT_VarTypeUnknown  =   -1      //
} AIKIT_VarType;

typedef enum _AIKIT_DataStatus {
    AIKIT_DataBegin    =  0,      
    AIKIT_DataContinue =  1,      
    AIKIT_DataEnd      =  2,
    AIKIT_DataOnce     =  3,
} AIKIT_DataStatus;

typedef enum _AIKIT_DataType {
    AIKIT_DataText    =   0,      
    AIKIT_DataAudio   =   1,      
    AIKIT_DataImage   =   2,      
    AIKIT_DataVideo   =   3,      
//    AIKIT_DataPer     =   4,      
} AIKIT_DataType;

typedef enum {
    AIKIT_Event_UnKnown  = 0,
    AIKIT_Event_Start    = 1,  
    AIKIT_Event_End      = 2,  
    AIKIT_Event_Timeout  = 3,  
    AIKIT_Event_Progress = 4,  

    
    AIKIT_Event_Null = 10,          
    AIKIT_Event_Init,               
    AIKIT_Event_Connecting,         
    AIKIT_Event_ConnTimeout,        
    AIKIT_Event_Failed,             
    AIKIT_Event_Connected,          //
    AIKIT_Event_Error,              //
    AIKIT_Event_Disconnected,       //
    AIKIT_Event_Closing,            //
    AIKIT_Event_Closed,             //
    AIKIT_Event_Responding,         //
    AIKIT_Event_ResponseTimeout,    //
    
    AIKIT_Event_VadBegin = 30, // 
    AIKIT_Event_VadEnd       // 
} AIKIT_EVENT;

typedef struct _AIKIT_BaseParam {
    struct _AIKIT_BaseParam *next;   // 
    const char *key;        // 
    void *value;            // 
    void* reserved;         // 
    int32_t len;            // 
    int32_t type;        // 
} AIKIT_BaseParam, *AIKIT_BaseParamPtr;      // 

typedef struct _AIKIT_BaseData {
    struct _AIKIT_BaseData *next;    // 
    AIKIT_BaseParam *desc;   // 
    const char *key;        // 
    void *value;            // 
    void* reserved;         // 
    int32_t len;            // 
    int32_t type;           // 
    int32_t status;         // 
    int32_t from;           // 
} AIKIT_BaseData, *AIKIT_BaseDataPtr;

typedef struct _AIKIT_CustomData {
    struct _AIKIT_CustomData*    next;       // 
    const char*         key;        // 
    void*               value;      // 
    void*               reserved;   // 
    int32_t             index;      // 
    int32_t             len;        // 
    int32_t             from;       // 
} AIKIT_CustomData, *AIKIT_CustomDataPtr;

/* Reset the structure packing alignments for different compilers. */
#if defined(_MSC_VER)            /* Microsoft Visual C++ */
#pragma pack(pop)
#elif defined(__BORLANDC__)      /* Borland C++ */
#pragma option -a.
#elif defined(__WATCOMC__)       /* Watcom C++ */
#pragma pack(pop)
#endif

#endif
