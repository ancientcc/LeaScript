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

#include <unistd.h>
#include <termios.h> 
#include <fcntl.h> 
#include <errno.h>
#include <time.h>
#include "SDL_peripheral.h"
#include "SDL_log.h"
#include "../../core/android/SDL_android.h"
#include <arpa/inet.h>

SDL_bool SDL_ReadCard(SDL_IDCardInfo* info)
{
	SDL_memset(info, 0, sizeof(SDL_IDCardInfo));
	return Android_JNI_ReadCard(info);
}

static speed_t getBaudrate(int baudrate)
{
	switch (baudrate) {  
	case 0:  
		return B0;  
	case 50:  
		return B50;  
	case 75:  
		return B75;  
	case 110:  
		return B110;  
	case 134:  
		return B134;  
	case 150:  
		return B150;  
	case 200:  
		return B200;  
	case 300:  
		return B300;  
	case 600:  
		return B600;  
	case 1200:  
		return B1200;  
	case 1800:  
		return B1800;  
	case 2400:  
		return B2400;  
	case 4800:  
		return B4800;  
	case 9600:  
		return B9600;  
	case 19200:  
		return B19200;  
	case 38400:  
		return B38400;  
	case 57600:  
		return B57600;  
	case 115200:  
		return B115200;  
	case 230400:  
		return B230400;  
	case 460800:  
		return B460800;  
	case 500000:  
		return B500000;  
	case 576000:  
		return B576000;  
	case 921600:  
		return B921600;  
	case 1000000:  
		return B1000000;  
	case 1152000:  
		return B1152000;  
	case 1500000:  
		return B1500000;  
	case 2000000:  
		return B2000000;  
	case 2500000:  
		return B2500000;  
	case 3000000:  
		return B3000000;  
	case 3500000:  
		return B3500000;  
	case 4000000:  
		return B4000000;  
	default:  
		return -1;  
	}
}

/* 
 * Class:     cedric_serial_SerialPort 
 * Method:    open 
 * Signature: (Ljava/lang/String;)V 
 */
SDL_handle SDL_OpenSerialPort(const char* path, int baudrate)
{  
	int fd;  
	speed_t speed;

	// baudrate = 9600;
	SDL_Log("SDL_OpenSerialPort(%s, %i)---", path? path: "<nil>", baudrate);  
	// Check arguments
	{
		speed = getBaudrate(baudrate);  
		if (speed == -1) {  
			// TODO: throw an exception
			SDL_Log("Invalid baudrate");  
			return SDL_INVALID_HANDLE_VALUE;
		}  
	}  
  
	// Opening device
	{  
	//  fd = open(path, O_RDWR | O_DIRECT | O_SYNC);  
		fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK | O_NDELAY);  
		SDL_Log("open(%s) fd = %d", path, fd);  
		if (fd == -1) {  
			/* Throw an exception */  
			SDL_Log("Cannot open port %d",baudrate);  
			/* TODO: throw an exception */  
			return SDL_INVALID_HANDLE_VALUE;
		}  
	}  
  
	// Configure device
	{  
		struct termios oldcfg, newtio;
		if (tcgetattr(fd, &oldcfg)) {  
			SDL_Log("Configure device tcgetattr() failed 1");
			close(fd);  
			return SDL_INVALID_HANDLE_VALUE;
		}

		// don't use ICANON mode.
		// 8-N-1
		int nBits = 8;
		char nEvent = 'N';
		int nStop = 1;

		SDL_memset(&newtio, 0, sizeof(newtio));
		// 步骤一，设置字符大小
		newtio.c_cflag  |=  CLOCAL | CREAD;  
		newtio.c_cflag &= ~CSIZE;
		// 设置停止位
		switch( nBits ) 
		{ 
		case 7: 
			newtio.c_cflag |= CS7; 
			break; 
		case 8: 
			newtio.c_cflag |= CS8; 
			break; 
		}
		// 设置奇偶校验位
		switch( nEvent ) 
		{ 
		case 'o':
		case 'O': // 奇数 
			newtio.c_cflag |= PARENB; 
			newtio.c_cflag |= PARODD; 
			newtio.c_iflag |= (INPCK | ISTRIP); 
			break; 
		case 'e':
		case 'E': // 偶数 
			newtio.c_iflag |= (INPCK | ISTRIP); 
			newtio.c_cflag |= PARENB; 
			newtio.c_cflag &= ~PARODD; 
			break;
		case 'n':
		case 'N':  // 无奇偶校验位 
			newtio.c_cflag &= ~PARENB; 
			break;
		default:
			break;
		}

		// 设置波特率
		cfsetispeed(&newtio, speed);
		cfsetospeed(&newtio, speed);

		// 设置停止位
		if (nStop == 1) {
			newtio.c_cflag &=  ~CSTOPB; 
		} else if (nStop == 2) {
			newtio.c_cflag |=  CSTOPB; 
		}
		// 设置等待时间和最小接收字符, see https://blog.csdn.net/JAZZSOLDIER/article/details/55508227
		newtio.c_cc[VTIME]  = 0; 
		newtio.c_cc[VMIN] = 0;

		// 处理未接收字符
		tcflush(fd, TCIFLUSH);
		
		SDL_Log("newtio.c_iflag: 0x%x", newtio.c_iflag);
		SDL_Log("newtio.c_oflag: 0x%x", newtio.c_oflag);
		SDL_Log("newtio.c_cflag: 0x%x", newtio.c_cflag);
		SDL_Log("newtio.c_lflag: 0x%x", newtio.c_lflag);
		SDL_Log("newtio.c_line: %i", newtio.c_line);
		SDL_Log("newtio.c_cc[VTIME]: %i", newtio.c_cc[VTIME]);
		SDL_Log("newtio.c_cc[VMIN]: %i", newtio.c_cc[VMIN]);

		if (tcsetattr(fd,TCSANOW, &newtio) != 0) { 
			SDL_Log("Configure device tcsetattr() failed 2");  
			close(fd);  
			/* TODO: throw an exception */  
			return SDL_INVALID_HANDLE_VALUE;  
		}  
	}  

	SDL_Log("---SDL_OpenSerialPort(%s, %i), fd: %i", path? path: "<nil>", baudrate, fd);
	return fd;  
}
  
