//------------------------------------------------------------------------------
// File: JILOpMacros.h                                         (c) 2005 jewe.org
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
// Defines helper macros used to generate code for the virtual machine
// instruction procedures.
//------------------------------------------------------------------------------

#ifndef JILOPMACROS_H
#define JILOPMACROS_H

//------------------------------------------------------------------------------
// JIL_GET_DATA
//------------------------------------------------------------------------------
// Get a data word directly from the code segment.

#define JIL_GET_DATA(STATE) (*pInstruction++)

//------------------------------------------------------------------------------
// JIL_LEA_R
//------------------------------------------------------------------------------
// Load effective address of a handle. Addressing mode 'rn'.

#define JIL_LEA_R(CONTEXT, OUTEA)	(OUTEA) = CONTEXT->vmppRegister + (*pInstruction++);

//------------------------------------------------------------------------------
// JIL_LEA_D
//------------------------------------------------------------------------------
// Load effective address of a handle. Addressing mode 'd(rn)'.
// 2004-12-20: Removed support for array and objectdesc types in favor of
// performance and simplicity.

#define JIL_LEA_D(CONTEXT, OUTEA) \
{\
	pHObject = (JILHandleObject*)(CONTEXT->vmppRegister[*pInstruction++]);\
	JIL_THROW_IF(pHObject->type == type_null, JIL_VM_Null_Reference)\
	JIL_INSERT_DEBUG_CODE(\
		typeInfo = JILTypeInfoFromType(pState, pHObject->type);\
		JIL_THROW_IF(typeInfo->family != tf_class, JIL_VM_Unsupported_Type)\
		JIL_THROW_IF(typeInfo->isNative, JIL_VM_Unsupported_Type)\
	)\
	(OUTEA) = pHObject->ppHandles + (*pInstruction++);\
}

//------------------------------------------------------------------------------
// JIL_LEA_X
//------------------------------------------------------------------------------
// Load effective address of a handle. Addressing mode 'rx(ry)'.
// 2004-12-20: Removed support for object and objectdesc types in favor of
// performance and simplicity.

#define JIL_LEA_X(CONTEXT, OUTEA) \
{\
	pHArray = (JILHandleArray*)(CONTEXT->vmppRegister[*pInstruction++]);\
	pHLong = (JILHandleInt*)(CONTEXT->vmppRegister[*pInstruction++]);\
	JIL_THROW_IF(pHArray->type == type_null, JIL_VM_Null_Reference)\
	JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(pHArray->type != type_array, JIL_VM_Unsupported_Type) )\
	JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(pHLong->type != type_int,   JIL_VM_Unsupported_Type) )\
	(OUTEA) = JILArray_GetEA(pHArray->arr, pHLong->l);\
}

//------------------------------------------------------------------------------
// JIL_LEA_S
//------------------------------------------------------------------------------
// Load effective address of a handle. Addressing mode 'd(sp)'.

#define JIL_LEA_S(CONTEXT, OUTEA) (OUTEA) = CONTEXT->vmppDataStack + (CONTEXT->vmDataStackPointer + (*pInstruction++));

//------------------------------------------------------------------------------
// JIL_STORE_HANDLE
//------------------------------------------------------------------------------
// Stores a handle in a variable or register and takes care about the correct
// reference counting.

#define JIL_STORE_HANDLE(STATE, PPVAR, POBJ) \
	JILAddRef( POBJ );\
	JILRelease( STATE, *PPVAR );\
	*PPVAR = POBJ;

//------------------------------------------------------------------------------
// JIL_THROW_IF
//------------------------------------------------------------------------------
// Generates a virtual machine exception if the condition is true

#define JIL_THROW_IF(CONDITION, EXCEP)	if(CONDITION){ result = EXCEP; goto exception; }

//------------------------------------------------------------------------------
// JIL_THROW
//------------------------------------------------------------------------------
// Generates a virtual machine exception from the given result code

#define JIL_THROW(EXCEP)				if((result = EXCEP)){ goto exception; }

//------------------------------------------------------------------------------
// JIL_IBEGIN
//------------------------------------------------------------------------------
// Sets the instruction size. This should be the first thing an instruction does.

