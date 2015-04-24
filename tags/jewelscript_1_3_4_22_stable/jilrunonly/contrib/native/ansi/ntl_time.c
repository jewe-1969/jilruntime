//------------------------------------------------------------------------------
// File: ntl_time.c                                            (c) 2005 jewe.org
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

#include <string.h>
#include "ntl_time.h"
#include <stdio.h>

//------------------------------------------------------------------------------
// function index numbers
//------------------------------------------------------------------------------
// It is important to keep these index numbers in the same order as the function
// declarations in the class declaration string!

enum
{
	kCtor,
	kCctor,
	kCtor2,
	kConv,
	kGetSec,
	kGetMin,
	kGetHour,
	kGetDay,
	kGetDayOfWeek,
	kGetDayOfYear,
	kGetMonth,
	kGetYear,
	kSetSec,
	kSetMin,
	kSetHour,
	kSetDay,
	kSetMonth,
	kSetYear,
	kFormat,
	kToFloat,
	kTickDiff,
	kIsTick,
	kSetDelay,
	kDelayElapsed,
	kDelayTriggered,
	kCurrentTime,
	kLocalTime,
	kDifference,
	kGetTicks
};

//------------------------------------------------------------------------------
// class declaration string
//------------------------------------------------------------------------------

static const char* kClassDeclaration =
	// constructors, convertor
	"method				time();"
	"method				time(const time);"
	"method				time(const float);"
	"method string		convertor();"
	// accessors
	"accessor int		second();"
	"accessor int		minute();"
	"accessor int		hour();"
	"accessor int		day();"
	"accessor int		dayOfWeek();"
	"accessor int		dayOfYear();"
	"accessor int		month();"
	"accessor int		year();"
	"accessor			second(const int);"
	"accessor			minute(const int);"
	"accessor			hour(const int);"
	"accessor			day(const int);"
	"accessor			month(const int);"
	"accessor			year(const int);"
	// methods
	"method string		format(const string);"
	"method float		toFloat();"
	"method int			tickDiff();"
	"method int			isTick(const int ms);"
	"method				setDelay(const int ms);"
	"method int			delayElapsed();"
	"method int			delayTriggered();"
	// global functions
	"function time		currentTime();"
	"function time		localTime();"
	"function float		difference(const time, const time);"
	"function int		getTicks();"
;

//------------------------------------------------------------------------------
// some constants
//------------------------------------------------------------------------------

static const char*	kClassName		=	"time";
static const char*	kAuthorName		=	"www.jewe.org";
static const char*	kAuthorString	=	"A time class for JewelScript.";
static const char*	kTimeStamp		=	"14.12.2006";
static const int	kAuthorVersion	=	0x00000002;

//------------------------------------------------------------------------------
// forward declare static functions
//------------------------------------------------------------------------------

static int TimeNew			(NTLInstance* pInst, NTime** ppObject);
static int TimeDelete		(NTLInstance* pInst, NTime* _this);
static int TimeCallMember	(NTLInstance* pInst, int funcID, NTime* _this);
static int TimeCallStatic	(NTLInstance* pInst, int funcID);

//------------------------------------------------------------------------------
// our main proc
//------------------------------------------------------------------------------
// This is the function you will need to register with the JIL virtual machine.
// Whenever the virtual machine needs to communicate with your native type,
// it will call this proc.

