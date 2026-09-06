/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2018 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "../../SDL_internal.h"

#include "SDL_peripheral.h"


#ifndef posix_mku32
	#define posix_mku32(l, h)		((uint32_t)(((uint16_t)(l)) | ((uint32_t)((uint16_t)(h))) << 16))
#endif

#ifndef posix_mku16
	#define posix_mku16(l, h)		((uint16_t)(((uint8_t)(l)) | ((uint16_t)((uint8_t)(h))) << 8))
#endif

SDL_bool SDL_ReadCard(SDL_IDCardInfo* info)
{
	static uint32_t times = 0;
	static int desire_card_type = SDL_CardIDCard;

	SDL_memset(info, 0, sizeof(SDL_IDCardInfo));

	if (++ times % 5) {
		return SDL_FALSE;
	}

/*
	{
		int ii = 0;
		return SDL_FALSE;
	}
*/
	if (desire_card_type == SDL_CardIDCard) {
		info->type = SDL_CardIDCard;
		SDL_bool leagor = SDL_TRUE;
		if (leagor) {
			SDL_strlcpy(info->name, "test", sizeof(info->name));
			info->gender = SDL_GenderMale;
			info->folk = 1;
			SDL_strlcpy(info->birthday, "19791204", sizeof(info->birthday));
			SDL_strlcpy(info->number, "12345678", sizeof(info->number));
			SDL_strlcpy(info->address, "Zhe Jiang", sizeof(info->address));

		} else {
			SDL_strlcpy(info->name, "cotest", sizeof(info->name));
			info->gender = SDL_GenderMale;
			info->folk = 1;
			SDL_strlcpy(info->birthday, "19751004", sizeof(info->birthday));
			SDL_strlcpy(info->number, "12345678X", sizeof(info->number));
			SDL_strlcpy(info->address, "Shang Hai", sizeof(info->address));
		}

		int width = 240;
		int height = 250;
		info->photo = SDL_CreateRGBSurface(0, width, height, 4 * 8,
				0xFF0000, 0xFF00, 0xFF, 0xFF000000); // SDL_PIXELFORMAT_ARGB8888
		uint32_t* pixels = info->photo->pixels;
		for (int row = 0; row < height; row ++) {
			for (int col = 0; col < width; col ++) {
				uint32_t* pixel = pixels + row * width + col;
				if ((times / 10) & 1) {
					*pixel = 0x80ff0000;
				} else {
					*pixel = 0x800000ff;
				}
			}
		}
		desire_card_type = SDL_CardIC34;

	} else if (desire_card_type == SDL_CardIC34) {
		info->type = SDL_CardIC34;
		SDL_strlcpy(info->number, "1234567890", sizeof(info->number));
		desire_card_type = SDL_CardIDCard;

	} else {
		return SDL_FALSE;
	}

	return SDL_TRUE;
}

