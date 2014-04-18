//------------------------------------------------------------------------------
// File: ntl_file.c                                            (c) 2005 jewe.org
//------------------------------------------------------------------------------
//
// Description: 
// ------------
//	A file input/output class for the JIL virtual machine.
//
//	Native types are classes or global functions written in C or C++. These
//	classes or functions can be used in the JewelScript language like any other
//	class or function written directly in JewelScript.
//
//	This native type implements a file object, based on the file functions found
//	in the ANSI standard 'stdio.h' include file. Thus, this implementation will
//	be limited to 32-bit file access.
//
//------------------------------------------------------------------------------

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ntl_file.h"
#include "jilstring.h"

//------------------------------------------------------------------------------
// function index numbers
//------------------------------------------------------------------------------
// It is important to keep these index numbers in the same order as the function
// declarations in the class declaration string!

enum
{
	// constructors
	kCtor,
	kCctor,
	kCtorFilespec,
	kConvString,

	// file specifier accessors
	kGetFilespec,
	kGetPath,
	kGetName,
	kGetType,
	kSetFilespec,
	kSetPath,
	kSetName,
	kSetType,

	// file mode accessors
	kGetMode,
	kSetMode,

	// file stream operations
	kOpen,
	kLength,
	kGetPosition,
	kSetPosition,
	kEof,
	kReadTextLine,
	kWriteTextLine,
	kReadText,
	kWriteText,
	kGetInt,
	kPutInt,
	kGetFloat,
	kPutFloat,
	kGetString,
	kPutString,
	kClose,

	// other operations
	kExists,
	kRename,
	kRemove
};

//------------------------------------------------------------------------------
// class declaration string
//------------------------------------------------------------------------------

static const char* kClassDeclaration =
	// constructors, convertor
	"method				file();"
	"method				file(const file);"
	"method				file(const string filespec, const int mode);"
	"method string		convertor();"
	// filespec accessors
	"accessor string	fileSpec();"
	"accessor string	path();"
	"accessor string	name();"
	"accessor string	type();"
	"accessor			fileSpec(const string filespec);"
	"accessor			path(const string path);"
	"accessor			name(const string name);"
	"accessor			type(const string type);"
	// file mode accessors
	"accessor int		mode();"
	"accessor			mode(const int mode);"
	// file stream operations
	"method	int			open();"
	"method	int			length();"
	"method	int			getPosition();"
	"method				setPosition(const int pos);"
	"method	int			eof();"
	"method	string		readTextLine();"
	"method	int			writeTextLine(const string text);"
	"method	string		readText();"
	"method	int			writeText(const string text);"
	"method	int			getInt();"
	"method	int			putInt(const int value);"
	"method	float		getFloat();"
	"method	int			putFloat(const float value);"
	"method	string		getString();"
	"method	int			putString(const string value);"
	"method	int			close();"
	// other operations
	"method	int			exists();"
	"method	int			rename(const string newFilespec);"
	"method	int			remove();"
;

//------------------------------------------------------------------------------
// some constants
//------------------------------------------------------------------------------

static const char*	kClassName		=	"file";
static const char*	kAuthorName		=	"www.jewe.org";
static const char*	kAuthorString	=	"A file input/output class for JewelScript.";
static const char*	kTimeStamp		=	"08.10.2005";
static const int	kAuthorVersion	=	0x00000004;

static const int	kLineFeed		=	10;
static const int	kReturn			=	13;
static const int	kFileBufferSize	=	32768;

//------------------------------------------------------------------------------
// forward declare static functions
//------------------------------------------------------------------------------

static int FileGetDecl	(void* pDataIn);
static int FileNew		(NTLInstance* pInst, NFile** ppObject);
static int FileDelete	(NTLInstance* pInst, NFile* _this);
static int FileCall		(NTLInstance* pInst, int funcID, NFile* _this);
static void File_CopyCtor (NFile*, const NFile*);

//------------------------------------------------------------------------------
// our main proc
//------------------------------------------------------------------------------
// This is the function you will need to register with the JIL virtual machine.
// Whenever the virtual machine needs to communicate with your native type,
// it will call this proc.

