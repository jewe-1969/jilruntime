//------------------------------------------------------------------------------
// File: ntl_stdlib.c                                          (c) 2005 jewe.org
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "jilnativetypeex.h"
#include "jilarray.h"
#include "jilstring.h"
#include "jiltools.h"

#include "ntl_stdlib.h"

//------------------------------------------------------------------------------
// function index numbers
//------------------------------------------------------------------------------
// It is important to keep these index numbers in the same order as the function
// declarations in the class declaration string!

enum
{
	// console input / output
	kPrintInt,
	kPrintFloat,
	kPrintString,
	kPrintArray,
	kPrintv,
	kPrintf,
	kPrintLine,
	kGetString,

	// random functions
	kRand,
	kRandL,
	kGRand,
	kGRandL,
	kChance,
	kRandInit,
	kRandSeed,

	// conversion functions
	kAtol,
	kAtof,
	kLtoa,
	kFtoa,
	kCtoa,

	kUnknownFunction
};

//------------------------------------------------------------------------------
// class declaration string
//------------------------------------------------------------------------------

static const char* kClassDeclaration =
	// console input / output
	"function 			print(const int value);"
	"function 			print(const float value);"
	"function 			print(const string value);"
	"function 			print(const array value);"
	"function 			printv(const var value);"
	"function 			printf(const string format, const var value);"
	"function 			println(const string value);"
	"function string	getString();"
	// random functions
	"function float		rand();"
	"function int		rand(const int min, const int max);"
	"function float		grand();"
	"function int		grand(const int min, const int max);"
	"function int		chance(const int val);"
	"function			randInit();"
	"function			randSeed(const float seed);"
	// conversion functions
	"function int		atol(const string text);"
	"function float		atof(const string text);"
	"function string	ltoa(const int);"
	"function string	ftoa(const float);"
	"function string	ctoa(const int);"
;

//------------------------------------------------------------------------------
// some constants
//------------------------------------------------------------------------------

static const char*	kClassName		=	"stdlib";
static const char*	kAuthorName		=	"www.jewe.org";
static const char*	kAuthorString	=	"Standard library for JewelScript.";
static const char*	kTimeStamp		=	"08.10.2005";
static const int	kAuthorVersion	=	0x00000008;

static const int	kStaticBufferSize = 4096;

//------------------------------------------------------------------------------
// forward declare static functions
//------------------------------------------------------------------------------

static int StdLibCall(NTLInstance* pInst, int funcID);

//------------------------------------------------------------------------------
// Helper functions
//------------------------------------------------------------------------------

static JILFloat RandomFloat()
{
	return ((JILFloat) rand()) / ((JILFloat) RAND_MAX);
}

static JILFloat GaussFloat()
{
	JILFloat r1 = RandomFloat();
	JILFloat r2 = RandomFloat();
	return (r1 + r2) * 0.5;
}

static JILLong RandL(JILFloat rnd, JILLong a, JILLong b)
{
	JILFloat min, max;
	min = (JILFloat) a;
	max = (JILFloat) b;
	rnd = rnd * (max - min) + min;
	rnd = (rnd < 0.0) ? ceil(rnd - 0.5) : floor(rnd + 0.5);
	return (JILLong) rnd;
}

//------------------------------------------------------------------------------
// our main proc
//------------------------------------------------------------------------------
// This is the function you will need to register with the JIL virtual machine.
// Whenever the virtual machine needs to communicate with your native type,
// it will call this proc.

