/*
* Author: Ray Hayes <ray.hayes@microsoft.com>
* ANSI TTY Reader - Maps Windows console input events to ANSI stream
*
* Author: Balu <bagajjal@microsoft.com>
* Misc fixes and code cleanup
*
* Copyright (c) 2017 Microsoft Corp.
* All rights reserved
*
* This file is responsible for console reading calls for building an emulator
* over Windows Console.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
*
* 1. Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
* IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
* IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
* NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
* THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <windows.h>
#include <wincon.h>
//#include "ansiprsr.h"
#include "tncon.h"
//#include "tnnet.h"

#define true TRUE
#define false FALSE
#define bool BOOL

#define ENUM_CRLF 0
#define ENUM_LF 1
#define ENUM_CR 2

bool gbVTAppMode;
BOOL isAnsiParsingRequired = false;
char *glob_out = NULL;
int glob_outlen = 0;
int glob_space = 0;
unsigned char  NAWSSTR[] = { "\xff\xfa\x1f\x00\x00\x00\x00\xff\xf0" };
extern int ScreenY;
extern int ScreenX;
extern int ScrollTop;
extern int ScrollBottom;
char tmp_buf[30];

typedef struct _TelParams
{
    int fLogging;
    FILE *fplogfile;

    char *pInputFile;

    char *szDebugInputFile;
    BOOL fDebugWait;

    int timeOut;
    int fLocalEcho;
    int fTreatLFasCRLF;
    int	fSendCROnly;
    int nReceiveCRLF;

    char sleepChar;
    char menuChar;

    SOCKET Socket;
    BOOL bVT100Mode;

    char *pAltKey;

} TelParams;

/* terminal global switches*/
TelParams Parameters = {
	0,		/* int fLogging */
	NULL,		/* FILE *fplogfile */
	NULL,		/* char *pInputFile */
	NULL,		/* char *szDebugInputFile */
	FALSE,		/* BOOL fDebugWait */
	0,		/* int timeOut */
	0,		/* int fLocalEcho */
	0,		/* int fTreatLFasCRLF */
	0,		/* int	fSendCROnly */
	ENUM_LF,	/* int nReceiveCRLF */
	'`',		/* char sleepChar */
	'\035',		/* char menuChar; // CTRL-]  */
	0,		/* SOCKET Socket */
	FALSE,		/* BOOL bVT100Mode */
	"\x01",		/* char *pAltKey */
};

TelParams* pParams = &Parameters;

//void queue_terminal_window_change_event();

/*
* For our case, in NetWriteString2(), we do not use socket, but write the out going data to
* a global buffer setup by ReadConsoleForTermEmul().
*/
int
NetWriteString2(SOCKET sock, char* source, size_t len, int options)
{
	while (len > 0) {
		if (glob_outlen >= glob_space)
			return glob_outlen;
		*glob_out++ = *source++;
		len--;
		glob_outlen++;
	}
	return glob_outlen;
}

BOOL
DataAvailable(HANDLE h)
{
	DWORD dwRet = WaitForSingleObject(h, INFINITE);
	if (dwRet == WAIT_OBJECT_0)
		return TRUE;
	if (dwRet == WAIT_FAILED)
		return FALSE;
	return FALSE;
}

int
GetModifierKey(DWORD dwControlKeyState)
{
	int modKey = 0;
	if ((dwControlKeyState & LEFT_ALT_PRESSED) || (dwControlKeyState & RIGHT_ALT_PRESSED))
		modKey += 2;

	if (dwControlKeyState & SHIFT_PRESSED)
		modKey += 1;

	if ((dwControlKeyState & LEFT_CTRL_PRESSED) || (dwControlKeyState & RIGHT_CTRL_PRESSED))
		modKey += 4;

	if (modKey){
		memset(tmp_buf, 0, sizeof(tmp_buf));
		modKey++;
	}		

	return modKey;
}

