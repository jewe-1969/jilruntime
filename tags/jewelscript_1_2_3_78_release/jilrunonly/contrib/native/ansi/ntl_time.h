//------------------------------------------------------------------------------
// File: ntl_time.h                                            (c) 2005 jewe.org
//------------------------------------------------------------------------------
//
// Description: 
// ------------
//	A native type that implements a Time object for JewelScript.
//
//	Native types are classes or global functions written in C or C++. These
//	classes or functions can be used in the JewelScript language like any other
//	class or function written directly in JewelScript.
//
//	This native type implements a Time object, based on the functions found in
//	the ANSI standard 'time.h' include file.
//
//------------------------------------------------------------------------------

#ifndef NTL_TIME_H
#define NTL_TIME_H

//------------------------------------------------------------------------------
// includes
//------------------------------------------------------------------------------

#include <time.h>
#include "jilnativetypeex.h"

//------------------------------------------------------------------------------
// class NTime
//------------------------------------------------------------------------------

typedef struct NTime NTime;

struct NTime
{
	struct tm	time;
	clock_t		lastTick;
	clock_t		diffTick;
};

JILEXTERN void NTLTimeUpdate(NTime* _this);	// Update the time structure when one of the members have been set.

//------------------------------------------------------------------------------
// the type proc
//------------------------------------------------------------------------------

JILEXTERN int TimeProc(NTLInstance* pInst, int msg, int param, void* pDataIn, void** ppDataOut);

#endif	// #ifndef NTL_TIME_H