int StdLibProc(NTLInstance* pInst, int msg, int param, void* pDataIn, void** ppDataOut)
{
	int result = JIL_No_Exception;

	switch( msg )
	{
		// runtime messages
		case NTL_Register:				break;
		case NTL_Initialize:			break;
		case NTL_NewObject:				result = JIL_ERR_Unsupported_Native_Call; break;
		case NTL_MarkHandles:			break;
		case NTL_CallStatic:			return StdLibCall(pInst, param);
		case NTL_CallMember:			result = JIL_ERR_Unsupported_Native_Call; break;
		case NTL_DestroyObject:			result = JIL_ERR_Unsupported_Native_Call; break;
		case NTL_Terminate:				break;
		case NTL_Unregister:			break;

		// class information queries
		case NTL_GetInterfaceVersion:	return NTLRevisionToLong(JIL_TYPE_INTERFACE_VERSION);
		case NTL_GetAuthorVersion:		return kAuthorVersion;
		case NTL_GetClassName:			(*(const char**) ppDataOut) = kClassName; break;
		case NTL_GetDeclString:			(*(const char**) ppDataOut) = kClassDeclaration; break;
		case NTL_GetBuildTimeStamp:		(*(const char**) ppDataOut) = kTimeStamp; break;
		case NTL_GetAuthorName:			(*(const char**) ppDataOut) = kAuthorName; break;
		case NTL_GetAuthorString:		(*(const char**) ppDataOut) = kAuthorString; break;

		default:						result = JIL_ERR_Unsupported_Native_Call; break;
	}
	return result;
}

//------------------------------------------------------------------------------
// StdLibCall
//------------------------------------------------------------------------------
// Called when the VM wants to call one of our GLOBAL functions.
// Process parameters on from the stack by calling the NTLGetArg...() functions.
// The argument index indicates the position of an argument on the stack:
// The first argument is at index 0, the second at 1, and so on.
// To return a value, call one of the NTLReturn...() functions.