SDL_handle SDL_OpenSerialPort(const char* _path, int baudrate)
{
	BYTE parity = NOPARITY;
	// 打开串口, >=COM10时, 必须使用: \\\\.\\COM10, 为方便, 1~9的也这样处理了
	char path[128];
	SDL_snprintf(path, sizeof(path), "\\\\.\\%s", _path);
	HANDLE m_hComm = CreateFile(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL); //独占方式打开串口
	// HANDLE m_hComm = CreateFile(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL); //独占方式打开串口

	if (m_hComm  == INVALID_HANDLE_VALUE) {  
		// _stprintf(err, _T("打开串口%s 失败，请查看该串口是否已被占用"), port);  
		return SDL_INVALID_HANDLE_VALUE;
	}  
  
	//MessageBox(NULL,_T("打开成功"),_T("提示"),MB_OK);  
  
	//获取串口默认配置  
	DCB dcb;  
	if (!GetCommState(m_hComm, &dcb)) {  
		// MessageBox(NULL, _T("获取串口当前属性参数失败"), _T("提示"), MB_OK);
		CloseHandle(m_hComm);
		return SDL_INVALID_HANDLE_VALUE;
	}  
  
	//配置串口参数  
	dcb.BaudRate  = baudrate;  //波特率  
	dcb.fBinary  = TRUE;            //二进制模式。必须为TRUE  
	dcb.ByteSize  = 8;  //数据位。范围4-8  
	dcb.StopBits  = ONESTOPBIT; //停止位  
  
	if (parity  == NOPARITY) {  
		dcb.fParity  = FALSE;   //奇偶校验。无奇偶校验  
		dcb.Parity  = parity;   //校验模式。无奇偶校验  
	} else {  
		dcb.fParity  = TRUE;        //奇偶校验。  
		dcb.Parity  = parity;   //校验模式。无奇偶校验  
	}  
  
	dcb.fOutxCtsFlow  = FALSE;  //CTS线上的硬件握手  
	dcb.fOutxDsrFlow  = FALSE;  //DST线上的硬件握手  
	dcb.fDtrControl  = DTR_CONTROL_ENABLE;//DTR控制  
	dcb.fDsrSensitivity  = FALSE;  
	dcb.fTXContinueOnXoff  = FALSE;//  
	dcb.fOutX  = FALSE;         //是否使用XON/XOFF协议  
	dcb.fInX  = FALSE;          //是否使用XON/XOFF协议  
	dcb.fErrorChar  = FALSE;        //是否使用发送错误协议  
	dcb.fNull  = FALSE;         //停用null stripping  
	dcb.fRtsControl  = RTS_CONTROL_ENABLE;//  
	dcb.fAbortOnError  = FALSE; //串口发送错误，并不终止串口读写  
  
								//设置串口参数  
	if (!SetCommState(m_hComm, &dcb)) {  
		// MessageBox(NULL, _T("设置串口参数失败"), _T("提示"), MB_OK);
		CloseHandle(m_hComm);
		return SDL_INVALID_HANDLE_VALUE;  
	}  
  
	//设置串口事件  
	SetCommMask(m_hComm, EV_RXCHAR);//在缓存中有字符时产生事件  
	SetupComm(m_hComm, 16384, 16384);  
  
	//设置串口读写时间  
	COMMTIMEOUTS CommTimeOuts;  
	GetCommTimeouts(m_hComm, &CommTimeOuts);  
	CommTimeOuts.ReadIntervalTimeout  = MAXDWORD;  
	CommTimeOuts.ReadTotalTimeoutMultiplier  = 0;  
	CommTimeOuts.ReadTotalTimeoutConstant  = 0;  
	CommTimeOuts.WriteTotalTimeoutMultiplier  = 10;  
	CommTimeOuts.WriteTotalTimeoutConstant  = 1000;  
  
	if (!SetCommTimeouts(m_hComm, &CommTimeOuts)) {  
		// MessageBox(NULL, _T("设置串口时间失败"), _T("提示"), MB_OK);
		CloseHandle(m_hComm);
		return SDL_INVALID_HANDLE_VALUE;  
	}  

	return m_hComm;
}

void SDL_CloseSerialPort(SDL_handle h)
{
	if (h == SDL_INVALID_HANDLE_VALUE) {
		return;
	}
	CloseHandle(h);
}

size_t SDL_ReadSerialPort(SDL_handle h, void* ptr, size_t size)
{
	if (h == INVALID_HANDLE_VALUE) {
		return 0;
	}


	DWORD read_byte;
	BOOL ok = ReadFile(h, ptr, size, &read_byte, NULL);
	if (!ok) {
		read_byte = 0;
	}

	return read_byte;
}

size_t SDL_WriteSerialPort(SDL_handle h, const void* ptr, size_t size)
{
	const char* ptr2 = ptr;

	if (h == INVALID_HANDLE_VALUE) {
		return 0;
	}
	// clear serial port
	PurgeComm(h, PURGE_RXCLEAR | PURGE_TXCLEAR);

	DWORD write_byte = 0;
	BOOL ok = WriteFile(h, ptr, size, &write_byte, NULL);
	if (!ok) {
		write_byte = 0;
	}

	// clear serial port
	// PurgeComm(h, PURGE_RXCLEAR | PURGE_TXCLEAR);
	return write_byte;
}

