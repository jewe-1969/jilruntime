//------------------------------------------------------------------------------
// File: JILNativeType.c                                    (c) 2003 jewe.org
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
//	Definitions for custom built type libraries implemented in C/C++.
//------------------------------------------------------------------------------

#include "jilstdinc.h"

#include "jiltypes.h"
#include "jilhandle.h"
#include "jiltools.h"
#include "jilallocators.h"
#include "jilcallntl.h"
#include "jilcodelist.h"
#include "jiltable.h"
#include "jclstring.h"
#include "jilmachine.h"

//------------------------------------------------------------------------------
// externals
//------------------------------------------------------------------------------

JILError JCLCreateType(JCLState*, const JILChar*, JILLong, JILTypeFamily, JILBool, JILLong*);
JILError JILDynamicConvert(JILState*, JILLong, JILHandle*, JILHandle**);

//------------------------------------------------------------------------------
// constants
//------------------------------------------------------------------------------

const JILLong kLoadResourceChunkSize = 1024 * 64;	// load files in chunks of 64K

//------------------------------------------------------------------------------
// get_stack_handle
//------------------------------------------------------------------------------

JILINLINE JILHandle* get_stack_handle(JILState* pState, JILLong argNum)
{
	return pState->vmpContext->vmppDataStack[pState->vmpContext->vmDataStackPointer + argNum];
}

//------------------------------------------------------------------------------
// NTLRevisionToLong
//------------------------------------------------------------------------------

JILLong NTLRevisionToLong(const JILChar* pRevision)
{
	return JILRevisionToLong(pRevision);
}

//------------------------------------------------------------------------------
// NTLIsValidTypeID
//------------------------------------------------------------------------------

JILBool NTLIsValidTypeID(JILState* ps, JILLong type)
{
	return (type >= 0 && type < ps->vmUsedTypeInfoSegSize);
}

//------------------------------------------------------------------------------
// NTLGetTypeIDForClass
//------------------------------------------------------------------------------

JILLong NTLGetTypeIDForClass(JILState* pState, const JILChar* pClassName)
{
	int i;
	JILTypeInfo* pTypeInfo;
	JILChar* pName;
	JILLong result = 0;

	// iterate through all typeinfo entries in the TypeInfo segment and check all classes
	for( i = 0; i < pState->vmUsedTypeInfoSegSize; i++ )
	{
		pTypeInfo = JILTypeInfoFromType(pState, i);
		if( pTypeInfo->family == tf_class )
		{
			pName = JILCStrGetString(pState, pTypeInfo->offsetName);
			if( strcmp(pName, pClassName) == 0 )
			{
				result = i;
				break;
			}
		}
	}
	return result;
}

//------------------------------------------------------------------------------
// NTLTypeNameToTypeID
//------------------------------------------------------------------------------

JILLong NTLTypeNameToTypeID(JILState* pState, const JILChar* pTypeName)
{
	int i;
	JILTypeInfo* pTypeInfo;
	JILChar* pName;
	JILLong typeId = 0;

	// if we don't have a table yet, create it!
	if( !pState->vmpTypeInfoTable )
	{
		pState->vmpTypeInfoTable = JILTable_NewNativeUnmanaged(pState);
		// iterate through all typeinfo entries in the TypeInfo segment and add them to the table
		for( i = 0; i < pState->vmUsedTypeInfoSegSize; i++ )
		{
			pTypeInfo = JILTypeInfoFromType(pState, i);
			pName = JILCStrGetString(pState, pTypeInfo->offsetName);
			JILTable_SetItem(pState->vmpTypeInfoTable, pName, pTypeInfo);
		}
	}
	// get the type info from the table
	pTypeInfo = JILTable_GetItem(pState->vmpTypeInfoTable, pTypeName);
	if( pTypeInfo )
		typeId = pTypeInfo->type;
	return typeId;
}

//------------------------------------------------------------------------------
// NTLGetTypeName
//------------------------------------------------------------------------------

const JILChar* NTLGetTypeName(JILState* pState, JILLong type)
{
	return JILGetHandleTypeName(pState, type);
}