#define JIL_IBEGIN(N) instruction_size = N;

//------------------------------------------------------------------------------
// JIL_IEND
//------------------------------------------------------------------------------
// Increases the program counter and leaves an instruction procedure. This must
// be done as the last thing, if the instruction was successful (no exception
// thrown!) and the program counter was not set (no branch instruction!).

#define JIL_IEND programCounter += instruction_size; break;

//------------------------------------------------------------------------------
// JIL_IENDBR
//------------------------------------------------------------------------------
// This must be done to correct the instruction read pointer at the end of a
// branch instruction (but only if it really branches)!

#define JIL_IENDBR pInstruction = pCodeSegment + programCounter; break;

//------------------------------------------------------------------------------
// JIL_PUSH_CS
//------------------------------------------------------------------------------
// Push a return address onto the VM's call stack

#define JIL_PUSH_CS(N) \
	JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(pContext->vmCallStackPointer <= 0, JIL_VM_Stack_Overflow) ) \
	pContext->vmpCallStack[--pContext->vmCallStackPointer] = N;

//------------------------------------------------------------------------------
// JIL_POP_CS
//------------------------------------------------------------------------------
// Pop a return address from the VM's call stack

#define JIL_POP_CS(N)	N = pContext->vmpCallStack[pContext->vmCallStackPointer++];

//------------------------------------------------------------------------------
// JIL_INCDEC
//------------------------------------------------------------------------------
// "template" macro for all inc / dec instructions

#define	JIL_INCDEC(DO,OP,IN) \
	JIL_IBEGIN( IN )\
	DO(pContext, operand1)\
	handle1 = *operand1;\
	switch( handle1->type )\
	{\
		case type_int: JILGetIntHandle(handle1)->l OP 1; break;\
		case type_float: JILGetFloatHandle(handle1)->f OP 1.0; break;\
		default: JIL_THROW(JIL_VM_Unsupported_Type)\
	}\
	JIL_IEND

//------------------------------------------------------------------------------
// JIL_MOVEH
//------------------------------------------------------------------------------
// "template" macro for all moveh instructions

#define JIL_MOVEH(DO,IN) \
	JIL_IBEGIN( IN )\
	hObj = pDataSegment[JIL_GET_DATA(pState)].index;\
	DO(pContext, operand1)\
	JIL_STORE_HANDLE(pState, operand1, pState->vmppHandles[hObj]);\
	JIL_IEND

//------------------------------------------------------------------------------
// JIL_NEG
//------------------------------------------------------------------------------
// "template" macro for all neg instructions

#define JIL_NEG(DO,IN) \
	JIL_IBEGIN( IN )\
	DO(pContext, operand1)\
	handle1 = *operand1;\
	switch( handle1->type )\
	{\
		case type_int: JILGetIntHandle(handle1)->l = -JILGetIntHandle(handle1)->l; break;\
		case type_float: JILGetFloatHandle(handle1)->f = -JILGetFloatHandle(handle1)->f; break;\
		default: JIL_THROW(JIL_VM_Unsupported_Type)\
	}\
	JIL_IEND

//------------------------------------------------------------------------------
// JIL_TSTB
//------------------------------------------------------------------------------
// "template" macro for all test-and-branch instructions

#define JIL_TSTB(DO,OP,IN) \
	JIL_IBEGIN( IN )\
	DO(pContext, operand1)\
	offs = JIL_GET_DATA(pState);\
	handle1 = *operand1;\
	JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(handle1->type != type_int, JIL_VM_Unsupported_Type) )\
	if( JILGetIntHandle(handle1)->l OP 0 ) {\
		programCounter += offs;\
		JIL_IENDBR\
	}\
	JIL_IEND

//------------------------------------------------------------------------------
// JIL_ADDSUB
//------------------------------------------------------------------------------
// "template" macro for all add, sub, mul instructions