/* 
* Class:     cedric_serial_SerialPort 
* Method:    close 
* Signature: ()V 
*/  
void SDL_CloseSerialPort(SDL_handle fd)
{ 
	SDL_Log("SDL_CloseSerialPort(%d), E", fd);
	if (fd == SDL_INVALID_HANDLE_VALUE) {
		return;
	}
	close(fd);  
}
  

size_t SDL_ReadSerialPort(SDL_handle fd, void* ptr, size_t size)
{
	// SDL_Log("SDL_ReadSerialPort(%d, %p, %i), E", fd, ptr, size);
	if (fd == SDL_INVALID_HANDLE_VALUE) {
		SDL_Log("SDL_ReadSerialPort, X, fd is SDL_INVALID_HANDLE_VALUE");
		return 0;
	}
	ssize_t ret = read(fd, ptr, size);
	if (ret == -1) {
		// if no data, it will return EAGAIN.
		if (errno != EAGAIN) {
			SDL_Log("SDL_ReadSerialPort, X, errno: %i <==> EAGAIN(%i)", errno, EAGAIN);
		}
		return 0;
	}
	return ret;
}

size_t SDL_WriteSerialPort(SDL_handle fd, const void* ptr, size_t size)
{
	if (fd == SDL_INVALID_HANDLE_VALUE) {
		return 0;
	}
	ssize_t ret = write(fd, ptr, size);
	if (ret == -1) {
		// think error as 0.
		SDL_Log("SDL_WriteSerialPort, X, errno: %i", errno);
		return 0;
	}
	return ret;
}

