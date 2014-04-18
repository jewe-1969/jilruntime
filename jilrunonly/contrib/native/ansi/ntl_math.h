//------------------------------------------------------------------------------
// File: ntl_math.h                                            (c) 2005 jewe.org
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
//	This native type exports advanced math functions derived from the ANSI
//	math functions declared in "math.h"
//
//------------------------------------------------------------------------------

#ifndef NTL_MATH_H
#define NTL_MATH_H

//------------------------------------------------------------------------------
// includes
//------------------------------------------------------------------------------

#include "jilnativetype.h"

//------------------------------------------------------------------------------
// the type proc
//------------------------------------------------------------------------------

JILEXTERN int MathProc(NTLInstance* pInst, int msg, int param, void* pDataIn, void** ppDataOut);

#endif	// #ifndef NTL_MATH_H
