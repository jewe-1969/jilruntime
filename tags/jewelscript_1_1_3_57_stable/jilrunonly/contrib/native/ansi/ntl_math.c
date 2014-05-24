//------------------------------------------------------------------------------
// File: ntl_math.c                                            (c) 2005 jewe.org
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

#include <math.h>
#include <time.h>	// to initialize random seed

#include "ntl_math.h"

//------------------------------------------------------------------------------
// function index numbers
//------------------------------------------------------------------------------
// It is important to keep these index numbers in the same order as the function
// declarations in the class declaration string!

enum
{
	kAbs,
	kAcos,
	kAsin,
	kAtan,
	kAtan2,
	kCos,
	kCosh,
	kExp,
	kFabs,
	kLog,
	kLog10,
	kPow,
	kSin,
	kSinh,
	kTan,
	kTanh,
	kSqrt,
	kCeil,
	kFloor,
	kInt,
	kFrac,
	kSgn,
	kMin,
	kMax,
	kFmin,
	kFmax,
	kRandomSeed,
	kRandom,
	kWhiteNoise,
	kGaussianNoise,
	kLinInterpolation,

	kUnknownFunction
};

//------------------------------------------------------------------------------
// class declaration string
//------------------------------------------------------------------------------

static const char* kClassDeclaration =
	"function int	abs(const int);"
	"function float	acos(const float);"
	"function float	asin(const float);"
	"function float	atan(const float);"
	"function float	atan2(const float, const float);"
	"function float	cos(const float);"
	"function float	cosh(const float);"
	"function float	exp(const float);"
	"function float	fabs(const float);"
	"function float	log(const float);"
	"function float	log10(const float);"
	"function float	pow(const float, const float);"
	"function float	sin(const float);"
	"function float	sinh(const float);"
	"function float	tan(const float);"
	"function float	tanh(const float);"
	"function float	sqrt(const float);"
	"function float	ceil(const float);"
	"function float	floor(const float);"
	"function float	integer(const float);"
	"function float	frac(const float);"
	"function float	sgn(const float);"
	"function int	min(const int, const int);"
	"function int	max(const int, const int);"
	"function float	fmin(const float, const float);"
	"function float	fmax(const float, const float);"
	"function		randomSeed(const int seed);"
	"function int	random();"
	"function float	whiteNoise();"
	"function float	gaussianNoise();"
	"function float	lin(const float, const float, const float);"
	// constants
	"const float PI = 3.141592653589793;"
	"const float E  = 2.718281828459045;"
;

//------------------------------------------------------------------------------
// some constants
//------------------------------------------------------------------------------

static const char*	kClassName		=	"math";
static const char*	kAuthorName		=	"www.jewe.org";
static const char*	kAuthorString	=	"A math library for JewelScript.";
static const char*	kTimeStamp		=	"08.10.2005";
static const int	kAuthorVersion	=	0x00000003;

//------------------------------------------------------------------------------
// forward declare static functions
//------------------------------------------------------------------------------

void				MathSetRandomSeed(JILDWord seed);
JILDWord			MathRandom();
static int			MathCall(NTLInstance* pInst, int funcID);
static JILFloat		MathWhiteNoise();
static JILFloat		MathGaussianNoise();

//------------------------------------------------------------------------------
// our main proc
//------------------------------------------------------------------------------
// This is the function you will need to register with the JIL virtual machine.
// Whenever the virtual machine needs to communicate with your native type,
// it will call this proc.

