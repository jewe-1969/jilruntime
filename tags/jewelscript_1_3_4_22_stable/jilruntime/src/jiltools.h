//------------------------------------------------------------------------------
// File: JILTools.h                                         (c) 2003 jewe.org
//------------------------------------------------------------------------------
//
// DISCLAIMER:
// -----------
//	THIS SOFTWARE IS SUBJECT TO THE LICENSE AGREEMENT FOUND IN "jilapi.h" AND
//	"COPYING". BY USING THIS SOFTWARE YOU IMPLICITLY DECLARE YOUR AGREEMENT TO
//	THE TERMS OF THIS LICENSE.
//
// Description: 
// ------------
/// @file jiltools.h
/// Static tool functions or function macros for handle type conversion,
/// retrieving pointers to objects in the CStrSegment, etc. Some of them
/// implemented as static functions in this include file, because some compilers
/// can inline them.
//------------------------------------------------------------------------------

#ifndef JILTOOLS_H
#define JILTOOLS_H

#include "jiltypes.h"

//------------------------------------------------------------------------------
// JIL_FORMAT_MAX_BUFFER_SIZE
//------------------------------------------------------------------------------

#define JIL_FORMAT_MAX_BUFFER_SIZE	16384

//------------------------------------------------------------------------------
// JILMIN, JILMAX
//------------------------------------------------------------------------------

#define JILMIN(X,Y)					(X<Y?X:Y)
#define JILMAX(X,Y)					(X>Y?X:Y)

//------------------------------------------------------------------------------
// Handle type converters
//------------------------------------------------------------------------------

#define	JILGetIntHandle(PTR)		((JILHandleInt*)(PTR))
#define	JILGetFloatHandle(PTR)		((JILHandleFloat*)(PTR))
#define	JILGetStringHandle(PTR)		((JILHandleString*)(PTR))
#define	JILGetObjectHandle(PTR)		((JILHandleObject*)(PTR))
#define	JILGetNObjectHandle(PTR)	((JILHandleNObject*)(PTR))
#define	JILGetArrayHandle(PTR)		((JILHandleArray*)(PTR))
#define JILGetContextHandle(PTR)	((JILHandleContext*)(PTR))
#define JILGetDelegateHandle(PTR)	((JILHandleDelegate*)(PTR))

#define JILGetDataHandleLong(PTR)	(*((JILLong*) PTR->data))
#define JILGetDataHandleFloat(PTR)	(*((JILFloat*) PTR->data))

//------------------------------------------------------------------------------
// Get addresses of items in the CStr segment
//------------------------------------------------------------------------------

#define	JILCStrGetString(STATE, OFFSET)			((JILChar*)(STATE->vmpCStrSegment + (OFFSET)))
#define	JILCStrGetObjectDesc(STATE, OFFSET)		((JILObjectDesc*)(STATE->vmpCStrSegment + (OFFSET)))
#define	JILCStrGetNObjectDesc(STATE, OFFSET)	((JILNObjectDesc*)(STATE->vmpCStrSegment + (OFFSET)))
#define JILCStrGetVTable(STATE, OFFSET)			((JILLong*)(STATE->vmpCStrSegment + (OFFSET)))

//------------------------------------------------------------------------------
// JILTypeInfoFromType
//------------------------------------------------------------------------------
/// Get the address of a TypeInfo entry from a type identifier.

#define JILTypeInfoFromType(STATE, TYPE)	(STATE->vmpTypeInfoSegment + (TYPE))

//------------------------------------------------------------------------------
// JILMessageLog
//------------------------------------------------------------------------------
/// Output a formatted string with variable arguments to the runtime's message
/// log callback. Uses standard ANSI format specifiers. @see JILSetLogCallback ()

JILEXTERN void	JILMessageLog		(JILState* pState, const JILChar* pFormat, ...);

//------------------------------------------------------------------------------
// JILSnprintf
//------------------------------------------------------------------------------
/// A snprintf function that does not leave a string unterminated if its longer
/// than the specified number of bytes. Argument 'destSize' specifies the size
/// in BYTES (not characters!) that can be written to pDest.