int
ReadConsoleForTermEmul(HANDLE hInput, char *destin, int destinlen, const std::function<void()> resizeHandler)
{
	HANDLE hHandle[] = { hInput, NULL };
	DWORD nHandle = 1;
	DWORD dwInput = 0;
	DWORD dwControlKeyState = 0;
	DWORD dwAltGrFlags = LEFT_CTRL_PRESSED | RIGHT_ALT_PRESSED;
	DWORD rc = 0;
	unsigned char octets[20];	
	char aChar = 0;
	INPUT_RECORD InputRecord;
	BOOL bCapsOn = FALSE;
	BOOL bShift = FALSE;
	int modKey = 0;
    const char* FN_KEY = NULL;
    const char *SHIFT_FN_KEY = NULL;
    const char *ALT_FN_KEY = NULL;
    const char *CTRL_FN_KEY = NULL;
    const char *SHIFT_ALT_FN_KEY = NULL;
    const char *SHIFT_CTRL_FN_KEY = NULL;
    const char *ALT_CTRL_FN_KEY = NULL;
    const char *SHIFT_ALT_CTRL_FN_KEY = NULL;

	glob_out = destin;
	glob_space = destinlen;
	glob_outlen = 0;
	while (DataAvailable(hInput)) {
		if (glob_outlen >= destinlen)
			return glob_outlen;
		ReadConsoleInputW(hInput, &InputRecord, 1, &dwInput);
		switch (InputRecord.EventType) {
		case WINDOW_BUFFER_SIZE_EVENT:
		    resizeHandler();
			break;

		case FOCUS_EVENT:
			/* FALLTHROUGH */
		case MENU_EVENT:
			break;

		case KEY_EVENT:
			bCapsOn = (InputRecord.Event.KeyEvent.dwControlKeyState & CAPSLOCK_ON);
			bShift = (InputRecord.Event.KeyEvent.dwControlKeyState & SHIFT_PRESSED);
			dwControlKeyState = InputRecord.Event.KeyEvent.dwControlKeyState &
				~(CAPSLOCK_ON | ENHANCED_KEY | NUMLOCK_ON | SCROLLLOCK_ON);

			/* ignore the AltGr flags*/
			if ((dwControlKeyState & dwAltGrFlags) == dwAltGrFlags)
				dwControlKeyState = dwControlKeyState & ~dwAltGrFlags;

			modKey = GetModifierKey(dwControlKeyState);
			if (InputRecord.Event.KeyEvent.bKeyDown) {
			    int n;

			    if( (dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0 && InputRecord.Event.KeyEvent.wVirtualKeyCode == 0x36 ) {
                                // handle Ctrl+^
                                *octets = '\x1E';
                                n = 1;
                            } else {
                                n = WideCharToMultiByte(
                                        CP_UTF8,
                                        0,
                                        &(InputRecord.Event.KeyEvent.uChar.UnicodeChar),
                                        1,
                                        (LPSTR) octets,
                                        20,
                                        NULL,
                                        NULL);
                            }
                //if (pParams->fLocalEcho)
                //	ConWriteString((char *)octets, n);

				switch (InputRecord.Event.KeyEvent.uChar.UnicodeChar) {
				case 0xd:
					if (pParams->nReceiveCRLF == ENUM_LF)
						NetWriteString2(pParams->Socket, "\r", 1, 0);
					else
						NetWriteString2(pParams->Socket, "\r\n", 2, 0);
					break;

				case VK_ESCAPE:
					NetWriteString2(pParams->Socket, (char *)ESCAPE_KEY, 1, 0);
					break;

				default:
					switch (InputRecord.Event.KeyEvent.wVirtualKeyCode) {
					case VK_UP:
						if(!modKey)
							NetWriteString2(pParams->Socket, (char *)(gbVTAppMode ? APP_UP_ARROW : UP_ARROW), 3, 0);
						else {
							/* ^[[1;mA */
                            const char p[] = "\033[1;";
							strcpy_s(tmp_buf, sizeof(tmp_buf), p);
							size_t index = strlen(p);
							tmp_buf[index++] = modKey + '0';
							tmp_buf[index] = 'A';
							
							NetWriteString2(pParams->Socket, tmp_buf, index+1, 0);
						}
						break;
					case VK_DOWN:
						if(!modKey)
							NetWriteString2(pParams->Socket, (char *)(gbVTAppMode ? APP_DOWN_ARROW : DOWN_ARROW), 3, 0);
						else {
							/* ^[[1;mB */
							char *p = "\033[1;";
							strcpy_s(tmp_buf, sizeof(tmp_buf), p);
							size_t index = strlen(p);
							tmp_buf[index++] = modKey + '0';
							tmp_buf[index] = 'B';

							NetWriteString2(pParams->Socket, tmp_buf, index+1, 0);
						}
						break;
					case VK_RIGHT:
						if(!modKey)
							NetWriteString2(pParams->Socket, (char *)(gbVTAppMode ? APP_RIGHT_ARROW : RIGHT_ARROW), 3, 0);
						else {
							/* ^[[1;mC */
							char *p = "\033[1;";			
							strcpy_s(tmp_buf, sizeof(tmp_buf), p);
							size_t index = strlen(p);
							tmp_buf[index++] = modKey + '0';
							tmp_buf[index] = 'C';
							
							NetWriteString2(pParams->Socket, tmp_buf, index+1, 0);
						}
						break;
					case VK_LEFT:
						if(!modKey)
							NetWriteString2(pParams->Socket, (char *)(gbVTAppMode ? APP_LEFT_ARROW : LEFT_ARROW), 3, 0);
						else {
							/* ^[[1;mD */
							char *p = "\033[1;";
							strcpy_s(tmp_buf, sizeof(tmp_buf), p);
							size_t index = strlen(p);
							tmp_buf[index++] = modKey + '0';
							tmp_buf[index] = 'D';

							NetWriteString2(pParams->Socket, tmp_buf, index+1, 0);
						}
						break;
					case VK_END:
						if(!modKey)
							NetWriteString2(pParams->Socket, (char *)SELECT_KEY, 4, 0);
						else {
							/* ^[[1;mF */
							char *p = "\033[1;";
							strcpy_s(tmp_buf, sizeof(tmp_buf), p);
							size_t index = strlen(p);
							tmp_buf[index++] = modKey + '0';
							tmp_buf[index] = 'F';

							NetWriteString2(pParams->Socket, tmp_buf, index+1, 0);
						}
						break;
					case VK_HOME:
						if(!modKey)
							NetWriteString2(pParams->Socket, (char *)FIND_KEY, 4, 0);
						else {
							/* ^[[1;mH */
							char *p = "\033[1;";
							strcpy_s(tmp_buf, sizeof(tmp_buf), p);
							size_t index = strlen(p);
							tmp_buf[index++] = modKey + '0';
							tmp_buf[index] = 'H';

							NetWriteString2(pParams->Socket, tmp_buf, index+1, 0);
						}
						break;
					case VK_INSERT:
						if(!modKey)
							NetWriteString2(pParams->Socket, (char *)INSERT_KEY, 4, 0);
						else {
							/* ^[[2;m~ */
							char *p = "\033[2;";
							strcpy_s(tmp_buf, sizeof(tmp_buf), p);
							size_t index = strlen(p);
							tmp_buf[index++] = modKey + '0';
							tmp_buf[index] = '~';

							NetWriteString2(pParams->Socket, tmp_buf, index+1, 0);
						}
						break;
					case VK_DELETE:
						if(!modKey)
							NetWriteString2(pParams->Socket, (char *)REMOVE_KEY, 4, 0);
						else {
							/* ^[[3;m~ */
							char *p = "\033[3;";
							strcpy_s(tmp_buf, sizeof(tmp_buf), p);
							size_t index = strlen(p);
							tmp_buf[index++] = modKey + '0';
							tmp_buf[index] = '~';

							NetWriteString2(pParams->Socket, tmp_buf, index+1, 0);
						}
						break;
					case VK_PRIOR: /* page up */
						if (!modKey)
							NetWriteString2(pParams->Socket, (char *)PREV_KEY, 4, 0);
						else {
							/* ^[[5;m~ */
							char *p = "\033[5;";
							strcpy_s(tmp_buf, sizeof(tmp_buf), p);
							size_t index = strlen(p);
							tmp_buf[index++] = modKey + '0';
							tmp_buf[index] = '~';

							NetWriteString2(pParams->Socket, tmp_buf, index+1, 0);
						}
						break;
					case VK_NEXT: /* page down */
						if(!modKey)
							NetWriteString2(pParams->Socket, (char *)NEXT_KEY, 4, 0);
						else {
							/* ^[[6;m~  */
							char *p = "\033[6;";
							strcpy_s(tmp_buf, sizeof(tmp_buf), p);
							size_t index = strlen(p);
							tmp_buf[index++] = modKey + '0';
							tmp_buf[index] = '~';

							NetWriteString2(pParams->Socket, tmp_buf, index+1, 0);
						}
						break;
					case VK_BACK:
						NetWriteString2(pParams->Socket, (char *)BACKSPACE_KEY, 1, 0);
						break;
					case VK_TAB:
						if (dwControlKeyState == SHIFT_PRESSED)
							NetWriteString2(pParams->Socket, (char *)SHIFT_TAB_KEY, 3, 0);
						else
							NetWriteString2(pParams->Socket, (char *)octets, n, 0);
						break;
					case VK_ESCAPE:
						NetWriteString2(pParams->Socket, (char *)ESCAPE_KEY, 1, 0);
						break;
					case VK_SHIFT:
					case VK_CONTROL:
					case VK_CAPITAL:
						break; /* NOP on these */
					case VK_F1:
						/* If isAnsiParsingRequired is false then we use XTERM VT sequence */
						FN_KEY = isAnsiParsingRequired ? PF1_KEY : XTERM_PF1_KEY;
						SHIFT_FN_KEY = isAnsiParsingRequired ? SHIFT_PF1_KEY : XTERM_SHIFT_PF1_KEY;
						ALT_FN_KEY = isAnsiParsingRequired ? ALT_PF1_KEY : XTERM_ALT_PF1_KEY;
						CTRL_FN_KEY = isAnsiParsingRequired ? CTRL_PF1_KEY : XTERM_CTRL_PF1_KEY;
						SHIFT_ALT_FN_KEY = isAnsiParsingRequired ? SHIFT_ALT_PF1_KEY : XTERM_SHIFT_ALT_PF1_KEY;
						SHIFT_CTRL_FN_KEY = isAnsiParsingRequired ? SHIFT_CTRL_PF1_KEY : XTERM_SHIFT_CTRL_PF1_KEY;
						ALT_CTRL_FN_KEY = isAnsiParsingRequired ? ALT_CTRL_PF1_KEY : XTERM_ALT_CTRL_PF1_KEY;
						SHIFT_ALT_CTRL_FN_KEY = isAnsiParsingRequired ? SHIFT_ALT_CTRL_PF1_KEY : XTERM_SHIFT_ALT_CTRL_PF1_KEY;

						if (dwControlKeyState == 0)
							NetWriteString2(pParams->Socket, (char *)FN_KEY, strlen(FN_KEY), 0);

						else if (dwControlKeyState == SHIFT_PRESSED)
							NetWriteString2(pParams->Socket, (char *)SHIFT_FN_KEY, strlen(SHIFT_FN_KEY), 0);

						else if (dwControlKeyState == LEFT_CTRL_PRESSED || dwControlKeyState == RIGHT_CTRL_PRESSED)
							NetWriteString2(pParams->Socket, (char *)CTRL_FN_KEY, strlen(CTRL_FN_KEY), 0);

						else if (dwControlKeyState == LEFT_ALT_PRESSED || dwControlKeyState == RIGHT_ALT_PRESSED)
							NetWriteString2(pParams->Socket, (char *)ALT_FN_KEY, strlen(ALT_FN_KEY), 0);

						else if ((dwControlKeyState & SHIFT_PRESSED) && ((dwControlKeyState & RIGHT_ALT_PRESSED) ||
							(dwControlKeyState & LEFT_ALT_PRESSED)) && ((dwControlKeyState & LEFT_CTRL_PRESSED) ||
							(dwControlKeyState & RIGHT_CTRL_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)SHIFT_ALT_CTRL_FN_KEY, strlen(SHIFT_ALT_CTRL_FN_KEY), 0);

						else if ((dwControlKeyState & RIGHT_ALT_PRESSED) || (dwControlKeyState & LEFT_ALT_PRESSED) &&
							((dwControlKeyState & LEFT_CTRL_PRESSED) || (dwControlKeyState & RIGHT_CTRL_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)ALT_CTRL_FN_KEY, strlen(ALT_CTRL_FN_KEY), 0);

						else if ((dwControlKeyState & SHIFT_PRESSED) && ((dwControlKeyState & RIGHT_ALT_PRESSED) ||
							(dwControlKeyState & LEFT_ALT_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)SHIFT_ALT_FN_KEY, strlen(SHIFT_ALT_FN_KEY), 0);

						else if ((dwControlKeyState & SHIFT_PRESSED) && ((dwControlKeyState & LEFT_CTRL_PRESSED) ||
							(dwControlKeyState & RIGHT_CTRL_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)SHIFT_CTRL_FN_KEY, strlen(SHIFT_CTRL_FN_KEY), 0);

						break;
					case VK_F2:
						/* If isAnsiParsingRequired is false then we use XTERM VT sequence */
						FN_KEY = isAnsiParsingRequired ? PF2_KEY : XTERM_PF2_KEY;
						SHIFT_FN_KEY = isAnsiParsingRequired ? SHIFT_PF2_KEY : XTERM_SHIFT_PF2_KEY;
						ALT_FN_KEY = isAnsiParsingRequired ? ALT_PF2_KEY : XTERM_ALT_PF2_KEY;
						CTRL_FN_KEY = isAnsiParsingRequired ? CTRL_PF2_KEY : XTERM_CTRL_PF2_KEY;
						SHIFT_ALT_FN_KEY = isAnsiParsingRequired ? SHIFT_ALT_PF2_KEY : XTERM_SHIFT_ALT_PF2_KEY;
						SHIFT_CTRL_FN_KEY = isAnsiParsingRequired ? SHIFT_CTRL_PF2_KEY : XTERM_SHIFT_CTRL_PF2_KEY;
						ALT_CTRL_FN_KEY = isAnsiParsingRequired ? ALT_CTRL_PF2_KEY : XTERM_ALT_CTRL_PF2_KEY;
						SHIFT_ALT_CTRL_FN_KEY = isAnsiParsingRequired ? SHIFT_ALT_CTRL_PF2_KEY : XTERM_SHIFT_ALT_CTRL_PF2_KEY;

						if (dwControlKeyState == 0)
							NetWriteString2(pParams->Socket, (char *)FN_KEY, strlen(FN_KEY), 0);

						else if (dwControlKeyState == SHIFT_PRESSED)
							NetWriteString2(pParams->Socket, (char *)SHIFT_FN_KEY, strlen(SHIFT_FN_KEY), 0);

						else if (dwControlKeyState == LEFT_CTRL_PRESSED || dwControlKeyState == RIGHT_CTRL_PRESSED)
							NetWriteString2(pParams->Socket, (char *)CTRL_FN_KEY, strlen(CTRL_FN_KEY), 0);

						else if (dwControlKeyState == LEFT_ALT_PRESSED || dwControlKeyState == RIGHT_ALT_PRESSED)
							NetWriteString2(pParams->Socket, (char *)ALT_FN_KEY, strlen(ALT_FN_KEY), 0);

						else if ((dwControlKeyState & SHIFT_PRESSED) && ((dwControlKeyState & RIGHT_ALT_PRESSED) ||
							(dwControlKeyState & LEFT_ALT_PRESSED)) && ((dwControlKeyState & LEFT_CTRL_PRESSED) ||
							(dwControlKeyState & RIGHT_CTRL_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)SHIFT_ALT_CTRL_FN_KEY, strlen(SHIFT_ALT_CTRL_FN_KEY), 0);

						else if ((dwControlKeyState & RIGHT_ALT_PRESSED) || (dwControlKeyState & LEFT_ALT_PRESSED) &&
							((dwControlKeyState & LEFT_CTRL_PRESSED) || (dwControlKeyState & RIGHT_CTRL_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)ALT_CTRL_FN_KEY, strlen(ALT_CTRL_FN_KEY), 0);

						else if ((dwControlKeyState & SHIFT_PRESSED) && ((dwControlKeyState & RIGHT_ALT_PRESSED) ||
							(dwControlKeyState & LEFT_ALT_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)SHIFT_ALT_FN_KEY, strlen(SHIFT_ALT_FN_KEY), 0);

						else if ((dwControlKeyState & SHIFT_PRESSED) && ((dwControlKeyState & LEFT_CTRL_PRESSED) ||
							(dwControlKeyState & RIGHT_CTRL_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)SHIFT_CTRL_FN_KEY, strlen(SHIFT_CTRL_FN_KEY), 0);

						break;
					case VK_F3:
						/* If isAnsiParsingRequired is false then we use XTERM VT sequence */
						FN_KEY = isAnsiParsingRequired ? PF3_KEY : XTERM_PF3_KEY;
						SHIFT_FN_KEY = isAnsiParsingRequired ? SHIFT_PF3_KEY : XTERM_SHIFT_PF3_KEY;
						ALT_FN_KEY = isAnsiParsingRequired ? ALT_PF3_KEY : XTERM_ALT_PF3_KEY;
						CTRL_FN_KEY = isAnsiParsingRequired ? CTRL_PF3_KEY : XTERM_CTRL_PF3_KEY;
						SHIFT_ALT_FN_KEY = isAnsiParsingRequired ? SHIFT_ALT_PF3_KEY : XTERM_SHIFT_ALT_PF3_KEY;
						SHIFT_CTRL_FN_KEY = isAnsiParsingRequired ? SHIFT_CTRL_PF3_KEY : XTERM_SHIFT_CTRL_PF3_KEY;
						ALT_CTRL_FN_KEY = isAnsiParsingRequired ? ALT_CTRL_PF3_KEY : XTERM_ALT_CTRL_PF3_KEY;
						SHIFT_ALT_CTRL_FN_KEY = isAnsiParsingRequired ? SHIFT_ALT_CTRL_PF3_KEY : XTERM_SHIFT_ALT_CTRL_PF3_KEY;

						if (dwControlKeyState == 0)
							NetWriteString2(pParams->Socket, (char *)FN_KEY, strlen(FN_KEY), 0);

						else if (dwControlKeyState == SHIFT_PRESSED)
							NetWriteString2(pParams->Socket, (char *)SHIFT_FN_KEY, strlen(SHIFT_FN_KEY), 0);

						else if (dwControlKeyState == LEFT_CTRL_PRESSED || dwControlKeyState == RIGHT_CTRL_PRESSED)
							NetWriteString2(pParams->Socket, (char *)CTRL_FN_KEY, strlen(CTRL_FN_KEY), 0);

						else if (dwControlKeyState == LEFT_ALT_PRESSED || dwControlKeyState == RIGHT_ALT_PRESSED)
							NetWriteString2(pParams->Socket, (char *)ALT_FN_KEY, strlen(ALT_FN_KEY), 0);

						else if ((dwControlKeyState & SHIFT_PRESSED) && ((dwControlKeyState & RIGHT_ALT_PRESSED) ||
							(dwControlKeyState & LEFT_ALT_PRESSED)) && ((dwControlKeyState & LEFT_CTRL_PRESSED) ||
							(dwControlKeyState & RIGHT_CTRL_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)SHIFT_ALT_CTRL_FN_KEY, strlen(SHIFT_ALT_CTRL_FN_KEY), 0);

						else if ((dwControlKeyState & RIGHT_ALT_PRESSED) || (dwControlKeyState & LEFT_ALT_PRESSED) &&
							((dwControlKeyState & LEFT_CTRL_PRESSED) || (dwControlKeyState & RIGHT_CTRL_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)ALT_CTRL_FN_KEY, strlen(ALT_CTRL_FN_KEY), 0);

						else if ((dwControlKeyState & SHIFT_PRESSED) && ((dwControlKeyState & RIGHT_ALT_PRESSED) ||
							(dwControlKeyState & LEFT_ALT_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)SHIFT_ALT_FN_KEY, strlen(SHIFT_ALT_FN_KEY), 0);

						else if ((dwControlKeyState & SHIFT_PRESSED) && ((dwControlKeyState & LEFT_CTRL_PRESSED) ||
							(dwControlKeyState & RIGHT_CTRL_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)SHIFT_CTRL_FN_KEY, strlen(SHIFT_CTRL_FN_KEY), 0);

						break;
					case VK_F4:
						/* If isAnsiParsingRequired is false then we use XTERM VT sequence */
						FN_KEY = isAnsiParsingRequired ? PF4_KEY : XTERM_PF4_KEY;
						SHIFT_FN_KEY = isAnsiParsingRequired ? SHIFT_PF4_KEY : XTERM_SHIFT_PF4_KEY;
						ALT_FN_KEY = isAnsiParsingRequired ? ALT_PF4_KEY : XTERM_ALT_PF4_KEY;
						CTRL_FN_KEY = isAnsiParsingRequired ? CTRL_PF4_KEY : XTERM_CTRL_PF4_KEY;
						SHIFT_ALT_FN_KEY = isAnsiParsingRequired ? SHIFT_ALT_PF4_KEY : XTERM_SHIFT_ALT_PF4_KEY;
						SHIFT_CTRL_FN_KEY = isAnsiParsingRequired ? SHIFT_CTRL_PF4_KEY : XTERM_SHIFT_CTRL_PF4_KEY;
						ALT_CTRL_FN_KEY = isAnsiParsingRequired ? ALT_CTRL_PF4_KEY : XTERM_ALT_CTRL_PF4_KEY;
						SHIFT_ALT_CTRL_FN_KEY = isAnsiParsingRequired ? SHIFT_ALT_CTRL_PF4_KEY : XTERM_SHIFT_ALT_CTRL_PF4_KEY;

						if (dwControlKeyState == 0)
							NetWriteString2(pParams->Socket, (char *)FN_KEY, strlen(FN_KEY), 0);

						else if (dwControlKeyState == SHIFT_PRESSED)
							NetWriteString2(pParams->Socket, (char *)SHIFT_FN_KEY, strlen(SHIFT_FN_KEY), 0);

						else if (dwControlKeyState == LEFT_CTRL_PRESSED || dwControlKeyState == RIGHT_CTRL_PRESSED)
							NetWriteString2(pParams->Socket, (char *)CTRL_FN_KEY, strlen(CTRL_FN_KEY), 0);

						else if (dwControlKeyState == LEFT_ALT_PRESSED || dwControlKeyState == RIGHT_ALT_PRESSED)
							NetWriteString2(pParams->Socket, (char *)ALT_FN_KEY, strlen(ALT_FN_KEY), 0);

						else if ((dwControlKeyState & SHIFT_PRESSED) && ((dwControlKeyState & RIGHT_ALT_PRESSED) ||
							(dwControlKeyState & LEFT_ALT_PRESSED)) && ((dwControlKeyState & LEFT_CTRL_PRESSED) ||
							(dwControlKeyState & RIGHT_CTRL_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)SHIFT_ALT_CTRL_FN_KEY, strlen(SHIFT_ALT_CTRL_FN_KEY), 0);

						else if ((dwControlKeyState & RIGHT_ALT_PRESSED) || (dwControlKeyState & LEFT_ALT_PRESSED) &&
							((dwControlKeyState & LEFT_CTRL_PRESSED) || (dwControlKeyState & RIGHT_CTRL_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)ALT_CTRL_FN_KEY, strlen(ALT_CTRL_FN_KEY), 0);

						else if ((dwControlKeyState & SHIFT_PRESSED) && ((dwControlKeyState & RIGHT_ALT_PRESSED) ||
							(dwControlKeyState & LEFT_ALT_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)SHIFT_ALT_FN_KEY, strlen(SHIFT_ALT_FN_KEY), 0);

						else if ((dwControlKeyState & SHIFT_PRESSED) && ((dwControlKeyState & LEFT_CTRL_PRESSED) ||
							(dwControlKeyState & RIGHT_CTRL_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)SHIFT_CTRL_FN_KEY, strlen(SHIFT_CTRL_FN_KEY), 0);

						break;
					case VK_F5:
						if (dwControlKeyState == 0)
							NetWriteString2(pParams->Socket, (char *)PF5_KEY, strlen(PF5_KEY), 0);

						else if (dwControlKeyState == SHIFT_PRESSED)
							NetWriteString2(pParams->Socket, (char *)SHIFT_PF5_KEY, strlen(SHIFT_PF5_KEY), 0);

						else if (dwControlKeyState == LEFT_CTRL_PRESSED || dwControlKeyState == RIGHT_CTRL_PRESSED)
							NetWriteString2(pParams->Socket, (char *)CTRL_PF5_KEY, strlen(CTRL_PF5_KEY), 0);

						else if (dwControlKeyState == LEFT_ALT_PRESSED || dwControlKeyState == RIGHT_ALT_PRESSED)
							NetWriteString2(pParams->Socket, (char *)ALT_PF5_KEY, strlen(ALT_PF5_KEY), 0);

						else if ((dwControlKeyState & SHIFT_PRESSED) && ((dwControlKeyState & RIGHT_ALT_PRESSED) ||
							(dwControlKeyState & LEFT_ALT_PRESSED)) && ((dwControlKeyState & LEFT_CTRL_PRESSED) ||
							(dwControlKeyState & RIGHT_CTRL_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)SHIFT_ALT_CTRL_PF5_KEY, strlen(SHIFT_ALT_CTRL_PF5_KEY), 0);

						else if ((dwControlKeyState & RIGHT_ALT_PRESSED) || (dwControlKeyState & LEFT_ALT_PRESSED) &&
							((dwControlKeyState & LEFT_CTRL_PRESSED) || (dwControlKeyState & RIGHT_CTRL_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)ALT_CTRL_PF5_KEY, strlen(ALT_CTRL_PF5_KEY), 0);

						else if ((dwControlKeyState & SHIFT_PRESSED) && ((dwControlKeyState & RIGHT_ALT_PRESSED) ||
							(dwControlKeyState & LEFT_ALT_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)SHIFT_ALT_PF5_KEY, strlen(SHIFT_ALT_PF5_KEY), 0);

						else if ((dwControlKeyState & SHIFT_PRESSED) && ((dwControlKeyState & LEFT_CTRL_PRESSED) ||
							(dwControlKeyState & RIGHT_CTRL_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)SHIFT_CTRL_PF5_KEY, strlen(SHIFT_CTRL_PF5_KEY), 0);
						break;
					case VK_F6:
						if (dwControlKeyState == 0)
							NetWriteString2(pParams->Socket, (char *)PF6_KEY, strlen(PF6_KEY), 0);

						else if (dwControlKeyState == SHIFT_PRESSED)
							NetWriteString2(pParams->Socket, (char *)SHIFT_PF6_KEY, strlen(SHIFT_PF6_KEY), 0);

						else if (dwControlKeyState == LEFT_CTRL_PRESSED || dwControlKeyState == RIGHT_CTRL_PRESSED)
							NetWriteString2(pParams->Socket, (char *)CTRL_PF6_KEY, strlen(CTRL_PF6_KEY), 0);

						else if (dwControlKeyState == LEFT_ALT_PRESSED || dwControlKeyState == RIGHT_ALT_PRESSED)
							NetWriteString2(pParams->Socket, (char *)ALT_PF6_KEY, strlen(ALT_PF6_KEY), 0);

						else if ((dwControlKeyState & SHIFT_PRESSED) && ((dwControlKeyState & RIGHT_ALT_PRESSED) ||
							(dwControlKeyState & LEFT_ALT_PRESSED)) && ((dwControlKeyState & LEFT_CTRL_PRESSED) ||
							(dwControlKeyState & RIGHT_CTRL_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)SHIFT_ALT_CTRL_PF6_KEY, strlen(SHIFT_ALT_CTRL_PF6_KEY), 0);

						else if ((dwControlKeyState & RIGHT_ALT_PRESSED) || (dwControlKeyState & LEFT_ALT_PRESSED) &&
							((dwControlKeyState & LEFT_CTRL_PRESSED) || (dwControlKeyState & RIGHT_CTRL_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)ALT_CTRL_PF6_KEY, strlen(ALT_CTRL_PF6_KEY), 0);

						else if ((dwControlKeyState & SHIFT_PRESSED) && ((dwControlKeyState & RIGHT_ALT_PRESSED) ||
							(dwControlKeyState & LEFT_ALT_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)SHIFT_ALT_PF6_KEY, strlen(SHIFT_ALT_PF6_KEY), 0);

						else if ((dwControlKeyState & SHIFT_PRESSED) && ((dwControlKeyState & LEFT_CTRL_PRESSED) ||
							(dwControlKeyState & RIGHT_CTRL_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)SHIFT_CTRL_PF6_KEY, strlen(SHIFT_CTRL_PF6_KEY), 0);
						break;
					case VK_F7:
						if (dwControlKeyState == 0)
							NetWriteString2(pParams->Socket, (char *)PF7_KEY, strlen(PF7_KEY), 0);

						else if (dwControlKeyState == SHIFT_PRESSED)
							NetWriteString2(pParams->Socket, (char *)SHIFT_PF7_KEY, strlen(SHIFT_PF7_KEY), 0);

						else if (dwControlKeyState == LEFT_CTRL_PRESSED || dwControlKeyState == RIGHT_CTRL_PRESSED)
							NetWriteString2(pParams->Socket, (char *)CTRL_PF7_KEY, strlen(CTRL_PF7_KEY), 0);

						else if (dwControlKeyState == LEFT_ALT_PRESSED || dwControlKeyState == RIGHT_ALT_PRESSED)
							NetWriteString2(pParams->Socket, (char *)ALT_PF7_KEY, strlen(ALT_PF7_KEY), 0);

						else if ((dwControlKeyState & SHIFT_PRESSED) && ((dwControlKeyState & RIGHT_ALT_PRESSED) ||
							(dwControlKeyState & LEFT_ALT_PRESSED)) && ((dwControlKeyState & LEFT_CTRL_PRESSED) ||
							(dwControlKeyState & RIGHT_CTRL_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)SHIFT_ALT_CTRL_PF7_KEY, strlen(SHIFT_ALT_CTRL_PF7_KEY), 0);

						else if ((dwControlKeyState & RIGHT_ALT_PRESSED) || (dwControlKeyState & LEFT_ALT_PRESSED) &&
							((dwControlKeyState & LEFT_CTRL_PRESSED) || (dwControlKeyState & RIGHT_CTRL_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)ALT_CTRL_PF7_KEY, strlen(ALT_CTRL_PF7_KEY), 0);

						else if ((dwControlKeyState & SHIFT_PRESSED) && ((dwControlKeyState & RIGHT_ALT_PRESSED) ||
							(dwControlKeyState & LEFT_ALT_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)SHIFT_ALT_PF7_KEY, strlen(SHIFT_ALT_PF7_KEY), 0);

						else if ((dwControlKeyState & SHIFT_PRESSED) && ((dwControlKeyState & LEFT_CTRL_PRESSED) ||
							(dwControlKeyState & RIGHT_CTRL_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)SHIFT_CTRL_PF7_KEY, strlen(SHIFT_CTRL_PF7_KEY), 0);
						break;
					case VK_F8:
						if (dwControlKeyState == 0)
							NetWriteString2(pParams->Socket, (char *)PF8_KEY, strlen(PF8_KEY), 0);

						else if (dwControlKeyState == SHIFT_PRESSED)
							NetWriteString2(pParams->Socket, (char *)SHIFT_PF8_KEY, strlen(SHIFT_PF8_KEY), 0);

						else if (dwControlKeyState == LEFT_CTRL_PRESSED || dwControlKeyState == RIGHT_CTRL_PRESSED)
							NetWriteString2(pParams->Socket, (char *)CTRL_PF8_KEY, strlen(CTRL_PF8_KEY), 0);

						else if (dwControlKeyState == LEFT_ALT_PRESSED || dwControlKeyState == RIGHT_ALT_PRESSED)
							NetWriteString2(pParams->Socket, (char *)ALT_PF8_KEY, strlen(ALT_PF8_KEY), 0);

						else if ((dwControlKeyState & SHIFT_PRESSED) && ((dwControlKeyState & RIGHT_ALT_PRESSED) ||
							(dwControlKeyState & LEFT_ALT_PRESSED)) && ((dwControlKeyState & LEFT_CTRL_PRESSED) ||
							(dwControlKeyState & RIGHT_CTRL_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)SHIFT_ALT_CTRL_PF8_KEY, strlen(SHIFT_ALT_CTRL_PF8_KEY), 0);

						else if ((dwControlKeyState & RIGHT_ALT_PRESSED) || (dwControlKeyState & LEFT_ALT_PRESSED) &&
							((dwControlKeyState & LEFT_CTRL_PRESSED) || (dwControlKeyState & RIGHT_CTRL_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)ALT_CTRL_PF8_KEY, strlen(ALT_CTRL_PF8_KEY), 0);

						else if ((dwControlKeyState & SHIFT_PRESSED) && ((dwControlKeyState & RIGHT_ALT_PRESSED) ||
							(dwControlKeyState & LEFT_ALT_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)SHIFT_ALT_PF8_KEY, strlen(SHIFT_ALT_PF8_KEY), 0);

						else if ((dwControlKeyState & SHIFT_PRESSED) && ((dwControlKeyState & LEFT_CTRL_PRESSED) ||
							(dwControlKeyState & RIGHT_CTRL_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)SHIFT_CTRL_PF8_KEY, strlen(SHIFT_CTRL_PF8_KEY), 0);
						break;
					case VK_F9:
						if (dwControlKeyState == 0)
							NetWriteString2(pParams->Socket, (char *)PF9_KEY, strlen(PF9_KEY), 0);

						else if (dwControlKeyState == SHIFT_PRESSED)
							NetWriteString2(pParams->Socket, (char *)SHIFT_PF9_KEY, strlen(SHIFT_PF9_KEY), 0);

						else if (dwControlKeyState == LEFT_CTRL_PRESSED || dwControlKeyState == RIGHT_CTRL_PRESSED)
							NetWriteString2(pParams->Socket, (char *)CTRL_PF9_KEY, strlen(CTRL_PF9_KEY), 0);

						else if (dwControlKeyState == LEFT_ALT_PRESSED || dwControlKeyState == RIGHT_ALT_PRESSED)
							NetWriteString2(pParams->Socket, (char *)ALT_PF9_KEY, strlen(ALT_PF9_KEY), 0);

						else if ((dwControlKeyState & SHIFT_PRESSED) && ((dwControlKeyState & RIGHT_ALT_PRESSED) ||
							(dwControlKeyState & LEFT_ALT_PRESSED)) && ((dwControlKeyState & LEFT_CTRL_PRESSED) ||
							(dwControlKeyState & RIGHT_CTRL_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)SHIFT_ALT_CTRL_PF9_KEY, strlen(SHIFT_ALT_CTRL_PF9_KEY), 0);

						else if ((dwControlKeyState & RIGHT_ALT_PRESSED) || (dwControlKeyState & LEFT_ALT_PRESSED) &&
							((dwControlKeyState & LEFT_CTRL_PRESSED) || (dwControlKeyState & RIGHT_CTRL_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)ALT_CTRL_PF9_KEY, strlen(ALT_CTRL_PF9_KEY), 0);

						else if ((dwControlKeyState & SHIFT_PRESSED) && ((dwControlKeyState & RIGHT_ALT_PRESSED) ||
							(dwControlKeyState & LEFT_ALT_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)SHIFT_ALT_PF9_KEY, strlen(SHIFT_ALT_PF9_KEY), 0);

						else if ((dwControlKeyState & SHIFT_PRESSED) && ((dwControlKeyState & LEFT_CTRL_PRESSED) ||
							(dwControlKeyState & RIGHT_CTRL_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)SHIFT_CTRL_PF9_KEY, strlen(SHIFT_CTRL_PF9_KEY), 0);
						break;
					case VK_F10:
						if (dwControlKeyState == 0)
							NetWriteString2(pParams->Socket, (char *)PF10_KEY, strlen(PF10_KEY), 0);

						else if (dwControlKeyState == SHIFT_PRESSED)
							NetWriteString2(pParams->Socket, (char *)SHIFT_PF10_KEY, strlen(SHIFT_PF10_KEY), 0);

						else if (dwControlKeyState == LEFT_CTRL_PRESSED || dwControlKeyState == RIGHT_CTRL_PRESSED)
							NetWriteString2(pParams->Socket, (char *)CTRL_PF10_KEY, strlen(CTRL_PF10_KEY), 0);

						else if (dwControlKeyState == LEFT_ALT_PRESSED || dwControlKeyState == RIGHT_ALT_PRESSED)
							NetWriteString2(pParams->Socket, (char *)ALT_PF10_KEY, strlen(ALT_PF10_KEY), 0);

						else if ((dwControlKeyState & SHIFT_PRESSED) && ((dwControlKeyState & RIGHT_ALT_PRESSED) ||
							(dwControlKeyState & LEFT_ALT_PRESSED)) && ((dwControlKeyState & LEFT_CTRL_PRESSED) ||
							(dwControlKeyState & RIGHT_CTRL_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)SHIFT_ALT_CTRL_PF10_KEY, strlen(SHIFT_ALT_CTRL_PF10_KEY), 0);

						else if ((dwControlKeyState & RIGHT_ALT_PRESSED) || (dwControlKeyState & LEFT_ALT_PRESSED) &&
							((dwControlKeyState & LEFT_CTRL_PRESSED) || (dwControlKeyState & RIGHT_CTRL_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)ALT_CTRL_PF10_KEY, strlen(ALT_CTRL_PF10_KEY), 0);

						else if ((dwControlKeyState & SHIFT_PRESSED) && ((dwControlKeyState & RIGHT_ALT_PRESSED) ||
							(dwControlKeyState & LEFT_ALT_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)SHIFT_ALT_PF10_KEY, strlen(SHIFT_ALT_PF10_KEY), 0);

						else if ((dwControlKeyState & SHIFT_PRESSED) && ((dwControlKeyState & LEFT_CTRL_PRESSED) ||
							(dwControlKeyState & RIGHT_CTRL_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)SHIFT_CTRL_PF10_KEY, strlen(SHIFT_CTRL_PF10_KEY), 0);
						break;
					case VK_F11:
						if (dwControlKeyState == 0)
							NetWriteString2(pParams->Socket, (char *)PF11_KEY, strlen(PF11_KEY), 0);

						else if (dwControlKeyState == SHIFT_PRESSED)
							NetWriteString2(pParams->Socket, (char *)SHIFT_PF11_KEY, strlen(SHIFT_PF11_KEY), 0);

						else if (dwControlKeyState == LEFT_CTRL_PRESSED || dwControlKeyState == RIGHT_CTRL_PRESSED)
							NetWriteString2(pParams->Socket, (char *)CTRL_PF11_KEY, strlen(CTRL_PF11_KEY), 0);

						else if (dwControlKeyState == LEFT_ALT_PRESSED || dwControlKeyState == RIGHT_ALT_PRESSED)
							NetWriteString2(pParams->Socket, (char *)ALT_PF11_KEY, strlen(ALT_PF11_KEY), 0);

						else if ((dwControlKeyState & SHIFT_PRESSED) && ((dwControlKeyState & RIGHT_ALT_PRESSED) ||
							(dwControlKeyState & LEFT_ALT_PRESSED)) && ((dwControlKeyState & LEFT_CTRL_PRESSED) ||
							(dwControlKeyState & RIGHT_CTRL_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)SHIFT_ALT_CTRL_PF11_KEY, strlen(SHIFT_ALT_CTRL_PF11_KEY), 0);

						else if ((dwControlKeyState & RIGHT_ALT_PRESSED) || (dwControlKeyState & LEFT_ALT_PRESSED) &&
							((dwControlKeyState & LEFT_CTRL_PRESSED) || (dwControlKeyState & RIGHT_CTRL_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)ALT_CTRL_PF11_KEY, strlen(ALT_CTRL_PF11_KEY), 0);

						else if ((dwControlKeyState & SHIFT_PRESSED) && ((dwControlKeyState & RIGHT_ALT_PRESSED) ||
							(dwControlKeyState & LEFT_ALT_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)SHIFT_ALT_PF11_KEY, strlen(SHIFT_ALT_PF11_KEY), 0);

						else if ((dwControlKeyState & SHIFT_PRESSED) && ((dwControlKeyState & LEFT_CTRL_PRESSED) ||
							(dwControlKeyState & RIGHT_CTRL_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)SHIFT_CTRL_PF11_KEY, strlen(SHIFT_CTRL_PF11_KEY), 0);
						break;
					case VK_F12:
						if (dwControlKeyState == 0)
							NetWriteString2(pParams->Socket, (char *)PF12_KEY, strlen(PF12_KEY), 0);

						else if (dwControlKeyState == SHIFT_PRESSED)
							NetWriteString2(pParams->Socket, (char *)SHIFT_PF12_KEY, strlen(SHIFT_PF12_KEY), 0);

						else if (dwControlKeyState == LEFT_CTRL_PRESSED || dwControlKeyState == RIGHT_CTRL_PRESSED)
							NetWriteString2(pParams->Socket, (char *)CTRL_PF12_KEY, strlen(CTRL_PF12_KEY), 0);

						else if (dwControlKeyState == LEFT_ALT_PRESSED || dwControlKeyState == RIGHT_ALT_PRESSED)
							NetWriteString2(pParams->Socket, (char *)ALT_PF12_KEY, strlen(ALT_PF12_KEY), 0);

						else if ((dwControlKeyState & SHIFT_PRESSED) && ((dwControlKeyState & RIGHT_ALT_PRESSED) ||
							(dwControlKeyState & LEFT_ALT_PRESSED)) && ((dwControlKeyState & LEFT_CTRL_PRESSED) ||
							(dwControlKeyState & RIGHT_CTRL_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)SHIFT_ALT_CTRL_PF12_KEY, strlen(SHIFT_ALT_CTRL_PF12_KEY), 0);

						else if ((dwControlKeyState & RIGHT_ALT_PRESSED) || (dwControlKeyState & LEFT_ALT_PRESSED) &&
							((dwControlKeyState & LEFT_CTRL_PRESSED) || (dwControlKeyState & RIGHT_CTRL_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)ALT_CTRL_PF12_KEY, strlen(ALT_CTRL_PF12_KEY), 0);

						else if ((dwControlKeyState & SHIFT_PRESSED) && ((dwControlKeyState & RIGHT_ALT_PRESSED) ||
							(dwControlKeyState & LEFT_ALT_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)SHIFT_ALT_PF12_KEY, strlen(SHIFT_ALT_PF12_KEY), 0);

						else if ((dwControlKeyState & SHIFT_PRESSED) && ((dwControlKeyState & LEFT_CTRL_PRESSED) ||
							(dwControlKeyState & RIGHT_CTRL_PRESSED)))
							NetWriteString2(pParams->Socket, (char *)SHIFT_CTRL_PF12_KEY, strlen(SHIFT_CTRL_PF12_KEY), 0);
						break;
					default:
						if(n > 0) { //(strcmp((char *) octets, "")) {
							if ((dwControlKeyState & LEFT_ALT_PRESSED) || (dwControlKeyState & RIGHT_ALT_PRESSED)) {								
								memset(tmp_buf, 0, sizeof(tmp_buf));
								tmp_buf[0] = '\x1b';
								memcpy(tmp_buf + 1, (char *)octets, n);
								NetWriteString2(pParams->Socket, tmp_buf, n + 1, 0);								
							}
							else
								NetWriteString2(pParams->Socket, (char *)octets, n, 0);
						}							
						break;
					}
				}
			}
			break;
		}
		break;
	}

	return glob_outlen;
}