int FileProc(NTLInstance* pInst, int msg, int param, void* pDataIn, void** ppDataOut)
{
	int result = JIL_No_Exception;

	switch( msg )
	{
		// runtime messages
		case NTL_Register:				break;
		case NTL_Initialize:			break;
		case NTL_NewObject:				return FileNew(pInst, (NFile**) ppDataOut);
		case NTL_MarkHandles:			break;
		case NTL_CallStatic:			break;
		case NTL_CallMember:			return FileCall(pInst, param, (NFile*) pDataIn);
		case NTL_DestroyObject:			return FileDelete(pInst, (NFile*) pDataIn);
		case NTL_Terminate:				break;
		case NTL_Unregister:			break;

		// class information queries
		case NTL_GetInterfaceVersion:	return NTLRevisionToLong(JIL_TYPE_INTERFACE_VERSION);
		case NTL_GetAuthorVersion:		return kAuthorVersion;
		case NTL_GetClassName:			(*(const char**) ppDataOut) = kClassName; break;
		case NTL_GetDeclString:			return FileGetDecl(pDataIn);
		case NTL_GetBuildTimeStamp:		(*(const char**) ppDataOut) = kTimeStamp; break;
		case NTL_GetAuthorName:			(*(const char**) ppDataOut) = kAuthorName; break;
		case NTL_GetAuthorString:		(*(const char**) ppDataOut) = kAuthorString; break;

		default:						result = JIL_ERR_Unsupported_Native_Call; break;
	}
	return result;
}

//------------------------------------------------------------------------------
// FileGetDecl
//------------------------------------------------------------------------------
// Dynamically build our file class declaration.