//------------------------------------------------------------------------------
// NTLGetTypeFamily
//------------------------------------------------------------------------------

JILLong NTLGetTypeFamily(JILState* pState, JILLong type)
{
	JILLong family = tf_undefined;
	JILTypeInfo* pTypeInfo = JILTypeInfoFromType(pState, type);
	if( pTypeInfo )
	{
		family = pTypeInfo->family;
	}
	return family;
}

//------------------------------------------------------------------------------
// NTLGetArgTypeID
//------------------------------------------------------------------------------

JILLong NTLGetArgTypeID(JILState* pState, JILLong argNum)
{
	JILHandle* pHandle = get_stack_handle(pState, argNum);
	return pHandle->type;
}

//------------------------------------------------------------------------------
// NTLGetArgInt
//------------------------------------------------------------------------------

JILLong NTLGetArgInt(JILState* pState, JILLong argNum)
{
	JILHandle* pHandle;
	JILLong result = 0;
	pHandle = get_stack_handle(pState, argNum);
	if( pHandle->type == type_int )
	{
		result = JILGetIntHandle(pHandle)->l;
	}
	return result;
}

//------------------------------------------------------------------------------
// NTLGetArgFloat
//------------------------------------------------------------------------------

JILFloat NTLGetArgFloat(JILState* pState, JILLong argNum)
{
	JILHandle* pHandle;
	JILFloat result = 0.0;
	pHandle = get_stack_handle(pState, argNum);
	if( pHandle->type == type_float )
	{
		result = JILGetFloatHandle(pHandle)->f;
	}
	return result;
}

//------------------------------------------------------------------------------
// NTLGetArgString
//------------------------------------------------------------------------------

const JILChar* NTLGetArgString(JILState* pState, JILLong argNum)
{
	JILHandle* pHandle;
	const JILChar* result = "";
	pHandle = get_stack_handle(pState, argNum);
	if( pHandle->type == type_string )
	{
		result = JILString_String(JILGetStringHandle(pHandle)->str);
	}
	return result;
}

//------------------------------------------------------------------------------
// NTLGetArgObject
//------------------------------------------------------------------------------

JILUnknown* NTLGetArgObject(JILState* pState, JILLong argNum, JILLong type)
{
	JILHandle* pHandle = get_stack_handle(pState, argNum);
	return NTLHandleToObject(pState, type, pHandle);
}

//------------------------------------------------------------------------------
// NTLReturnInt
//------------------------------------------------------------------------------

void NTLReturnInt(JILState* pState, JILLong value)
{
	JILHandle* pHandle;

	pHandle = JILGetNewHandle( pState );
	pHandle->type = type_int;
	JILGetIntHandle(pHandle)->l = value;
	JILRelease( pState, pState->vmpContext->vmppRegister[kReturnRegister] );
	pState->vmpContext->vmppRegister[kReturnRegister] = pHandle;
}

//------------------------------------------------------------------------------
// NTLReturnFloat
//------------------------------------------------------------------------------

void NTLReturnFloat(JILState* pState, JILFloat value)
{
	JILHandle* pHandle;

	pHandle = JILGetNewHandle( pState );
	pHandle->type = type_float;
	JILGetFloatHandle(pHandle)->f = value;
	JILRelease( pState, pState->vmpContext->vmppRegister[kReturnRegister] );
	pState->vmpContext->vmppRegister[kReturnRegister] = pHandle;
}

//------------------------------------------------------------------------------
// NTLReturnString
//------------------------------------------------------------------------------

void NTLReturnString(JILState* pState, const JILChar* value)
{
	JILHandle* pHandle;
	if( value == NULL )
	{
		JIL_INSERT_DEBUG_CODE(
			JILMessageLog(pState, "WARNING: NULL passed to NTLReturnString(), returning null-reference. Older versions of the RT returned empty string.\n");
		)
		pHandle = JILGetNullHandle(pState);
		JILAddRef( pHandle );
	}
	else
	{
		pHandle = JILGetNewHandle( pState );
		pHandle->type = type_string;
		JILGetStringHandle(pHandle)->str = JILAllocString( pState, value );
	}
	JILRelease( pState, pState->vmpContext->vmppRegister[kReturnRegister] );
	pState->vmpContext->vmppRegister[kReturnRegister] = pHandle;
}

