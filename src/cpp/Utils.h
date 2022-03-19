/*----------------------------------------------------------------------------s
	NAME
		Utils.h

	PURPOSE
		Defines for the general-purpose functions for the WinTab demos.

	COPYRIGHT
		This file is Copyright (c) Wacom Company, Ltd. 2020 All Rights Reserved
		with portions copyright 1991-1998 by LCS/Telegraphics.

		The text and information contained in this file may be freely used,
		copied, or distributed without compensation or licensing restrictions.
---------------------------------------------------------------------------- */
#pragma once

#include    <windows.h>
#include    <cstdio>
#include    <cassert>
#include    <cstdarg>

#include    <wacom-wintab/WINTAB.H>

// Ignore warnings about using unsafe string functions.
#pragma warning( disable : 4996 )

// Function pointers to Wintab functions exported from wintab32.dll.
typedef UINT ( API *WTINFOA )(UINT, UINT, LPVOID);

typedef HCTX ( API *WTOPENA )(HWND, LPLOGCONTEXTA, bool);

typedef bool ( API *WTGETA )(HCTX, LPLOGCONTEXT);

typedef bool ( API *WTSETA )(HCTX, LPLOGCONTEXT);

typedef bool ( API *WTCLOSE )(HCTX);

typedef bool ( API *WTENABLE )(HCTX, bool);

typedef bool ( API *WTPACKET )(HCTX, UINT, LPVOID);

typedef bool ( API *WTOVERLAP )(HCTX, bool);

typedef bool ( API *WTSAVE )(HCTX, LPVOID);

typedef bool ( API *WTCONFIG )(HCTX, HWND);

typedef HCTX ( API *WTRESTORE )(HWND, LPVOID, bool);

typedef bool ( API *WTEXTSET )(HCTX, UINT, LPVOID);

typedef bool ( API *WTEXTGET )(HCTX, UINT, LPVOID);

typedef bool ( API *WTQUEUESIZESET )(HCTX, int);

typedef int  ( API *WTDATAPEEK )(HCTX, UINT, UINT, int, LPVOID, LPINT);

typedef int  ( API *WTPACKETSGET )(HCTX, int, LPVOID);

typedef HMGR ( API *WTMGROPEN )(HWND, UINT);

typedef bool ( API *WTMGRCLOSE )(HMGR);

typedef HCTX ( API *WTMGRDEFCONTEXT )(HMGR, bool);

typedef HCTX ( API *WTMGRDEFCONTEXTEX )(HMGR, UINT, bool);

// Loaded Wintab32 API functions.
extern HINSTANCE ghWintab;

extern WTINFOA gpWTInfoA;
extern WTOPENA gpWTOpenA;
extern WTGETA gpWTGetA;
extern WTSETA gpWTSetA;
extern WTCLOSE gpWTClose;
extern WTPACKET gpWTPacket;
extern WTENABLE gpWTEnable;
extern WTOVERLAP gpWTOverlap;
extern WTSAVE gpWTSave;
extern WTCONFIG gpWTConfig;
extern WTRESTORE gpWTRestore;
extern WTEXTSET gpWTExtSet;
extern WTEXTGET gpWTExtGet;
extern WTQUEUESIZESET gpWTQueueSizeSet;
extern WTDATAPEEK gpWTDataPeek;
extern WTPACKETSGET gpWTPacketsGet;
extern WTMGROPEN gpWTMgrOpen;
extern WTMGRCLOSE gpWTMgrClose;
extern WTMGRDEFCONTEXT gpWTMgrDefContext;
extern WTMGRDEFCONTEXTEX gpWTMgrDefContextEx;

bool LoadWintab();

void UnloadWintab();
