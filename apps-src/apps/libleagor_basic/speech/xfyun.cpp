/*
* Speech recognition(iFly Auto Transform)technology can convert speech into corresponding text in real time
* 
* base on <Windows_iat1226_tts_online1226_xxxxxxxx>/samples/iat_online_sample/iat_online_sample.c
*/

#include "xfyun.hpp"
// #include <errno.h>
#include <SDL.h>
#include <SDL_log.h>
#include "rose_filesystem.hpp"
#include "rose_exception.hpp"
#include "aplt_common.hpp"

#include "xfyun/include/qisr.h"
#include "xfyun/include/msp_cmn.h"
#include "xfyun/include/msp_errors.h"

struct tmsc_lib {
    void* fp;
	// MSPAPI must be used. Otherwise, on windows, executing QISRAudioWrite will result in an crash.
	const char* (MSPAPI *QISRSessionBegin)(const char* grammarList, const char* params, int* errorCode);
	int (MSPAPI *QISRAudioWrite)(const char* sessionID, const void* waveData, unsigned int waveLen, int audioStatus, int *epStatus, int *recogStatus);
	const char * (MSPAPI *QISRGetResult)(const char* sessionID, int* rsltStatus, int waitTime, int *errorCode);
	int (MSPAPI *QISRSessionEnd)(const char* sessionID, const char* hints);
	int (MSPAPI *MSPLogin)(const char* usr, const char* pwd, const char* params);
	int (MSPAPI *MSPLogout)();
};

static struct tmsc_lib lib = {nullptr};

#define	BUFFER_SIZE	4096
#define FRAME_LEN	640 
#define HINTS_SIZE  100

#ifdef _WIN32
// on windows, if don't define LOAD_PNG_DYNAMIC.
// 1)when compile, link msc.lib
// 2)copy msc.dll to directory that launncher.exe in.
#define LOAD_PNG_DYNAMIC
#else
// on android, if don't define LOAD_PNG_DYNAMIC.
// 1)when Android.mk, link libmsc.so
// 2)load libmsc.so when launcher.apk startup.
#define LOAD_PNG_DYNAMIC
#endif

