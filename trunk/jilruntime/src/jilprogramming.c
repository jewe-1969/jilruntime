//------------------------------------------------------------------------------
// File: JILProgramming.c                                   (c) 2003 jewe.org
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
//	Provides functions to program the virtual machine, such as functions to
//	create global data, build object descriptors, read and write program memory
//	and so forth. These functions will normally be used by a JIL assembler or
//	JIL compiler to create an executable.
//------------------------------------------------------------------------------

#include "jilstdinc.h"

#include "jilprogramming.h"
#include "jilcstrsegment.h"
#include "jilopcodes.h"
#include "jiltypelist.h"
#include "jilhandle.h"
#include "jilmachine.h"
#include "jilsymboltable.h"
#include "jiltypeinfo.h"
#include "jilcallntl.h"
#include "jilallocators.h"

//------------------------------------------------------------------------------
// JILCreateLong
//------------------------------------------------------------------------------

JILError JILCreateLong(JILState* pState, JILLong value, JILLong* hLong)
{
	JILLong i;
	JILDataHandle* pHandle;

	// check if value already exists
	for( i = 0; i < pState->vmpDataSegment->usedSize; i++ )
	{
		pHandle = pState->vmpDataSegment->pData + i;
		if( pHandle->type == type_int && JILGetDataHandleLong(pHandle) == value )
		{
			*hLong = i;
			break;
		}
	}
	if( i == pState->vmpDataSegment->usedSize )
	{
		*hLong = NewElement_JILDataHandle(pState->vmpDataSegment, &pHandle);
		pHandle->type = type_int;
		JILGetDataHandleLong(pHandle) = value;
	}
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// JILCreateFloat
//------------------------------------------------------------------------------

JILError JILCreateFloat(JILState* pState, JILFloat value, JILLong* hFloat)
{
	JILLong i;
	JILDataHandle* pHandle;

	// check if value already exists
	for( i = 0; i < pState->vmpDataSegment->usedSize; i++ )
	{
		pHandle = pState->vmpDataSegment->pData + i;
		if( pHandle->type == type_float && JILGetDataHandleFloat(pHandle) == value )
		{
			*hFloat = i;
			break;
		}
	}
	if( i == pState->vmpDataSegment->usedSize )
	{
		*hFloat = NewElement_JILDataHandle(pState->vmpDataSegment, &pHandle);
		pHandle->type = type_float;
		JILGetDataHandleFloat(pHandle) = value;
	}
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// JILCreateString
//------------------------------------------------------------------------------

JILError JILCreateString(JILState* pState, const JILChar* pStr, JILLong* hString)
{
	JILLong i;
	JILLong offset;
	JILDataHandle* pHandle;

	// check if value already exists
	for( i = 0; i < pState->vmpDataSegment->usedSize; i++ )
	{
		pHandle = pState->vmpDataSegment->pData + i;
		if( pHandle->type == type_string )
		{
			offset = JILGetDataHandleLong(pHandle);
			if( strcmp(JILCStrGetString(pState, offset), pStr) == 0 )
			{
				*hString = i;
				break;
			}
		}
	}
	if( i == pState->vmpDataSegment->usedSize )
	{
		*hString = NewElement_JILDataHandle(pState->vmpDataSegment, &pHandle);
		pHandle->type = type_string;
		JILGetDataHandleLong(pHandle) = JILAddCStrPoolData(pState, pStr, strlen(pStr) + 1);
	}
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// JILCreateFunction
//------------------------------------------------------------------------------

JILError JILCreateFunction(JILState* pState, JILLong type, JILLong memberIdx, JILLong flags, const JILChar* pName, JILLong* pFuncIndex)
{
	JILFuncInfo* pFuncInfo;

	*pFuncIndex = NewElement_JILFuncInfo(pState->vmpFuncSegment, &pFuncInfo);
	pFuncInfo->type = type;
	pFuncInfo->flags = flags;
	pFuncInfo->codeAddr = 0;
	pFuncInfo->codeSize = 0;
	pFuncInfo->memberIdx = memberIdx;
	pFuncInfo->offsetName = JILAddCStrPoolData(pState, pName, strlen(pName) + 1);

	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// JILGetFunctionByName
//------------------------------------------------------------------------------

JILLong JILGetFunctionByName(JILState* pState, JILLong type, const JILChar* pName, JILFuncInfo** ppOut)
{
	JILLong i;
	JILLong offset;
	*ppOut = NULL;
	for( i = 0; i < pState->vmpFuncSegment->usedSize; i++ )
	{
		if( pState->vmpFuncSegment->pData[i].type == type )
		{
			offset = pState->vmpFuncSegment->pData[i].offsetName;
			if( strcmp(JILCStrGetString(pState, offset), pName) == 0 )
			{
				*ppOut = pState->vmpFuncSegment->pData + i;
				return i;
			}
		}
	}
	return -1;
}

//------------------------------------------------------------------------------
// JILGetFunctionByAddr
//------------------------------------------------------------------------------

JILError JILGetFunctionByAddr(JILState* pState, JILLong addr, JILFuncInfo** ppOut)
{
	JILLong i;
	JILLong min;
	JILLong max;
	JILFuncInfo* pFuncInfo = pState->vmpFuncSegment->pData;
	JILLong numFunc = pState->vmpFuncSegment->usedSize;
	*ppOut = NULL;
	for( i = 0; i < numFunc; i++ )
	{
		if( pFuncInfo[i].codeSize )	// skip native type functions!
		{
			min = pFuncInfo[i].codeAddr;
			max = min + pFuncInfo[i].codeSize;
			if( addr >= min && addr < max )
			{
				*ppOut = pFuncInfo + i;
				return i;
			}
		}
	}
	return -1;
}

//------------------------------------------------------------------------------
// JILGetFunctionByIndex
//------------------------------------------------------------------------------

JILError JILGetFunctionByIndex(JILState* pState, JILLong type, JILLong index, JILFuncInfo** ppOut)
{
	JILLong i;
	*ppOut = NULL;
	for( i = 0; i < pState->vmpFuncSegment->usedSize; i++ )
	{
		if( pState->vmpFuncSegment->pData[i].type == type
			&& pState->vmpFuncSegment->pData[i].memberIdx == index )
		{
			*ppOut = pState->vmpFuncSegment->pData + i;
			return i;
		}
	}
	return -1;
}

//------------------------------------------------------------------------------
// JILGetNumFunctions
//------------------------------------------------------------------------------

JILLong JILGetNumFunctions(JILState* pState)
{
	return pState->vmpFuncSegment->usedSize;
}

//------------------------------------------------------------------------------
// JILSetFunctionAddress
//------------------------------------------------------------------------------

JILError JILSetFunctionAddress(JILState* pState, JILLong funcIndex, JILLong address, JILLong size, JILLong args)
{
	// ensure valid handle address
	if( funcIndex < 0 || funcIndex >= pState->vmpFuncSegment->usedSize )
		return JIL_ERR_Invalid_Function_Index;

	pState->vmpFuncSegment->pData[funcIndex].codeAddr = address;
	pState->vmpFuncSegment->pData[funcIndex].codeSize = size;
	pState->vmpFuncSegment->pData[funcIndex].args = args;

	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// JILCreateType
//------------------------------------------------------------------------------

JILError JILCreateType(JILState* pState, const JILChar* pName, JILTypeFamily family, JILBool bNative, JILLong* pType)
{
	JILTypeInfo* pTypeInfo;
	JILTypeListItem* pItem;

	// allocate a new TypeInfo entry
	*pType = JILNewTypeInfo(pState, pName, &pTypeInfo);

	// set type information
	pTypeInfo->base = 0;
	pTypeInfo->family = family;
	pTypeInfo->isNative = bNative;
	pTypeInfo->methodInfo.ctor = -1;
	pTypeInfo->methodInfo.cctor = -1;
	pTypeInfo->methodInfo.dtor = -1;
	pTypeInfo->methodInfo.tostr = -1;

	// try to find the native type
	if( bNative )
	{
		pItem = JILGetNativeType( pState, pName );
		if( !pItem )
			return JIL_ERR_Undefined_Type;
		pTypeInfo->interfaceVersion = CallNTLGetInterfaceVersion(pItem->typeProc);
		pTypeInfo->authorVersion = CallNTLGetAuthorVersion(pItem->typeProc);
	}
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// JILSetClassInstanceSize
//------------------------------------------------------------------------------

JILError JILSetClassInstanceSize(JILState* pState, JILLong type, JILLong instanceSize)
{
	JILTypeInfo* pTypeInfo;

	// ensure type is valid
	if( type < 0 || type >= pState->vmUsedTypeInfoSegSize )
		return JIL_ERR_Illegal_Argument;

	pTypeInfo = JILTypeInfoFromType(pState, type);
	if( pTypeInfo->family != tf_class || pTypeInfo->isNative )
		return JIL_ERR_Illegal_Argument;

	// update type info
	pTypeInfo->instanceSize = instanceSize;
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// JILSetClassVTable
//------------------------------------------------------------------------------

JILError JILSetClassVTable(JILState* pState, JILLong type, JILLong size, JILLong* pVtab)
{
	JILTypeInfo* pTypeInfo;

	// ensure type is valid
	if( type < 0 || type >= pState->vmUsedTypeInfoSegSize )
		return JIL_ERR_Illegal_Argument;

	pTypeInfo = JILTypeInfoFromType(pState, type);
	if( pTypeInfo->family != tf_class )
		return JIL_ERR_Illegal_Argument;
	if( pTypeInfo->isNative )
		pVtab = NULL;
	// update type info
	if( pVtab )
		pTypeInfo->offsetVtab = JILAddCStrData(pState, pVtab, size * sizeof(JILLong));
	pTypeInfo->sizeVtab = size;

	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// JILSetClassMethodInfo
//------------------------------------------------------------------------------

JILError JILSetClassMethodInfo(JILState* pState, JILLong type, JILMethodInfo* pInfo)
{
	JILTypeInfo* pTypeInfo;

	// ensure type is valid
	if( type < 0 || type >= pState->vmUsedTypeInfoSegSize )
		return JIL_ERR_Illegal_Argument;

	pTypeInfo = JILTypeInfoFromType(pState, type);
	if( pTypeInfo->family != tf_class )
		return JIL_ERR_Illegal_Argument;

	// update type info
	memcpy(&(pTypeInfo->methodInfo), pInfo, sizeof(JILMethodInfo));

	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// JILSetGlobalObjectSize
//------------------------------------------------------------------------------

JILError JILSetGlobalObjectSize(JILState* pState, JILLong type, JILLong newSize)
{
	// already initialized?
	if( pState->vmInitialized )
	{
		JILTypeInfo* pTypeInfo;

		// ensure type is valid
		if( type < 0 || type >= pState->vmUsedTypeInfoSegSize )
			return JIL_ERR_Illegal_Argument;

		pTypeInfo = JILTypeInfoFromType(pState, type);
		if( pTypeInfo->family != tf_class || pTypeInfo->isNative )
			return JIL_ERR_Illegal_Argument;

		// new size bigger than old size?
		if( newSize > pTypeInfo->instanceSize )
		{
			JILLong i;
			JILLong oldSize;
			JILHandle* pGlobal;
			JILHandle** ppOld;
			JILHandle** ppNew;
			JILHandle** ppNew2;
			// must resize global object
			oldSize = pTypeInfo->instanceSize;
			pGlobal = pState->vmpRootContext->vmppRegister[2];	// TODO: What if we already have other contexts????
			ppOld = JILGetObjectHandle(pGlobal)->ppHandles;
			ppNew2 = ppNew = JILAllocObjectNoInit(pState, newSize);
			for( i = 0; i < oldSize; i++ )
				*ppNew++ = *ppOld++;
			for( i = oldSize; i < newSize; i++ )
				*ppNew++ = JILGetNullHandle(pState);
			JILGetNullHandle(pState)->refCount += (newSize - oldSize);
			pState->vmFree( pState, JILGetObjectHandle(pGlobal)->ppHandles );
			JILGetObjectHandle(pGlobal)->ppHandles = ppNew2;
		}
	}
	return JILSetClassInstanceSize(pState, type, newSize);
}

//------------------------------------------------------------------------------
// JILSetMemory
//------------------------------------------------------------------------------

JILError JILSetMemory(JILState* pState, JILLong address, const JILLong* pData, JILLong size)
{
	JILError result = JIL_No_Exception;
	Seg_JILLong* pCodeSegment = pState->vmpCodeSegment;

	if( (address + size) >= pCodeSegment->maxSize )
	{
		// must resize code segment
		long grain = pState->vmSegmentAllocGrain;
		long oldMax = pCodeSegment->maxSize;
		long newMax = oldMax + size + grain;
		JILLong* pOldSegment = pCodeSegment->pData;

		// allocate new buffer
		pCodeSegment->pData = (JILLong*) malloc( newMax * sizeof(JILLong) );
		pCodeSegment->maxSize = newMax;
		memset( pCodeSegment->pData + oldMax, 0, (grain + size) * sizeof(JILLong) );
		memcpy( pCodeSegment->pData, pOldSegment, oldMax * sizeof(JILLong) );

		// free old buffer
		free( pOldSegment );
	}
	if( (address + size) > pCodeSegment->usedSize )
		pCodeSegment->usedSize = address + size;
	memcpy( pCodeSegment->pData + address, pData, size * sizeof(JILLong) );

	return result;
}

//------------------------------------------------------------------------------
// JILGetMemory
//------------------------------------------------------------------------------

JILError JILGetMemory(JILState* pState, JILLong address, JILLong* pData, JILLong size)
{
	if( (address + size) > pState->vmpCodeSegment->usedSize )
		return JIL_ERR_Out_Of_Code;

	memcpy( pData, pState->vmpCodeSegment->pData + address, size * sizeof(JILLong) );

	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// JILCreateRestorePoint
//------------------------------------------------------------------------------

void JILCreateRestorePoint(JILState* pS, JILRestorePoint* pRP)
{
	if( pRP->reMagic != 'JRes' )
	{
		JILTermVM( pS );
		pRP->reUsedCodeSegSize = pS->vmpCodeSegment->usedSize;
		pRP->reUsedDataSegSize = pS->vmpDataSegment->usedSize;
		pRP->reUsedCStrSegSize = pS->vmUsedCStrSegSize;
		pRP->reUsedTypeSegSize = pS->vmUsedTypeInfoSegSize;
		pRP->reUsedSymTabSize = JILGetNumSymbolTableEntries(pS);
		pRP->reMagic = 'JRes';
	}
}

//------------------------------------------------------------------------------
// JILGotoRestorePoint
//------------------------------------------------------------------------------

void JILGotoRestorePoint(JILState* pS, JILRestorePoint* pRP)
{
	if( pRP->reMagic == 'JRes' )
	{
		JILTermVM( pS );
		pS->vmpCodeSegment->usedSize = pRP->reUsedCodeSegSize;
		pS->vmpDataSegment->usedSize = pRP->reUsedDataSegSize;
		pS->vmUsedCStrSegSize = pRP->reUsedCStrSegSize;
		pS->vmUsedTypeInfoSegSize = pRP->reUsedTypeSegSize;
		JILTruncateSymbolTable(pS, pRP->reUsedSymTabSize);
		pRP->reMagic = 0;
	}
}

//------------------------------------------------------------------------------
// JILGetCodeLength
//------------------------------------------------------------------------------

JILLong JILGetCodeLength(JILState* pState)
{
	return pState->vmpCodeSegment->usedSize;
}