// return: 0, no usb tty.
//   !=0, usb tty count. and pttyUSb is valid. sizeof(pttyUSB) is fix_count + usb_count.
static int fill_usb_tty(int fix_count, SDL_ttyUSB** ppttyUSB)
{
#define CMD_COUNT	2
	const char cmds[CMD_COUNT][128] = {
		// ROC-RK3588S-PC
		{"su -c \"cat /proc/tty/driver/usbserial\""},
		// lubancat4
		{"su 0 cat /proc/tty/driver/usbserial"},
	};

	int pos = 0;
	const int max_bytes = 4096;
	char* buf = (char*)SDL_malloc(max_bytes);
	SDL_memset(buf, 0, max_bytes);

	for (int n = 0; n < CMD_COUNT && pos == 0; n ++) {
		const char* cmd = cmds[n];
		FILE* fp = popen(cmd, "r");
		if (fp == NULL) {
			SDL_Log("{fill_usb_tty}[%i/%i]popen(%s, 'r') fail, errno: %i", n, CMD_COUNT, cmd, errno);
			continue;
		}

		pos = 0;
		int one_block = 10;
		int ret_bytes = 0;
		do {
			ret_bytes = (int)fread(buf + pos, 1, one_block, fp);
			pos += ret_bytes;
		} while (ret_bytes == one_block && pos < max_bytes);
		pclose(fp);

		if (pos == 0 || buf[pos - 1] != '\n') {
			SDL_Log("{fill_usb_tty}[%i/%i]cmd: %s, pos(%i) == 0 || buf[pos - 1] != (nl), set pos = 0", 
				n, CMD_COUNT, cmd, pos);
			pos = 0;
		}
	}

	if (pos == 0) {
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

//
// system control
//
SDL_bool SDL_SetTime(int64_t utctime)
{
	// I suspect that "date 020117301998.55" cannot be called directly in c/cpp, 
	// android may have a legality check, and directly killed the app that called "date".
/*
	SDL_Log("SDL_SetTime------utctime: %lld", utctime);
	// MMDDhhmm[[CC]YY][.ss]
	time_t ts = utctime + 3600;
	// const struct tm* timeptr = localtime(&ts);
	const struct tm* timeptr = gmtime(&ts);
	const char* cmd = "su -c \"date 020117301998.55\"";
	// char cmd[128];
	// SDL_snprintf(cmd, sizeof(cmd), "su -c \"date %02d%02d%02d%02d%04d.%02d\"", 
	//	timeptr->tm_mon + 1, timeptr->tm_mday, timeptr->tm_hour, timeptr->tm_min, 1900 + timeptr->tm_year, timeptr->tm_sec);

	FILE* fp = popen(cmd, "r");
	SDL_Log("SDL_SetTime------cmd: %s, fp: 0x%p", cmd, fp);
	if (fp != NULL) {
		const int max_bytes = 4096;
		char* buf = (char*)SDL_malloc(max_bytes);
		SDL_memset(buf, 0, max_bytes);

		int one_block = 10;
		int pos = 0;
		int ret_bytes = 0;
		do {
			ret_bytes = fread(buf + pos, 1, one_block, fp);
			pos += ret_bytes;
			SDL_Log("SDL_SetTime,[2] pos: %i", pos);
		} while (ret_bytes == one_block && pos < max_bytes);
		SDL_Log("SDL_SetTime, pos: %i", pos);
		SDL_free(buf);

		pclose(fp);
		fp = NULL;
	}
*/

/*
	time_t ts = utctime;
	// const struct tm* timeptr = localtime(&ts);
	const struct tm* timeptr = gmtime(&ts);
	return Android_JNI_SetTime(1900 + timeptr->tm_year, timeptr->tm_mon + 1, timeptr->tm_mday,
		timeptr->tm_hour, timeptr->tm_min, timeptr->tm_sec, utctime);
*/
	return Android_JNI_SetTime(utctime);
}

void SDL_UpdateApp(const char* package)
{
	Android_JNI_UpdateApp(package);
}

SDL_bool SDL_NotifyMediaFileAdded(const char* filename)
{
	Android_JNI_NotifyMediaFileAdded(filename);
	return SDL_TRUE;
}

void SDL_OpenUrl(const char* url)
{
	Android_JNI_OpenUrl(url);
}

int SDL_GetPublicDirectory(SDL_PublicDir type, char* name, int maxlen)
{
	return Android_GetPublicDirectory(type, name, maxlen);
}

void SDL_StartScreenRecording(const char* filename)
{
	Android_StartScreenRecording(filename);
}

SDL_bool SDL_StopScreenRecording(char* filename, int maxlen)
{
	Android_StopScreenRecording(filename, maxlen);
	return SDL_TRUE;
}

SDL_bool SDL_IsScreenRecording(void)
{
	return Android_IsScreenRecording();
}

void SDL_GetOsInfo(SDL_OsInfo* info)
{
	Android_JNI_GetOsInfo(info);
}

SDL_bool SDL_PrintText(const uint8_t* bytes, int size)
{
	return Android_JNI_PrintText(bytes, size);
}

void SDL_EnableRelay(SDL_bool enable)
{
	Android_EnableRelay(enable);
}

void SDL_SetFlashlight(SDL_LightColor color)
{
	Android_TurnOnFlashlight(color);
}

void SDL_SetBrightness(int brightness)
{
	Android_SetBrightness(brightness);
}

SDL_bool SDL_OrbbecCreateContext(void)
{
	return Android_JNI_OrbbecCreateContext();
}

void SDL_OrbbecDestroyContext(void)
{
	Android_JNI_OrbbecDestroyContext();
}

void SDL_WifiSetEnable(SDL_bool enable)
{
	Android_JNI_WifiSetEnable(enable);
}

uint32_t SDL_WifiGetFlags(void)
{
	return Android_JNI_WifiGetFlags();
}

int SDL_WifiGetScanResults(SDL_WifiScanResult** results)
{
	return Android_JNI_WifiGetScanResults(results);
}

SDL_bool SDL_WifiConnect(const char* ssid, const char* password)
{
	return Android_JNI_WifiConnect(ssid, password);
}

SDL_bool SDL_WifiRemove(const char* ssid)
{
	return Android_JNI_WifiRemove(ssid);
}

/* vi: set ts=4 sw=4 expandtab: */