#define JIL_ADDSUB(SO,DO,OP,IN) \
	JIL_IBEGIN( IN )\
	SO(pContext, operand1)\
	DO(pContext, operand2)\
	handle1 = *operand1;\
	handle2 = *operand2;\
	JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(handle1->type != handle2->type, JIL_VM_Type_Mismatch) )\
	switch( handle1->type )\
	{\
		case type_int: JILGetIntHandle(handle2)->l OP JILGetIntHandle(handle1)->l; break;\
		case type_float: JILGetFloatHandle(handle2)->f OP JILGetFloatHandle(handle1)->f; break;\
		default: JIL_THROW(JIL_VM_Unsupported_Type)\
	}\
	JIL_IEND

//------------------------------------------------------------------------------
// JIL_DIV
//------------------------------------------------------------------------------
// "template" macro for all div instructions

#define JIL_DIV(SO,DO,IN) \
	JIL_IBEGIN( IN )\
	SO(pContext, operand1)\
	DO(pContext, operand2)\
	handle1 = *operand1;\
	handle2 = *operand2;\
	JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(handle1->type != handle2->type, JIL_VM_Type_Mismatch) )\
	switch( handle1->type )\
	{\
		case type_int:\
			JIL_THROW_IF(!JILGetIntHandle(handle1)->l, JIL_VM_Division_By_Zero)\
			JILGetIntHandle(handle2)->l /= JILGetIntHandle(handle1)->l; break;\
		case type_float:\
			JIL_THROW_IF(!JILGetFloatHandle(handle1)->f, JIL_VM_Division_By_Zero)\
			JILGetFloatHandle(handle2)->f /= JILGetFloatHandle(handle1)->f; break;\
		default: JIL_THROW(JIL_VM_Unsupported_Type)\
	}\
	JIL_IEND

//------------------------------------------------------------------------------
// JIL_MODULO
//------------------------------------------------------------------------------
// "template" macro for all mod instructions

#define JIL_MODULO(SO,DO,IN) \
	JIL_IBEGIN( IN )\
	SO(pContext, operand1)\
	DO(pContext, operand2)\
	handle1 = *operand1;\
	handle2 = *operand2;\
	JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(handle1->type != handle2->type, JIL_VM_Type_Mismatch) )\
	switch( handle1->type )\
	{\
		case type_int:\
			JIL_THROW_IF(!JILGetIntHandle(handle1)->l, JIL_VM_Division_By_Zero)\
			JILGetIntHandle(handle2)->l %= JILGetIntHandle(handle1)->l; break;\
		case type_float:\
			JIL_THROW_IF(!JILGetFloatHandle(handle1)->f, JIL_VM_Division_By_Zero)\
			JILGetFloatHandle(handle2)->f = fmod(JILGetFloatHandle(handle2)->f, JILGetFloatHandle(handle1)->f); break;\
		default: JIL_THROW(JIL_VM_Unsupported_Type)\
	}\
	JIL_IEND

//------------------------------------------------------------------------------
// JIL_ANDOR
//------------------------------------------------------------------------------
// "template" macro for all binary and / or / xor instructions

#define JIL_ANDOR(SO,DO,OP,IN) \
	JIL_IBEGIN( IN )\
	SO(pContext, operand1)\
	DO(pContext, operand2)\
	handle1 = *operand1;\
	handle2 = *operand2;\
	JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(handle1->type != handle2->type, JIL_VM_Type_Mismatch) )\
	JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(handle1->type != type_int, JIL_VM_Unsupported_Type) )\
	JILGetIntHandle(handle2)->l OP JILGetIntHandle(handle1)->l;\
	JIL_IEND

//------------------------------------------------------------------------------
// JIL_LSLLSR
//------------------------------------------------------------------------------
// "template" macro for all lsl / lsr instructions

#define JIL_LSLLSR(SO,DO,OP,IN) \
	JIL_IBEGIN( IN )\
	SO(pContext, operand1)\
	DO(pContext, operand2)\
	handle1 = *operand1;\
	handle2 = *operand2;\
	JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(handle1->type != handle2->type, JIL_VM_Type_Mismatch) )\
	JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(handle1->type != type_int, JIL_VM_Unsupported_Type) )\
	JILGetIntHandle(handle2)->l = (JILLong)(((unsigned int)JILGetIntHandle(handle2)->l) OP ((unsigned int)JILGetIntHandle(handle1)->l));\
	JIL_IEND