// return: 0, no usb tty.
//   !=0, usb tty count. and pttyUSb is valid. sizeof(pttyUSB) is fix_count + usb_count.
static int fill_usb_tty(int fix_count, SDL_ttyUSB** ppttyUSB)
{
	*ppttyUSB = NULL;
	SDL_RWops* fp = SDL_RWFromFile("c:/ddksample/ttyUSB-0-1.dat", "rb");
	// SDL_RWops* fp = SDL_RWFromFile("c:/ddksample/ttyUSB-0.dat", "rb");
	// SDL_RWops* fp = SDL_RWFromFile("c:/ddksample/ttyUSB-1.dat", "rb");
	// SDL_RWops* fp = SDL_RWFromFile("c:/ddksample/ttyUSB-.dat", "rb");
	if (fp == NULL) {
		return 0;
	}


	// usbserinfo:1.0 driver:2.0
    // 0: name:"ch341-uart" vendor:1a86 product:7523 num_ports:1 port:0 path:usb-fe3c0000.usb-1.4
    // 1: name:"cp210x" vendor:10c4 product:ea60 num_ports:1 port:0 path:usb-fe3c0000.usb-1.7

	const int max_bytes = 4096;
	const int max_valid_bytes = max_bytes - 1;
	char* buf = (char*)SDL_malloc(max_bytes);
	SDL_memset(buf, 0, max_bytes);

	int one_block = 10;
	int pos = 0;
	int ret_bytes = 0;
	do {
		one_block = 10;
		if (pos + one_block > max_valid_bytes) {
			one_block = max_valid_bytes - pos;
		}
		ret_bytes = SDL_RWread(fp, buf + pos, 1, one_block);
		pos += ret_bytes;
	} while (ret_bytes == one_block);
	SDL_RWclose(fp);

	if (pos == 0 || buf[pos - 1] != '\n') {
		SDL_free(buf);
		return 0;
	}

	const int max_usb_count = 20;
	const char* items[20];

	int usb_count = 0;
	int n = 0;
	char ch = buf[n];
	while (ch != '\0') {
		if (ch == '\n') {
			buf[n] = '\0';
			if (n + 1 == pos) {
				break;
			}
			char next = buf[n + 1];
			if (next >= '0' && next <= '9') {
				items[usb_count] = buf + n + 1;
				usb_count ++;
				if (usb_count == max_usb_count) {
					break;
				}
			}

		}
		ch = buf[++ n];
	}

	if (usb_count == 0) {
		SDL_free(buf);
		return 0;
	}

	int total_count = fix_count + usb_count;
	char prefix[20];
	SDL_ttyUSB* ttyUSBs = (SDL_ttyUSB*)SDL_malloc(sizeof(SDL_ttyUSB) * total_count);
	SDL_memset(ttyUSBs, 0, sizeof(SDL_ttyUSB) * total_count);
	for (int at = fix_count; at < total_count; at ++) {
		SDL_ttyUSB* tty = ttyUSBs + at;
		const char* ptr = items[at - fix_count];
		char ch = *ptr;
		SDL_snprintf(tty->dev_node, sizeof(tty->dev_node), "/dev/ttyUSB%i", ch - '0');

		// name
		SDL_strlcpy(prefix, "name:\"", sizeof(prefix));
		char* tmp = SDL_strstr(ptr, prefix);
		if (tmp == NULL) {
			// should not occur.
			continue;
		}
		tmp += SDL_strlen(prefix);
		char* endp = SDL_strchr(tmp, '\"');
		if (endp == NULL) {
			// should not occur.
			continue;
		}
		*endp = '\0';
		SDL_strlcpy(tty->name, tmp, sizeof(tty->name));

		// vid
		tmp = endp + 1;
		SDL_strlcpy(prefix, "vendor:", sizeof(prefix));
		tmp = SDL_strstr(tmp, prefix);
		if (tmp == NULL) {
			// should not occur.
			continue;
		}
		tmp += SDL_strlen(prefix);
		endp = tmp + 4;
		*endp = '\0';
		long l = SDL_strtol(tmp, &endp, 16);
		tty->vid = (uint32_t)l;

		// pid
		tmp = endp + 1;
		SDL_strlcpy(prefix, "product:", sizeof(prefix));
		tmp = SDL_strstr(tmp, prefix);
		if (tmp == NULL) {
			// should not occur.
			continue;
		}
		tmp += SDL_strlen(prefix);
		endp = tmp + 4;
		*endp = '\0';
		l = SDL_strtol(tmp, &endp, 16);
		tty->pid = (uint32_t)l;

		// path
		tmp = endp + 1;
		SDL_strlcpy(prefix, "usb-", sizeof(prefix));
		tmp = SDL_strstr(tmp, prefix);
		if (tmp == NULL) {
			// should not occur.
			continue;
		}
		tmp += SDL_strlen(prefix);
		SDL_strlcpy(tty->path, tmp, sizeof(tty->path));

	}
	SDL_free(buf);

	*ppttyUSB = ttyUSBs;
	return usb_count;
}

int SDL_GetTtyUSB(const char fix_dev_nodes[][48], int fix_count, SDL_ttyUSB** ppttyUSB)
{
	SDL_ttyUSB* ttyUSBs = NULL;

	int usb_count = fill_usb_tty(fix_count, &ttyUSBs);
	int total_count = fix_count + usb_count;
	if (usb_count == 0 && total_count != 0) {
		ttyUSBs = (SDL_ttyUSB*)SDL_malloc(sizeof(SDL_ttyUSB) * total_count);
		SDL_memset(ttyUSBs, 0, sizeof(SDL_ttyUSB) * total_count);
	}
	
	for (int n = 0; n < fix_count; n ++) {
		SDL_ttyUSB* tty = ttyUSBs + n;
        SDL_strlcpy(tty->dev_node, fix_dev_nodes[n], sizeof(tty->dev_node));
		SDL_strlcpy(tty->path, fix_dev_nodes[n], sizeof(tty->dev_node));
    }

	*ppttyUSB = ttyUSBs;
	return total_count;
}