JILEXTERN JILLong	JILSnprintf		(JILChar* pDest, JILLong destSize, const JILChar* pFormat, ...);

//------------------------------------------------------------------------------
// JILStrcat
//------------------------------------------------------------------------------
/// A strcat function that does not cause buffer overruns. Argument 'destSize'
/// specifies the size in BYTES (not characters!) that can be written to pDest.

JILEXTERN void	JILStrcat			(JILChar* pDest, JILLong destSize, const JILChar* pSrc);

//------------------------------------------------------------------------------
// JILStrcpy
//------------------------------------------------------------------------------
/// A strcpy function that does not cause buffer overruns. Argument 'destSize'
/// specifies the size in BYTES (not characters!) that can be written to pDest.

JILEXTERN void	JILStrcpy			(JILChar* pDest, JILLong destSize, const JILChar* pSrc);

//------------------------------------------------------------------------------
// JILStrncpy
//------------------------------------------------------------------------------
/// A strncpy function that does not carry the risk of cause buffer overruns or
/// leaving a string unterminated. Argument 'destSize' specifies the size
/// in BYTES (not characters!) that can be written to pDest.

JILEXTERN void	JILStrncpy			(JILChar* pDest, JILLong destSize, const JILChar* pSrc, JILLong length);

//------------------------------------------------------------------------------
// JILRevisionToLong
//------------------------------------------------------------------------------
/// Converts a string in the form "a.b.c.d" into an integer version number.

JILINLINE JILLong JILRevisionToLong(const JILChar* pRevision)
{
	JILLong a, b, c, d;
	const JILChar* pos;
	a = atol(pRevision) & 255;			// "a"
	pos = strchr(pRevision, '.') + 1;	// "a."
	b = atol(pos) & 255;				// "a.b"
	pos = strchr(pos, '.') + 1;			// "a.b."
	c = atol(pos) & 255;				// "a.b.c"
	pos = strchr(pos, '.') + 1;			// "a.b.c."
	d = atol(pos) & 255;				// "a.b.c.d"
	return (a << 24) | (b << 16) | (c << 8) | d;
}

//------------------------------------------------------------------------------
// JILLongToRevision
//------------------------------------------------------------------------------
/// Converts an integer version number into a string in the form "a.b.c.d".

JILINLINE const JILChar* JILLongToRevision(JILChar* pRevision, JILLong revision)
{
	JILSnprintf(pRevision, sizeof(JILChar) * 16, "%d.%d.%d.%d",
		(revision >> 24) & 255,
		(revision >> 16) & 255,
		(revision >> 8)  & 255,
		revision         & 255);
	return pRevision;
}

//------------------------------------------------------------------------------
// JILFuncInfoFlags accessor functions
//------------------------------------------------------------------------------

JILINLINE JILBool JILFuncIsMethod(JILLong flags)	{ return (flags & fi_method) == fi_method; }
JILINLINE JILBool JILFuncIsCtor(JILLong flags)		{ return (flags & fi_ctor) == fi_ctor; }
JILINLINE JILBool JILFuncIsConvertor(JILLong flags)	{ return (flags & fi_convertor) == fi_convertor; }
JILINLINE JILBool JILFuncIsAccessor(JILLong flags)	{ return (flags & fi_accessor) == fi_accessor; }
JILINLINE JILBool JILFuncIsCofunc(JILLong flags)	{ return (flags & fi_cofunc) == fi_cofunc; }
JILINLINE JILBool JILFuncIsAnonymous(JILLong flags)	{ return (flags & fi_anonymous) == fi_anonymous; }
JILINLINE JILBool JILFuncIsExplicit(JILLong flags)	{ return (flags & fi_explicit) == fi_explicit; }
JILINLINE JILBool JILFuncIsStrict(JILLong flags)	{ return (flags & fi_strict) == fi_strict; }

#endif	// #ifndef JILTOOLS_H