//------------------------------------------------------------------------------
// JIL_POPEA
//------------------------------------------------------------------------------
// "template" macro for all pop ea instructions

#define JIL_POPEA(DO,IN) \
	JIL_IBEGIN( IN )\
	DO(pContext, operand1)\
	JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(pContext->vmDataStackPointer >= pState->vmDataStackSize, JIL_VM_Stack_Overflow) )\
	handle1 = pContext->vmppDataStack[pContext->vmDataStackPointer++];\
	JIL_STORE_HANDLE(pState, operand1, handle1);\
	JILRelease(pState, handle1);\
	JIL_IEND

//------------------------------------------------------------------------------
// JIL_PUSHEA
//------------------------------------------------------------------------------
// "template" macro for all push ea instructions

#define JIL_PUSHEA(DO,IN) \
	JIL_IBEGIN( IN )\
	DO(pContext, operand1)\
	JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(pContext->vmDataStackPointer <= 0, JIL_VM_Stack_Overflow) )\
	JILAddRef(*operand1);\
	pContext->vmppDataStack[--pContext->vmDataStackPointer] = *operand1;\
	JIL_IEND

//------------------------------------------------------------------------------
// JIL_COPYH
//------------------------------------------------------------------------------
// "template" macro for all copyh instructions

#define JIL_COPYH(DO,IN) \
	JIL_IBEGIN( IN )\
	hObj = pDataSegment[JIL_GET_DATA(pState)].index;\
	DO(pContext, operand1)\
	JIL_THROW( JILCopyHandle(pState, pState->vmppHandles[hObj], &pNewHandle) )\
	JIL_STORE_HANDLE(pState, operand1, pNewHandle);\
	JILRelease(pState, pNewHandle);\
	pNewHandle = NULL;\
	JIL_IEND

//------------------------------------------------------------------------------
// JIL_CMPS
//------------------------------------------------------------------------------
// "template" macro for all compare-and-set instructions

#define JIL_CMPS(SO,DO,OP,IN) \
	JIL_IBEGIN( IN )\
	pNewHandle = JILGetNewHandle(pState);\
	SO(pContext, operand1)\
	DO(pContext, operand2)\
	JIL_LEA_R(pContext, operand3)\
	handle1 = *operand1;\
	handle2 = *operand2;\
	JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(handle1->type != handle2->type, JIL_VM_Type_Mismatch) )\
	switch( handle1->type )\
	{\
		case type_int: offs = (JILGetIntHandle(handle2)->l OP JILGetIntHandle(handle1)->l); break;\
		case type_float: offs = (JILGetFloatHandle(handle2)->f OP JILGetFloatHandle(handle1)->f); break;\
		default: JIL_THROW(JIL_VM_Unsupported_Type)\
	}\
	pNewHandle->type = type_int;\
	JILGetIntHandle(pNewHandle)->l = offs;\
	JIL_STORE_HANDLE(pState, operand3, pNewHandle);\
	JILRelease(pState, pNewHandle);\
	pNewHandle = NULL;\
	JIL_IEND

//------------------------------------------------------------------------------
// JIL_SNUL
//------------------------------------------------------------------------------
// "template" for all the snul instructions

#define JIL_SNUL(SO,IN) \
	JIL_IBEGIN( IN )\
	pNewHandle = JILGetNewHandle(pState);\
	SO(pContext, operand1)\
	JIL_LEA_R(pContext, operand2)\
	handle1 = *operand1;\
	pNewHandle->type = type_int;\
	JILGetIntHandle(pNewHandle)->l = (handle1->type == type_null);\
	JIL_STORE_HANDLE(pState, operand2, pNewHandle);\
	JILRelease(pState, pNewHandle);\
	pNewHandle = NULL;\
	JIL_IEND

//------------------------------------------------------------------------------
// JIL_SNNUL
//------------------------------------------------------------------------------
// "template" for all the snnul instructions