#ifdef LOAD_PNG_DYNAMIC
#define FUNCTION_LOADER(FUNC, SIG) \
    lib.FUNC = (SIG) SDL_LoadFunction(lib.fp, #FUNC);
#else
#define FUNCTION_LOADER(FUNC, SIG) \
    lib.FUNC = FUNC;
#endif

namespace aplt {
txfyun::txfyun(const std::string& res_path, txf3params& xf3params)
	: xf3params_(xf3params)
	// , login_params_("appid = <xxxxxxxx>, work_dir = .") // change 'xxxxxxxx' into your appid
	// result_encoding = gb2312
	// result_encoding = unicode is 
	, session_begin_params_("sub = iat, domain = iat, language = zh_cn, accent = mandarin, sample_rate = 16000, result_type = plain, result_encoding = utf8")
	, started_(false)
{
	if (lib.fp == nullptr) {
		const std::string path = aplt_so_path(res_path, LIBROSEAPLT2_SO);
		lib.fp = SDL_LoadObject(path.c_str());

		FUNCTION_LOADER(QISRSessionBegin, const char* (MSPAPI *)(const char* grammarList, const char* params, int* errorCode));
		FUNCTION_LOADER(QISRAudioWrite, int (MSPAPI *)(const char* sessionID, const void* waveData, unsigned int waveLen, int audioStatus, int *epStatus, int *recogStatus));
		FUNCTION_LOADER(QISRGetResult, const char * (MSPAPI *)(const char* sessionID, int* rsltStatus, int waitTime, int *errorCode));
		FUNCTION_LOADER(QISRSessionEnd, int (MSPAPI *)(const char* sessionID, const char* hints));
		FUNCTION_LOADER(MSPLogin, int (MSPAPI *)(const char* usr, const char* pwd, const char* params));
		FUNCTION_LOADER(MSPLogout, int (MSPAPI *)());

		SDL_Log("path: %s lib.fp: 0x%p QISRSessionBegin: 0x%p QISRAudioWrite: 0x%p", path.c_str(), lib.fp, 
			lib.QISRSessionBegin, lib.QISRAudioWrite);

		if (lib.QISRSessionBegin == nullptr || lib.QISRAudioWrite == nullptr || lib.QISRGetResult == nullptr ||
			lib.QISRSessionEnd == nullptr || lib.MSPLogin == nullptr || lib.MSPLogout == nullptr) {
			SDL_UnloadObject(lib.fp);
			lib.fp = nullptr;
		}
	}
}

txfyun::~txfyun()
{
	if (started_) {
		recognition_stop();
	}

	if (lib.fp != nullptr) {
		SDL_UnloadObject(lib.fp);
		lib.fp = nullptr;
	}
}

bool txfyun::libmsc_loaded() const
{
	return lib.fp != nullptr;
}

std::string txfyun::recognition_wav_file(const std::string& audio_file)
{
	VALIDATE(!audio_file.empty(), null_str);
	VALIDATE(!started_, null_str);

	tfile f_pcm(audio_file, GENERIC_READ, OPEN_EXISTING);
	if (!f_pcm.valid()) {
		SDL_Log("recognition_wav_file, open [%s] failed!", audio_file.c_str());
		return null_str;
	}
	
	int pcm_size = f_pcm.read_2_data();
	if (pcm_size == 0) {
		SDL_Log("recognition_wav_file, read [%s] data failed!", audio_file.c_str());
		return null_str;
	}

	recognition_start();
	if (!started_) {
		return null_str;
	}
	std::string result = iat_piece((const uint8_t*)f_pcm.data, pcm_size);
	recognition_stop();
	return result;
}

bool txfyun::recognition_start()
{
	VALIDATE(!started_, null_str);
	SDL_Log("recognition_start(), enter");

	std::string appid;
	{
		threading::lock lock(xf3params_.mutex);
		appid = xf3params_.appid;
	}
	if (appid.empty()) {
		// pinyin_.speak(_("The APPID used to access iFlytek is correct"));
		errcode_ = MSP_ERROR_DB_INVALID_APPID;
		return false;
	}

	char login_params[128];
	SDL_snprintf(login_params, sizeof(login_params), "appid = %s, work_dir = .", appid.c_str());

	// int ret = lib.MSPLogin(NULL, NULL, login_params_.c_str());
	int ret = lib.MSPLogin(NULL, NULL, login_params);
	if (MSP_SUCCESS != ret) {
		SDL_Log("MSPLogin failed , Error code %d.", ret);
		return false;
	}
	started_ = true;
	return true;
}

void txfyun::recognition_stop()
{
	VALIDATE(started_, null_str);
	lib.MSPLogout();
	started_ = false;
}

std::string txfyun::iat_piece(const uint8_t* wav, int len2)
{
	const char*		session_id					=	NULL;
	char			rec_result[BUFFER_SIZE]		=	{NULL};	
	// hints is a description of the reason for ending this session, which is customized by the user
	char			hints[HINTS_SIZE]			=	{NULL};
	unsigned int	total_len					=	0; 
	// audio state
	int				aud_stat					=	MSP_AUDIO_SAMPLE_CONTINUE ;
	// Endpoint detection
	int				ep_stat						=	MSP_EP_LOOKING_FOR_SPEECH;
	// the status of recognizer
	int				rec_stat					=	MSP_REC_STATUS_SUCCESS ;
	int				errcode						=	MSP_SUCCESS ;

	const uint8_t*			p_pcm						=	wav;
	int			pcm_count					=	0;
	int			pcm_size					=	len2;
	int			read_size					=	0;

	SDL_Log("Start voice dictation ... pcm_size: %i", (int)pcm_size);
	// Dictation does not require syntax, and the first parameter is NULL
	session_id = lib.QISRSessionBegin(NULL, session_begin_params_.c_str(), &errcode);
	if (MSP_SUCCESS != errcode) {
		SDL_Log("QISRSessionBegin failed! error code:%d", errcode);
		goto iat_exit;
	}
	
	while (1) {
		// Write 200ms audio (16K, 16bit) each time: 
		//   1 frame of audio 20ms, 10 frames = 200ms. 
		//   16-bit audio at 16k sample rate, one frame size of 640Byte
		int len = 10 * FRAME_LEN; //
		int ret = 0;

		if (pcm_size < 2 * len) 
			len = pcm_size;
		if (len <= 0)
			break;

		aud_stat = MSP_AUDIO_SAMPLE_CONTINUE;
		if (0 == pcm_count)
			aud_stat = MSP_AUDIO_SAMPLE_FIRST;

		// SDL_Log("> len: %i", len);
		ret = lib.QISRAudioWrite(session_id, (const void *)&p_pcm[pcm_count], len, aud_stat, &ep_stat, &rec_stat);
		// ret = lib.QISRAudioWrite(session_id, (const void *)&wav[pcm_count], len, aud_stat, &ep_stat, &rec_stat);
		if (MSP_SUCCESS != ret)
		{
			errcode = ret;
			SDL_Log("QISRAudioWrite failed! error code:%d", ret);
			goto iat_exit;
		}
			
		pcm_count += len;
		pcm_size  -= len;
		
		// There are already partial dictation results
		if (MSP_REC_STATUS_SUCCESS == rec_stat) {
			const char *rslt = lib.QISRGetResult(session_id, &rec_stat, 0, &errcode);
			if (MSP_SUCCESS != errcode)
			{
				SDL_Log("QISRGetResult failed! error code: %d", errcode);
				goto iat_exit;
			}
			if (NULL != rslt)
			{
				unsigned int rslt_len = strlen(rslt);
				total_len += rslt_len;
				if (total_len >= BUFFER_SIZE)
				{
					SDL_Log("no enough buffer for rec_result !");
					goto iat_exit;
				}
				strncat(rec_result, rslt, rslt_len);
			}
		}

		if (MSP_EP_AFTER_SPEECH == ep_stat)
			break;
		// Simulates a human speaking time slot. 200ms corresponds to 10 frames of audio
		SDL_Delay(200);
	}
	errcode = lib.QISRAudioWrite(session_id, NULL, 0, MSP_AUDIO_SAMPLE_LAST, &ep_stat, &rec_stat);
	if (MSP_SUCCESS != errcode)
	{
		SDL_Log("QISRAudioWrite failed! error code:%d", errcode);
		goto iat_exit;	
	}

	while (MSP_REC_STATUS_COMPLETE != rec_stat) 
	{
		const char *rslt = lib.QISRGetResult(session_id, &rec_stat, 0, &errcode);
		if (MSP_SUCCESS != errcode)
		{
			SDL_Log("QISRGetResult failed, error code: %d", errcode);
			goto iat_exit;
		}
		if (NULL != rslt)
		{
			unsigned int rslt_len = strlen(rslt);
			total_len += rslt_len;
			if (total_len >= BUFFER_SIZE)
			{
				SDL_Log("no enough buffer for rec_result !");
				goto iat_exit;
			}
			strncat(rec_result, rslt, rslt_len);
		}
		// Prevent frequent CPU usage
		SDL_Delay(150);
	}
	SDL_Log("End of voice dictation");
	SDL_Log("=============================================================");
	SDL_Log("%s", rec_result);
	SDL_Log("=============================================================");

iat_exit:

	lib.QISRSessionEnd(session_id, hints);

	errcode_ = errcode;
	return rec_result;
}

}