//------------------------------------------------------------------------------
// NTLInstanceSetUser
//------------------------------------------------------------------------------

JILUnknown* NTLInstanceSetUser(NTLInstance* pInst, JILUnknown* pData)
{
	JILUnknown* old = pInst->userData;
	pInst->userData = pData;
	return old;
}

//------------------------------------------------------------------------------
// NTLInstanceGetUser
//------------------------------------------------------------------------------

JILUnknown* NTLInstanceGetUser(NTLInstance* pInst)
{
	return pInst->userData;
}

//------------------------------------------------------------------------------
// NTLInstanceTypeID
//------------------------------------------------------------------------------

JILLong NTLInstanceTypeID(NTLInstance* pInst)
{
	return pInst->typeID;
}

//------------------------------------------------------------------------------
// NTLInstanceGetVM
//------------------------------------------------------------------------------

JILState* NTLInstanceGetVM(NTLInstance* pInst)
{
	return pInst->pState;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Advanced functions - only for experts!                                     //
// The following functions are declared in JILNativeTypeEx.h                  //
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// NTLHandleToTypeID
//------------------------------------------------------------------------------

JILLong NTLHandleToTypeID(JILState* pState, JILHandle* handle)
{
	if( handle == NULL )
		return type_null;
	return handle->type;
}

//------------------------------------------------------------------------------
// NTLHandleToBaseID
//------------------------------------------------------------------------------

JILLong NTLHandleToBaseID(JILState* pState, JILHandle* handle)
{
	if( handle == NULL )
		return type_null;
	return JILTypeInfoFromType(pState, handle->type)->base;
}

//------------------------------------------------------------------------------
// NTLHandleToInt
//------------------------------------------------------------------------------

JILLong NTLHandleToInt(JILState* pState, JILHandle* handle)
{
	if( handle == NULL || handle->type != type_int )
		return 0;
	else
		return JILGetIntHandle(handle)->l;
}

//------------------------------------------------------------------------------
// NTLHandleToFloat
//------------------------------------------------------------------------------

JILFloat NTLHandleToFloat(JILState* pState, JILHandle* handle)
{
	if( handle == NULL || handle->type != type_float )
		return 0.0;
	else
		return JILGetFloatHandle(handle)->f;
}

//------------------------------------------------------------------------------
// NTLHandleToString
//------------------------------------------------------------------------------

const JILChar* NTLHandleToString(JILState* pState, JILHandle* handle)
{
	if( handle == NULL || handle->type != type_string )
		return "";
	else
		return JILString_String(JILGetStringHandle(handle)->str);
}

//------------------------------------------------------------------------------
// NTLConvertToString
//------------------------------------------------------------------------------

JILHandle* NTLConvertToString(JILState* pState, JILHandle* handle)
{
	JILHandle* hStr = NULL;
	if( handle != NULL )
	{
		if( JILDynamicConvert(pState, type_string, handle, &hStr) != JIL_No_Exception )
		{
			NTLFreeHandle(pState, hStr);
			hStr = NULL;
		}
	}
	return hStr;
}

//------------------------------------------------------------------------------
// NTLHandleToError
//------------------------------------------------------------------------------

JILError NTLHandleToError(JILState* pState, JILHandle* handle)
{
	if( handle == NULL )
		return JIL_No_Exception;
	if( NTLHandleToBaseID(pState, handle) == type_exception )
	{
		// call getError() and return error code
		return JILExceptionCallGetError(pState, handle);
	}
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// NTLHandleToErrorMessage
//------------------------------------------------------------------------------

JILHandle* NTLHandleToErrorMessage(JILState* pState, JILHandle* handle)
{
	if( handle == NULL )
		return NULL;
	if( NTLHandleToBaseID(pState, handle) == type_exception )
	{
		// call getMessage() and return error message
		return JILExceptionCallGetMessage(pState, handle);
	}
	return NULL;
}

//------------------------------------------------------------------------------
// NTLGetArgHandle
//------------------------------------------------------------------------------

JILHandle* NTLGetArgHandle(JILState* pState, JILLong argNum)
{
	JILHandle* result = get_stack_handle(pState, argNum);
	JILAddRef(result);
	return result;
}

//------------------------------------------------------------------------------
// NTLGetNullHandle
//------------------------------------------------------------------------------

JILHandle* NTLGetNullHandle(JILState* pState)
{
	JILHandle* result = JILGetNullHandle(pState);
	JILAddRef(result);
	return result;
}

//------------------------------------------------------------------------------
// NTLReturnHandle
//------------------------------------------------------------------------------

void NTLReturnHandle(JILState* pState, JILHandle* handle)
{
	if( handle == NULL )
		handle = JILGetNullHandle(pState);
	JILAddRef( handle );
	JILRelease( pState, pState->vmpContext->vmppRegister[kReturnRegister] );
	pState->vmpContext->vmppRegister[kReturnRegister] = handle;
}

//------------------------------------------------------------------------------
// NTLReferHandle
//------------------------------------------------------------------------------

void NTLReferHandle(JILState* pState, JILHandle* handle)
{
	if( handle != NULL )
	{
		JILAddRef(handle);
	}
}

//------------------------------------------------------------------------------
// NTLFreeHandle
//------------------------------------------------------------------------------

void NTLFreeHandle(JILState* pState, JILHandle* handle)
{
	if( handle != NULL )
	{
		if( handle->refCount > 0 )
		{
			JILRelease(pState, handle);
		}
		else
		{
			JIL_INSERT_DEBUG_CODE(
				JILMessageLog(pState, "Native type releases dead handle (%s). This may indicate a reference cycle.\n", JILGetHandleTypeName(pState, handle->type));
			)
		}
	}
}

//------------------------------------------------------------------------------
// NTLMarkHandle
//------------------------------------------------------------------------------

JILError NTLMarkHandle(JILState* pState, JILHandle* pHandle)
{
	JILError err = JIL_No_Exception;
	if( pHandle != NULL )
	{
		err = JILMarkHandle(pState, pHandle);
	}
	return err;
}

//------------------------------------------------------------------------------
// NTLCopyHandle
//------------------------------------------------------------------------------

JILHandle* NTLCopyHandle(JILState* pState, JILHandle* pSource)
{
	JILHandle* pDest = NULL;

	if( JILCopyHandle(pState, pSource, &pDest) )
		pDest = NULL;

	return pDest;
}

//------------------------------------------------------------------------------
// NTLCopyValueType
//------------------------------------------------------------------------------

JILHandle* NTLCopyValueType(JILState* pState, JILHandle* pSource)
{
	JILHandle* pDest = NULL;

	if( JILCopyValueType(pState, pSource, &pDest) )
		pDest = NULL;

	return pDest;
}

//------------------------------------------------------------------------------
// NTLNewHandleForObject
//------------------------------------------------------------------------------

JILHandle* NTLNewHandleForObject(JILState* pState, JILLong type, JILUnknown* pObject)
{
	JILHandle* pHandle = NULL;
	JILTypeInfo* pTypeInfo = JILTypeInfoFromType(pState, type);
	if( pObject == NULL )
		type = type_null;
	switch( type )
	{
		case type_null:
			pHandle = NTLGetNullHandle(pState);
			break;
		case type_int:
			pHandle = JILGetNewHandle(pState);
			pHandle->type = type_int;
			JILGetIntHandle(pHandle)->l = *((JILLong*) pObject);
			break;
		case type_float:
			pHandle = JILGetNewHandle(pState);
			pHandle->type = type_float;
			JILGetFloatHandle(pHandle)->f = *((JILFloat*) pObject);
			break;
		case type_string:
			pHandle = JILGetNewHandle(pState);
			pHandle->type = type_string;
			JILGetStringHandle(pHandle)->str = (JILString*) pObject;
			break;
		case type_array:
			pHandle = JILGetNewHandle(pState);
			pHandle->type = type_array;
			JILGetArrayHandle(pHandle)->arr = (JILArray*) pObject;
			break;
		default:
		{
			if( pTypeInfo->isNative )
			{
				pHandle = JILGetNewHandle(pState);
				pHandle->type = type;
				JILGetNObjectHandle(pHandle)->ptr = pObject;
			}
			else
			{
				if( pTypeInfo->family == tf_class )
				{
					pHandle = JILGetNewHandle(pState);
					pHandle->type = type;
					JILGetObjectHandle(pHandle)->ppHandles = pObject;
				}
				else if( pTypeInfo->family == tf_delegate )
				{
					pHandle = JILGetNewHandle(pState);
					pHandle->type = type;
					JILGetDelegateHandle(pHandle)->pDelegate = pObject;
				}
				else
				{
					// don't know what to do for non-class, non-native, user-defined types!
					pHandle = NTLGetNullHandle(pState);
				}
			}
			break;
		}
	}
	return pHandle;
}

//------------------------------------------------------------------------------
// NTLNewWeakRefForObject
//------------------------------------------------------------------------------

JILHandle* NTLNewWeakRefForObject(JILState* pState, JILLong TypeID, JILUnknown* pObject)
{
	JILHandle* pResult = NTLNewHandleForObject(pState, TypeID, pObject);
	if( pResult && pResult->type != type_null )
	{
		pResult->flags |= HF_PERSIST;
	}
	return pResult;
}

//------------------------------------------------------------------------------
// NTLHandleToObject
//------------------------------------------------------------------------------

JILUnknown* NTLHandleToObject(JILState* pState, JILLong type, JILHandle* pHandle)
{
	JILTypeInfo* pTypeInfo = JILTypeInfoFromType(pState, pHandle->type);
	if( type == pHandle->type || type == pTypeInfo->base )
	{
		switch( pHandle->type )
		{
			case type_int:
				return &(JILGetIntHandle(pHandle)->l);
			case type_float:
				return &(JILGetFloatHandle(pHandle)->f);
			case type_string:
				return JILGetStringHandle(pHandle)->str;
			case type_array:
				return JILGetArrayHandle(pHandle)->arr;
			default:
			{
				if( pTypeInfo->isNative )
				{
					return JILGetNObjectHandle(pHandle)->ptr;
				}
				else
				{
					if( pTypeInfo->family == tf_class )
					{
						return JILGetObjectHandle(pHandle)->ppHandles;
					}
					else if( pTypeInfo->family == tf_delegate )
					{
						return JILGetDelegateHandle(pHandle)->pDelegate;
					}
					else
					{
						// again don't know how to handle non-class, non-native, user-def'd type
					}
				}
				break;
			}
		}
	}
	return NULL;
}

//------------------------------------------------------------------------------
// NTLNewObject
//------------------------------------------------------------------------------

JILHandle* NTLNewObject(JILState* pState, JILLong type)
{
	JILTypeInfo* pTypeInfo;
	JILHandle* pNewHandle = NULL;

	pTypeInfo = JILTypeInfoFromType(pState, type);
	if( pTypeInfo->isNative )
	{
		int result;
		void* objptr = NULL;
		pNewHandle = JILGetNewHandle(pState);
		// call proc
		result = CallNTLNewObject(pTypeInfo, &objptr);
		if( result != 0 || objptr == NULL )
		{
			JILRelease(pState, pNewHandle);
			return NULL;
		}
		pNewHandle->type = type;
		JILGetNObjectHandle(pNewHandle)->ptr = objptr;
	}
	else	// not native
	{
		if( pTypeInfo->family == tf_class )
		{
			JILLong size;
			pNewHandle = JILGetNewHandle(pState);
			size = pTypeInfo->instanceSize;
			// allocate object
			pNewHandle->type = type;
			JILGetObjectHandle(pNewHandle)->ppHandles = JILAllocObject(pState, size);
		}
		else if( pTypeInfo->family == tf_delegate )
		{
			// TODO: we cannot allocate a delegate without arguments (function index, object reference)
		}
		else
		{
			// TODO: no code to handle non-class, non-native, user-defined types
		}
	}
	return pNewHandle;
}

//------------------------------------------------------------------------------
// NTLLoadResource
//------------------------------------------------------------------------------

JILError NTLLoadResource(JILState* pVM, const JILChar* pName, JILUnknown** ppData, JILLong* pSize)
{
	JILError err;
	JILFileHandle* pFile = NULL;
	JILChar* pData = NULL;
	JILLong fileSize = 0;

	if( pName == NULL || ppData == NULL || pSize == NULL )
		return JIL_ERR_Generic_Error;

	// open the file
	err = NTLFileOpen(pVM, pName, &pFile);
	if( err )
		return err;

	// get the file length
	fileSize = NTLFileLength(pFile);
	*pSize = fileSize;

	// allocate a buffer
	*ppData = pData = (JILChar*) pVM->vmMalloc(pVM, fileSize);
	if( pData == NULL )
		goto exit;

	// read the entire file into the buffer
	if( NTLFileRead(pFile, pData, fileSize) != fileSize )
	{
		err = JIL_ERR_File_Generic;
		goto exit;
	}

	// close the file
	err = NTLFileClose(pFile);
	if( err )
		goto exit;

	// success
	return JIL_No_Exception;

exit:
	NTLFileClose(pFile);
	pVM->vmFree(pVM, pData);
	*ppData = NULL;
	*pSize = 0;
	return err;
}

//------------------------------------------------------------------------------
// NTLFreeResource
//------------------------------------------------------------------------------

JILError NTLFreeResource(JILState* pVM, JILUnknown* pData)
{
	if( pData == NULL )
		return JIL_ERR_Generic_Error;
	pVM->vmFree(pVM, pData);
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// NTLFileOpen
//------------------------------------------------------------------------------

JILError NTLFileOpen(JILState* pState, const JILChar* pName, JILFileHandle** ppFileOut)
{
	JILError err;
	JILUnknown* pUserStream = NULL;
	JILFileHandle* pFH = NULL;
	*ppFileOut = NULL;
	// try to open the file
	err = pState->vmFileInput(pState, JILFileInputOpen, (JILChar*) pName, 0, &pUserStream);
	if( err )
		return JIL_ERR_File_Open;
	// allocate a JILFileHandle
	*ppFileOut = pFH = (JILFileHandle*)pState->vmMalloc(pState, sizeof(JILFileHandle));
	pFH->pStream = pUserStream;
	pFH->pState = pState;
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// NTLFileRead
//------------------------------------------------------------------------------

JILLong NTLFileRead(JILFileHandle* pFile, JILUnknown* pData, JILLong size)
{
	JILLong length = 0;
	if( pFile && pFile->pState && pFile->pStream && pData )
	{
		JILState* pVM = pFile->pState;
		length = pVM->vmFileInput(pVM, JILFileInputRead, pData, size, &pFile->pStream);
	}
	return length;
}

//------------------------------------------------------------------------------
// NTLFileSeek
//------------------------------------------------------------------------------

JILError NTLFileSeek(JILFileHandle* pFile, JILLong position)
{
	JILError err = JIL_ERR_File_Generic;
	if( pFile && pFile->pState && pFile->pStream )
	{
		JILState* pVM = pFile->pState;
		err = pVM->vmFileInput(pVM, JILFileInputSeek, NULL, position, &pFile->pStream);
		if( err )
			err = JIL_ERR_File_Generic;
	}
	return err;
}

//------------------------------------------------------------------------------
// NTLFileLength
//------------------------------------------------------------------------------

JILLong NTLFileLength(JILFileHandle* pFile)
{
	JILLong length = 0;
	if( pFile && pFile->pState && pFile->pStream )
	{
		JILState* pVM = pFile->pState;
		length = pVM->vmFileInput(pVM, JILFileInputLength, NULL, 0, &pFile->pStream);
	}
	return length;
}

//------------------------------------------------------------------------------
// NTLFileClose
//------------------------------------------------------------------------------

JILError NTLFileClose(JILFileHandle* pFile)
{
	JILError err = JIL_ERR_File_Generic;
	if( pFile && pFile->pState && pFile->pStream )
	{
		JILState* pVM = pFile->pState;
		err = pVM->vmFileInput(pVM, JILFileInputClose, NULL, 0, &pFile->pStream);
		if( err )
		{
			err = JIL_ERR_File_Generic;
		}
		else
		{
			pFile->pState = NULL;
			pFile->pStream = NULL;
			pVM->vmFree(pVM, pFile);
			err = JIL_No_Exception;
		}
	}
	return err;
}

//------------------------------------------------------------------------------
// NTLDeclareConstantInt
//------------------------------------------------------------------------------

JILError NTLDeclareConstantInt(JILUnknown* pDataIn, JILLong typeID, const JILChar* pName, JILLong value)
{
	JILError err = JIL_ERR_Illegal_Argument;
	if( pDataIn && pName )
	{
		JCLDeclStruct* pDS = (JCLDeclStruct*) pDataIn;
		const JILChar* pType;
		JILChar buf[32];
		JILSnprintf( buf, 32, "%d", value );
		pType = NTLGetTypeName(pDS->pState, typeID);
		if( !pType )
			return err;
		JCLAppend(pDS->pString, "const ");
		JCLAppend(pDS->pString, pType);
		JCLAppend(pDS->pString, " ");
		JCLAppend(pDS->pString, pName);
		JCLAppend(pDS->pString, " = ");
		JCLAppend(pDS->pString, buf);
		JCLAppend(pDS->pString, ";\n");
		err = JIL_No_Exception;
	}
	return err;
}

//------------------------------------------------------------------------------
// NTLDeclareConstantFloat
//------------------------------------------------------------------------------

JILError NTLDeclareConstantFloat(JILUnknown* pDataIn, JILLong typeID, const JILChar* pName, JILFloat value)
{
	JILError err = JIL_ERR_Illegal_Argument;
	if( pDataIn && pName )
	{
		JCLDeclStruct* pDS = (JCLDeclStruct*) pDataIn;
		const JILChar* pType;
		JILChar buf[32];
		JILSnprintf( buf, 32, "%f", value );
		pType = NTLGetTypeName(pDS->pState, typeID);
		if( !pType )
			return err;
		JCLAppend(pDS->pString, "const ");
		JCLAppend(pDS->pString, pType);
		JCLAppend(pDS->pString, " ");
		JCLAppend(pDS->pString, pName);
		JCLAppend(pDS->pString, " = ");
		JCLAppend(pDS->pString, buf);
		JCLAppend(pDS->pString, ";\n");
		err = JIL_No_Exception;
	}
	return err;
}

//------------------------------------------------------------------------------
// NTLDeclareConstantString
//------------------------------------------------------------------------------

JILError NTLDeclareConstantString(JILUnknown* pDataIn, JILLong typeID, const JILChar* pName, const JILChar* pValue)
{
	JILError err = JIL_ERR_Illegal_Argument;
	if( pDataIn && pName && pValue )
	{
		JCLDeclStruct* pDS = (JCLDeclStruct*) pDataIn;
		const JILChar* pType;
		pType = NTLGetTypeName(pDS->pState, typeID);
		if( !pType )
			return err;
		JCLAppend(pDS->pString, "const ");
		JCLAppend(pDS->pString, pType);
		JCLAppend(pDS->pString, " ");
		JCLAppend(pDS->pString, pName);
		JCLAppend(pDS->pString, " = \"");
		JCLAppend(pDS->pString, pValue);
		JCLAppend(pDS->pString, "\";\n");
		err = JIL_No_Exception;
	}
	return err;
}

//------------------------------------------------------------------------------
// NTLDeclareVerbatim
//------------------------------------------------------------------------------

JILError NTLDeclareVerbatim(JILUnknown* pDataIn, const JILChar* pText)
{
	JILError err = JIL_ERR_Illegal_Argument;
	if( pDataIn && pText )
	{
		JCLDeclStruct* pDS = (JCLDeclStruct*) pDataIn;
		if( pText )
			JCLAppend(pDS->pString, pText);
		err = JIL_No_Exception;
	}
	return err;
}