static int FileGetDecl(void* pDataIn)
{
	// add the static part of the class declaration
	NTLDeclareVerbatim(pDataIn, kClassDeclaration);
	// add constants
	NTLDeclareConstantInt(pDataIn, type_int, "kRead",		kFileModeRead);
	NTLDeclareConstantInt(pDataIn, type_int, "kWrite",		kFileModeWrite);
	NTLDeclareConstantInt(pDataIn, type_int, "kAppend",		kFileModeAppend);
	NTLDeclareConstantInt(pDataIn, type_int, "kRWExisting",	kFileModeRWExisting);
	NTLDeclareConstantInt(pDataIn, type_int, "kRWEmpty",	kFileModeRWEmpty);
	NTLDeclareConstantInt(pDataIn, type_int, "kRWAppend",	kFileModeRWAppend);
	NTLDeclareConstantInt(pDataIn, type_int, "kBinary",		kFileModeBinary);
	// done
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// FileNew
//------------------------------------------------------------------------------
// Return a new instance of your class in ppObject.

static int FileNew(NTLInstance* pInst, NFile** ppObject)
{
	*ppObject = New_File( NTLInstanceGetVM(pInst) );
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// FileDelete
//------------------------------------------------------------------------------
// Destroy the instance of your class given in pData

static int FileDelete(NTLInstance* pInst, NFile* _this)
{
	Delete_File( _this );
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// FileCall
//------------------------------------------------------------------------------
// Called when the VM wants to call one of the methods of this class.
// Process parameters on from the stack by calling the NTLGetArg...() functions.
// The argument index indicates the position of an argument on the stack:
// The first argument is at index 0, the second at 1, and so on.
// To return a value, call one of the NTLReturn...() functions.
// The address of the object (this pointer) is passed in '_this'.

static int FileCall(NTLInstance* pInst, int funcID, NFile* _this)
{
	int result = JIL_No_Exception;
	JILState* ps = NTLInstanceGetVM(pInst);
	JILHandle* hStr;
	JILHandle* hLong;
	JILHandle* hFloat;
	JILLong* pLong;
	JILFloat* pFloat;
	JILLong gLong;
	JILFloat gFloat;
	JILString* pStr;
	const JILString* pCStr;
	switch( funcID )
	{
		case kCtor:
			File_Create(_this);
			break;
		case kCctor:
		{
			JILHandle* hSrc = NTLGetArgHandle(ps, 0);
			NFile* pSrc = (NFile*)NTLHandleToObject(ps, NTLInstanceTypeID(pInst), hSrc);
			File_CopyCtor(_this, pSrc);
			NTLFreeHandle(ps, hSrc);
			break;
		}
		case kCtorFilespec:
			File_Create2(_this,
				NTLGetArgString(ps, 0),
				NTLGetArgInt(ps, 1));
			break;
		case kConvString:
		case kGetFilespec:
			pStr = JILString_New(ps);
			File_GetFilespec(_this, pStr);
			NTLReturnString(ps, JILString_String(pStr));
			JILString_Delete(pStr);
			break;
		case kGetPath:
			pCStr = File_GetPath(_this);
			NTLReturnString(ps, JILString_String(pCStr));
			break;
		case kGetName:
			pCStr = File_GetName(_this);
			NTLReturnString(ps, JILString_String(pCStr));
			break;
		case kGetType:
			pCStr = File_GetType(_this);
			NTLReturnString(ps, JILString_String(pCStr));
			break;
		case kSetFilespec:
			hStr = NTLGetArgHandle(ps, 0);
			pCStr = (const JILString*) NTLHandleToObject(ps, type_string, hStr);
			File_SetFilespec(_this, pCStr);
			NTLFreeHandle(ps, hStr);
			break;
		case kSetPath:
			hStr = NTLGetArgHandle(ps, 0);
			pCStr = (const JILString*) NTLHandleToObject(ps, type_string, hStr);
			File_SetPath(_this, pCStr);
			NTLFreeHandle(ps, hStr);
			break;
		case kSetName:
			hStr = NTLGetArgHandle(ps, 0);
			pCStr = (const JILString*) NTLHandleToObject(ps, type_string, hStr);
			File_SetName(_this, pCStr);
			NTLFreeHandle(ps, hStr);
			break;
		case kSetType:
			hStr = NTLGetArgHandle(ps, 0);
			pCStr = (const JILString*) NTLHandleToObject(ps, type_string, hStr);
			File_SetType(_this, pCStr);
			NTLFreeHandle(ps, hStr);
			break;
		case kGetMode:
			NTLReturnInt(ps, File_GetMode(_this));
			break;
		case kSetMode:
			File_SetMode(_this, NTLGetArgInt(ps, 0));
			break;
		case kOpen:
			NTLReturnInt(ps, File_Open(_this));
			break;
		case kLength:
			NTLReturnInt(ps, File_Length(_this));
			break;
		case kGetPosition:
			NTLReturnInt(ps, File_GetPosition(_this));
			break;
		case kSetPosition:
			File_SetPosition(_this, NTLGetArgInt(ps, 0));
			break;
		case kEof:
			NTLReturnInt(ps, File_Eof(_this));
			break;
		case kReadTextLine:
			pStr = JILString_New(ps);
			File_ReadTextLine(_this, pStr);
			hStr = NTLNewHandleForObject(ps, type_string, pStr);
			NTLReturnHandle(ps, hStr);
			NTLFreeHandle(ps, hStr);
			break;
		case kWriteTextLine:
			hStr = NTLGetArgHandle(ps, 0);
			pCStr = (const JILString*) NTLHandleToObject(ps, type_string, hStr);
			NTLReturnInt(ps, File_WriteTextLine(_this, pCStr));
			NTLFreeHandle(ps, hStr);
			break;
		case kReadText:
			pStr = JILString_New(ps);
			File_ReadText(_this, pStr);
			hStr = NTLNewHandleForObject(ps, type_string, pStr);
			NTLReturnHandle(ps, hStr);
			NTLFreeHandle(ps, hStr);
			break;
		case kWriteText:
			hStr = NTLGetArgHandle(ps, 0);
			pCStr = (const JILString*) NTLHandleToObject(ps, type_string, hStr);
			NTLReturnInt(ps, File_WriteText(_this, pCStr));
			NTLFreeHandle(ps, hStr);
			break;
		case kGetInt:
			gLong = 0;
			File_GetLong(_this, &gLong);
			NTLReturnInt(ps, gLong);
			break;
		case kPutInt:
			hLong = NTLGetArgHandle(ps, 0);
			pLong = (JILLong*) NTLHandleToObject(ps, type_int, hLong);
			NTLReturnInt(ps, File_PutLong(_this, pLong));
			NTLFreeHandle(ps, hLong);
			break;
		case kGetFloat:
			gFloat = 0;
			File_GetFloat(_this, &gFloat);
			NTLReturnFloat(ps, gFloat);
			break;
		case kPutFloat:
			hFloat = NTLGetArgHandle(ps, 0);
			pFloat = (JILFloat*) NTLHandleToObject(ps, type_float, hFloat);
			NTLReturnInt(ps, File_PutFloat(_this, pFloat));
			NTLFreeHandle(ps, hFloat);
			break;
		case kGetString:
			pStr = JILString_New(ps);
			File_GetString(_this, pStr);
			hStr = NTLNewHandleForObject(ps, type_string, pStr);
			NTLReturnHandle(ps, hStr);
			NTLFreeHandle(ps, hStr);
			break;
		case kPutString:
			hStr = NTLGetArgHandle(ps, 0);
			pStr = (JILString*) NTLHandleToObject(ps, type_string, hStr);
			NTLReturnInt(ps, File_PutString(_this, pStr));
			NTLFreeHandle(ps, hStr);
			break;
		case kClose:
			NTLReturnInt(ps, File_Close(_this));
			break;
		case kExists:
			NTLReturnInt(ps, File_Exists(_this));
			break;
		case kRename:
			NTLReturnInt(ps, File_Rename(_this, NTLGetArgString(ps, 0)));
			break;
		case kRemove:
			NTLReturnInt(ps, File_Remove(_this));
			break;
		default:
			result = JIL_ERR_Invalid_Function_Index;
			break;
	}
	return result;
}

//------------------------------------------------------------------------------
// Create
//------------------------------------------------------------------------------
// 

void File_Create (NFile* _this)
{
	_this->pFile = NULL;
	_this->pPath = JILString_New(_this->pState);
	_this->pName = JILString_New(_this->pState);
	_this->pType = JILString_New(_this->pState);
	_this->mode = 0;
}

//------------------------------------------------------------------------------
// Create2
//------------------------------------------------------------------------------
// 

void File_Create2 (NFile* _this, const JILChar* pFilespec, JILLong mode)
{
	JILString* pTemp;

	_this->pFile = NULL;
	_this->pPath = JILString_New(_this->pState);
	_this->pName = JILString_New(_this->pState);
	_this->pType = JILString_New(_this->pState);
	_this->mode = mode;

	pTemp = JILString_New(_this->pState);
	JILString_Assign(pTemp, pFilespec);
	File_SetFilespec(_this, pTemp);
	JILString_Delete(pTemp);
}

//------------------------------------------------------------------------------
// Destroy
//------------------------------------------------------------------------------
// 

void File_Destroy (NFile* _this)
{
	File_Close(_this);
	JILString_Delete(_this->pPath);
	_this->pPath = NULL;
	JILString_Delete(_this->pName);
	_this->pName = NULL;
	JILString_Delete(_this->pType);
	_this->pType = NULL;
}

//------------------------------------------------------------------------------
// File_CopyCtor
//------------------------------------------------------------------------------
// 

void File_CopyCtor (NFile* _this, const NFile* src)
{
	JILString* pTemp = JILString_New(src->pState);
	File_GetFilespec(src, pTemp);
	File_Create2(_this, JILString_String(pTemp), src->mode);
	JILString_Delete(pTemp);
}

//------------------------------------------------------------------------------
// GetFilespec
//------------------------------------------------------------------------------
// 

void File_GetFilespec (const NFile* _this, JILString* pResult)
{
	JILString_Clear(pResult);
	if( JILString_Length(_this->pPath) )
	{
		JILString_Append(pResult, _this->pPath);
		JILString_InsChr(pResult, JIL_PATHSEPARATOR, JILString_Length(pResult));
	}
	JILString_Append(pResult, _this->pName);
	JILString_InsChr(pResult, '.', JILString_Length(pResult));
	JILString_Append(pResult, _this->pType);
}

//------------------------------------------------------------------------------
// GetPath
//------------------------------------------------------------------------------
// 

const JILString* File_GetPath (const NFile* _this)
{
	return _this->pPath;
}

//------------------------------------------------------------------------------
// GetName
//------------------------------------------------------------------------------
// 

const JILString* File_GetName (const NFile* _this)
{
	return _this->pName;
}

//------------------------------------------------------------------------------
// GetType
//------------------------------------------------------------------------------
// 

const JILString* File_GetType (const NFile* _this)
{
	return _this->pType;
}

//------------------------------------------------------------------------------
// SetFilespec
//------------------------------------------------------------------------------
// 

void File_SetFilespec (NFile* _this, const JILString* pFilespec)
{
	JILLong pos;

	// find last separator in filespec and cut out path
	pos = JILString_FindCharR(pFilespec, JIL_PATHSEPARATOR, JILString_Length(pFilespec));
	if( pos >= 0 )
	{
		JILString_SubStr(_this->pPath, pFilespec, 0, pos);
	}
	else
	{
		JILString_Clear(_this->pPath);
	}

	// everything following separator goes into name
	JILString_SubStr(_this->pName, pFilespec, pos + 1, JILString_Length(pFilespec)); // length is clipped ;)

	// find last decimal point in name and cut out type (extension)
	pos = JILString_FindCharR(_this->pName, '.', JILString_Length(_this->pName));
	if( pos >= 0 )
	{
		JILString_SubStr(_this->pType, _this->pName, pos + 1, JILString_Length(_this->pName));
		JILString_SubStr(_this->pName, _this->pName, 0, pos);
	}
	else
	{
		JILString_Clear(_this->pType);
	}
}

//------------------------------------------------------------------------------
// SetPath
//------------------------------------------------------------------------------
// 

void File_SetPath (NFile* _this, const JILString* pPath)
{
	JILString_Set(_this->pPath, pPath);
}

//------------------------------------------------------------------------------
// SetName
//------------------------------------------------------------------------------
// 

void File_SetName (NFile* _this, const JILString* pName)
{
	JILString_Set(_this->pName, pName);
}

//------------------------------------------------------------------------------
// SetType
//------------------------------------------------------------------------------
// 

void File_SetType (NFile* _this, const JILString* pType)
{
	JILString_Set(_this->pType, pType);
}

//------------------------------------------------------------------------------
// GetMode
//------------------------------------------------------------------------------
// 

JILLong File_GetMode (const NFile* _this)
{
	return _this->mode;
}

//------------------------------------------------------------------------------
// SetMode
//------------------------------------------------------------------------------
// 

JILLong File_SetMode (NFile* _this, JILLong mode)
{
	_this->mode = mode;
	return 0;
}

//------------------------------------------------------------------------------
// Open
//------------------------------------------------------------------------------
// 

JILLong File_Open (NFile* _this)
{
	JILLong result = -1;
	char modeStr[4] = {0, 0, 0, 0};
	char* pMode = modeStr;
	JILString* pFilespec = JILString_New(_this->pState);

	// check access mode
	switch( _this->mode & 0x0f )
	{
		case kFileModeRead:
			*pMode++ = 'r';
			break;
		case kFileModeWrite:
			*pMode++ = 'w';
			break;
		case kFileModeAppend:
			*pMode++ = 'a';
			break;
		case kFileModeRWExisting:
			*pMode++ = 'r';
			*pMode++ = '+';
			break;
		case kFileModeRWEmpty:
			*pMode++ = 'w';
			*pMode++ = '+';
			break;
		case kFileModeRWAppend:
			*pMode++ = 'a';
			*pMode++ = '+';
			break;
		default:
			JILString_Delete(pFilespec);
			return -1;
	}
	// check binary or text mode
	if( _this->mode & kFileModeBinary )
		*pMode++ = 'b';
	else
		*pMode++ = 't';
	// get filespec
	File_GetFilespec(_this, pFilespec);
	// try to open the file
	_this->pFile = fopen(JILString_String(pFilespec), modeStr);
	if( _this->pFile )
		result = 0;	// success
	JILString_Delete(pFilespec);
	return result;
}

//------------------------------------------------------------------------------
// Length
//------------------------------------------------------------------------------
// 

JILLong File_Length (NFile* _this)
{
	fpos_t pos;
	JILLong result = -1;
	if( _this->pFile )	// file must be open
	{
		fgetpos(_this->pFile, &pos);
		if( !fseek(_this->pFile, 0, SEEK_END) )
		{
			result = ftell(_this->pFile);
		}
		fsetpos(_this->pFile, &pos);
	}
	return result;
}

//------------------------------------------------------------------------------
// File
//------------------------------------------------------------------------------
// 

FILE* File_File (NFile* _this)
{
	return _this->pFile;
}

//------------------------------------------------------------------------------
// GetPosition
//------------------------------------------------------------------------------
// NOTE: Getting / setting position does not work reliably in text-mode on
// windows PC. Therefore it cannot be used to saving/restoring read position in
// textfiles. (Same applies to fgetpos / fsetpos)

JILLong File_GetPosition (NFile* _this)
{
	JILLong result = 0;
	if( _this->pFile )
	{
		result = ftell(_this->pFile);
	}
	return result;
}

//------------------------------------------------------------------------------
// SetPosition
//------------------------------------------------------------------------------
// NOTE: Getting / setting position does not work reliably in text-mode on
// windows PC. Therefore it cannot be used to saving/restoring read position in
// textfiles. (Same applies to fgetpos / fsetpos)

void File_SetPosition (NFile* _this, JILLong pos)
{
	if( _this->pFile )
	{
		// we clip everything greater than 2GB...
		if( pos >= 0 )
		{
			fseek(_this->pFile, pos, SEEK_SET);
		}
	}
}

//------------------------------------------------------------------------------
// Eof
//------------------------------------------------------------------------------
// 

JILLong File_Eof (NFile* _this)
{
	if( _this->pFile && feof(_this->pFile) != 0 )
		return 1;
	else
		return 0;
}

//------------------------------------------------------------------------------
// ReadTextLine
//------------------------------------------------------------------------------
// 

JILLong File_ReadTextLine (NFile* _this, JILString* pString)
{
	JILLong result = -1;
	if( _this->pFile )
	{
		if( !(_this->mode & kFileModeBinary) )
		{
			// allocate a worst case buffer to load the string
			char* buffer;
			char* pos;
			buffer = (char*) malloc(kFileBufferSize);
			// load a line
			if( fgets(buffer, kFileBufferSize, _this->pFile) )
			{
				pos = strchr(buffer, kReturn);
				if( pos )
					*pos = 0;
				pos = strchr(buffer, kLineFeed);
				if( pos )
					*pos = 0;
				JILString_Assign(pString, buffer);
				result = 0;
			}
			else if( File_Eof(_this) )
			{
				JILString_Clear(pString);
				result = 0;
			}
			free( buffer );
		}
	}
	return result;
}

//------------------------------------------------------------------------------
// WriteTextLine
//------------------------------------------------------------------------------
// 

JILLong File_WriteTextLine (NFile* _this, const JILString* pString)
{
	JILLong result = -1;
	if( _this->pFile )
	{
		if( !(_this->mode & kFileModeBinary) )
		{
			if( fputs(JILString_String(pString), _this->pFile) >= 0 )
			{
				fputs("\n", _this->pFile);
				result = 0;
			}
		}
	}
	return result;
}

//------------------------------------------------------------------------------
// ReadText
//------------------------------------------------------------------------------
// 

JILLong File_ReadText (NFile* _this, JILString* pString)
{
	JILLong result = -1;
	if( _this->pFile )
	{
		// we can handle binary and text modes equally
		JILLong size;
		size = File_Length(_this) - File_GetPosition(_this);
		JILString_Fill(pString, 32, size);
		size = fread(pString->string, 1, size, _this->pFile);
		// must update string size
		pString->length = size;
		pString->string[size] = 0;
		result = 0;
	}
	return result;
}

//------------------------------------------------------------------------------
// WriteText
//------------------------------------------------------------------------------
// 

JILLong File_WriteText (NFile* _this, const JILString* pString)
{
	JILLong result = -1;
	if( _this->pFile )
	{
		if( _this->mode & kFileModeBinary )
		{
			JILLong size = JILString_Length(pString);
			fwrite(JILString_String(pString), 1, size, _this->pFile);
			result = 0;
		}
		else
		{
			if( fputs(JILString_String(pString), _this->pFile) >= 0 )
				result = 0;
		}
	}
	return result;
}

//------------------------------------------------------------------------------
// GetLong
//------------------------------------------------------------------------------
// 

JILLong File_GetLong (NFile* _this, JILLong* ptr)
{
	JILLong result = -1;
	if( _this->pFile && _this->mode & kFileModeBinary )
	{
		unsigned int buffer;
		fread(&buffer, 1, sizeof(int), _this->pFile);
		SWAPLONGWORD( buffer )
		*ptr = buffer;
		result = 0;
	}
	return result;
}

//------------------------------------------------------------------------------
// PutLong
//------------------------------------------------------------------------------
// 

JILLong File_PutLong (NFile* _this, const JILLong* ptr)
{
	JILLong result = -1;
	if( _this->pFile && _this->mode & kFileModeBinary )
	{
		unsigned int buffer = *ptr;
		SWAPLONGWORD( buffer )
		fwrite(&buffer, 1, sizeof(int), _this->pFile);
		result = 0;
	}
	return result;
}

//------------------------------------------------------------------------------
// GetFloat
//------------------------------------------------------------------------------
// 

JILLong File_GetFloat (NFile* _this, JILFloat* ptr)
{
	JILLong result = -1;
	if( _this->pFile && _this->mode & kFileModeBinary )
	{
		JILFloat buffer;
		fread(&buffer, 1, sizeof(JILFloat), _this->pFile);
		if( sizeof(JILFloat) == sizeof(double) )
		{
			SWAPQUADWORD( buffer )
		}
		else
		{
			SWAPLONGWORD( buffer )
		}
		*ptr = buffer;
		result = 0;
	}
	return result;
}

//------------------------------------------------------------------------------
// PutFloat
//------------------------------------------------------------------------------
// 

JILLong File_PutFloat (NFile* _this, const JILFloat* ptr)
{
	JILLong result = -1;
	if( _this->pFile && _this->mode & kFileModeBinary )
	{
		JILFloat buffer = *ptr;
		if( sizeof(JILFloat) == sizeof(double) )
		{
			SWAPQUADWORD( buffer )
		}
		else
		{
			SWAPLONGWORD( buffer )
		}
		fwrite(&buffer, 1, sizeof(JILFloat), _this->pFile);
		result = 0;
	}
	return result;
}

//------------------------------------------------------------------------------
// GetString
//------------------------------------------------------------------------------
// 

JILLong File_GetString (NFile* _this, JILString* ptr)
{
	JILLong result = -1;
	if( _this->pFile && _this->mode & kFileModeBinary )
	{
		// load length
		JILLong length;
		result = File_GetLong(_this, &length);
		if( !result )
		{
			JILString_Fill(ptr, 32, length);
			// load string
			length = fread(ptr->string, 1, length, _this->pFile);
			// must update string size
			ptr->length = length;
			ptr->string[length] = 0;
		}
	}
	return result;
}

//------------------------------------------------------------------------------
// PutString
//------------------------------------------------------------------------------
// 

JILLong File_PutString (NFile* _this, const JILString* ptr)
{
	JILLong result = -1;
	if( _this->pFile && _this->mode & kFileModeBinary )
	{
		// save length
		result = File_PutLong(_this, &ptr->length);
		if( !result )
		{
			// save string
			fwrite(ptr->string, 1, ptr->length, _this->pFile);
		}
	}
	return result;
}

//------------------------------------------------------------------------------
// Close
//------------------------------------------------------------------------------
// 

JILLong File_Close (NFile* _this)
{
	if( _this->pFile )
	{
		fclose(_this->pFile);
		_this->pFile = NULL;
	}
	return 0;
}

//------------------------------------------------------------------------------
// Exists
//------------------------------------------------------------------------------
// 

JILLong File_Exists (NFile* _this)
{
	JILLong result = 0;
	if( !File_Open(_this) )
	{
		File_Close(_this);
		result = 1;
	}
	return result;
}

//------------------------------------------------------------------------------
// Rename
//------------------------------------------------------------------------------
// 

JILLong File_Rename (NFile* _this, const JILChar* pNewFilespec)
{
	JILLong result = -1;
	JILString* oldFilespec = JILString_New(_this->pState);
	JILString* newFilespec = JILString_New(_this->pState);
	JILString_Assign(newFilespec, pNewFilespec);
	File_GetFilespec(_this, oldFilespec);
	File_SetFilespec(_this, newFilespec);
	File_GetFilespec(_this, newFilespec);
	result = rename( JILString_String(oldFilespec), JILString_String(newFilespec));
	if( result )
	{
		// rename was not successful, restore old filespec
		File_SetFilespec(_this, oldFilespec);
	}
	JILString_Delete(newFilespec);
	JILString_Delete(oldFilespec);
	return result;
}

//------------------------------------------------------------------------------
// Remove
//------------------------------------------------------------------------------
// 

JILLong File_Remove (NFile* _this)
{
	JILLong result = -1;
	JILString* pTemp = JILString_New(_this->pState);
	File_GetFilespec(_this, pTemp);
	result = remove( JILString_String(pTemp) );
	JILString_Delete(pTemp);
	return result;
}

//------------------------------------------------------------------------------
// New_File
//------------------------------------------------------------------------------
// Allocate an instance of class NFile. Constructor function will be called
// separately.

NFile* New_File(JILState* pState)
{
	NFile* _this = (NFile*) pState->vmMalloc(pState, sizeof(NFile));
	_this->pState = pState;
	return _this;
}

//------------------------------------------------------------------------------
// Delete_File
//------------------------------------------------------------------------------
// Destroy an instance of class NFile. Destructor function is called
// automatically.

void Delete_File(NFile* _this)
{
	File_Destroy(_this);
	_this->pState->vmFree( _this->pState, _this );
}