#define JIL_SNNUL(SO,IN) \
	JIL_IBEGIN( IN )\
	pNewHandle = JILGetNewHandle(pState);\
	SO(pContext, operand1)\
	JIL_LEA_R(pContext, operand2)\
	handle1 = *operand1;\
	pNewHandle->type = type_int;\
	JILGetIntHandle(pNewHandle)->l = (handle1->type != type_null);\
	JIL_STORE_HANDLE(pState, operand2, pNewHandle);\
	JILRelease(pState, pNewHandle);\
	pNewHandle = NULL;\
	JIL_IEND

//------------------------------------------------------------------------------
// JIL_NOTUNOT
//------------------------------------------------------------------------------
// "template" macro for all not and unot instructions

#define JIL_NOTUNOT(DO,OP,IN) \
	JIL_IBEGIN( IN )\
	DO(pContext, operand1)\
	handle1 = *operand1;\
	JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(handle1->type != type_int, JIL_VM_Unsupported_Type) )\
	JILGetIntHandle(handle1)->l = OP (JILGetIntHandle(handle1)->l);\
	JIL_IEND

//------------------------------------------------------------------------------
// JIL_MOVE
//------------------------------------------------------------------------------
// Template macro for all the move instructions.

#define JIL_MOVE(SO,DO,IN) \
	JIL_IBEGIN( IN )\
	SO(pContext, operand1)\
	DO(pContext, operand2)\
	JIL_STORE_HANDLE(pState, operand2, *operand1);\
	JIL_IEND

//------------------------------------------------------------------------------
// JIL_COPY
//------------------------------------------------------------------------------
// Template macro for all the copy instructions.

#define JIL_COPY(SO,DO,IN) \
	JIL_IBEGIN( IN )\
	SO(pContext, operand1)\
	DO(pContext, operand2)\
	switch((*operand1)->type) {\
		case type_int:\
			pNewHandle = JILGetNewHandle(pState);\
			pNewHandle->type = type_int;\
			JILGetIntHandle(pNewHandle)->l = JILGetIntHandle(*operand1)->l;\
			break;\
		case type_float:\
			pNewHandle = JILGetNewHandle(pState);\
			pNewHandle->type = type_float;\
			JILGetFloatHandle(pNewHandle)->f = JILGetFloatHandle(*operand1)->f;\
			break;\
		default:\
			JIL_THROW( JILCopyHandle(pState, *operand1, &pNewHandle) )\
			break;\
		}\
	JIL_STORE_HANDLE(pState, operand2, pNewHandle);\
	JILRelease(pState, pNewHandle);\
	pNewHandle = NULL;\
	JIL_IEND

//------------------------------------------------------------------------------
// JIL_CMPSTR
//------------------------------------------------------------------------------
// "template" for all string compare instructions

#define JIL_CMPSTR(SO,DO,OP,IN) \
	JIL_IBEGIN( IN )\
	pNewHandle = JILGetNewHandle(pState);\
	SO(pContext, operand1)\
	DO(pContext, operand2)\
	JIL_LEA_R(pContext, operand3)\
	handle1 = *operand1;\
	handle2 = *operand2;\
	JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(handle1->type != handle2->type, JIL_VM_Type_Mismatch) )\
	JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(handle1->type != type_string, JIL_VM_Unsupported_Type) )\
	pNewHandle->type = type_int;\
	JILGetIntHandle(pNewHandle)->l = OP(JILGetStringHandle(handle2)->str, JILGetStringHandle(handle1)->str);\
	JIL_STORE_HANDLE(pState, operand3, pNewHandle);\
	JILRelease(pState, pNewHandle);\
	pNewHandle = NULL;\
	JIL_IEND

//------------------------------------------------------------------------------
// JIL_STRADD
//------------------------------------------------------------------------------
// Template macro for all the stradd instructions.

#define JIL_STRADD(SO,DO,IN) \
	JIL_IBEGIN( IN )\
	SO(pContext, operand1)\
	DO(pContext, operand2)\
	handle1 = *operand1;\
	handle2 = *operand2;\
	JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(handle1->type != handle2->type, JIL_VM_Type_Mismatch) )\
	JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(handle1->type != type_string, JIL_VM_Unsupported_Type) )\
	JILString_Append(JILGetStringHandle(handle2)->str, JILGetStringHandle(handle1)->str);\
	JIL_IEND

