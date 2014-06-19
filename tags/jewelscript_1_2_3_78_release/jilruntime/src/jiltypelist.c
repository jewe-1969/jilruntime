//------------------------------------------------------------------------------
// File: JILTypeList.c                                      (c) 2003 jewe.org
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
//	Manages a list of class names and callback procs for native type libs.
//	Also provides functions to register a type library and unregister all
//	type libs.
//------------------------------------------------------------------------------

#include "jilstdinc.h"

//------------------------------------------------------------------------------
// JIL includes
//------------------------------------------------------------------------------

#include "jiltypelist.h"
#include "jiltools.h"
#include "jilcallntl.h"

//------------------------------------------------------------------------------
// JILInitTypeList
//------------------------------------------------------------------------------

JILError JILInitTypeList(JILState* pState, long size)
{
	pState->vmpTypeList = (JILTypeListItem*) malloc( size * sizeof(JILTypeListItem) );
	memset(pState->vmpTypeList, 0, size * sizeof(JILTypeListItem));
	pState->vmUsedNativeTypes = 0;
	pState->vmMaxNativeTypes = size;
	pState->vmNativeTypeGrain = size;
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// JILDestroyTypeList
//------------------------------------------------------------------------------

JILError JILDestroyTypeList(JILState* pState)
{
	JILLong i;

	// unregister native type libraries first
	JILLong err = JILUnregisterNativeTypes( pState );

	for( i = 0; i < pState->vmUsedNativeTypes; i++ )
	{
		free( pState->vmpTypeList[i].pClassName );
		pState->vmpTypeList[i].pClassName = NULL;
	}
	free( pState->vmpTypeList );
	pState->vmpTypeList = NULL;
	pState->vmUsedNativeTypes = 0;
	pState->vmMaxNativeTypes = 0;
	pState->vmNativeTypeGrain = 0;
	return err;
}

//------------------------------------------------------------------------------
// JILCheckClassName
//------------------------------------------------------------------------------

JILError JILCheckClassName(JILState* pState, const JILChar* name)
{
	JILChar c;
	c = *name++;
	if( !c )
		return JIL_ERR_Illegal_Type_Name;
	if(  c != '_'
	&&	(c < 'A' || c > 'Z')
	&&	(c < 'a' || c > 'z') )
		return JIL_ERR_Illegal_Type_Name;
	for(;;)
	{
		c = *name++;
		if( !c )
			break;
		if(  c != '_' && c != ':'
		&&	(c < 'A' || c > 'Z')
		&&	(c < 'a' || c > 'z')
		&&	(c < '0' || c > '9') )
			return JIL_ERR_Illegal_Type_Name;
	}
	return JIL_No_Exception;
}

//------------------------------------------------------------------------------
// JILNewNativeType
//------------------------------------------------------------------------------

JILTypeListItem* JILNewNativeType(JILState* pState, const JILChar* pClassName, JILTypeProc proc)
{
	JILTypeListItem* pItem = NULL;
	JILLong nLength;
	// make sure the name is unique!
	if( JILCheckClassName(pState, pClassName) == JIL_No_Exception && JILGetNativeType(pState, pClassName) == NULL )
	{
		if( pState->vmUsedNativeTypes >= pState->vmMaxNativeTypes )
		{
			// must resize buffer!
			long grain = pState->vmNativeTypeGrain;
			long oldMax = pState->vmMaxNativeTypes;
			long newMax = pState->vmMaxNativeTypes + grain;
			JILTypeListItem* pOldList = pState->vmpTypeList;

			// allocate new buffer
			pState->vmpTypeList = (JILTypeListItem*) malloc( newMax * sizeof(JILTypeListItem) );
			pState->vmMaxNativeTypes = newMax;
			memset( pState->vmpTypeList + oldMax, 0, grain * sizeof(JILTypeListItem) );
			memcpy( pState->vmpTypeList, pOldList, oldMax * sizeof(JILTypeListItem) );

			// free old buffer
			free( pOldList );
		}
		// get new identifier address
		pItem = pState->vmpTypeList + pState->vmUsedNativeTypes;

		// initialize identifier
		nLength = strlen(pClassName);
		pItem->typeProc = proc;
		pItem->pClassName = (JILChar*) malloc( nLength + 1 );
		JILStrcpy( pItem->pClassName, nLength + 1, pClassName );

		// one up!
		pState->vmUsedNativeTypes++;
	}
	return pItem;
}

//------------------------------------------------------------------------------
// JILGetNativeType
//------------------------------------------------------------------------------

JILTypeListItem* JILGetNativeType(JILState* pState, const JILChar* pClassName)
{
	JILLong i;
	JILTypeListItem* result = NULL;
	for( i = 0; i < pState->vmUsedNativeTypes; i++ )
	{
		if( strcmp(pState->vmpTypeList[i].pClassName, pClassName) == 0 )
		{
			result = pState->vmpTypeList + i;
			break;
		}
	}
	return result;
}

//------------------------------------------------------------------------------
// JILRegisterNativeType
//------------------------------------------------------------------------------

JILError JILRegisterNativeType(JILState* pState, JILTypeProc proc)
{
	JILError err = JIL_ERR_Register_Type_Failed;
	JILLong vers;
	const JILChar* pClassName = NULL;
	JILTypeListItem* pItem = NULL;

	if( proc )
	{
		// get the interface version used by the lib
		vers = CallNTLGetInterfaceVersion(proc);
		// we could filter out no longer supported versions here, but for now we accept all valid version numbers
		if( !vers || vers > JILRevisionToLong(JIL_TYPE_INTERFACE_VERSION) )
			goto exit;
		// get class name
		err = CallNTLGetClassName(proc, &pClassName);
		// if null or error we fail
		if( err || pClassName == NULL )
			goto exit;
		err = JIL_ERR_Register_Type_Failed;
		// try to allocate a type list item
		pItem = JILNewNativeType( pState, pClassName, proc );
		// if we get a null pointer the class name is invalid or already taken
		if( pItem == NULL )
			goto exit;
		// all is well, call NTL_Register now
		err = CallNTLRegister(proc, JILRevisionToLong(JIL_TYPE_INTERFACE_VERSION), pState);
	}

exit:
	return err;
}

//------------------------------------------------------------------------------
// JILUnregisterNativeTypes
//------------------------------------------------------------------------------

JILError JILUnregisterNativeTypes(JILState* pState)
{
	JILLong i;

	for( i = 0; i < pState->vmUsedNativeTypes; i++ )
	{
		CallNTLUnregister(pState->vmpTypeList[i].typeProc, pState);
	}
	return JIL_No_Exception;
}
