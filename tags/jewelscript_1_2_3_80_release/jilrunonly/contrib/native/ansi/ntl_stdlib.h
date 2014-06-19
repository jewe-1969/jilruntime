//------------------------------------------------------------------------------
// File: ntl_stdlib.h                                          (c) 2005 jewe.org
//------------------------------------------------------------------------------
//
// Description:
// ------------
//	An example "native type" for the JIL virtual machine.
//
//	Native types are classes or global functions written in C or C++. These
//	classes or functions can be used in the JewelScript language like any other
//	class or function written directly in JewelScript.
//
//	This native type contains only some standard global functions like printing
//	strings to console, reading strings from console, converting strings to
//	numbers, etc.
//
//------------------------------------------------------------------------------

#ifndef NTL_STDLIB_H
#define NTL_STDLIB_H

//------------------------------------------------------------------------------
// includes
//------------------------------------------------------------------------------

#include "jilnativetype.h"

//------------------------------------------------------------------------------
// the type proc
//------------------------------------------------------------------------------

JILEXTERN int StdLibProc(NTLInstance* pInst, int msg, int param, void* pDataIn, void** ppDataOut);

#endif	// #ifndef NTL_STDLIB_H