SDL_bool SDL_SetTime(int64_t utctime)
{
	return SDL_FALSE;
}

void SDL_UpdateApp(const char* package)
{
}

SDL_bool SDL_NotifyMediaFileAdded(const char* filename)
{
	return SDL_TRUE;
}

void win_ShellExecuteW_open(const char* url)
{
	// VALIDATE(game_config::os == os_windows, null_str);
	// VALIDATE(!url.empty(), null_str);
	if (url == NULL) {
		return;
	}

	// wchar_t* urlw = (wchar_t *)SDL_iconv_string("UTF-16LE", "UTF-8", (char *)(url.c_str()), url.size()+1);
	wchar_t* urlw = (wchar_t *)SDL_iconv_string("UTF-16LE", "UTF-8", (char *)(url), SDL_strlen(url)+1);
	ShellExecuteW(NULL, L"open", urlw, NULL, NULL, SW_SHOWNORMAL);
	SDL_free(urlw);
}

void SDL_OpenUrl(const char* url)
{
	win_ShellExecuteW_open(url);
}

int SDL_GetPublicDirectory(SDL_PublicDir type, char* name, int maxlen)
{
	name[0] = '\0';
	return 0;

	// SDL_strlcpy(name, "c:/ddksample", maxlen);
	// return (int)SDL_strlen(name);
}

void SDL_StartScreenRecording(const char* filename)
{
}

SDL_bool SDL_StopScreenRecording(char* filename, int maxlen)
{
	filename[0] = '\0';
	return SDL_TRUE;
}

SDL_bool SDL_IsScreenRecording(void)
{
	return SDL_FALSE;
}

void SDL_GetOsInfo(SDL_OsInfo* info)
{
	SDL_strlcpy(info->manufacturer, "Microsoft", sizeof(info->manufacturer));
	SDL_strlcpy(info->model, "Windows", sizeof(info->model));
	SDL_snprintf(info->display, sizeof(info->display), "%s", "windows");

	SDL_strlcpy(info->cpuid, "3e6368418cc05f16", sizeof(info->cpuid));
	SDL_strlcpy(info->serialnumber, "aplt.nlsd.basic__C025CBC3FVH5", sizeof(info->serialnumber));
}

SDL_bool SDL_PrintText(const uint8_t* bytes, int size)
{
	return SDL_TRUE;
}

void SDL_EnableRelay(SDL_bool enable)
{
}

void SDL_SetFlashlight(SDL_LightColor color)
{
}

void SDL_SetBrightness(int brightness)
{
}

SDL_bool SDLCALL SDL_OrbbecCreateContext(void)
{
	return SDL_TRUE;
}

void SDL_OrbbecDestroyContext(void)
{
}

void SDL_WifiSetEnable(SDL_bool enable)
{
}

uint32_t SDL_WifiGetFlags(void)
{
	return SDL_WifiFlagEnable + SDL_WifiFlagScanResultChanged;
}

int SDL_WifiGetScanResults(SDL_WifiScanResult** results)
{
	char buf[32];
	int count = 24; // max is 24, see leagor_ble.cpp's max_wifiaps
	SDL_WifiScanResult* ptr = SDL_malloc(sizeof(SDL_WifiScanResult) * count);
    SDL_memset(ptr, 0, sizeof(SDL_WifiScanResult) * count);
	for (int n = 0; n < count; n ++) {
		SDL_WifiScanResult* result = ptr + n;
		SDL_snprintf(buf, sizeof(buf), "leagor-%02i", n);
		SDL_strlcpy(result->ssid, buf, sizeof(result->ssid));
		result->rssi = -36 + n;
		if (n == 2) {
			result->flags = SDL_WifiApFlagConnected;
		} else if (n == 6) {
			result->rssi = 172;
		}
	}
	*results = ptr;
	return count;
}

SDL_bool SDL_WifiConnect(const char* ssid, const char* password)
{
	return SDL_TRUE;
}

SDL_bool SDL_WifiRemove(const char* ssid)
{
	return SDL_TRUE;
}

/* vi: set ts=4 sw=4 expandtab: */