//------------------------------------------------------------------------------
// JIL_ARRADD
//------------------------------------------------------------------------------
// Template macro for all the arradd instructions.

#define JIL_ARRADD(SO,DO,IN,FN) \
	JIL_IBEGIN( IN )\
	SO(pContext, operand1)\
	DO(pContext, operand2)\
	handle2 = *operand2;\
	JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(handle2->type != type_array, JIL_VM_Unsupported_Type) )\
	FN(JILGetArrayHandle(handle2)->arr, *operand1);\
	JIL_IEND

//------------------------------------------------------------------------------
// JIL_ADDSUBL
//------------------------------------------------------------------------------
// "template" macro for long add, sub, mul instructions

#define JIL_ADDSUBL(SO,DO,OP,IN) \
	JIL_IBEGIN( IN )\
	SO(pContext, operand1)\
	DO(pContext, operand2)\
	handle1 = *operand1;\
	handle2 = *operand2;\
	JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(handle2->type != type_int, JIL_VM_Unsupported_Type) )\
	JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(handle1->type != handle2->type, JIL_VM_Type_Mismatch) )\
	JILGetIntHandle(handle2)->l OP JILGetIntHandle(handle1)->l;\
	JIL_IEND

//------------------------------------------------------------------------------
// JIL_DIVL
//------------------------------------------------------------------------------
// "template" macro for long div / mod instructions

#define JIL_DIVL(SO,DO,OP,IN) \
	JIL_IBEGIN( IN )\
	SO(pContext, operand1)\
	DO(pContext, operand2)\
	handle1 = *operand1;\
	handle2 = *operand2;\
	JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(handle2->type != type_int, JIL_VM_Unsupported_Type) )\
	JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(handle1->type != handle2->type, JIL_VM_Type_Mismatch) )\
	JIL_THROW_IF(!JILGetIntHandle(handle1)->l, JIL_VM_Division_By_Zero)\
	JILGetIntHandle(handle2)->l OP JILGetIntHandle(handle1)->l;\
	JIL_IEND

//------------------------------------------------------------------------------
// JIL_INCDECL
//------------------------------------------------------------------------------
// "template" macro for long inc / dec instructions

#define	JIL_INCDECL(DO,OP,IN) \
	JIL_IBEGIN( IN )\
	DO(pContext, operand1)\
	handle1 = *operand1;\
	JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(handle1->type != type_int, JIL_VM_Unsupported_Type) )\
	JILGetIntHandle(handle1)->l OP 1;\
	JIL_IEND

//------------------------------------------------------------------------------
// JIL_NEGL
//------------------------------------------------------------------------------
// "template" macro for long neg instructions

#define JIL_NEGL(DO,IN) \
	JIL_IBEGIN( IN )\
	DO(pContext, operand1)\
	handle1 = *operand1;\
	JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(handle1->type != type_int, JIL_VM_Unsupported_Type) )\
	JILGetIntHandle(handle1)->l = -JILGetIntHandle(handle1)->l;\
	JIL_IEND

//------------------------------------------------------------------------------
// JIL_CMPSL
//------------------------------------------------------------------------------
// "template" macro for long compare-and-set instructions

#define JIL_CMPSL(SO,DO,OP,IN) \
	JIL_IBEGIN( IN )\
	pNewHandle = JILGetNewHandle(pState);\
	SO(pContext, operand1)\
	DO(pContext, operand2)\
	JIL_LEA_R(pContext, operand3)\
	handle1 = *operand1;\
	handle2 = *operand2;\
	JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(handle2->type != type_int, JIL_VM_Unsupported_Type) )\
	JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(handle1->type != handle2->type, JIL_VM_Type_Mismatch) )\
	pNewHandle->type = type_int;\
	JILGetIntHandle(pNewHandle)->l = (JILGetIntHandle(handle2)->l OP JILGetIntHandle(handle1)->l);\
	JIL_STORE_HANDLE(pState, operand3, pNewHandle);\
	JILRelease(pState, pNewHandle);\
	pNewHandle = NULL;\
	JIL_IEND

