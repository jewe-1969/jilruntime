//------------------------------------------------------------------------------
// File: JILExecByteCode.c                                     (c) 2005 jewe.org
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
//	The bytecode execution function. This function runs until the virtual
//  machine code has run through and returns. So, if the virtual machine
//	code contains an infinite loop, this function will never return.
//------------------------------------------------------------------------------

#include "jilstdinc.h"

#include "jilopcodes.h"
#include "jildebug.h"
#include "jilhandle.h"
#include "jilallocators.h"
#include "jilopmacros.h"
#include "jilcallntl.h"
#include "jilmachine.h"

//------------------------------------------------------------------------------
// External references
//------------------------------------------------------------------------------

JILError	JILGenerateException	(JILState* pState, JILError e);

//------------------------------------------------------------------------------
// JILExecuteInfinite
//------------------------------------------------------------------------------
// Executes bytecode until the entry-point function returns with a 'RET'
// instruction or an unhandled exception is thrown.
// In other words: If the virtual machine program performs a while(1); this
// function will run infinitely.

JILLong JILExecuteInfinite( JILState* pState, JILContext* pContext )
{
	register JILLong* pInstruction;
	JILLong programCounter;

	JILLong result, instruction_size;
	JILHandle **operand1, **operand2, **operand3;
	JILHandle *handle1, *handle2, *pNewHandle;
	JILHandleArray* pHArray;
	JILHandleObject* pHObject;
	JILHandleInt* pHLong;
	JILTypeInfo* typeInfo;
	JILFuncInfo* funcInfo;
	JILLong hObj, offs, i;
	JILLong* pCodeSegment = pState->vmpCodeSegment->pData;
	JILDataHandle* pDataSegment = pState->vmpDataSegment->pData;

	pState->vmRunLevel++;
	pState->vmRunning = (pState->vmRunLevel > 0);
	do
	{
		programCounter = pContext->vmProgramCounter;
		pInstruction = pCodeSegment + programCounter;
		pNewHandle = NULL;
		for(;;)
		{
			// TRACE exception
			#if defined(_DEBUG) || JIL_TRACE_RELEASE
			if( pState->vmTraceFlag )
			{
				pState->errProgramCounter = pContext->vmProgramCounter = programCounter;
				if( (result = JILGenerateException(pState, JIL_VM_Trace_Exception)) )
					goto terminate;
			}
			#endif
			#if JIL_USE_INSTRUCTION_COUNTER
			pState->vmInstructionCounter++;
			#endif
			// dispatch instruction
			switch( *pInstruction++ )
			{
				case op_nop:
					programCounter++;
					break;
				case op_alloc:
					JIL_IBEGIN( 3 )
					pNewHandle = JILGetNewHandle(pState);
					typeInfo = JILTypeInfoFromType(pState, JIL_GET_DATA(pState));
					JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(typeInfo->family != tf_class, JIL_VM_Unsupported_Type) )
					JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(typeInfo->isNative, JIL_VM_Unsupported_Type) )
					JIL_LEA_R(pContext, operand1)
					offs = typeInfo->instanceSize;
					pNewHandle->type = typeInfo->type;
					JILGetObjectHandle(pNewHandle)->ppHandles = JILAllocObject(pState, offs);
					JIL_STORE_HANDLE(pState, operand1, pNewHandle);
					JILRelease(pState, pNewHandle);
					pNewHandle = NULL;
					JIL_IEND
				case op_alloca:
					JIL_IBEGIN( 4 )
					pNewHandle = JILGetNewHandle(pState);
					hObj = JIL_GET_DATA(pContext);
					offs = JIL_GET_DATA(pContext);
					JIL_LEA_R(pContext, operand1)
					JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(offs < 0, JIL_VM_Invalid_Operand) )
					pNewHandle->type = type_array;
					JILGetArrayHandle(pNewHandle)->arr = JILAllocArrayMulti(pState, hObj, offs, 0);
					JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(JILGetArrayHandle(pNewHandle)->arr == NULL, JIL_VM_Invalid_Operand) )
					JIL_STORE_HANDLE(pState, operand1, pNewHandle);
					JILRelease(pState, pNewHandle);
					pNewHandle = NULL;
					JIL_IEND
				case op_allocn:
				{
					void* objptr = NULL;
					JIL_IBEGIN( 3 )
					pNewHandle = JILGetNewHandle(pState);
					typeInfo = JILTypeInfoFromType(pState, JIL_GET_DATA(pState));
					JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(typeInfo->family != tf_class, JIL_VM_Unsupported_Type) )
					JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(!typeInfo->isNative, JIL_VM_Unsupported_Type) )
					JIL_LEA_R(pContext, operand1)
					result = CallNTLNewObject(typeInfo, &objptr);
					JIL_THROW_IF( result != 0 || objptr == NULL, JIL_VM_Allocation_Failed )
					pNewHandle->type = typeInfo->type;
					JILGetNObjectHandle(pNewHandle)->ptr = objptr;
					JIL_STORE_HANDLE(pState, operand1, pNewHandle);
					JILRelease(pState, pNewHandle);
					pNewHandle = NULL;
					JIL_IEND
				}
				case op_bra:
					JIL_IBEGIN(2)
					programCounter += JIL_GET_DATA(pState);
					JIL_IENDBR
				case op_brk:
					JIL_IBEGIN( 1 )
					JIL_THROW( JIL_VM_Break_Exception )
					break;
				case op_callm:
				{
					JIL_IBEGIN( 3 )
					hObj = JIL_GET_DATA(pState);
					i = JIL_GET_DATA(pState);
					// get native object address from R0
					handle1 = pContext->vmppRegister[0];
					// get typeinfo from operand
					typeInfo = JILTypeInfoFromType(pState, handle1->type);
					// check if we have a class
					JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(typeInfo->family != tf_class, JIL_VM_Unsupported_Type) );
					// check if the types match
					JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(hObj != typeInfo->type && hObj != typeInfo->base, JIL_VM_Type_Mismatch) );
					if( typeInfo->isNative )
					{
						// call native callback proc
						pState->errProgramCounter = pContext->vmProgramCounter = programCounter;
						JIL_PUSH_CS(programCounter + instruction_size)
						result = CallNTLCallMember(typeInfo, i, JILGetNObjectHandle(handle1)->ptr);
						JIL_POP_CS(i)
						JIL_THROW( result )
						JIL_IEND
					}
					else
					{
						// access v-table
						JILLong* pVt = JILCStrGetVTable(pState, typeInfo->offsetVtab);
						funcInfo = pState->vmpFuncSegment->pData + pVt[i];
						JIL_PUSH_CS( programCounter + instruction_size )
						programCounter = funcInfo->codeAddr;
						JIL_IENDBR
					}
				}
				case op_calls:
					JIL_IBEGIN( 2 )
					funcInfo = pState->vmpFuncSegment->pData + JIL_GET_DATA(pState);
					JIL_PUSH_CS( programCounter + instruction_size )
					programCounter = funcInfo->codeAddr;
					JIL_IENDBR
				case op_calln:
					JIL_IBEGIN( 3 )
					typeInfo = JILTypeInfoFromType(pState, JIL_GET_DATA(pState));
					offs = JIL_GET_DATA(pState);
					JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(typeInfo->family != tf_class, JIL_VM_Unsupported_Type) )
					JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(!typeInfo->isNative, JIL_VM_Unsupported_Type) )
					pState->errProgramCounter = pContext->vmProgramCounter = programCounter;
					JIL_PUSH_CS(programCounter + instruction_size)
					result = CallNTLCallStatic(typeInfo, offs);
					JIL_POP_CS(i)
					JIL_THROW( result )
					JIL_IEND
				case op_cvf:
					JIL_IBEGIN( 3 )
					pNewHandle = JILGetNewHandle(pState);
					JIL_LEA_R(pContext, operand1)
					JIL_LEA_R(pContext, operand2)
					handle1 = *operand1;
					JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(handle1->type != type_int, JIL_VM_Unsupported_Type) )
					pNewHandle->type = type_float;
					JILGetFloatHandle(pNewHandle)->f = (JILFloat) JILGetIntHandle(handle1)->l;
					JIL_STORE_HANDLE(pState, operand2, pNewHandle);
					JILRelease(pState, pNewHandle);
					pNewHandle = NULL;
					JIL_IEND
				case op_cvl:
					JIL_IBEGIN( 3 )
					pNewHandle = JILGetNewHandle(pState);
					JIL_LEA_R(pContext, operand1)
					JIL_LEA_R(pContext, operand2)
					handle1 = *operand1;
					JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(handle1->type != type_float, JIL_VM_Unsupported_Type) )
					pNewHandle->type = type_int;
					JILGetIntHandle(pNewHandle)->l = (JILLong) JILGetFloatHandle(handle1)->f;
					JIL_STORE_HANDLE(pState, operand2, pNewHandle);
					JILRelease(pState, pNewHandle);
					pNewHandle = NULL;
					JIL_IEND
				case op_popm:
					JIL_IBEGIN( 2 )
					offs = JIL_GET_DATA(pState);
					JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(offs < 0, JIL_VM_Invalid_Operand) )
					for( i = 0; i < offs; i++ )
					{
						JIL_INSERT_DEBUG_CODE(
							JIL_THROW_IF(pContext->vmDataStackPointer >= pState->vmDataStackSize, JIL_VM_Stack_Overflow)
						)
						JILRelease(pState, pContext->vmppDataStack[pContext->vmDataStackPointer++]);
					}
					JIL_IEND
				case op_popr:
					JIL_IBEGIN( 3 )
					offs = JIL_GET_DATA(pState);
					i  = JIL_GET_DATA(pState);
					operand1 = pContext->vmppRegister + (offs + i - 1);
					// pop handles from stack into registers
					while( i-- )
					{
						JIL_INSERT_DEBUG_CODE(
							JIL_THROW_IF(pContext->vmDataStackPointer >= pState->vmDataStackSize, JIL_VM_Stack_Overflow)
						)
						JILRelease(pState, *operand1);
						*operand1-- = pContext->vmppDataStack[pContext->vmDataStackPointer++];
					}
					JIL_IEND
				case op_pushm:
					JIL_IBEGIN( 2 )
					offs = JIL_GET_DATA(pState);
					JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(offs < 0, JIL_VM_Invalid_Operand) )
					JIL_INSERT_DEBUG_CODE( JIL_THROW_IF((pContext->vmDataStackPointer - offs) <= 0, JIL_VM_Stack_Overflow) )
					handle1 = JILGetNullHandle(pState);
					for( i = 0; i < offs; i++ )
						pContext->vmppDataStack[--pContext->vmDataStackPointer] = handle1;
					handle1->refCount += offs;
					JIL_IEND
				case op_pushr:
					JIL_IBEGIN( 3 )
					offs = JIL_GET_DATA(pState);
					i  = JIL_GET_DATA(pState);
					operand1 = pContext->vmppRegister + offs;
					while( i-- )
					{
						JIL_INSERT_DEBUG_CODE(
							JIL_THROW_IF(pContext->vmDataStackPointer <= 0, JIL_VM_Stack_Overflow)
						)
						JILAddRef(*operand1);
						pContext->vmppDataStack[--pContext->vmDataStackPointer] = *operand1++;
					}
					JIL_IEND
				case op_ret:
					// pop return address from stack
					JIL_POP_CS( offs )
					// check if we must return to native
					if( offs == kReturnToNative )
					{
						pState->vmRunLevel--;
						pState->vmRunning = (pState->vmRunLevel > 0);
						pContext->vmProgramCounter = programCounter;
						return JIL_No_Exception;
					}
					programCounter = offs;
					JIL_IENDBR
				case op_size:
					JIL_IBEGIN( 3 )
					offs = 0;
					pNewHandle = JILGetNewHandle(pState);
					JIL_LEA_R(pContext, operand1)
					JIL_LEA_R(pContext, operand2)
					handle1 = *operand1;
					switch( handle1->type )
					{
						case type_string:
							offs = JILGetStringHandle(handle1)->str->length;
							break;
						case type_array:
							offs = JILGetArrayHandle(handle1)->arr->size;
							break;
						default:
							JIL_THROW( JIL_VM_Unsupported_Type )
					}
					pNewHandle->type = type_int;
					JILGetIntHandle(pNewHandle)->l = offs;
					JIL_STORE_HANDLE(pState, operand2, pNewHandle);
					JILRelease(pState, pNewHandle);
					pNewHandle = NULL;
					JIL_IEND
				case op_type:
					JIL_IBEGIN( 3 )
					offs = 0;
					pNewHandle = JILGetNewHandle(pState);
					JIL_LEA_R(pContext, operand1)
					JIL_LEA_R(pContext, operand2)
					handle1 = *operand1;
					pNewHandle->type = type_int;
					JILGetIntHandle(pNewHandle)->l = handle1->type;
					JIL_STORE_HANDLE(pState, operand2, pNewHandle);
					JILRelease(pState, pNewHandle);
					pNewHandle = NULL;
					JIL_IEND
				case op_dec_r:
					JIL_INCDEC( JIL_LEA_R, -=, 2 )
				case op_dec_d:
					JIL_INCDEC( JIL_LEA_D, -=, 3 )
				case op_dec_x:
					JIL_INCDEC( JIL_LEA_X, -=, 3 )
				case op_dec_s:
					JIL_INCDEC( JIL_LEA_S, -=, 2 )
				case op_inc_r:
					JIL_INCDEC( JIL_LEA_R, +=, 2 )
				case op_inc_d:
					JIL_INCDEC( JIL_LEA_D, +=, 3 )
				case op_inc_x:
					JIL_INCDEC( JIL_LEA_X, +=, 3 )
				case op_inc_s:
					JIL_INCDEC( JIL_LEA_S, +=, 2 )
				case op_moveh_r:
					JIL_MOVEH( JIL_LEA_R, 3 )
				case op_moveh_d:
					JIL_MOVEH( JIL_LEA_D, 4 )
				case op_moveh_x:
					JIL_MOVEH( JIL_LEA_X, 4 )
				case op_moveh_s:
					JIL_MOVEH( JIL_LEA_S, 3 )
				case op_neg_r:
					JIL_NEG( JIL_LEA_R, 2 )
				case op_neg_d:
					JIL_NEG( JIL_LEA_D, 3 )
				case op_neg_x:
					JIL_NEG( JIL_LEA_X, 3 )
				case op_neg_s:
					JIL_NEG( JIL_LEA_S, 2 )
				case op_not_r:
					JIL_NOTUNOT( JIL_LEA_R, ~, 2 )
				case op_not_d:
					JIL_NOTUNOT( JIL_LEA_D, ~, 3 )
				case op_not_x:
					JIL_NOTUNOT( JIL_LEA_X, ~, 3 )
				case op_not_s:
					JIL_NOTUNOT( JIL_LEA_S, ~, 2 )
				case op_tsteq_r:
					JIL_TSTB( JIL_LEA_R, ==, 3 )
				case op_tsteq_d:
					JIL_TSTB( JIL_LEA_D, ==, 4 )
				case op_tsteq_x:
					JIL_TSTB( JIL_LEA_X, ==, 4 )
				case op_tsteq_s:
					JIL_TSTB( JIL_LEA_S, ==, 3 )
				case op_tstne_r:
					JIL_TSTB( JIL_LEA_R, !=, 3 )
				case op_tstne_d:
					JIL_TSTB( JIL_LEA_D, !=, 4 )
				case op_tstne_x:
					JIL_TSTB( JIL_LEA_X, !=, 4 )
				case op_tstne_s:
					JIL_TSTB( JIL_LEA_S, !=, 3 )
				case op_add_rr:
					JIL_ADDSUB( JIL_LEA_R, JIL_LEA_R, +=, 3 )
				case op_add_rd:
					JIL_ADDSUB( JIL_LEA_R, JIL_LEA_D, +=, 4 )
				case op_add_rx:
					JIL_ADDSUB( JIL_LEA_R, JIL_LEA_X, +=, 4 )
				case op_add_rs:
					JIL_ADDSUB( JIL_LEA_R, JIL_LEA_S, +=, 3 )
				case op_add_dr:
					JIL_ADDSUB( JIL_LEA_D, JIL_LEA_R, +=, 4 )
				case op_add_xr:
					JIL_ADDSUB( JIL_LEA_X, JIL_LEA_R, +=, 4 )
				case op_add_sr:
					JIL_ADDSUB( JIL_LEA_S, JIL_LEA_R, +=, 3 )
				case op_and_rr:
					JIL_ANDOR( JIL_LEA_R, JIL_LEA_R, &=, 3 )
				case op_and_rd:
					JIL_ANDOR( JIL_LEA_R, JIL_LEA_D, &=, 4 )
				case op_and_rx:
					JIL_ANDOR( JIL_LEA_R, JIL_LEA_X, &=, 4 )
				case op_and_rs:
					JIL_ANDOR( JIL_LEA_R, JIL_LEA_S, &=, 3 )
				case op_and_dr:
					JIL_ANDOR( JIL_LEA_D, JIL_LEA_R, &=, 4 )
				case op_and_xr:
					JIL_ANDOR( JIL_LEA_X, JIL_LEA_R, &=, 4 )
				case op_and_sr:
					JIL_ANDOR( JIL_LEA_S, JIL_LEA_R, &=, 3 )
				case op_asl_rr:
					JIL_ANDOR( JIL_LEA_R, JIL_LEA_R, <<=, 3 )
				case op_asl_rd:
					JIL_ANDOR( JIL_LEA_R, JIL_LEA_D, <<=, 4 )
				case op_asl_rx:
					JIL_ANDOR( JIL_LEA_R, JIL_LEA_X, <<=, 4 )
				case op_asl_rs:
					JIL_ANDOR( JIL_LEA_R, JIL_LEA_S, <<=, 3 )
				case op_asl_dr:
					JIL_ANDOR( JIL_LEA_D, JIL_LEA_R, <<=, 4 )
				case op_asl_xr:
					JIL_ANDOR( JIL_LEA_X, JIL_LEA_R, <<=, 4 )
				case op_asl_sr:
					JIL_ANDOR( JIL_LEA_S, JIL_LEA_R, <<=, 3 )
				case op_asr_rr:
					JIL_ANDOR( JIL_LEA_R, JIL_LEA_R, >>=, 3 )
				case op_asr_rd:
					JIL_ANDOR( JIL_LEA_R, JIL_LEA_D, >>=, 4 )
				case op_asr_rx:
					JIL_ANDOR( JIL_LEA_R, JIL_LEA_X, >>=, 4 )
				case op_asr_rs:
					JIL_ANDOR( JIL_LEA_R, JIL_LEA_S, >>=, 3 )
				case op_asr_dr:
					JIL_ANDOR( JIL_LEA_D, JIL_LEA_R, >>=, 4 )
				case op_asr_xr:
					JIL_ANDOR( JIL_LEA_X, JIL_LEA_R, >>=, 4 )
				case op_asr_sr:
					JIL_ANDOR( JIL_LEA_S, JIL_LEA_R, >>=, 3 )
				case op_div_rr:
					JIL_DIV( JIL_LEA_R, JIL_LEA_R, 3 )
				case op_div_rd:
					JIL_DIV( JIL_LEA_R, JIL_LEA_D, 4 )
				case op_div_rx:
					JIL_DIV( JIL_LEA_R, JIL_LEA_X, 4 )
				case op_div_rs:
					JIL_DIV( JIL_LEA_R, JIL_LEA_S, 3 )
				case op_div_dr:
					JIL_DIV( JIL_LEA_D, JIL_LEA_R, 4 )
				case op_div_xr:
					JIL_DIV( JIL_LEA_X, JIL_LEA_R, 4 )
				case op_div_sr:
					JIL_DIV( JIL_LEA_S, JIL_LEA_R, 3 )
				case op_lsl_rr:
					JIL_LSLLSR( JIL_LEA_R, JIL_LEA_R, <<, 3 )
				case op_lsl_rd:
					JIL_LSLLSR( JIL_LEA_R, JIL_LEA_D, <<, 4 )
				case op_lsl_rx:
					JIL_LSLLSR( JIL_LEA_R, JIL_LEA_X, <<, 4 )
				case op_lsl_rs:
					JIL_LSLLSR( JIL_LEA_R, JIL_LEA_S, <<, 3 )
				case op_lsl_dr:
					JIL_LSLLSR( JIL_LEA_D, JIL_LEA_R, <<, 4 )
				case op_lsl_xr:
					JIL_LSLLSR( JIL_LEA_X, JIL_LEA_R, <<, 4 )
				case op_lsl_sr:
					JIL_LSLLSR( JIL_LEA_S, JIL_LEA_R, <<, 3 )
				case op_lsr_rr:
					JIL_LSLLSR( JIL_LEA_R, JIL_LEA_R, >>, 3 )
				case op_lsr_rd:
					JIL_LSLLSR( JIL_LEA_R, JIL_LEA_D, >>, 4 )
				case op_lsr_rx:
					JIL_LSLLSR( JIL_LEA_R, JIL_LEA_X, >>, 4 )
				case op_lsr_rs:
					JIL_LSLLSR( JIL_LEA_R, JIL_LEA_S, >>, 3 )
				case op_lsr_dr:
					JIL_LSLLSR( JIL_LEA_D, JIL_LEA_R, >>, 4 )
				case op_lsr_xr:
					JIL_LSLLSR( JIL_LEA_X, JIL_LEA_R, >>, 4 )
				case op_lsr_sr:
					JIL_LSLLSR( JIL_LEA_S, JIL_LEA_R, >>, 3 )
				case op_mod_rr:
					JIL_MODULO( JIL_LEA_R, JIL_LEA_R, 3 )
				case op_mod_rd:
					JIL_MODULO( JIL_LEA_R, JIL_LEA_D, 4 )
				case op_mod_rx:
					JIL_MODULO( JIL_LEA_R, JIL_LEA_X, 4 )
				case op_mod_rs:
					JIL_MODULO( JIL_LEA_R, JIL_LEA_S, 3 )
				case op_mod_dr:
					JIL_MODULO( JIL_LEA_D, JIL_LEA_R, 4 )
				case op_mod_xr:
					JIL_MODULO( JIL_LEA_X, JIL_LEA_R, 4 )
				case op_mod_sr:
					JIL_MODULO( JIL_LEA_S, JIL_LEA_R, 3 )
				case op_mul_rr:
					JIL_ADDSUB( JIL_LEA_R, JIL_LEA_R, *=, 3 )
				case op_mul_rd:
					JIL_ADDSUB( JIL_LEA_R, JIL_LEA_D, *=, 4 )
				case op_mul_rx:
					JIL_ADDSUB( JIL_LEA_R, JIL_LEA_X, *=, 4 )
				case op_mul_rs:
					JIL_ADDSUB( JIL_LEA_R, JIL_LEA_S, *=, 3 )
				case op_mul_dr:
					JIL_ADDSUB( JIL_LEA_D, JIL_LEA_R, *=, 4 )
				case op_mul_xr:
					JIL_ADDSUB( JIL_LEA_X, JIL_LEA_R, *=, 4 )
				case op_mul_sr:
					JIL_ADDSUB( JIL_LEA_S, JIL_LEA_R, *=, 3 )
				case op_or_rr:
					JIL_ANDOR( JIL_LEA_R, JIL_LEA_R, |=, 3 )
				case op_or_rd:
					JIL_ANDOR( JIL_LEA_R, JIL_LEA_D, |=, 4 )
				case op_or_rx:
					JIL_ANDOR( JIL_LEA_R, JIL_LEA_X, |=, 4 )
				case op_or_rs:
					JIL_ANDOR( JIL_LEA_R, JIL_LEA_S, |=, 3 )
				case op_or_dr:
					JIL_ANDOR( JIL_LEA_D, JIL_LEA_R, |=, 4 )
				case op_or_xr:
					JIL_ANDOR( JIL_LEA_X, JIL_LEA_R, |=, 4 )
				case op_or_sr:
					JIL_ANDOR( JIL_LEA_S, JIL_LEA_R, |=, 3 )
				case op_sub_rr:
					JIL_ADDSUB( JIL_LEA_R, JIL_LEA_R, -=, 3 )
				case op_sub_rd:
					JIL_ADDSUB( JIL_LEA_R, JIL_LEA_D, -=, 4 )
				case op_sub_rx:
					JIL_ADDSUB( JIL_LEA_R, JIL_LEA_X, -=, 4 )
				case op_sub_rs:
					JIL_ADDSUB( JIL_LEA_R, JIL_LEA_S, -=, 3 )
				case op_sub_dr:
					JIL_ADDSUB( JIL_LEA_D, JIL_LEA_R, -=, 4 )
				case op_sub_xr:
					JIL_ADDSUB( JIL_LEA_X, JIL_LEA_R, -=, 4 )
				case op_sub_sr:
					JIL_ADDSUB( JIL_LEA_S, JIL_LEA_R, -=, 3 )
				case op_xor_rr:
					JIL_ANDOR( JIL_LEA_R, JIL_LEA_R, ^=, 3 )
				case op_xor_rd:
					JIL_ANDOR( JIL_LEA_R, JIL_LEA_D, ^=, 4 )
				case op_xor_rx:
					JIL_ANDOR( JIL_LEA_R, JIL_LEA_X, ^=, 4 )
				case op_xor_rs:
					JIL_ANDOR( JIL_LEA_R, JIL_LEA_S, ^=, 3 )
				case op_xor_dr:
					JIL_ANDOR( JIL_LEA_D, JIL_LEA_R, ^=, 4 )
				case op_xor_xr:
					JIL_ANDOR( JIL_LEA_X, JIL_LEA_R, ^=, 4 )
				case op_xor_sr:
					JIL_ANDOR( JIL_LEA_S, JIL_LEA_R, ^=, 3 )
				case op_move_rr:
					JIL_MOVE( JIL_LEA_R, JIL_LEA_R, 3 )
				case op_move_rd:
					JIL_MOVE( JIL_LEA_R, JIL_LEA_D, 4 )
				case op_move_rx:
					JIL_MOVE( JIL_LEA_R, JIL_LEA_X, 4 )
				case op_move_rs:
					JIL_MOVE( JIL_LEA_R, JIL_LEA_S, 3 )
				case op_move_dr:
					JIL_MOVE( JIL_LEA_D, JIL_LEA_R, 4 )
				case op_move_dd:
					JIL_MOVE( JIL_LEA_D, JIL_LEA_D, 5 )
				case op_move_dx:
					JIL_MOVE( JIL_LEA_D, JIL_LEA_X, 5 )
				case op_move_ds:
					JIL_MOVE( JIL_LEA_D, JIL_LEA_S, 4 )
				case op_move_xr:
					JIL_MOVE( JIL_LEA_X, JIL_LEA_R, 4 )
				case op_move_xd:
					JIL_MOVE( JIL_LEA_X, JIL_LEA_D, 5 )
				case op_move_xx:
					JIL_MOVE( JIL_LEA_X, JIL_LEA_X, 5 )
				case op_move_xs:
					JIL_MOVE( JIL_LEA_X, JIL_LEA_S, 4 )
				case op_move_sr:
					JIL_MOVE( JIL_LEA_S, JIL_LEA_R, 3 )
				case op_move_sd:
					JIL_MOVE( JIL_LEA_S, JIL_LEA_D, 4 )
				case op_move_sx:
					JIL_MOVE( JIL_LEA_S, JIL_LEA_X, 4 )
				case op_move_ss:
					JIL_MOVE( JIL_LEA_S, JIL_LEA_S, 3 )
				case op_ldz_r:
					JIL_IBEGIN( 2 )
					pNewHandle = JILGetNewHandle(pState);
					JIL_LEA_R(pContext, operand1)
					pNewHandle->type = type_int;
					JILGetIntHandle(pNewHandle)->l = 0;
					JIL_STORE_HANDLE(pState, operand1, pNewHandle);
					JILRelease(pState, pNewHandle);
					pNewHandle = NULL;
					JIL_IEND
				case op_copy_rr:
					JIL_COPY( JIL_LEA_R, JIL_LEA_R, 3 )
				case op_copy_rd:
					JIL_COPY( JIL_LEA_R, JIL_LEA_D, 4 )
				case op_copy_rx:
					JIL_COPY( JIL_LEA_R, JIL_LEA_X, 4 )
				case op_copy_rs:
					JIL_COPY( JIL_LEA_R, JIL_LEA_S, 3 )
				case op_copy_dr:
					JIL_COPY( JIL_LEA_D, JIL_LEA_R, 4 )
				case op_copy_dd:
					JIL_COPY( JIL_LEA_D, JIL_LEA_D, 5 )
				case op_copy_dx:
					JIL_COPY( JIL_LEA_D, JIL_LEA_X, 5 )
				case op_copy_ds:
					JIL_COPY( JIL_LEA_D, JIL_LEA_S, 4 )
				case op_copy_xr:
					JIL_COPY( JIL_LEA_X, JIL_LEA_R, 4 )
				case op_copy_xd:
					JIL_COPY( JIL_LEA_X, JIL_LEA_D, 5 )
				case op_copy_xx:
					JIL_COPY( JIL_LEA_X, JIL_LEA_X, 5 )
				case op_copy_xs:
					JIL_COPY( JIL_LEA_X, JIL_LEA_S, 4 )
				case op_copy_sr:
					JIL_COPY( JIL_LEA_S, JIL_LEA_R, 3 )
				case op_copy_sd:
					JIL_COPY( JIL_LEA_S, JIL_LEA_D, 4 )
				case op_copy_sx:
					JIL_COPY( JIL_LEA_S, JIL_LEA_X, 4 )
				case op_copy_ss:
					JIL_COPY( JIL_LEA_S, JIL_LEA_S, 3 )
				case op_pop_r:
					JIL_POPEA( JIL_LEA_R, 2 )
				case op_pop_d:
					JIL_POPEA( JIL_LEA_D, 3 )
				case op_pop_x:
					JIL_POPEA( JIL_LEA_X, 3 )
				case op_pop_s:
					JIL_POPEA( JIL_LEA_S, 2 )
				case op_push_r:
					JIL_PUSHEA( JIL_LEA_R, 2 )
				case op_push_d:
					JIL_PUSHEA( JIL_LEA_D, 3 )
				case op_push_x:
					JIL_PUSHEA( JIL_LEA_X, 3 )
				case op_push_s:
					JIL_PUSHEA( JIL_LEA_S, 2 )
				case op_copyh_r:
					JIL_COPYH( JIL_LEA_R, 3 )
				case op_copyh_d:
					JIL_COPYH( JIL_LEA_D, 4 )
				case op_copyh_x:
					JIL_COPYH( JIL_LEA_X, 4 )
				case op_copyh_s:
					JIL_COPYH( JIL_LEA_S, 3 )
				case op_cseq_rr:
					JIL_CMPS( JIL_LEA_R, JIL_LEA_R, ==, 4 )
				case op_cseq_rd:
					JIL_CMPS( JIL_LEA_R, JIL_LEA_D, ==, 5 )
				case op_cseq_rx:
					JIL_CMPS( JIL_LEA_R, JIL_LEA_X, ==, 5 )
				case op_cseq_rs:
					JIL_CMPS( JIL_LEA_R, JIL_LEA_S, ==, 4 )
				case op_cseq_dr:
					JIL_CMPS( JIL_LEA_D, JIL_LEA_R, ==, 5 )
				case op_cseq_xr:
					JIL_CMPS( JIL_LEA_X, JIL_LEA_R, ==, 5 )
				case op_cseq_sr:
					JIL_CMPS( JIL_LEA_S, JIL_LEA_R, ==, 4 )
				case op_csne_rr:
					JIL_CMPS( JIL_LEA_R, JIL_LEA_R, !=, 4 )
				case op_csne_rd:
					JIL_CMPS( JIL_LEA_R, JIL_LEA_D, !=, 5 )
				case op_csne_rx:
					JIL_CMPS( JIL_LEA_R, JIL_LEA_X, !=, 5 )
				case op_csne_rs:
					JIL_CMPS( JIL_LEA_R, JIL_LEA_S, !=, 4 )
				case op_csne_dr:
					JIL_CMPS( JIL_LEA_D, JIL_LEA_R, !=, 5 )
				case op_csne_xr:
					JIL_CMPS( JIL_LEA_X, JIL_LEA_R, !=, 5 )
				case op_csne_sr:
					JIL_CMPS( JIL_LEA_S, JIL_LEA_R, !=, 4 )
				case op_csgt_rr:
					JIL_CMPS( JIL_LEA_R, JIL_LEA_R, >, 4 )
				case op_csgt_rd:
					JIL_CMPS( JIL_LEA_R, JIL_LEA_D, >, 5 )
				case op_csgt_rx:
					JIL_CMPS( JIL_LEA_R, JIL_LEA_X, >, 5 )
				case op_csgt_rs:
					JIL_CMPS( JIL_LEA_R, JIL_LEA_S, >, 4 )
				case op_csgt_dr:
					JIL_CMPS( JIL_LEA_D, JIL_LEA_R, >, 5 )
				case op_csgt_xr:
					JIL_CMPS( JIL_LEA_X, JIL_LEA_R, >, 5 )
				case op_csgt_sr:
					JIL_CMPS( JIL_LEA_S, JIL_LEA_R, >, 4 )
				case op_csge_rr:
					JIL_CMPS( JIL_LEA_R, JIL_LEA_R, >=, 4 )
				case op_csge_rd:
					JIL_CMPS( JIL_LEA_R, JIL_LEA_D, >=, 5 )
				case op_csge_rx:
					JIL_CMPS( JIL_LEA_R, JIL_LEA_X, >=, 5 )
				case op_csge_rs:
					JIL_CMPS( JIL_LEA_R, JIL_LEA_S, >=, 4 )
				case op_csge_dr:
					JIL_CMPS( JIL_LEA_D, JIL_LEA_R, >=, 5 )
				case op_csge_xr:
					JIL_CMPS( JIL_LEA_X, JIL_LEA_R, >=, 5 )
				case op_csge_sr:
					JIL_CMPS( JIL_LEA_S, JIL_LEA_R, >=, 4 )
				case op_cslt_rr:
					JIL_CMPS( JIL_LEA_R, JIL_LEA_R, <, 4 )
				case op_cslt_rd:
					JIL_CMPS( JIL_LEA_R, JIL_LEA_D, <, 5 )
				case op_cslt_rx:
					JIL_CMPS( JIL_LEA_R, JIL_LEA_X, <, 5 )
				case op_cslt_rs:
					JIL_CMPS( JIL_LEA_R, JIL_LEA_S, <, 4 )
				case op_cslt_dr:
					JIL_CMPS( JIL_LEA_D, JIL_LEA_R, <, 5 )
				case op_cslt_xr:
					JIL_CMPS( JIL_LEA_X, JIL_LEA_R, <, 5 )
				case op_cslt_sr:
					JIL_CMPS( JIL_LEA_S, JIL_LEA_R, <, 4 )
				case op_csle_rr:
					JIL_CMPS( JIL_LEA_R, JIL_LEA_R, <=, 4 )
				case op_csle_rd:
					JIL_CMPS( JIL_LEA_R, JIL_LEA_D, <=, 5 )
				case op_csle_rx:
					JIL_CMPS( JIL_LEA_R, JIL_LEA_X, <=, 5 )
				case op_csle_rs:
					JIL_CMPS( JIL_LEA_R, JIL_LEA_S, <=, 4 )
				case op_csle_dr:
					JIL_CMPS( JIL_LEA_D, JIL_LEA_R, <=, 5 )
				case op_csle_xr:
					JIL_CMPS( JIL_LEA_X, JIL_LEA_R, <=, 5 )
				case op_csle_sr:
					JIL_CMPS( JIL_LEA_S, JIL_LEA_R, <=, 4 )
				case op_snul_rr:
					JIL_SNUL( JIL_LEA_R, 3 )
				case op_snnul_rr:
					JIL_SNNUL( JIL_LEA_R, 3 )
				case op_unot_r:
					JIL_NOTUNOT( JIL_LEA_R, !, 2 )
				case op_unot_d:
					JIL_NOTUNOT( JIL_LEA_D, !, 3 )
				case op_unot_x:
					JIL_NOTUNOT( JIL_LEA_X, !, 3 )
				case op_unot_s:
					JIL_NOTUNOT( JIL_LEA_S, !, 2 )
				case op_streq_rr:
					JIL_CMPSTR( JIL_LEA_R, JIL_LEA_R, JILString_Equal, 4 )
				case op_streq_rd:
					JIL_CMPSTR( JIL_LEA_R, JIL_LEA_D, JILString_Equal, 5 )
				case op_streq_rx:
					JIL_CMPSTR( JIL_LEA_R, JIL_LEA_X, JILString_Equal, 5 )
				case op_streq_rs:
					JIL_CMPSTR( JIL_LEA_R, JIL_LEA_S, JILString_Equal, 4 )
				case op_streq_dr:
					JIL_CMPSTR( JIL_LEA_D, JIL_LEA_R, JILString_Equal, 5 )
				case op_streq_xr:
					JIL_CMPSTR( JIL_LEA_X, JIL_LEA_R, JILString_Equal, 5 )
				case op_streq_sr:
					JIL_CMPSTR( JIL_LEA_S, JIL_LEA_R, JILString_Equal, 4 )
				case op_strne_rr:
					JIL_CMPSTR( JIL_LEA_R, JIL_LEA_R, !JILString_Equal, 4 )
				case op_strne_rd:
					JIL_CMPSTR( JIL_LEA_R, JIL_LEA_D, !JILString_Equal, 5 )
				case op_strne_rx:
					JIL_CMPSTR( JIL_LEA_R, JIL_LEA_X, !JILString_Equal, 5 )
				case op_strne_rs:
					JIL_CMPSTR( JIL_LEA_R, JIL_LEA_S, !JILString_Equal, 4 )
				case op_strne_dr:
					JIL_CMPSTR( JIL_LEA_D, JIL_LEA_R, !JILString_Equal, 5 )
				case op_strne_xr:
					JIL_CMPSTR( JIL_LEA_X, JIL_LEA_R, !JILString_Equal, 5 )
				case op_strne_sr:
					JIL_CMPSTR( JIL_LEA_S, JIL_LEA_R, !JILString_Equal, 4 )
				case op_stradd_rr:
					JIL_STRADD( JIL_LEA_R, JIL_LEA_R, 3 )
				case op_stradd_rd:
					JIL_STRADD( JIL_LEA_R, JIL_LEA_D, 4 )
				case op_stradd_rx:
					JIL_STRADD( JIL_LEA_R, JIL_LEA_X, 4 )
				case op_stradd_rs:
					JIL_STRADD( JIL_LEA_R, JIL_LEA_S, 3 )
				case op_stradd_dr:
					JIL_STRADD( JIL_LEA_D, JIL_LEA_R, 4 )
				case op_stradd_xr:
					JIL_STRADD( JIL_LEA_X, JIL_LEA_R, 4 )
				case op_stradd_sr:
					JIL_STRADD( JIL_LEA_S, JIL_LEA_R, 3 )
				case op_arrcp_rr:
					JIL_ARRADD( JIL_LEA_R, JIL_LEA_R, 3, JILArray_ArrCopy )
				case op_arrcp_rd:
					JIL_ARRADD( JIL_LEA_R, JIL_LEA_D, 4, JILArray_ArrCopy )
				case op_arrcp_rx:
					JIL_ARRADD( JIL_LEA_R, JIL_LEA_X, 4, JILArray_ArrCopy )
				case op_arrcp_rs:
					JIL_ARRADD( JIL_LEA_R, JIL_LEA_S, 3, JILArray_ArrCopy )
				case op_arrcp_dr:
					JIL_ARRADD( JIL_LEA_D, JIL_LEA_R, 4, JILArray_ArrCopy )
				case op_arrcp_xr:
					JIL_ARRADD( JIL_LEA_X, JIL_LEA_R, 4, JILArray_ArrCopy )
				case op_arrcp_sr:
					JIL_ARRADD( JIL_LEA_S, JIL_LEA_R, 3, JILArray_ArrCopy )
				case op_arrmv_rr:
					JIL_ARRADD( JIL_LEA_R, JIL_LEA_R, 3, JILArray_ArrMove )
				case op_arrmv_rd:
					JIL_ARRADD( JIL_LEA_R, JIL_LEA_D, 4, JILArray_ArrMove )
				case op_arrmv_rx:
					JIL_ARRADD( JIL_LEA_R, JIL_LEA_X, 4, JILArray_ArrMove )
				case op_arrmv_rs:
					JIL_ARRADD( JIL_LEA_R, JIL_LEA_S, 3, JILArray_ArrMove )
				case op_arrmv_dr:
					JIL_ARRADD( JIL_LEA_D, JIL_LEA_R, 4, JILArray_ArrMove )
				case op_arrmv_xr:
					JIL_ARRADD( JIL_LEA_X, JIL_LEA_R, 4, JILArray_ArrMove )
				case op_arrmv_sr:
					JIL_ARRADD( JIL_LEA_S, JIL_LEA_R, 3, JILArray_ArrMove )
				case op_addl_rr:
					JIL_ADDSUBL( JIL_LEA_R, JIL_LEA_R, +=, 3 )
				case op_addl_rd:
					JIL_ADDSUBL( JIL_LEA_R, JIL_LEA_D, +=, 4 )
				case op_addl_rx:
					JIL_ADDSUBL( JIL_LEA_R, JIL_LEA_X, +=, 4 )
				case op_addl_rs:
					JIL_ADDSUBL( JIL_LEA_R, JIL_LEA_S, +=, 3 )
				case op_addl_dr:
					JIL_ADDSUBL( JIL_LEA_D, JIL_LEA_R, +=, 4 )
				case op_addl_xr:
					JIL_ADDSUBL( JIL_LEA_X, JIL_LEA_R, +=, 4 )
				case op_addl_sr:
					JIL_ADDSUBL( JIL_LEA_S, JIL_LEA_R, +=, 3 )
				case op_subl_rr:
					JIL_ADDSUBL( JIL_LEA_R, JIL_LEA_R, -=, 3 )
				case op_subl_rd:
					JIL_ADDSUBL( JIL_LEA_R, JIL_LEA_D, -=, 4 )
				case op_subl_rx:
					JIL_ADDSUBL( JIL_LEA_R, JIL_LEA_X, -=, 4 )
				case op_subl_rs:
					JIL_ADDSUBL( JIL_LEA_R, JIL_LEA_S, -=, 3 )
				case op_subl_dr:
					JIL_ADDSUBL( JIL_LEA_D, JIL_LEA_R, -=, 4 )
				case op_subl_xr:
					JIL_ADDSUBL( JIL_LEA_X, JIL_LEA_R, -=, 4 )
				case op_subl_sr:
					JIL_ADDSUBL( JIL_LEA_S, JIL_LEA_R, -=, 3 )
				case op_mull_rr:
					JIL_ADDSUBL( JIL_LEA_R, JIL_LEA_R, *=, 3 )
				case op_mull_rd:
					JIL_ADDSUBL( JIL_LEA_R, JIL_LEA_D, *=, 4 )
				case op_mull_rx:
					JIL_ADDSUBL( JIL_LEA_R, JIL_LEA_X, *=, 4 )
				case op_mull_rs:
					JIL_ADDSUBL( JIL_LEA_R, JIL_LEA_S, *=, 3 )
				case op_mull_dr:
					JIL_ADDSUBL( JIL_LEA_D, JIL_LEA_R, *=, 4 )
				case op_mull_xr:
					JIL_ADDSUBL( JIL_LEA_X, JIL_LEA_R, *=, 4 )
				case op_mull_sr:
					JIL_ADDSUBL( JIL_LEA_S, JIL_LEA_R, *=, 3 )
				case op_divl_rr:
					JIL_DIVL( JIL_LEA_R, JIL_LEA_R, /=, 3 )
				case op_divl_rd:
					JIL_DIVL( JIL_LEA_R, JIL_LEA_D, /=, 4 )
				case op_divl_rx:
					JIL_DIVL( JIL_LEA_R, JIL_LEA_X, /=, 4 )
				case op_divl_rs:
					JIL_DIVL( JIL_LEA_R, JIL_LEA_S, /=, 3 )
				case op_divl_dr:
					JIL_DIVL( JIL_LEA_D, JIL_LEA_R, /=, 4 )
				case op_divl_xr:
					JIL_DIVL( JIL_LEA_X, JIL_LEA_R, /=, 4 )
				case op_divl_sr:
					JIL_DIVL( JIL_LEA_S, JIL_LEA_R, /=, 3 )
				case op_modl_rr:
					JIL_DIVL( JIL_LEA_R, JIL_LEA_R, %=, 3 )
				case op_modl_rd:
					JIL_DIVL( JIL_LEA_R, JIL_LEA_D, %=, 4 )
				case op_modl_rx:
					JIL_DIVL( JIL_LEA_R, JIL_LEA_X, %=, 4 )
				case op_modl_rs:
					JIL_DIVL( JIL_LEA_R, JIL_LEA_S, %=, 3 )
				case op_modl_dr:
					JIL_DIVL( JIL_LEA_D, JIL_LEA_R, %=, 4 )
				case op_modl_xr:
					JIL_DIVL( JIL_LEA_X, JIL_LEA_R, %=, 4 )
				case op_modl_sr:
					JIL_DIVL( JIL_LEA_S, JIL_LEA_R, %=, 3 )
				case op_decl_r:
					JIL_INCDECL( JIL_LEA_R, -=, 2 )
				case op_decl_d:
					JIL_INCDECL( JIL_LEA_D, -=, 3 )
				case op_decl_x:
					JIL_INCDECL( JIL_LEA_X, -=, 3 )
				case op_decl_s:
					JIL_INCDECL( JIL_LEA_S, -=, 2 )
				case op_incl_r:
					JIL_INCDECL( JIL_LEA_R, +=, 2 )
				case op_incl_d:
					JIL_INCDECL( JIL_LEA_D, +=, 3 )
				case op_incl_x:
					JIL_INCDECL( JIL_LEA_X, +=, 3 )
				case op_incl_s:
					JIL_INCDECL( JIL_LEA_S, +=, 2 )
				case op_negl_r:
					JIL_NEGL( JIL_LEA_R, 2 )
				case op_negl_d:
					JIL_NEGL( JIL_LEA_D, 3 )
				case op_negl_x:
					JIL_NEGL( JIL_LEA_X, 3 )
				case op_negl_s:
					JIL_NEGL( JIL_LEA_S, 2 )
				case op_cseql_rr:
					JIL_CMPSL( JIL_LEA_R, JIL_LEA_R, ==, 4 )
				case op_cseql_rd:
					JIL_CMPSL( JIL_LEA_R, JIL_LEA_D, ==, 5 )
				case op_cseql_rx:
					JIL_CMPSL( JIL_LEA_R, JIL_LEA_X, ==, 5 )
				case op_cseql_rs:
					JIL_CMPSL( JIL_LEA_R, JIL_LEA_S, ==, 4 )
				case op_cseql_dr:
					JIL_CMPSL( JIL_LEA_D, JIL_LEA_R, ==, 5 )
				case op_cseql_xr:
					JIL_CMPSL( JIL_LEA_X, JIL_LEA_R, ==, 5 )
				case op_cseql_sr:
					JIL_CMPSL( JIL_LEA_S, JIL_LEA_R, ==, 4 )
				case op_csnel_rr:
					JIL_CMPSL( JIL_LEA_R, JIL_LEA_R, !=, 4 )
				case op_csnel_rd:
					JIL_CMPSL( JIL_LEA_R, JIL_LEA_D, !=, 5 )
				case op_csnel_rx:
					JIL_CMPSL( JIL_LEA_R, JIL_LEA_X, !=, 5 )
				case op_csnel_rs:
					JIL_CMPSL( JIL_LEA_R, JIL_LEA_S, !=, 4 )
				case op_csnel_dr:
					JIL_CMPSL( JIL_LEA_D, JIL_LEA_R, !=, 5 )
				case op_csnel_xr:
					JIL_CMPSL( JIL_LEA_X, JIL_LEA_R, !=, 5 )
				case op_csnel_sr:
					JIL_CMPSL( JIL_LEA_S, JIL_LEA_R, !=, 4 )
				case op_csgtl_rr:
					JIL_CMPSL( JIL_LEA_R, JIL_LEA_R, >, 4 )
				case op_csgtl_rd:
					JIL_CMPSL( JIL_LEA_R, JIL_LEA_D, >, 5 )
				case op_csgtl_rx:
					JIL_CMPSL( JIL_LEA_R, JIL_LEA_X, >, 5 )
				case op_csgtl_rs:
					JIL_CMPSL( JIL_LEA_R, JIL_LEA_S, >, 4 )
				case op_csgtl_dr:
					JIL_CMPSL( JIL_LEA_D, JIL_LEA_R, >, 5 )
				case op_csgtl_xr:
					JIL_CMPSL( JIL_LEA_X, JIL_LEA_R, >, 5 )
				case op_csgtl_sr:
					JIL_CMPSL( JIL_LEA_S, JIL_LEA_R, >, 4 )
				case op_csgel_rr:
					JIL_CMPSL( JIL_LEA_R, JIL_LEA_R, >=, 4 )
				case op_csgel_rd:
					JIL_CMPSL( JIL_LEA_R, JIL_LEA_D, >=, 5 )
				case op_csgel_rx:
					JIL_CMPSL( JIL_LEA_R, JIL_LEA_X, >=, 5 )
				case op_csgel_rs:
					JIL_CMPSL( JIL_LEA_R, JIL_LEA_S, >=, 4 )
				case op_csgel_dr:
					JIL_CMPSL( JIL_LEA_D, JIL_LEA_R, >=, 5 )
				case op_csgel_xr:
					JIL_CMPSL( JIL_LEA_X, JIL_LEA_R, >=, 5 )
				case op_csgel_sr:
					JIL_CMPSL( JIL_LEA_S, JIL_LEA_R, >=, 4 )
				case op_csltl_rr:
					JIL_CMPSL( JIL_LEA_R, JIL_LEA_R, <, 4 )
				case op_csltl_rd:
					JIL_CMPSL( JIL_LEA_R, JIL_LEA_D, <, 5 )
				case op_csltl_rx:
					JIL_CMPSL( JIL_LEA_R, JIL_LEA_X, <, 5 )
				case op_csltl_rs:
					JIL_CMPSL( JIL_LEA_R, JIL_LEA_S, <, 4 )
				case op_csltl_dr:
					JIL_CMPSL( JIL_LEA_D, JIL_LEA_R, <, 5 )
				case op_csltl_xr:
					JIL_CMPSL( JIL_LEA_X, JIL_LEA_R, <, 5 )
				case op_csltl_sr:
					JIL_CMPSL( JIL_LEA_S, JIL_LEA_R, <, 4 )
				case op_cslel_rr:
					JIL_CMPSL( JIL_LEA_R, JIL_LEA_R, <=, 4 )
				case op_cslel_rd:
					JIL_CMPSL( JIL_LEA_R, JIL_LEA_D, <=, 5 )
				case op_cslel_rx:
					JIL_CMPSL( JIL_LEA_R, JIL_LEA_X, <=, 5 )
				case op_cslel_rs:
					JIL_CMPSL( JIL_LEA_R, JIL_LEA_S, <=, 4 )
				case op_cslel_dr:
					JIL_CMPSL( JIL_LEA_D, JIL_LEA_R, <=, 5 )
				case op_cslel_xr:
					JIL_CMPSL( JIL_LEA_X, JIL_LEA_R, <=, 5 )
				case op_cslel_sr:
					JIL_CMPSL( JIL_LEA_S, JIL_LEA_R, <=, 4 )
				case op_addf_rr:
					JIL_ADDSUBF( JIL_LEA_R, JIL_LEA_R, +=, 3 )
				case op_addf_rd:
					JIL_ADDSUBF( JIL_LEA_R, JIL_LEA_D, +=, 4 )
				case op_addf_rx:
					JIL_ADDSUBF( JIL_LEA_R, JIL_LEA_X, +=, 4 )
				case op_addf_rs:
					JIL_ADDSUBF( JIL_LEA_R, JIL_LEA_S, +=, 3 )
				case op_addf_dr:
					JIL_ADDSUBF( JIL_LEA_D, JIL_LEA_R, +=, 4 )
				case op_addf_xr:
					JIL_ADDSUBF( JIL_LEA_X, JIL_LEA_R, +=, 4 )
				case op_addf_sr:
					JIL_ADDSUBF( JIL_LEA_S, JIL_LEA_R, +=, 3 )
				case op_subf_rr:
					JIL_ADDSUBF( JIL_LEA_R, JIL_LEA_R, -=, 3 )
				case op_subf_rd:
					JIL_ADDSUBF( JIL_LEA_R, JIL_LEA_D, -=, 4 )
				case op_subf_rx:
					JIL_ADDSUBF( JIL_LEA_R, JIL_LEA_X, -=, 4 )
				case op_subf_rs:
					JIL_ADDSUBF( JIL_LEA_R, JIL_LEA_S, -=, 3 )
				case op_subf_dr:
					JIL_ADDSUBF( JIL_LEA_D, JIL_LEA_R, -=, 4 )
				case op_subf_xr:
					JIL_ADDSUBF( JIL_LEA_X, JIL_LEA_R, -=, 4 )
				case op_subf_sr:
					JIL_ADDSUBF( JIL_LEA_S, JIL_LEA_R, -=, 3 )
				case op_mulf_rr:
					JIL_ADDSUBF( JIL_LEA_R, JIL_LEA_R, *=, 3 )
				case op_mulf_rd:
					JIL_ADDSUBF( JIL_LEA_R, JIL_LEA_D, *=, 4 )
				case op_mulf_rx:
					JIL_ADDSUBF( JIL_LEA_R, JIL_LEA_X, *=, 4 )
				case op_mulf_rs:
					JIL_ADDSUBF( JIL_LEA_R, JIL_LEA_S, *=, 3 )
				case op_mulf_dr:
					JIL_ADDSUBF( JIL_LEA_D, JIL_LEA_R, *=, 4 )
				case op_mulf_xr:
					JIL_ADDSUBF( JIL_LEA_X, JIL_LEA_R, *=, 4 )
				case op_mulf_sr:
					JIL_ADDSUBF( JIL_LEA_S, JIL_LEA_R, *=, 3 )
				case op_divf_rr:
					JIL_DIVF( JIL_LEA_R, JIL_LEA_R, 3 )
				case op_divf_rd:
					JIL_DIVF( JIL_LEA_R, JIL_LEA_D, 4 )
				case op_divf_rx:
					JIL_DIVF( JIL_LEA_R, JIL_LEA_X, 4 )
				case op_divf_rs:
					JIL_DIVF( JIL_LEA_R, JIL_LEA_S, 3 )
				case op_divf_dr:
					JIL_DIVF( JIL_LEA_D, JIL_LEA_R, 4 )
				case op_divf_xr:
					JIL_DIVF( JIL_LEA_X, JIL_LEA_R, 4 )
				case op_divf_sr:
					JIL_DIVF( JIL_LEA_S, JIL_LEA_R, 3 )
				case op_modf_rr:
					JIL_MODF( JIL_LEA_R, JIL_LEA_R, 3 )
				case op_modf_rd:
					JIL_MODF( JIL_LEA_R, JIL_LEA_D, 4 )
				case op_modf_rx:
					JIL_MODF( JIL_LEA_R, JIL_LEA_X, 4 )
				case op_modf_rs:
					JIL_MODF( JIL_LEA_R, JIL_LEA_S, 3 )
				case op_modf_dr:
					JIL_MODF( JIL_LEA_D, JIL_LEA_R, 4 )
				case op_modf_xr:
					JIL_MODF( JIL_LEA_X, JIL_LEA_R, 4 )
				case op_modf_sr:
					JIL_MODF( JIL_LEA_S, JIL_LEA_R, 3 )
				case op_decf_r:
					JIL_INCDECF( JIL_LEA_R, -=, 2 )
				case op_decf_d:
					JIL_INCDECF( JIL_LEA_D, -=, 3 )
				case op_decf_x:
					JIL_INCDECF( JIL_LEA_X, -=, 3 )
				case op_decf_s:
					JIL_INCDECF( JIL_LEA_S, -=, 2 )
				case op_incf_r:
					JIL_INCDECF( JIL_LEA_R, +=, 2 )
				case op_incf_d:
					JIL_INCDECF( JIL_LEA_D, +=, 3 )
				case op_incf_x:
					JIL_INCDECF( JIL_LEA_X, +=, 3 )
				case op_incf_s:
					JIL_INCDECF( JIL_LEA_S, +=, 2 )
				case op_negf_r:
					JIL_NEGF( JIL_LEA_R, 2 )
				case op_negf_d:
					JIL_NEGF( JIL_LEA_D, 3 )
				case op_negf_x:
					JIL_NEGF( JIL_LEA_X, 3 )
				case op_negf_s:
					JIL_NEGF( JIL_LEA_S, 2 )
				case op_cseqf_rr:
					JIL_CMPSF( JIL_LEA_R, JIL_LEA_R, ==, 4 )
				case op_cseqf_rd:
					JIL_CMPSF( JIL_LEA_R, JIL_LEA_D, ==, 5 )
				case op_cseqf_rx:
					JIL_CMPSF( JIL_LEA_R, JIL_LEA_X, ==, 5 )
				case op_cseqf_rs:
					JIL_CMPSF( JIL_LEA_R, JIL_LEA_S, ==, 4 )
				case op_cseqf_dr:
					JIL_CMPSF( JIL_LEA_D, JIL_LEA_R, ==, 5 )
				case op_cseqf_xr:
					JIL_CMPSF( JIL_LEA_X, JIL_LEA_R, ==, 5 )
				case op_cseqf_sr:
					JIL_CMPSF( JIL_LEA_S, JIL_LEA_R, ==, 4 )
				case op_csnef_rr:
					JIL_CMPSF( JIL_LEA_R, JIL_LEA_R, !=, 4 )
				case op_csnef_rd:
					JIL_CMPSF( JIL_LEA_R, JIL_LEA_D, !=, 5 )
				case op_csnef_rx:
					JIL_CMPSF( JIL_LEA_R, JIL_LEA_X, !=, 5 )
				case op_csnef_rs:
					JIL_CMPSF( JIL_LEA_R, JIL_LEA_S, !=, 4 )
				case op_csnef_dr:
					JIL_CMPSF( JIL_LEA_D, JIL_LEA_R, !=, 5 )
				case op_csnef_xr:
					JIL_CMPSF( JIL_LEA_X, JIL_LEA_R, !=, 5 )
				case op_csnef_sr:
					JIL_CMPSF( JIL_LEA_S, JIL_LEA_R, !=, 4 )
				case op_csgtf_rr:
					JIL_CMPSF( JIL_LEA_R, JIL_LEA_R, >, 4 )
				case op_csgtf_rd:
					JIL_CMPSF( JIL_LEA_R, JIL_LEA_D, >, 5 )
				case op_csgtf_rx:
					JIL_CMPSF( JIL_LEA_R, JIL_LEA_X, >, 5 )
				case op_csgtf_rs:
					JIL_CMPSF( JIL_LEA_R, JIL_LEA_S, >, 4 )
				case op_csgtf_dr:
					JIL_CMPSF( JIL_LEA_D, JIL_LEA_R, >, 5 )
				case op_csgtf_xr:
					JIL_CMPSF( JIL_LEA_X, JIL_LEA_R, >, 5 )
				case op_csgtf_sr:
					JIL_CMPSF( JIL_LEA_S, JIL_LEA_R, >, 4 )
				case op_csgef_rr:
					JIL_CMPSF( JIL_LEA_R, JIL_LEA_R, >=, 4 )
				case op_csgef_rd:
					JIL_CMPSF( JIL_LEA_R, JIL_LEA_D, >=, 5 )
				case op_csgef_rx:
					JIL_CMPSF( JIL_LEA_R, JIL_LEA_X, >=, 5 )
				case op_csgef_rs:
					JIL_CMPSF( JIL_LEA_R, JIL_LEA_S, >=, 4 )
				case op_csgef_dr:
					JIL_CMPSF( JIL_LEA_D, JIL_LEA_R, >=, 5 )
				case op_csgef_xr:
					JIL_CMPSF( JIL_LEA_X, JIL_LEA_R, >=, 5 )
				case op_csgef_sr:
					JIL_CMPSF( JIL_LEA_S, JIL_LEA_R, >=, 4 )
				case op_csltf_rr:
					JIL_CMPSF( JIL_LEA_R, JIL_LEA_R, <, 4 )
				case op_csltf_rd:
					JIL_CMPSF( JIL_LEA_R, JIL_LEA_D, <, 5 )
				case op_csltf_rx:
					JIL_CMPSF( JIL_LEA_R, JIL_LEA_X, <, 5 )
				case op_csltf_rs:
					JIL_CMPSF( JIL_LEA_R, JIL_LEA_S, <, 4 )
				case op_csltf_dr:
					JIL_CMPSF( JIL_LEA_D, JIL_LEA_R, <, 5 )
				case op_csltf_xr:
					JIL_CMPSF( JIL_LEA_X, JIL_LEA_R, <, 5 )
				case op_csltf_sr:
					JIL_CMPSF( JIL_LEA_S, JIL_LEA_R, <, 4 )
				case op_cslef_rr:
					JIL_CMPSF( JIL_LEA_R, JIL_LEA_R, <=, 4 )
				case op_cslef_rd:
					JIL_CMPSF( JIL_LEA_R, JIL_LEA_D, <=, 5 )
				case op_cslef_rx:
					JIL_CMPSF( JIL_LEA_R, JIL_LEA_X, <=, 5 )
				case op_cslef_rs:
					JIL_CMPSF( JIL_LEA_R, JIL_LEA_S, <=, 4 )
				case op_cslef_dr:
					JIL_CMPSF( JIL_LEA_D, JIL_LEA_R, <=, 5 )
				case op_cslef_xr:
					JIL_CMPSF( JIL_LEA_X, JIL_LEA_R, <=, 5 )
				case op_cslef_sr:
					JIL_CMPSF( JIL_LEA_S, JIL_LEA_R, <=, 4 )
				case op_pop:
					JIL_IBEGIN( 1 )
					JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(pContext->vmDataStackPointer >= pState->vmDataStackSize, JIL_VM_Stack_Overflow) )
					handle1 = pContext->vmppDataStack[pContext->vmDataStackPointer++];
					JILRelease(pState, handle1);
					JIL_IEND
				case op_push:
					JIL_IBEGIN( 1 )
					JIL_INSERT_DEBUG_CODE( JIL_THROW_IF((pContext->vmDataStackPointer - 1) <= 0, JIL_VM_Stack_Overflow) )
					handle1 = JILGetNullHandle(pState);
					pContext->vmppDataStack[--pContext->vmDataStackPointer] = handle1;
					JILAddRef(handle1);
					JIL_IEND
				case op_rtchk_r:
					JIL_RTCHKEA( JIL_LEA_R, 3 );
				case op_rtchk_d:
					JIL_RTCHKEA( JIL_LEA_D, 4 );
				case op_rtchk_x:
					JIL_RTCHKEA( JIL_LEA_X, 4 );
				case op_rtchk_s:
					JIL_RTCHKEA( JIL_LEA_S, 3 );
				case op_jsr:
					JIL_IBEGIN( 2 )
					offs = JIL_GET_DATA(pState);
					JIL_PUSH_CS( programCounter + instruction_size )
					programCounter = offs;
					JIL_IENDBR
				case op_jsr_r:
					JIL_JSREA( JIL_LEA_R, 2 )
				case op_jsr_d:
					JIL_JSREA( JIL_LEA_D, 3 )
				case op_jsr_x:
					JIL_JSREA( JIL_LEA_X, 3 )
				case op_jsr_s:
					JIL_JSREA( JIL_LEA_S, 2 )
				case op_newctx:
					JIL_IBEGIN( 5 )
					pNewHandle = JILGetNewHandle(pState);
					pNewHandle->type = JIL_GET_DATA(pState);
					funcInfo = pState->vmpFuncSegment->pData + JIL_GET_DATA(pState);
					offs = JIL_GET_DATA(pState);
					JIL_LEA_R(pContext, operand1)
					handle1 = *operand1;
					JILGetContextHandle(pNewHandle)->pContext = JILAllocContext(pState, offs, funcInfo->codeAddr);
					JIL_STORE_HANDLE( pState, operand1, pNewHandle );
					JILRelease( pState, pNewHandle );
					pNewHandle = NULL;
					JIL_IEND
				case op_resume_r:
					JIL_RESU( JIL_LEA_R, 2 );
				case op_resume_d:
					JIL_RESU( JIL_LEA_D, 3 );
				case op_resume_x:
					JIL_RESU( JIL_LEA_X, 3 );
				case op_resume_s:
					JIL_RESU( JIL_LEA_S, 2 );
				case op_yield:
					JIL_IBEGIN( 1 )
					pContext->vmProgramCounter = programCounter + instruction_size;
					handle1 = pContext->vmppRegister[kReturnRegister];
					pState->vmpContext = pContext = pContext->vmpYieldContext;
					programCounter = pContext->vmProgramCounter;
					JIL_STORE_HANDLE( pState, (pContext->vmppRegister + kReturnRegister), handle1 );
					JIL_IENDBR
				case op_wref_rr:
					JIL_WREF( JIL_LEA_R, JIL_LEA_R, 3 )
				case op_wref_rd:
					JIL_WREF( JIL_LEA_R, JIL_LEA_D, 4 )
				case op_wref_rx:
					JIL_WREF( JIL_LEA_R, JIL_LEA_X, 4 )
				case op_wref_rs:
					JIL_WREF( JIL_LEA_R, JIL_LEA_S, 3 )
				case op_wref_dr:
					JIL_WREF( JIL_LEA_D, JIL_LEA_R, 4 )
				case op_wref_dd:
					JIL_WREF( JIL_LEA_D, JIL_LEA_D, 5 )
				case op_wref_dx:
					JIL_WREF( JIL_LEA_D, JIL_LEA_X, 5 )
				case op_wref_ds:
					JIL_WREF( JIL_LEA_D, JIL_LEA_S, 4 )
				case op_wref_xr:
					JIL_WREF( JIL_LEA_X, JIL_LEA_R, 4 )
				case op_wref_xd:
					JIL_WREF( JIL_LEA_X, JIL_LEA_D, 5 )
				case op_wref_xx:
					JIL_WREF( JIL_LEA_X, JIL_LEA_X, 5 )
				case op_wref_xs:
					JIL_WREF( JIL_LEA_X, JIL_LEA_S, 4 )
				case op_wref_sr:
					JIL_WREF( JIL_LEA_S, JIL_LEA_R, 3 )
				case op_wref_sd:
					JIL_WREF( JIL_LEA_S, JIL_LEA_D, 4 )
				case op_wref_sx:
					JIL_WREF( JIL_LEA_S, JIL_LEA_X, 4 )
				case op_wref_ss:
					JIL_WREF( JIL_LEA_S, JIL_LEA_S, 3 )
				case op_cmpref_rr:
					JIL_IBEGIN( 4 )
					pNewHandle = JILGetNewHandle(pState);
					JIL_LEA_R(pContext, operand1);
					JIL_LEA_R(pContext, operand2);
					JIL_LEA_R(pContext, operand3);
					pNewHandle->type = type_int;
					JILGetIntHandle(pNewHandle)->l = (*operand1 == *operand2);
					JIL_STORE_HANDLE(pState, operand3, pNewHandle);
					JILRelease(pState, pNewHandle);
					pNewHandle = NULL;
					JIL_IEND
				case op_newdg:
					JIL_IBEGIN( 4 )
					pNewHandle = JILGetNewHandle(pState);
					i = JIL_GET_DATA(pState);
					offs = JIL_GET_DATA(pState);
					JIL_LEA_R(pContext, operand1);
					pNewHandle->type = i;
					JILGetDelegateHandle(pNewHandle)->pDelegate = JILAllocDelegate(pState, offs, NULL);
					JIL_STORE_HANDLE(pState, operand1, pNewHandle);
					JILRelease(pState, pNewHandle);
					pNewHandle = NULL;
					JIL_IEND
				case op_newdgm:
					JIL_IBEGIN( 5 )
					pNewHandle = JILGetNewHandle(pState);
					i = JIL_GET_DATA(pState);
					offs = JIL_GET_DATA(pState);
					JIL_LEA_R(pContext, operand1);
					JIL_LEA_R(pContext, operand2);
					JIL_THROW_IF((*operand1)->type == type_null, JIL_VM_Null_Reference);
					pNewHandle->type = i;
					JILGetDelegateHandle(pNewHandle)->pDelegate = JILAllocDelegate(pState, offs, *operand1);
					JIL_STORE_HANDLE(pState, operand2, pNewHandle);
					JILRelease(pState, pNewHandle);
					pNewHandle = NULL;
					JIL_IEND
				case op_calldg_r:
					JIL_CALLDG( JIL_LEA_R, 2 );
				case op_calldg_d:
					JIL_CALLDG( JIL_LEA_D, 3 );
				case op_calldg_x:
					JIL_CALLDG( JIL_LEA_X, 3 );
				case op_calldg_s:
					JIL_CALLDG( JIL_LEA_S, 2 );
				case op_throw:
					JIL_IBEGIN( 1 )
					handle1 = pContext->vmppRegister[kReturnRegister];
					JILAddRef(handle1);
					pState->vmpThrowHandle = handle1;
					JIL_THROW( JIL_VM_Software_Exception )
				case op_alloci:
					JIL_IBEGIN( 3 )
					pNewHandle = JILGetNewHandle(pState);
					hObj = JIL_GET_DATA(pState);
					JIL_LEA_R(pContext, operand1)
					JIL_INSERT_DEBUG_CODE( typeInfo = JILTypeInfoFromType(pState, hObj) );
					JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(typeInfo->family != tf_interface, JIL_VM_Unsupported_Type) )
					pNewHandle->type = type_array;
					JILGetArrayHandle(pNewHandle)->arr = JILAllocFactory(pState, hObj);
					JIL_STORE_HANDLE(pState, operand1, pNewHandle);
					JILRelease(pState, pNewHandle);
					pNewHandle = NULL;
					JIL_IEND
				case op_calli:
					JIL_IBEGIN( 3 )
					hObj = JIL_GET_DATA(pState);
					i = JIL_GET_DATA(pState);
					handle1 = pContext->vmppRegister[0];
					// make sure the type is an interface
					JIL_INSERT_DEBUG_CODE( typeInfo = JILTypeInfoFromType(pState, hObj) );
					JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(typeInfo->family != tf_interface, JIL_VM_Unsupported_Type) );
					// check if we have an array
					JIL_INSERT_DEBUG_CODE( JIL_THROW_IF(handle1->type != type_array, JIL_VM_Unsupported_Type) );
					pState->errProgramCounter = pContext->vmProgramCounter = programCounter;
					JIL_PUSH_CS(programCounter + instruction_size)
					result = JILCallFactory(pState, JILGetArrayHandle(handle1)->arr, i);
					JIL_POP_CS(i)
					JIL_THROW( result )
					JIL_IEND
				case op_dcvt:
					JIL_IBEGIN( 4 )
					hObj = JIL_GET_DATA(pState);
					JIL_LEA_R(pContext, operand1)
					JIL_LEA_R(pContext, operand2)
					result = JILDynamicConvert(pState, hObj, *operand1, &pNewHandle);
					JIL_THROW( result )
					JIL_STORE_HANDLE(pState, operand2, pNewHandle);
					JILRelease(pState, pNewHandle);
					pNewHandle = NULL;
					JIL_IEND
				default:
					JIL_IBEGIN( 1 )
					JIL_THROW( JIL_VM_Illegal_Instruction )
					break;
			}
		}
		// we will only get here if an exception occurs. The only way to leave this function
		// without an exception is by a "return" statement in the switch.
exception:
		if( pNewHandle )
		{
			JILRelease(pState, pNewHandle);
			pNewHandle = NULL;
		}
		pState->errProgramCounter = programCounter;
		pContext->vmProgramCounter = programCounter + instruction_size;
		result = JILGenerateException(pState, result);
	}
	while( !result );

#if defined(_DEBUG) || JIL_TRACE_RELEASE
terminate:
#endif

	pState->vmRunLevel--;
	pState->vmRunning = (pState->vmRunLevel > 0);
	return result;
}
