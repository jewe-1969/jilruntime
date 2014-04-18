//------------------------------------------------------------------------------
// File: JCLOption.c                                           (c) 2005 jewe.org
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
/// @file jcloption.c
/// A class for maintaining compiler options, such as enabling warnings,
/// optimizations, and so forth.
//------------------------------------------------------------------------------

#include "jilstdinc.h"

#include "jclstring.h"
#include "jclvar.h"
#include "jcloption.h"
#include "jclfile.h"

//------------------------------------------------------------------------------
// option tokens
//------------------------------------------------------------------------------

enum
{
	opt_verbose = 1,
	opt_warn_enable,
	opt_locals_on_stack,
	opt_optimize_level,
	opt_use_rtchk,
	opt_file_ext,
	opt_file_import,
	opt_error_format,
};

//------------------------------------------------------------------------------
// kOptionTokenList
//------------------------------------------------------------------------------

const JCLToken kOptionTokenList[] =
{
	opt_verbose,			"verbose",
	opt_warn_enable,		"warning-level",
	opt_locals_on_stack,	"stack-locals",
	opt_optimize_level,		"optimize",
	opt_use_rtchk,			"use-rtchk",
	opt_file_ext,			"file-ext",
	opt_file_import,		"file-import",
	opt_error_format,		"error-format",

	0,						NULL
};

//------------------------------------------------------------------------------
// kErrorFormatList
//------------------------------------------------------------------------------

const JCLToken kErrorFormatList[] =
{
	kErrorFormatDefault,	"default",
	kErrorFormatMS,			"ms",

	0,						NULL
};

static const JILChar* kFileNameChars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz";

//------------------------------------------------------------------------------
// static functions
//------------------------------------------------------------------------------

static JILError	SetStdIntValue			(JILLong*, JILLong, JILLong, JCLString*);
static JILError	SetLocalVarMode			(JCLOption*, JCLString*);
static JILError SetFileExt				(JCLOption*, JCLString*);
static JILError SetErrorFormat			(JCLOption*, JCLString*);
static JILError parseOption_JCLOption	(JCLOption*, const JCLString*, JILOptionHandler, JILUnknown*);
static JILError SetOptLevel				(JCLOption*, JCLString*);

//------------------------------------------------------------------------------
// JCLOption
//------------------------------------------------------------------------------
// constructor

void create_JCLOption( JCLOption* _this )
{
	_this->ParseOption = parseOption_JCLOption;

	_this->miUseRTCHK = JILTrue;
	_this->miAllowFileImport = JIL_USE_LOCAL_FILESYS;
	_this->miErrorFormat = kErrorFormatDefault;
	_this->mipFileExt = NEW(JCLString);
	JCLSetString(_this->mipFileExt, "jc");
	_this->mipUsing = NEW( Array_JILLong );

#ifdef _DEBUG
	_this->miWarningLevel = 4;
	_this->miOptimizeLevel = 0;
	_this->miLocalVarMode = kLocalAuto;
	_this->miVerboseEnable = JILTrue;
#else
	_this->miWarningLevel = 3;
	_this->miOptimizeLevel = 3;
	_this->miLocalVarMode = kLocalStack;
	_this->miVerboseEnable = JILFalse;
#endif
}

//------------------------------------------------------------------------------
// JCLOption
//------------------------------------------------------------------------------
// destructor

void destroy_JCLOption( JCLOption* _this )
{
	DELETE(_this->mipFileExt);
	DELETE(_this->mipUsing);
}

//------------------------------------------------------------------------------
// JCLOption::Copy
//------------------------------------------------------------------------------
// Copy all data from a JCLOption to this JCLOption

void copy_JCLOption(JCLOption* _this, const JCLOption* src)
{
	_this->miVerboseEnable = src->miVerboseEnable;
	_this->miWarningLevel = src->miWarningLevel;
	_this->miLocalVarMode = src->miLocalVarMode;
	_this->miOptimizeLevel = src->miOptimizeLevel;
	_this->miUseRTCHK = src->miUseRTCHK;
	_this->miAllowFileImport = src->miAllowFileImport;
	_this->miErrorFormat = src->miErrorFormat;
	_this->mipFileExt->Copy(_this->mipFileExt, src->mipFileExt);
	_this->mipUsing->Copy(_this->mipUsing, src->mipUsing);
}

//------------------------------------------------------------------------------
// JCLOption::ParseOption
//------------------------------------------------------------------------------
// Parse a given option string and set member. The string must have the syntax:
// "option-name=value"

