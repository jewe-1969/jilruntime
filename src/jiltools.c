//------------------------------------------------------------------------------
// File: JILTools.c                                         (c) 2003 jewe.org
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
//	Static tool functions for handle type conversion, retrieving pointers to
//	objects in the CStrSegment, etc.
//	Implemented as static functions in the include file, because some compilers
//	can inline them.
//------------------------------------------------------------------------------

#include "jilstdinc.h"

#include "jiltools.h"
#include "jclstring.h"

//------------------------------------------------------------------------------
// kFormatWorstCaseBufferSize
//------------------------------------------------------------------------------

static const JILLong kFormatWorstCaseBufferSize = 4096;

//------------------------------------------------------------------------------
// JILMessageLog
//------------------------------------------------------------------------------

void JILMessageLog(JILState* pState, const JILChar* pFormat, ...)
{
	if( pState->vmLogOutputProc )
	{
		JILLong len;
		JILChar* pBuffer;
		va_list arguments;
		va_start( arguments, pFormat );

		pBuffer = (JILChar*) malloc(kFormatWorstCaseBufferSize);
		len = JIL_VSNPRINTF( pBuffer, kFormatWorstCaseBufferSize - 1, pFormat, arguments );
		pBuffer[len] = 0;

		pState->vmLogOutputProc( pState, pBuffer );

		free(pBuffer);
		va_end( arguments );
	}
}

//------------------------------------------------------------------------------
// JILSnprintf
//------------------------------------------------------------------------------

JILLong JILSnprintf(JILChar* pDest, JILLong destSize, const JILChar* pFormat, ...)
{
	JILLong len = 0;
	va_list arguments;
	va_start( arguments, pFormat );
	if( pDest && destSize > 0 )
	{
		len = JIL_VSNPRINTF( pDest, destSize, pFormat, arguments );
		pDest[destSize - 1] = 0;
	}
	va_end( arguments );
	return len;
}

//------------------------------------------------------------------------------
// JILStrcat
//------------------------------------------------------------------------------

void JILStrcat(JILChar* pDest, JILLong destSize, const JILChar* pSrc)
{
	if( pDest && destSize > 0 )
	{
		// get destination length
		JILLong destLen = strlen(pDest);
		JILLong charsToCopy = destSize - (destLen + 1); // count 0 byte!
		// some space left in buffer?
		if( charsToCopy > 0 )
		{
			strncpy(pDest + destLen, pSrc, charsToCopy);
			pDest[destLen + charsToCopy] = 0;
		}
	}
}

//------------------------------------------------------------------------------
// JILStrcpy
//------------------------------------------------------------------------------

void JILStrcpy(JILChar* pDest, JILLong destSize, const JILChar* pSrc)
{
	if( pDest && destSize > 0 )
	{
		strncpy(pDest, pSrc, destSize - 1);
		pDest[destSize - 1] = 0;
	}
}

//------------------------------------------------------------------------------
// JILStrncpy
//------------------------------------------------------------------------------

void JILStrncpy(JILChar* pDest, JILLong destSize, const JILChar* pSrc, JILLong length)
{
	if( pDest && destSize > 0 )
	{
		if( length > (destSize - 1) )
			length = destSize - 1;
		strncpy(pDest, pSrc, length);
		pDest[length] = 0;
	}
}