static int StdLibCall(NTLInstance* pInst, int funcID)
{
	int result = JIL_No_Exception;
	JILState* ps = NTLInstanceGetVM(pInst);

	switch( funcID )
	{
		case kRand:
		{
			NTLReturnFloat(ps, RandomFloat());
			break;
		}
		case kRandL:
		{
			NTLReturnInt(ps,
				RandL(RandomFloat(),
					NTLGetArgInt(ps, 0),
					NTLGetArgInt(ps, 1)));
			break;
		}
		case kGRand:
		{
			NTLReturnFloat(ps, GaussFloat());
			break;
		}
		case kGRandL:
		{
			NTLReturnInt(ps,
				RandL(GaussFloat(),
					NTLGetArgInt(ps, 0),
					NTLGetArgInt(ps, 1)));
			break;
		}
		case kChance:
		{
			// This will gamble a value and return true if the given value matches the gambled value.
			// The likelyhood of a given value to yield true increases with higher values. A given value
			// of 0 is the least likely to yield true, while a given value of 100 is the most likely to yield true.
			JILLong w = NTLGetArgInt(ps, 0);
			if( w < 0 )
				w = 0;
			if( w > 100 )
				w = 100;
			NTLReturnInt(ps, RandL(GaussFloat(), -100, 100) == (100 - w));
			break;
		}
		case kRandInit:
		{
			srand( (JILDWord)time(NULL) );
			break;
		}
		case kRandSeed:
		{
			unsigned int seed = (unsigned int) (NTLGetArgFloat(ps, 0) * RAND_MAX);
			srand(seed);
			break;
		}
		case kAtol:
		{
			char* dummy;
			long lResult = 0;
			const char* str = NTLGetArgString(ps, 10);
			if( str )
				lResult = strtol(str, &dummy, 0);
			NTLReturnInt(ps, lResult);
			break;
		}
		case kAtof:
		{
			JILFloat fResult = 0.0;
			const char* str = NTLGetArgString(ps, 0);
			if( str )
				fResult = atof(str);
			NTLReturnFloat(ps, fResult);
			break;
		}
		case kLtoa:
		{
			char buf[64];
			JILSnprintf(buf, 64, "%d", NTLGetArgInt(ps, 0));
			NTLReturnString(ps, buf);
			break;
		}
		case kFtoa:
		{
			char buf[64];
			JILSnprintf(buf, 64, "%g", NTLGetArgFloat(ps, 0));
			NTLReturnString(ps, buf);
			break;
		}
		case kCtoa:
		{
			char buf[4];
			buf[0] = (char) NTLGetArgInt(ps, 0);
			buf[1] = (char) 0;
			NTLReturnString(ps, buf);
			break;
		}
		case kPrintInt:
		{
			printf( "%d", NTLGetArgInt(ps, 0) );
			break;
		}
		case kPrintFloat:
		{
			printf( "%g", NTLGetArgFloat(ps, 0) );
			break;
		}
		case kPrintString:
		{
			const JILChar* pString = NTLGetArgString(ps, 0);
			if( pString )
			{
				printf( "%s", pString );	// MUST use printf() for this since its the only text output function that does NOT append CR+LF!!!
			}
			break;
		}
		case kPrintLine:
		{
			const JILChar* pString = NTLGetArgString(ps, 0);
			if( pString )
			{
				puts( pString );
			}
			break;
		}
		case kPrintArray:
		{
			JILArray* pArray;
			JILString* pResult;
			pArray = NTLGetArgObject(ps, 0, type_array);
			if( pArray )
			{
				pResult = JILArray_ToString(pArray);
				printf( "%s", JILString_String(pResult) );
				JILString_Delete(pResult);
			}
			break;
		}
		case kPrintv:
		{
			int t = NTLGetArgTypeID(ps, 0);
			if( t == type_int )
			{
				printf( "%d", NTLGetArgInt(ps, 0) );
			}
			else if( t == type_float )
			{
				printf( "%g", NTLGetArgFloat(ps, 0) );
			}
			else if( t == type_string )
			{
				printf( "%s", NTLGetArgString(ps, 0) );
			}
			else if( t == type_array )
			{
				JILString* pResult;
				JILArray* pArray;
				pArray = NTLGetArgObject(ps, 0, type_array);
				if( pArray )
				{
					pResult = JILArray_ToString(pArray);
					printf( "%s", JILString_String(pResult) );
					JILString_Delete(pResult);
				}
			}
			else
			{
				printf( "%s", NTLGetTypeName(ps, t) );
			}
			break;
		}
		case kPrintf:
		{
			int t = NTLGetArgTypeID(ps, 1);
			if( t == type_int )
			{
				printf( NTLGetArgString(ps, 0), NTLGetArgInt(ps, 1) );
			}
			else if( t == type_float )
			{
				printf( NTLGetArgString(ps, 0), NTLGetArgFloat(ps, 1) );
			}
			else if( t == type_string )
			{
				printf( NTLGetArgString(ps, 0), NTLGetArgString(ps, 1) );
			}
			else if( t == type_array )
			{
				JILString* pResult;
				JILString* pFormat;
				JILArray* pArray;
				pFormat = NTLGetArgObject(ps, 0, type_string);
				if( pFormat )
				{
					pArray = NTLGetArgObject(ps, 1, type_array);
					if( pArray )
					{
						pResult = JILArray_Format(pArray, pFormat);
						printf( "%s", JILString_String(pResult) );
						JILString_Delete(pResult);
					}
				}
			}
			else
			{
				printf( "%s", NTLGetTypeName(ps, t) );
			}
			break;
		}
		case kGetString:
		{
			char* pos;
			char* pBuffer = (char*) malloc(kStaticBufferSize);
			*pBuffer = 0; // incase fgets() doesnt write anything!
			fgets(pBuffer, kStaticBufferSize, stdin);
			// search for return or linefeed and kill it
			pos = strchr(pBuffer, 13);
			if( pos )
				*pos = 0;
			pos = strchr(pBuffer, 10);
			if( pos )
				*pos = 0;
			NTLReturnString(ps, pBuffer);
			free( pBuffer );
			break;
		}
		default:
		{
			result = JIL_ERR_Invalid_Function_Index;
			break;
		}
	}
	return result;
}