int MathProc(NTLInstance* pInst, int msg, int param, void* pDataIn, void** ppDataOut)
{
	int result = JIL_No_Exception;

	switch( msg )
	{
		// runtime messages
		case NTL_Register:				break;
		case NTL_Initialize:			MathSetRandomSeed((JILDWord)time(NULL)); break;
		case NTL_NewObject:				result = JIL_ERR_Unsupported_Native_Call; break;
		case NTL_MarkHandles:			break;
		case NTL_CallStatic:			return MathCall(pInst, param);
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
// MathCall
//------------------------------------------------------------------------------
// Called when the VM wants to call one of our GLOBAL functions.
// Process parameters on from the stack by calling the NTLGetArg...() functions.
// The argument index indicates the position of an argument on the stack:
// The first argument is at index 0, the second at 1, and so on.
// To return a value, call one of the NTLReturn...() functions.

static int MathCall(NTLInstance* pInst, int funcID)
{
	int result = JIL_No_Exception;
	JILState* ps = NTLInstanceGetVM(pInst);

	switch( funcID )
	{
		case kAbs:
			NTLReturnInt(ps, abs(NTLGetArgInt(ps, 0)));
			break;
		case kAcos:
			NTLReturnFloat(ps, acos(NTLGetArgFloat(ps, 0)));
			break;
		case kAsin:
			NTLReturnFloat(ps, asin(NTLGetArgFloat(ps, 0)));
			break;
		case kAtan:
			NTLReturnFloat(ps, atan(NTLGetArgFloat(ps, 0)));
			break;
		case kAtan2:
			NTLReturnFloat(ps, atan2(NTLGetArgFloat(ps, 0), NTLGetArgFloat(ps, 1)));
			break;
		case kCos:
			NTLReturnFloat(ps, cos(NTLGetArgFloat(ps, 0)));
			break;
		case kCosh:
			NTLReturnFloat(ps, cosh(NTLGetArgFloat(ps, 0)));
			break;
		case kExp:
			NTLReturnFloat(ps, exp(NTLGetArgFloat(ps, 0)));
			break;
		case kFabs:
			NTLReturnFloat(ps, fabs(NTLGetArgFloat(ps, 0)));
			break;
		case kLog:
			NTLReturnFloat(ps, log(NTLGetArgFloat(ps, 0)));
			break;
		case kLog10:
			NTLReturnFloat(ps, log10(NTLGetArgFloat(ps, 0)));
			break;
		case kPow:
			NTLReturnFloat(ps, pow(NTLGetArgFloat(ps, 0), NTLGetArgFloat(ps, 1)));
			break;
		case kSin:
			NTLReturnFloat(ps, sin(NTLGetArgFloat(ps, 0)));
			break;
		case kSinh:
			NTLReturnFloat(ps, sinh(NTLGetArgFloat(ps, 0)));
			break;
		case kTan:
			NTLReturnFloat(ps, tan(NTLGetArgFloat(ps, 0)));
			break;
		case kTanh:
			NTLReturnFloat(ps, tanh(NTLGetArgFloat(ps, 0)));
			break;
		case kSqrt:
			NTLReturnFloat(ps, sqrt(NTLGetArgFloat(ps, 0)));
			break;
		case kCeil:
			NTLReturnFloat(ps, ceil(NTLGetArgFloat(ps, 0)));
			break;
		case kFloor:
			NTLReturnFloat(ps, floor(NTLGetArgFloat(ps, 0)));
			break;
		case kInt:
		{
			JILFloat f = NTLGetArgFloat(ps, 0);
			NTLReturnFloat(ps, (f < 0.0) ? ceil(f) : floor(f) );
			break;
		}
		case kFrac:
		{
			JILFloat f = NTLGetArgFloat(ps, 0);
			JILFloat g = (f < 0.0) ? ceil(f) : floor(f);
			NTLReturnFloat(ps, f - g);
			break;
		}
		case kSgn:
		{
			JILFloat f = NTLGetArgFloat(ps, 0);
			NTLReturnFloat(ps, (f < 0.0) ? -1.0 : 1.0 );
			break;
		}
		case kMin:
		{
			JILLong l1 = NTLGetArgInt(ps, 0);
			JILLong l2 = NTLGetArgInt(ps, 1);
			NTLReturnInt(ps, (l1 < l2) ? l1 : l2);
			break;
		}
		case kMax:
		{
			JILLong l1 = NTLGetArgInt(ps, 0);
			JILLong l2 = NTLGetArgInt(ps, 1);
			NTLReturnInt(ps, (l2 < l1) ? l1 : l2);
			break;
		}
		case kFmin:
		{
			JILFloat f1 = NTLGetArgFloat(ps, 0);
			JILFloat f2 = NTLGetArgFloat(ps, 1);
			NTLReturnFloat(ps, (f1 < f2) ? f1 : f2);
			break;
		}
		case kFmax:
		{
			JILFloat f1 = NTLGetArgFloat(ps, 0);
			JILFloat f2 = NTLGetArgFloat(ps, 1);
			NTLReturnFloat(ps, (f2 < f1) ? f1 : f2);
			break;
		}
		case kRandomSeed:
			MathSetRandomSeed( NTLGetArgInt(ps, 0) );
			break;
		case kRandom:
			NTLReturnInt(ps, MathRandom());
			break;
		case kWhiteNoise:
			NTLReturnFloat(ps, MathWhiteNoise());
			break;
		case kGaussianNoise:
			NTLReturnFloat(ps, MathGaussianNoise());
			break;
		case kLinInterpolation:
		{
			JILFloat n1 = NTLGetArgFloat(ps, 0);
			JILFloat n2 = NTLGetArgFloat(ps, 1);
			JILFloat f  = NTLGetArgFloat(ps, 2);
			NTLReturnFloat(ps, n1 + (n2 - n1) * f);
			break;
		}
		default:
			result = JIL_ERR_Invalid_Function_Index;
			break;
	}
	return result;
}

//------------------------------------------------------------------------------
// Random and White noise generation (musicdsp.org)
//------------------------------------------------------------------------------
// Calculate pseudo-random number based on linear congruential method.
// References: Hal Chamberlain, "Musical Applications of Microprocessors"
// (Posted by Phil Burk)

static JILDWord MathRandSeed = 0;

void MathSetRandomSeed(JILDWord seed){ MathRandSeed = seed; }

JILDWord MathRandom()
{
	MathRandSeed = (MathRandSeed * 196314165) + 907633515;
	return MathRandSeed;
}

static JILFloat MathWhiteNoise()
{
	static const double kMaxRand = 2147483647.0;
	static const double kMinRand = 2147483648.0;

	double rand = (JILLong) MathRandom();
	return (JILFloat) ((rand < 0.0) ? rand / kMinRand : rand / kMaxRand);
}

//------------------------------------------------------------------------------
// Gaussian noise generation
//------------------------------------------------------------------------------
// Well, not really gaussian, but similar. Its more like oversampled noise :)

static JILFloat MathGaussianNoise()
{
	return (MathWhiteNoise() + MathWhiteNoise() + MathWhiteNoise()) / 3.0;
}