static JILError parseOption_JCLOption(JCLOption* _this, const JCLString* str, JILOptionHandler proc, JILUnknown* user)
{
	JILError err = JCL_No_Error;
	JCLString* pName;
	JCLString* pValue;
	JILLong tokenID;

	pName = NEW(JCLString);
	pValue = NEW(JCLString);
	pValue->Copy(pValue, str);
	JCLSpanExcluding(pValue, "=", pName);
	JCLSeekForward(pValue, 1);
	JCLSubString(pValue, pValue, JCLGetLocator(pValue), JCLGetLength(pValue));
	JCLTrim(pName);
	JCLTrim(pValue);
	JCLSetLocator(pValue, 0);

	// replace true/yes/on by 1 and false/no/off by 0
	JCLReplace(pValue, "true", "1");
	JCLReplace(pValue, "yes", "1");
	JCLReplace(pValue, "on", "1");
	JCLReplace(pValue, "false", "0");
	JCLReplace(pValue, "no", "0");
	JCLReplace(pValue, "off", "0");

	tokenID = GetTokenID(JCLGetString(pName), kOptionTokenList);
	switch( tokenID )
	{
		case opt_verbose:
			err = SetStdIntValue(&_this->miVerboseEnable, 0, 1, pValue);
			break;
		case opt_warn_enable:
			err = SetStdIntValue(&_this->miWarningLevel, 0, 4, pValue);
			break;
		case opt_locals_on_stack:
			err = SetLocalVarMode(_this, pValue);
			break;
		case opt_optimize_level:
			err = SetOptLevel(_this, pValue);
			break;
		case opt_use_rtchk:
			err = SetStdIntValue(&_this->miUseRTCHK, 0, 1, pValue);
			break;
		case opt_file_ext:
			err = SetFileExt(_this, pValue);
			break;
		case opt_file_import:
			err = SetStdIntValue(&_this->miAllowFileImport, 0, 1, pValue);
			break;
		case opt_error_format:
			err = SetErrorFormat(_this, pValue);
			break;
		default:
			err = proc(user, JCLGetString(pName), JCLGetString(pValue));
			break;
	}

	DELETE(pName);
	DELETE(pValue);
	return err;
}

//------------------------------------------------------------------------------
// SetStdIntValue
//------------------------------------------------------------------------------

static JILError SetStdIntValue(JILLong* pResult, JILLong min, JILLong max, JCLString* pValue)
{
	JILError err = JCL_WARN_Invalid_Option_Value;
	JILChar* pStopPos;
	// try to convert value string to a long
	JILLong lValue = strtol(JCLGetString(pValue), &pStopPos, 0);
	if( pStopPos != JCLGetString(pValue) )
	{
		if( lValue >= min && lValue <= max )
		{
			*pResult = lValue;
			err = JCL_No_Error;
		}
	}
	return err;
}

//------------------------------------------------------------------------------
// SetLocalVarMode
//------------------------------------------------------------------------------

static JILError SetLocalVarMode(JCLOption* _this, JCLString* pValue)
{
	JILError err = JCL_WARN_Invalid_Option_Value;
	JILChar* pStopPos;
	// try to convert value string to a long
	JILLong lValue = strtol(JCLGetString(pValue), &pStopPos, 0);
	if( pStopPos != JCLGetString(pValue) )
	{
		if( lValue == 0 || lValue == 1 )
		{
			_this->miLocalVarMode = lValue ? kLocalStack : kLocalAuto;
			err = JCL_No_Error;
		}
	}
	return err;
}

//------------------------------------------------------------------------------
// SetUseRTCHK
//------------------------------------------------------------------------------

static JILError SetUseRTCHK(JCLOption* _this, JILLong lValue, JCLString* pValue)
{
	JILError err = JCL_WARN_Invalid_Option_Value;
	if( lValue == 0 || lValue == 1 )
	{
		_this->miUseRTCHK = lValue;
		err = JCL_No_Error;
	}
	return err;
}

//------------------------------------------------------------------------------
// SetFileExt
//------------------------------------------------------------------------------

static JILError SetFileExt(JCLOption* _this, JCLString* pValue)
{
	JILError err = JCL_WARN_Invalid_Option_Value;
	if( JCLContainsOnly(pValue, kFileNameChars) )
	{
		_this->mipFileExt->Copy(_this->mipFileExt, pValue);
		err = JCL_No_Error;
	}
	return err;
}

//------------------------------------------------------------------------------
// SetErrorFormat
//------------------------------------------------------------------------------

static JILError SetErrorFormat(JCLOption* _this, JCLString* pValue)
{
	JILError err = JCL_WARN_Invalid_Option_Value;
	JILLong formatID = GetTokenID(JCLGetString(pValue), kErrorFormatList);
	if( formatID )
	{
		_this->miErrorFormat = formatID;
		err = JCL_No_Error;
	}
	return err;
}

//------------------------------------------------------------------------------
// SetOptLevel
//------------------------------------------------------------------------------

static JILError SetOptLevel(JCLOption* _this, JCLString* pValue)
{
	JILError err = JCL_WARN_Invalid_Option_Value;
	JILChar* pStopPos;
	// try to convert value string to a long
	JILLong lValue = strtol(JCLGetString(pValue), &pStopPos, 0);
	if( pStopPos != JCLGetString(pValue) )
	{
		if( lValue >= 0 && lValue <= 3 )
		{
			_this->miOptimizeLevel = lValue;
			if( lValue == 3 )
				_this->miLocalVarMode = kLocalStack;
			err = JCL_No_Error;
		}
	}
	return err;
}
