//------------------------------------------------------------------------------
// File: ntl_file.h                                            (c) 2005 jewe.org
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

#ifndef NTL_FILE_H
#define NTL_FILE_H

//------------------------------------------------------------------------------
// includes
//------------------------------------------------------------------------------

#include "jilnativetype.h"
#include "jilstring.h"
#include "jilarray.h"

//------------------------------------------------------------------------------
// byteswapping macros
//------------------------------------------------------------------------------

#if JIL_USE_LITTLE_ENDIAN
	#define	SWAPLONGWORD(X)		\
	{\
		unsigned int* p##X = (unsigned int*) &X, a##X;\
		a##X = *p##X;\
		*p##X = ((a##X<<24)|((a##X & 0xff00)<<8)|((a##X & 0xff0000)>>8)|(a##X>>24));\
	}
	#define SWAPQUADWORD(X)		\
	{\
		unsigned int* p##X = (unsigned int*) &X, a##X, b##X;\
		a##X = p##X[0];\
		b##X = p##X[1];\
		p##X[0] = ((b##X<<24)|((b##X & 0xff00)<<8)|((b##X & 0xff0000)>>8)|(b##X>>24));\
		p##X[1] = ((a##X<<24)|((a##X & 0xff00)<<8)|((a##X & 0xff0000)>>8)|(a##X>>24));\
	}
#else
	#define	SWAPLONGWORD(X)
	#define	SWAPQUADWORD(X)
#endif

//------------------------------------------------------------------------------
// class NFile
//------------------------------------------------------------------------------

typedef struct NFile NFile;

struct NFile
{
	JILState*	pState;
	FILE*		pFile;
	JILString*	pPath;
	JILString*	pName;
	JILString*	pType;
	JILLong		mode;
};

// file access modes
enum
{
	kFileModeRead		= 0,
	kFileModeWrite		= 1,
	kFileModeAppend		= 2,
	kFileModeRWExisting	= 3,
	kFileModeRWEmpty	= 4,
	kFileModeRWAppend	= 5,

	kFileModeBinary		= 16	// used as a flag
};

//------------------------------------------------------------------------------
// file functions
//------------------------------------------------------------------------------

// constructor, destructor, copy, assign
void				File_Create			(NFile* _this);
void				File_Create2		(NFile* _this, const JILChar* pFilespec, JILLong mode);
void				File_Destroy		(NFile* _this);
NFile*				File_Clone			(NFile* _this);
void				File_Set			(NFile* _this, const NFile* src);

// file specifier accessors
void				File_GetFilespec	(const NFile* _this, JILString* pResult);
const JILString*	File_GetPath		(const NFile* _this);
const JILString*	File_GetName		(const NFile* _this);
const JILString*	File_GetType		(const NFile* _this);
void				File_SetFilespec	(NFile* _this, const JILString* pFilespec);
void				File_SetPath		(NFile* _this, const JILString* pPath);
void				File_SetName		(NFile* _this, const JILString* pName);
void				File_SetType		(NFile* _this, const JILString* pType);

// file mode accessors
JILLong				File_GetMode		(const NFile* _this);
JILLong				File_SetMode		(NFile* _this, JILLong mode);

// file stream operations
JILLong				File_Open			(NFile* _this);
JILLong				File_Length			(NFile* _this);
FILE*				File_File			(NFile* _this);
JILLong				File_GetPosition	(NFile* _this);
void				File_SetPosition	(NFile* _this, JILLong pos);
JILLong				File_Eof			(NFile* _this);
JILLong				File_ReadTextLine	(NFile* _this, JILString* pString);
JILLong				File_WriteTextLine	(NFile* _this, const JILString* pString);
JILLong				File_ReadText		(NFile* _this, JILString* pString);
JILLong				File_WriteText		(NFile* _this, const JILString* pString);
JILLong				File_GetLong		(NFile* _this, JILLong* ptr);
JILLong				File_PutLong		(NFile* _this, const JILLong* ptr);
JILLong				File_GetFloat		(NFile* _this, JILFloat* ptr);
JILLong				File_PutFloat		(NFile* _this, const JILFloat* ptr);
JILLong				File_GetString		(NFile* _this, JILString* ptr);
JILLong				File_PutString		(NFile* _this, const JILString* ptr);
JILLong				File_Close			(NFile* _this);

// additional operations
JILLong				File_Exists			(NFile* _this);
JILLong				File_Rename			(NFile* _this, const JILChar* pNewFilespec);
JILLong				File_Remove			(NFile* _this);

NFile*				New_File			(JILState* pState);
void				Delete_File			(NFile* _this);

//------------------------------------------------------------------------------
// the type proc
//------------------------------------------------------------------------------

JILEXTERN int FileProc(NTLInstance* pInst, int msg, int param, void* pDataIn, void** ppDataOut);

#endif	// #ifndef NTL_FILE_H
