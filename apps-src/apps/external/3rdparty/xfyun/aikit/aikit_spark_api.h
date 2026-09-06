#ifndef AIKIT_SPARK_H
#define AIKIT_SPARK_H
#include "aikit_biz_api.h"

namespace AIKIT {


class AIKITAPI ChatParam {
public:
    static ChatParam* builder();
#ifndef _WIN32   
    virtual ~ChatParam();
#else
    virtual ~ChatParam() {}
#endif
    virtual ChatParam* uid(const char* uid) = 0;

    virtual ChatParam* domain(const char* domain) = 0;

    virtual ChatParam* auditing(const char* auditing) = 0;

    virtual ChatParam* chatID(const char* chatID) = 0;

    virtual ChatParam* temperature(const float& temperature) = 0;

    virtual ChatParam* topK(const int& topK) = 0;

    virtual ChatParam* maxToken(const int& maxToken) = 0;

    
    virtual ChatParam* url(const char* url) = 0;

    virtual ChatParam* param(const char* key, const char* value) = 0;
    virtual ChatParam* param(const char* key, int value)                          = 0;
    virtual ChatParam* param(const char* key, double value)                       = 0;
    virtual ChatParam* param(const char* key, bool value)                         = 0;

};

using AIChat_Handle = AIKIT_HANDLE;

typedef void (*onChatOutput)(AIChat_Handle* handle, const char* role, const char* content, const int& index);
typedef void (*onChatToken)(AIChat_Handle* handle, const int& completionTokens, const int& promptTokens, const int& totalTokens);
typedef void (*onChatError)(AIChat_Handle* handle, const int& err, const char* errDesc);
typedef struct {
    onChatOutput outputCB;     
    onChatToken  tokenCB;      //token计算信息回调
    onChatError  errorCB;      //错误回调
} AIKIT_ChatCBS;

AIKITAPI int32_t AIKIT_ChatCallback(const AIKIT_ChatCBS& cbs);
//异步chat
AIKITAPI int32_t AIKIT_AsyncChat(const ChatParam* params, const char* inputText, void* usrContext);

} // end of namespace AIKIT
#endif