//------------------------------------------------------------------------------
// JIL_ADDSUBF
//------------------------------------------------------------------------------
// "template" macro for float add, sub, mul instructions

#define JIL_ADDSUBF(SO,DO,OP,IN) \
	JIL_IBEGIN( IN )\
	SO(pContext, operand1)\
	DO(pContext, operand2)\
	handle1 = *operand1;\
	handle2 = *operand2;\
	JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(handle2->type != type_float, JIL_VM_Unsupported_Type) )\
	JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(handle1->type != handle2->type, JIL_VM_Type_Mismatch) )\
	JILGetFloatHandle(handle2)->f OP JILGetFloatHandle(handle1)->f;\
	JIL_IEND

//------------------------------------------------------------------------------
// JIL_DIVF
//------------------------------------------------------------------------------
// "template" macro for float div instructions

#define JIL_DIVF(SO,DO,IN) \
	JIL_IBEGIN( IN )\
	SO(pContext, operand1)\
	DO(pContext, operand2)\
	handle1 = *operand1;\
	handle2 = *operand2;\
	JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(handle2->type != type_float, JIL_VM_Unsupported_Type) )\
	JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(handle1->type != handle2->type, JIL_VM_Type_Mismatch) )\
	JIL_THROW_IF(!JILGetFloatHandle(handle1)->f, JIL_VM_Division_By_Zero)\
	JILGetFloatHandle(handle2)->f /= JILGetFloatHandle(handle1)->f;\
	JIL_IEND

//------------------------------------------------------------------------------
// JIL_MODF
//------------------------------------------------------------------------------
// "template" macro for all float mod instructions

#define JIL_MODF(SO,DO,IN) \
	JIL_IBEGIN( IN )\
	SO(pContext, operand1)\
	DO(pContext, operand2)\
	handle1 = *operand1;\
	handle2 = *operand2;\
	JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(handle2->type != type_float, JIL_VM_Unsupported_Type) )\
	JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(handle1->type != handle2->type, JIL_VM_Type_Mismatch) )\
	JIL_THROW_IF(!JILGetFloatHandle(handle1)->f, JIL_VM_Division_By_Zero)\
	JILGetFloatHandle(handle2)->f = fmod(JILGetFloatHandle(handle2)->f, JILGetFloatHandle(handle1)->f);\
	JIL_IEND

//------------------------------------------------------------------------------
// JIL_INCDECF
//------------------------------------------------------------------------------
// "template" macro for long inc / dec instructions

#define	JIL_INCDECF(DO,OP,IN) \
	JIL_IBEGIN( IN )\
	DO(pContext, operand1)\
	handle1 = *operand1;\
	JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(handle1->type != type_float, JIL_VM_Unsupported_Type) )\
	JILGetFloatHandle(handle1)->f OP 1.0;\
	JIL_IEND

//------------------------------------------------------------------------------
// JIL_NEGF
//------------------------------------------------------------------------------
// "template" macro for long neg instructions

#define JIL_NEGF(DO,IN) \
	JIL_IBEGIN( IN )\
	DO(pContext, operand1)\
	handle1 = *operand1;\
	JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(handle1->type != type_float, JIL_VM_Unsupported_Type) )\
	JILGetFloatHandle(handle1)->f = -JILGetFloatHandle(handle1)->f;\
	JIL_IEND

//------------------------------------------------------------------------------
// JIL_CMPSF
//------------------------------------------------------------------------------
// "template" macro for long compare-and-set instructions

#define JIL_CMPSF(SO,DO,OP,IN) \
	JIL_IBEGIN( IN )\
	pNewHandle = JILGetNewHandle(pState);\
	SO(pContext, operand1)\
	DO(pContext, operand2)\
	JIL_LEA_R(pContext, operand3)\
	handle1 = *operand1;\
	handle2 = *operand2;\
	JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(handle2->type != type_float, JIL_VM_Unsupported_Type) )\
	JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(handle1->type != handle2->type, JIL_VM_Type_Mismatch) )\
	pNewHandle->type = type_int;\
	JILGetIntHandle(pNewHandle)->l = (JILGetFloatHandle(handle2)->f OP JILGetFloatHandle(handle1)->f);\
	JIL_STORE_HANDLE(pState, operand3, pNewHandle);\
	JILRelease(pState, pNewHandle);\
	pNewHandle = NULL;\
	JIL_IEND