int TimeProc(NTLInstance* pInst, int msg, int param, void* pDataIn, void** ppDataOut)
{
	int result = JIL_No_Exception;

	switch( msg )
	{
		// runtime messages
		case NTL_Register:				break;
		case NTL_Initialize:			break;
		case NTL_NewObject:				return TimeNew(pInst, (NTime**) ppDataOut);
		case NTL_MarkHandles:			break;
		case NTL_CallStatic:			return TimeCallStatic(pInst, param);
		case NTL_CallMember:			return TimeCallMember(pInst, param, (NTime*) pDataIn);
		case NTL_DestroyObject:			return TimeDelete(pInst, (NTime*) pDataIn);
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
// TimeFromTM
//------------------------------------------------------------------------------
// Set the time from an ANSI 'tm' struct.

static void TimeFromTM(NTime* _this, const struct tm* pTime)
{
	memcpy(&(_this->time), pTime, sizeof(struct tm));
}

//------------------------------------------------------------------------------
// TimeNew
//------------------------------------------------------------------------------
// Return a new instance of your class in ppObject.

static int TimeNew(NTLInstance* pInst, NTime** ppObject)
{
	JILState* ps = NTLInstanceGetVM(pInst);
	*ppObject = (NTime*) ps->vmMalloc(ps, sizeof(NTime));
	memset(*ppObject, 0, sizeof(NTime));
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// TimeDelete
//------------------------------------------------------------------------------
// Destroy the instance of your class given in _this

static int TimeDelete(NTLInstance* pInst, NTime* _this)
{
	JILState* ps = NTLInstanceGetVM(pInst);
	ps->vmFree(ps, _this);
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// NTLTimeUpdate
//------------------------------------------------------------------------------
// Update the time structure when one of the members have been set.

void NTLTimeUpdate(NTime* _this)
{
	time_t time = mktime(&_this->time);
	if( time != (time_t) -1 )
		TimeFromTM(_this, localtime(&time));
}

//------------------------------------------------------------------------------
// ClockMsec
//------------------------------------------------------------------------------
// Helper function to return ANSI clock in milliseconds.

static clock_t ClockMsec()
{
	clock_t t = clock();
	JILFloat f = ((JILFloat) t) * (1000.0 / ((JILFloat) CLOCKS_PER_SEC)) + 0.5;
	return (clock_t) f;
}

//------------------------------------------------------------------------------
// TimeCallMember
//------------------------------------------------------------------------------
// Called when the VM wants to call one of the methods of this class.
// Process parameters from the stack by calling the NTLGetArg...() functions.
// The argument index indicates the position of an argument on the stack:
// The first argument is at index 0, the second at 1, and so on.
// To return a value, call one of the NTLReturn...() functions.
// The address of the object (this pointer) is passed in '_this'.

static int TimeCallMember(NTLInstance* pInst, int funcID, NTime* _this)
{
	int result = JIL_No_Exception;
	JILState* ps = NTLInstanceGetVM(pInst);
	char buffer[256];	// TODO: Get rid of this!!
	time_t aTime;
	clock_t cl;
	clock_t interval;

	switch( funcID )
	{
		case kCtor:
			break;
		case kCctor:
		{
			JILHandle* hSrc = NTLGetArgHandle(ps, 0);
			NTime* pSrc = (NTime*)NTLHandleToObject(ps, NTLInstanceTypeID(pInst), hSrc);
			memcpy(_this, pSrc, sizeof(NTime));
			NTLFreeHandle(ps, hSrc);
			NTLTimeUpdate(_this);
			break;
		}
		case kCtor2:
			aTime = (time_t) NTLGetArgFloat(ps, 0);
			TimeFromTM(_this, localtime(&aTime));
			break;
		case kConv:
			// we cannot use asctime() for this, cos it adds a \n to the string :(
			strftime(buffer, 255, "%a %b %d %H:%M:%S %Y", &(_this->time));
			NTLReturnString(ps, buffer);
			break;
		case kGetSec:
			NTLReturnInt(ps, _this->time.tm_sec);
			break;
		case kGetMin:
			NTLReturnInt(ps, _this->time.tm_min);
			break;
		case kGetHour:
			NTLReturnInt(ps, _this->time.tm_hour);
			break;
		case kGetDay:
			NTLReturnInt(ps, _this->time.tm_mday);
			break;
		case kGetDayOfWeek:
			NTLReturnInt(ps, _this->time.tm_wday);
			break;
		case kGetDayOfYear:
			NTLReturnInt(ps, _this->time.tm_yday);
			break;
		case kGetMonth:
			NTLReturnInt(ps, _this->time.tm_mon + 1);
			break;
		case kGetYear:
			NTLReturnInt(ps, _this->time.tm_year + 1900);
			break;
		case kSetSec:
			_this->time.tm_sec = NTLGetArgInt(ps, 0);
			NTLTimeUpdate(_this);
			break;
		case kSetMin:
			_this->time.tm_min = NTLGetArgInt(ps, 0);
			NTLTimeUpdate(_this);
			break;
		case kSetHour:
			_this->time.tm_hour = NTLGetArgInt(ps, 0);
			NTLTimeUpdate(_this);
			break;
		case kSetDay:
			_this->time.tm_mday = NTLGetArgInt(ps, 0);
			NTLTimeUpdate(_this);
			break;
		case kSetMonth:
			_this->time.tm_mon = NTLGetArgInt(ps, 0) - 1;
			NTLTimeUpdate(_this);
			break;
		case kSetYear:
			_this->time.tm_year = NTLGetArgInt(ps, 0) - 1900;
			NTLTimeUpdate(_this);
			break;
		case kFormat:
			strftime(buffer, 255, NTLGetArgString(ps, 0), &(_this->time));
			NTLReturnString(ps, buffer);
			break;
		case kToFloat:
			aTime = mktime(&(_this->time));
			NTLReturnFloat(ps, (JILFloat) aTime);
			break;
		case kTickDiff:
			cl = ClockMsec();
			NTLReturnInt(ps, cl - _this->diffTick);
			_this->diffTick = cl;
			break;
		case kIsTick:
			interval = NTLGetArgInt(ps, 0);
			cl = ClockMsec();
			if( (cl < _this->lastTick) || ((cl - _this->lastTick) >= interval) )
			{
				_this->lastTick = cl;
				NTLReturnInt(ps, JILTrue);
			}
			else
			{
				NTLReturnInt(ps, JILFalse);
			}
			break;
		case kSetDelay:
			_this->lastTick = ClockMsec();
			_this->diffTick = _this->lastTick + NTLGetArgInt(ps, 0);
			break;
		case kDelayElapsed:	// returns false as long as delay time not reached, then continously returns true
			cl = ClockMsec();
			if( cl < _this->lastTick || _this->diffTick == 0 || _this->diffTick < cl )
			{
				_this->diffTick = 0;
				NTLReturnInt(ps, JILTrue);
			}
			else
			{
				NTLReturnInt(ps, JILFalse);
			}
			break;
		case kDelayTriggered:	// returns true ONCE when the delay time is reached, all other times false
			cl = ClockMsec();
			if( cl < _this->lastTick || (_this->diffTick && _this->diffTick < cl) )
			{
				_this->diffTick = 0;
				NTLReturnInt(ps, JILTrue);
			}
			else
			{
				NTLReturnInt(ps, JILFalse);
			}
			break;
		default:
			result = JIL_ERR_Invalid_Function_Index;
			break;
	}
	return result;
}

//------------------------------------------------------------------------------
// TimeCallStatic
//------------------------------------------------------------------------------

static int TimeCallStatic(NTLInstance* pInst, int funcID)
{
	int result = JIL_No_Exception;
	JILState* ps = NTLInstanceGetVM(pInst);
	JILLong thisID = NTLInstanceTypeID(pInst);
	time_t curtime;
	NTime* pTime;
	JILHandle* pHandle;

	switch( funcID )
	{
		case kCurrentTime:
			TimeNew(pInst, &pTime);
			time(&curtime);
			TimeFromTM(pTime, gmtime(&curtime));
			pHandle = NTLNewHandleForObject(ps, thisID, pTime);
			NTLReturnHandle(ps, pHandle);
			NTLFreeHandle(ps, pHandle);
			break;
		case kLocalTime:
			TimeNew(pInst, &pTime);
			time(&curtime);
			TimeFromTM(pTime, localtime(&curtime));
			pHandle = NTLNewHandleForObject(ps, thisID, pTime);
			NTLReturnHandle(ps, pHandle);
			NTLFreeHandle(ps, pHandle);
			break;
		case kDifference:
		{
			JILFloat result = 0.0;
			NTime* pTime1 = NTLGetArgObject(ps, 0, thisID);
			NTime* pTime2 = NTLGetArgObject(ps, 1, thisID);
			if( pTime1 && pTime2 )
			{
				time_t time1 = mktime(&(pTime1->time));
				time_t time2 = mktime(&(pTime2->time));
				result = difftime(time1, time2);
			}
			NTLReturnFloat(ps, result);
			break;
		}
		case kGetTicks:
			NTLReturnInt(ps, ClockMsec());
			break;
		default:
			result = JIL_ERR_Invalid_Function_Index;
			break;
	}
	return result;
}
