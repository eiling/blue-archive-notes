/*----------------------------------------------------------------------------
	NAME
		Utils.c

	PURPOSE
		Defines for the general-purpose functions for the WinTab demos.

	COPYRIGHT
		This file is Copyright (c) Wacom Company, Ltd. 2020 All Rights Reserved
		with portions copyright 1991-1998 by LCS/Telegraphics.

		The text and information contained in this file may be freely used,
		copied, or distributed without compensation or licensing restrictions.
---------------------------------------------------------------------------- */

#include "Utils.h"

HINSTANCE ghWintab = nullptr;

WTINFOA gpWTInfoA = nullptr;
WTOPENA gpWTOpenA = nullptr;
WTGETA gpWTGetA = nullptr;
WTSETA gpWTSetA = nullptr;
WTCLOSE gpWTClose = nullptr;
WTPACKET gpWTPacket = nullptr;
WTENABLE gpWTEnable = nullptr;
WTOVERLAP gpWTOverlap = nullptr;
WTSAVE gpWTSave = nullptr;
WTCONFIG gpWTConfig = nullptr;
WTRESTORE gpWTRestore = nullptr;
WTEXTSET gpWTExtSet = nullptr;
WTEXTGET gpWTExtGet = nullptr;
WTQUEUESIZESET gpWTQueueSizeSet = nullptr;
WTDATAPEEK gpWTDataPeek = nullptr;
WTPACKETSGET gpWTPacketsGet = nullptr;
WTMGROPEN gpWTMgrOpen = nullptr;
WTMGRCLOSE gpWTMgrClose = nullptr;
WTMGRDEFCONTEXT gpWTMgrDefContext = nullptr;
WTMGRDEFCONTEXTEX gpWTMgrDefContextEx = nullptr;

// GETPROCADDRESS macro used to create the gpWT* () dynamic function pointers, which allow the
// same built program to run on both 32bit and 64bit systems w/o having to rebuild the app.
#define GETPROCADDRESS(type, func) \
    gp##func = (type)GetProcAddress(ghWintab, #func); \
    if (!gp##func){ UnloadWintab(); return false; }

// Purpose
//		Find wintab32.dll and load it.  
//		Find the exported functions we need from it.
//
//	Returns
//		true on success.
//		false on failure.
//
bool LoadWintab() {
    // Wintab32.dll is a module installed by the tablet driver
    ghWintab = LoadLibraryA("Wintab32.dll");

    if (!ghWintab) {
        return false;
    }

    // Explicitly find the exported Wintab functions in which we are interested.
    // We are using the ASCII, not unicode versions (where applicable).
    GETPROCADDRESS(WTOPENA, WTOpenA)
    GETPROCADDRESS(WTINFOA, WTInfoA)
    GETPROCADDRESS(WTGETA, WTGetA)
    GETPROCADDRESS(WTSETA, WTSetA)
    GETPROCADDRESS(WTPACKET, WTPacket)
    GETPROCADDRESS(WTCLOSE, WTClose)
    GETPROCADDRESS(WTENABLE, WTEnable)
    GETPROCADDRESS(WTOVERLAP, WTOverlap)
    GETPROCADDRESS(WTSAVE, WTSave)
    GETPROCADDRESS(WTCONFIG, WTConfig)
    GETPROCADDRESS(WTRESTORE, WTRestore)
    GETPROCADDRESS(WTEXTSET, WTExtSet)
    GETPROCADDRESS(WTEXTGET, WTExtGet)
    GETPROCADDRESS(WTQUEUESIZESET, WTQueueSizeSet)
    GETPROCADDRESS(WTDATAPEEK, WTDataPeek)
    GETPROCADDRESS(WTPACKETSGET, WTPacketsGet)
    GETPROCADDRESS(WTMGROPEN, WTMgrOpen)
    GETPROCADDRESS(WTMGRCLOSE, WTMgrClose)
    GETPROCADDRESS(WTMGRDEFCONTEXT, WTMgrDefContext)
    GETPROCADDRESS(WTMGRDEFCONTEXTEX, WTMgrDefContextEx)

    return true;
}

// Purpose
//		Uninitializes use of wintab32.dll
//		Frees the loaded Wintab32.dll module and zeros all dynamic function pointers.
//
// Returns
//		Nothing.
//
void UnloadWintab() {
    if (ghWintab) {
        FreeLibrary(ghWintab);
        ghWintab = nullptr;
    }

    gpWTOpenA = nullptr;
    gpWTClose = nullptr;
    gpWTInfoA = nullptr;
    gpWTPacket = nullptr;
    gpWTEnable = nullptr;
    gpWTOverlap = nullptr;
    gpWTSave = nullptr;
    gpWTConfig = nullptr;
    gpWTGetA = nullptr;
    gpWTSetA = nullptr;
    gpWTRestore = nullptr;
    gpWTExtSet = nullptr;
    gpWTExtGet = nullptr;
    gpWTQueueSizeSet = nullptr;
    gpWTDataPeek = nullptr;
    gpWTPacketsGet = nullptr;
    gpWTMgrOpen = nullptr;
    gpWTMgrClose = nullptr;
    gpWTMgrDefContext = nullptr;
    gpWTMgrDefContextEx = nullptr;
}