//------------------------------------------------------------------------------
// JIL_JSREA
//------------------------------------------------------------------------------
// "template" macro for all JSR [ea] instructions

#define JIL_JSREA(DO,IN) \
	JIL_IBEGIN( IN )\
	DO(pContext, operand1)\
	handle1 = *operand1;\
	JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(handle1->type != type_int, JIL_VM_Call_To_NonFunction) )\
	JIL_PUSH_CS( programCounter + instruction_size )\
	programCounter = JILGetIntHandle(handle1)->l;\
	JIL_IENDBR

//------------------------------------------------------------------------------
// JIL_RTCHKEA
//------------------------------------------------------------------------------
// "template" macro for all RTCHK [ea] instructions

#define JIL_RTCHKEA(DO,IN) \
	JIL_IBEGIN( IN )\
	offs = JIL_GET_DATA(pState);\
	DO(pContext, operand1)\
	JIL_THROW_IF(JILRTCheck(pState, offs, *operand1), JIL_VM_Type_Mismatch);\
	JIL_IEND

//------------------------------------------------------------------------------
// JIL_RESU
//------------------------------------------------------------------------------
// "template" macro for all RESUME [ea] instructions

#define JIL_RESU(DO,IN) \
	JIL_IBEGIN( IN )\
	DO(pContext, operand1)\
	handle1 = *operand1;\
	JIL_INSERT_DEBUG_CODE( typeInfo = JILTypeInfoFromType(pState, handle1->type) );\
	JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(typeInfo->family != tf_thread, JIL_VM_Unsupported_Type) );\
	pContext->vmProgramCounter = programCounter + instruction_size;\
	JILGetContextHandle(handle1)->pContext->vmpYieldContext = pContext;\
	pState->vmpContext = pContext = JILGetContextHandle(handle1)->pContext;\
	programCounter = pContext->vmProgramCounter;\
	JIL_IENDBR

//------------------------------------------------------------------------------
// JIL_WREF
//------------------------------------------------------------------------------
// Template macro for all the wref instructions.

#define JIL_WREF(SO,DO,IN) \
	JIL_IBEGIN( IN )\
	pNewHandle = JILGetNewHandle(pState);\
	SO(pContext, operand1)\
	DO(pContext, operand2)\
	handle1 = *operand1;\
	pNewHandle->type = handle1->type;\
	pNewHandle->flags |= HF_PERSIST;\
	memcpy(pNewHandle->data, handle1->data, sizeof(JILHandleData));\
	JIL_STORE_HANDLE(pState, operand2, pNewHandle);\
	JILRelease( pState, pNewHandle );\
	pNewHandle = NULL;\
	JIL_IEND

//------------------------------------------------------------------------------
// JIL_CALLDG
//------------------------------------------------------------------------------
// "template" macro for all call delegate instructions. This instruction can
// in certain cases modify register R0, so R0 should be saved to the stack!

#define JIL_CALLDG(DO,IN) \
	JIL_IBEGIN( IN )\
	DO(pContext, operand1)\
	handle1 = *operand1;\
	if(handle1->type == type_null) { JIL_STORE_HANDLE(pState, (pContext->vmppRegister + 1), *pState->vmppHandles); JIL_IEND }\
	JIL_INSERT_DEBUG_CODE( typeInfo = JILTypeInfoFromType(pState, handle1->type) );\
	JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(typeInfo->family != tf_delegate, JIL_VM_Unsupported_Type) );\
	pState->errProgramCounter = pContext->vmProgramCounter = programCounter;\
	JIL_PUSH_CS(programCounter + instruction_size)\
	result = JILCallDelegate(pState, handle1);\
	JIL_POP_CS(i)\
	JIL_THROW( result )\
	JIL_IEND

#endif	// #ifndef JILOPMACROS_H
