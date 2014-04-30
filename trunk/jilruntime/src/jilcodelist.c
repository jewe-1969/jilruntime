//------------------------------------------------------------------------------
// File: JILCodeList.c                                         (c) 2005 jewe.org
//------------------------------------------------------------------------------
//
// Description:
// ------------
//	Provides functions to generate an ascii text listing of JIL bytecode.
//------------------------------------------------------------------------------

#include "jilstdinc.h"

#include "jilprogramming.h"
#include "jildebug.h"
#include "jilhandle.h"
#include "jilopcodes.h"
#include "jilcodelist.h"
#include "jilstring.h"
#include "jilarray.h"
#include "jilmachine.h"

#include "jclstring.h"

//------------------------------------------------------------------------------
// externals
//------------------------------------------------------------------------------

JILEXTERN const JILInstrInfo g_InstructionInfo[JILNumOpcodes];

//------------------------------------------------------------------------------
// Operand sizes
//------------------------------------------------------------------------------

static const JILLong kOperandTypeSize[kNumOperandTypes] =
{
	0,	// ot_none,		No operand
	1,	// ot_number,	Immediate integer number
	1,	// ot_handle,	Immediate handle number
	1,	// ot_type,		Immediate type identifier number
	1,	// ot_label,	A branch label (used by assembler/disassembler)
	1,	// ot_ear,		Operand is addressing mode "register direct", i.e. "r7"
	2,	// ot_ead,		Operand is addressing mode "register indirect, displacement", i.e. "(r5+12)"
	2,	// ot_eax,		Operand is addressing mode "register indirect, indexed", i.e. "(r5+r7)"
	1,	// ot_eas,		Operand is addressing mode "stack, displacement", i.e. "(sp+12)"
	2,	// ot_regrng,	Operand is a register range, i.e. "r3-r7"
};

//------------------------------------------------------------------------------
// static functions in this file
//------------------------------------------------------------------------------

static JILLong	JILListOperand		(JILState* pState, JILLong address, JILLong adr, JILLong type, JILChar* pString, JILLong maxString, JILChar* pComment, JILLong maxComment);
static void		JILTabTo			(JILChar* pString, JILLong maxDestLen, JILLong column);
static void		JILCopyEscString	(JILChar* pDst, const JILChar* pSrc, JILLong maxLen);
static JILLong	JILStrEquNoCase		(const JILChar* str1, const JILChar* str2);
static void		JILGetCalln			(JILState* pState, JILChar* pDst, JILLong maxLen, JILLong addr);
static void		JILGetJsr			(JILState* pState, JILChar* pDst, JILLong maxLen, JILLong addr);
static void		JILGetNewctx		(JILState* pState, JILChar* pDst, JILLong maxLen, JILLong addr);

//------------------------------------------------------------------------------
// JILGetInstructionSize
//------------------------------------------------------------------------------
// Get the size, in 'instruction words' of a complete instruction (including
// data for operands, etc.). If the given opcode is undefined, 0 will be
// returned.

JILLong JILGetInstructionSize(JILLong opcode)
{
	JILLong size = 0;
	const JILInstrInfo* pi = JILGetInfoFromOpcode( opcode );
	if( pi != NULL )
	{
		JILLong j;
		size = 1;
		for( j = 0; j < pi->numOperands; j++ )
			size += JILGetOperandSize( pi->opType[j] );
	}
	return size;
}

//------------------------------------------------------------------------------
// JILGetOperandSize
//------------------------------------------------------------------------------
// Get the size, in instruction words of an operand of the specified type.
// See enum JILOperandType in jiltypes.h for predefined operand types.
// You can obtain the operand type(s) of an instruction by examining the
// JILInstrInfo structure returned by JILGetInstructionInfo() and
// JILGetInfoFromOpcode().

JILLong JILGetOperandSize(JILLong operandType)
{
	if( operandType >= 0 && operandType < kNumOperandTypes )
		return kOperandTypeSize[operandType];
	return 0;
}

//------------------------------------------------------------------------------
// JILGetInstructionIndex
//------------------------------------------------------------------------------
// Find an entry in the instruction table by the given name of the mnemonic.
// This can be used by a virtual machine assembler program.

JILLong JILGetInstructionIndex(const JILChar* pName, JILLong startIndex)
{
	JILLong i;
	if( startIndex >= 0 && startIndex < JILNumOpcodes )
	{
		for( i = startIndex; i < JILNumOpcodes; i++ )
		{
			if( strcmp(g_InstructionInfo[i].name, pName) == 0 )
				return i;
		}
	}
	return -1;
}

//------------------------------------------------------------------------------
// JILGetInstructionInfo
//------------------------------------------------------------------------------
// Get the instruction info from the instruction table by the given table index.

const JILInstrInfo* JILGetInstructionInfo(JILLong index)
{
	if( index >= 0 && index < JILNumOpcodes )
		return g_InstructionInfo + index;
	return NULL;
}

//------------------------------------------------------------------------------
// JILGetInfoFromOpcode
//------------------------------------------------------------------------------
// The opcode is now enforced to be equal to the table index.

const JILInstrInfo* JILGetInfoFromOpcode(JILLong opcode)
{
	return JILGetInstructionInfo(opcode);
}

//------------------------------------------------------------------------------
// JILGetHandleTypeName
//------------------------------------------------------------------------------
// Returns a string constant for the given handle type.
// See also enum JILHandleType in file "jilapitypes.h".

const JILChar* JILGetHandleTypeName(JILState* pState, JILLong type)
{
	const JILChar* result = "INVALID TYPE ID";
	if( type >= 0 && type < pState->vmUsedTypeInfoSegSize )
	{
		JILTypeInfo* pTypeInfo = JILTypeInfoFromType(pState, type);
		result = JILCStrGetString(pState, pTypeInfo->offsetName);
	}
	return result;
}

//------------------------------------------------------------------------------
// JILListCode
//------------------------------------------------------------------------------
// Outputs multiple lines of virtual machine instructions to given stream.

void JILListCode(JILState* pState, JILLong from, JILLong to, JILLong extInfo, JILFILE* stream)
{
	#if !JIL_NO_FPRINTF
	JILLong i;
	JILLong size;
	JILChar str[128];

	if( stream == NULL )
		stream = stdout;
	if( from > pState->vmpCodeSegment->usedSize )
		from = pState->vmpCodeSegment->usedSize;
	if( to > pState->vmpCodeSegment->usedSize )
		to = pState->vmpCodeSegment->usedSize;
	if( (!from && !to) || (from > to) )
	{
		from = 0;
		to = pState->vmpCodeSegment->usedSize;
	}
	for( i = from; i < to; i += size )
	{
		if( i == 21 )
			i = 21;
		size = JILListInstruction( pState, i, str, 128, extInfo );
		if( !size )
		{
			fprintf(stream, "ERROR IN INSTRUCTION TABLE OR INVALID INSTRUCTION!\n");
			break;
		}
		fprintf(stream, "%s\n", str);
	}
	#endif
}

//------------------------------------------------------------------------------
// JILListInstruction
//------------------------------------------------------------------------------
// Creates a string from the instruction at the given address. The parameter
// 'maxLength' specifies the maximum number of bytes the function is allowed to
// write into 'pString'. Return value is the size of the listed instruction in
// instruction words or 0 if an error occurred.

JILLong JILListInstruction(JILState* pState, JILLong address, JILChar* pOutput, JILLong maxLength, JILLong extInfo)
{
	JILLong err;
	const JILInstrInfo* pInfo;
	JILLong l;
	JILLong i;
	JILLong opcode = 0;
	JILLong size = 0;
	JILLong adr;
	JILChar pComment[128];
	JILChar pString[128];

	pComment[0] = 0;
	JILSnprintf(pString, 128, "%d", address);
	JILTabTo(pString, 128, 8);
	err = JILGetMemory( pState, address, &opcode, 1 );
	if( err )
		return 0;
	pInfo = JILGetInfoFromOpcode( opcode );
	if( pInfo )
	{
		size = JILGetInstructionSize(opcode);
		JILStrcat( pString, 128, pInfo->name );
		JILTabTo(pString, 128, 16);
		adr = address + 1;
		for( i = 0; i < pInfo->numOperands; i++ )
		{
			l = JILListOperand( pState, address, adr, pInfo->opType[i], pString, 128, pComment, 128 );
			if( !l )
				return 0;
			adr += l;
			if( (i + 1) < pInfo->numOperands )
			{
				JILStrcat(pString, 128, ",");
			}
		}
		// check for opcode with special pComment
		switch( opcode )
		{
			case op_calln:
			case op_callm:
				JILGetCalln(pState, pComment, 128, address);
				break;
			case op_jsr:
				JILGetJsr(pState, pComment, 128, address);
				break;
			case op_newctx:
				JILStrcpy(pComment, 128, "cofunction ");
				JILGetNewctx(pState, pComment + 11, 117, address);
				break;
		}
		if( *pComment && extInfo )
		{
			JILTabTo( pString, 128, 32 );
			JILStrcat( pString, 128, ";" );
			JILStrcat( pString, 128, pComment );
		}
	}
	// try to get function name from address
	if( JILGetFunctionName(pState, pComment, 128, address) )
	{
		JILStrcpy(pOutput, maxLength, "function ");
		JILStrcat(pOutput, maxLength, pComment);
		JILStrcat(pOutput, maxLength, " :\n");
		JILStrcat(pOutput, maxLength, pString);
	}
	else
	{
		JILStrcpy(pOutput, maxLength, pString);
	}
	return size;
}

//------------------------------------------------------------------------------
// JILListHandle
//------------------------------------------------------------------------------
//

JILLong JILListHandle(JILState* pState, JILLong hObj, JILChar* pString, JILLong maxString, JILChar* pComment, JILLong maxComment, JILLong isData)
{
	// get handle
	JILLong err;
	JILLong type;
	JILHandle handle;
	JILHandle* pHandle = &handle;
	JILChar tempstr[32];

	if( isData )
		err = JILGetDataHandle( pState, hObj, &handle );
	else
		err = JILGetRuntimeHandle( pState, hObj, &handle );
	if( err )
		return err;
	type = pHandle->type;
	switch( type )
	{
		case type_null:
			JILSnprintf( pString, maxString, "%d", hObj );
			JILSnprintf( pComment, maxComment, "%s", JILGetHandleTypeName(pState, type) );
			break;
		case type_int:
			JILSnprintf( pString, maxString, "%d", hObj );
			JILSnprintf( tempstr, 32, "%d", JILGetIntHandle(pHandle)->l );
			JILSnprintf( pComment, maxComment, "%s %s", JILGetHandleTypeName(pState, type), tempstr );
			break;
		case type_float:
			JILSnprintf( pString, maxString, "%d", hObj );
			JILSnprintf( pComment, maxComment, "%s %g", JILGetHandleTypeName(pState, type), JILGetFloatHandle(pHandle)->f );
			break;
		case type_string:
			JILSnprintf( pString, maxString, "%d", hObj );
			if( isData )
				JILCopyEscString( tempstr, JILCStrGetString(pState, (JILLong) JILGetStringHandle(pHandle)->str), 30 );
			else
				JILCopyEscString( tempstr, JILString_String(JILGetStringHandle(pHandle)->str), 30 );
			JILSnprintf( pComment, maxComment, "%s \"%s\"", JILGetHandleTypeName(pState, type), tempstr );
			break;
		case type_array:
			if( isData )
			{
				JILSnprintf( pString, maxString, "%d", hObj );
				JILSnprintf( pComment, maxComment, "%s", JILGetHandleTypeName(pState, type) );
			}
			else
			{
				JILSnprintf( pString, maxString, "%d", hObj );
				JILSnprintf( tempstr, 32, "%d", JILGetArrayHandle(pHandle)->arr->size );
				JILSnprintf( pComment, maxComment, "%s [%s]", JILGetHandleTypeName(pState, type), tempstr );
			}
			break;
		default:
			if( type >= 0 && type < pState->vmUsedTypeInfoSegSize )	// could list garbage data so must check
			{
				JILSnprintf( pString, maxString, "%d", hObj );
				JILSnprintf( pComment, maxComment, "%s", JILGetHandleTypeName(pState, type) );
			}
			else
			{
				JILSnprintf( pString, maxString, "%d", hObj );
				JILSnprintf( pComment, maxComment, "INVALID TYPE IDENTIFIER" );
			}
			break;
	}
	return 0;
}

//------------------------------------------------------------------------------
// JILListCallStack
//------------------------------------------------------------------------------
// Outputs function names on the callstack to the given stream.

void JILListCallStack(JILState* pState, JILLong maxTraceback, JILFILE* stream)
{
	#if !JIL_NO_FPRINTF
	JILLong numStack;
	JILLong addr;
	JILLong i;
	JILContext* pContext;
	char buf[256];

	if( !pState->vmInitialized )
		return;
	if( stream == NULL )
		stream = stdout;

	// show current program counter
	pContext = pState->vmpContext;
	addr = pContext->vmProgramCounter;
	JILGetFunctionName(pState, buf, 256, addr);
	fprintf(stream, "%4d: %s()\n", 0, buf);

	// iterate over callstack
	numStack = pState->vmCallStackSize - pContext->vmCallStackPointer;
	if( !numStack )
		goto error;
	numStack = (numStack < maxTraceback) ? numStack : maxTraceback;
	for( i = 0; i < numStack; i++ )
	{
		addr = pContext->vmpCallStack[pContext->vmCallStackPointer + i];
		if( addr == kReturnToNative )
		{
			fprintf(stream, "%4d: native_entry_point()\n", i+1, addr);
		}
		else
		{
			JILGetFunctionName(pState, buf, 256, addr);
			fprintf(stream, "%4d: %s()\n", i+1, buf);
		}
	}
	return;

error:
	fprintf(stream, "INVALID FUNCTION ADDRESS ERROR\n");
	return;
	#endif
}

//------------------------------------------------------------------------------
// JILGetFunctionName
//------------------------------------------------------------------------------

JILLong JILGetFunctionName(JILState* pState, JILChar* pDst, JILLong maxLength, JILLong addr)
{
	JILFuncInfo* pFuncInfo;
	JILLong offset;
	JILLong isStart = 0;
	JCLString* className = NEW(JCLString);
	JCLString* funcName = NEW(JCLString);

	JILGetFunctionByAddr(pState, addr, &pFuncInfo);
	if( pFuncInfo )
	{
		offset = pState->vmpTypeInfoSegment[pFuncInfo->type].offsetName;
		JCLSetString(className, JILCStrGetString(pState, offset));
		JCLSetString(funcName, JILCStrGetString(pState, pFuncInfo->offsetName));
		JCLAppend(className, "::");
		JCLAppend(className, JCLGetString(funcName));
		JCLSetString(funcName, JCLGetString(className));
		JILStrcpy(pDst, maxLength, JCLGetString(funcName));
		isStart = (pFuncInfo->codeAddr == addr);
	}

	DELETE(funcName);
	DELETE(className);
	return isStart;
}

//-----------------------------------------------------------------------------
// JILListOperand
//------------------------------------------------------------------------------
//

static JILLong JILListOperand(JILState* pState, JILLong address, JILLong adr, JILLong type, JILChar* pString, JILLong maxString, JILChar* pComment, JILLong maxComment)
{
	JILLong err;
	JILChar tempstr[32];
	JILChar buf[32];
	switch( type )
	{
		case ot_number:
		{
			JILLong val;
			err = JILGetMemory( pState, adr, &val, 1 );
			if( err )
				return 0;
			JILSnprintf( tempstr, 32, "%d", val );
			JILStrcat( pString, maxString, tempstr );
			return 1;
		}
		case ot_handle:
		{
			JILLong val;
			err = JILGetMemory( pState, adr, &val, 1 );
			if( err )
				return 0;
			err = JILListHandle(pState, val, tempstr, 32, pComment, maxComment, 1);
			if( err )
				return 0;
			JILStrcat( pString, maxString, tempstr );
			return 1;
		}
		case ot_type:
		{
			JILLong val;
			JILTypeInfo* pInfo;
			err = JILGetMemory( pState, adr, &val, 1 );
			if( err )
				return 0;
			JILSnprintf( tempstr, 32, "%d", val );
			JILStrcat( pString, maxString, tempstr );
			pInfo = JILTypeInfoFromType(pState, val);
			if( pInfo->family == tf_class )
				JILSnprintf( pComment, maxComment, "class %s", JILGetHandleTypeName(pState, val) );
			else if( pInfo->family == tf_thread )
				JILSnprintf( pComment, maxComment, "cofunction %s", JILGetHandleTypeName(pState, val) );
			else if( pInfo->family == tf_delegate )
				JILSnprintf( pComment, maxComment, "delegate %s", JILGetHandleTypeName(pState, val) );
			else if( pInfo->family == tf_interface )
				JILSnprintf( pComment, maxComment, "interface %s", JILGetHandleTypeName(pState, val) );
			else
				JILSnprintf( pComment, maxComment, "type %s", JILGetHandleTypeName(pState, val) );
			return 1;
		}
		case ot_label:
		{
			JILLong val;
			err = JILGetMemory( pState, adr, &val, 1 );
			if( err )
				return 0;
			JILSnprintf(tempstr, 32, "%d", val);
			JILStrcat( pString, maxString, tempstr );
			JILSnprintf( pComment, maxComment, "to %d", address + val );
			return 1;
		}
		case ot_ear:
		{
			JILLong val;
			err = JILGetMemory( pState, adr, &val, 1 );
			if( err )
				return 0;
			JILSnprintf( tempstr, 32, "r%d", val );
			JILStrcat( pString, maxString, tempstr );
			return 1;
		}
		case ot_ead:
		{
			JILLong reg;
			JILLong dis;
			err = JILGetMemory( pState, adr, &reg, 1 );
			if( err )
				return 0;
			err = JILGetMemory( pState, adr + 1, &dis, 1 );
			if( err )
				return 0;
			JILSnprintf(buf, 32, "%d", dis);
			JILSnprintf( tempstr, 32, "(r%d+%s)", reg, buf );
			JILStrcat( pString, maxString, tempstr );
			return 2;
		}
		case ot_eax:
		{
			JILLong reg1;
			JILLong reg2;
			err = JILGetMemory( pState, adr, &reg1, 1 );
			if( err )
				return 0;
			err = JILGetMemory( pState, adr + 1, &reg2, 1 );
			if( err )
				return 0;
			JILSnprintf( tempstr, 32, "(r%d+r%d)", reg1, reg2 );
			JILStrcat( pString, maxString, tempstr );
			return 2;
		}
		case ot_eas:
		{
			JILLong dis;
			err = JILGetMemory( pState, adr, &dis, 1 );
			if( err )
				return 0;
			JILSnprintf( buf, 32, "%d", dis );
			JILSnprintf( tempstr, 32, "(sp+%s)", buf );
			JILStrcat( pString, maxString, tempstr );
			return 1;
		}
		case ot_regrng:
		{
			JILLong reg;
			JILLong cnt;
			err = JILGetMemory( pState, adr, (JILLong*) &reg, 1 );
			if( err )
				return 0;
			err = JILGetMemory( pState, adr + 1, (JILLong*) &cnt, 1 );
			if( err )
				return 0;
			JILSnprintf( tempstr, 32, "r%d-r%d", reg, reg + cnt - 1 );
			JILStrcat( pString, maxString, tempstr );
			return 2;
		}
	}
	return 0;
}

//------------------------------------------------------------------------------
// JILTabTo
//------------------------------------------------------------------------------
//

static void JILTabTo(JILChar* pString, JILLong maxDestLen, JILLong column)
{
	JILLong x = strlen(pString);
	while( x < column )
	{
		JILStrcat( pString, maxDestLen, " " );
		x++;
	}
}

//------------------------------------------------------------------------------
// JILCopyEscString
//------------------------------------------------------------------------------
//

static void JILCopyEscString(JILChar* pDst, const JILChar* pSrc, JILLong maxLen)
{
	JILLong c;
	JILLong i = 0;
	JILLong l = 0;
	for( ; ; i++ )
	{
		c = pSrc[i];
		if( !c )
		{
			break;
		}
		else if( c < 32 )
		{
			*pDst++ = '\\';
			l++;
			if( l == maxLen )
				break;
			*pDst++ = 'x';
			l++;
			if( l == maxLen )
				break;
			if( (l + 2) >= maxLen )
				break;
			JILSnprintf( pDst, 2, "%2.2x", c & 0xff );
			pDst += 2;
			l += 2;
		}
		else
		{
			*pDst++ = (JILChar) c;
			l++;
			if( l == maxLen )
				break;
		}
	}
	*pDst = 0;
}

//------------------------------------------------------------------------------
// JILStrEquNoCase
//------------------------------------------------------------------------------
// Like StrEqu, but ignores the case of characters "A" to "Z".

static int JILStrEquNoCase(const char* str1, const char* str2)
{
	register int c1;
	register int c2;
	for(;;)
	{
		c1 = tolower( *str1++ );
		c2 = tolower( *str2++ );
		if( !c1 || !c2 )
		{
			if( c1 == c2 )
				return 1;
			else
				return 0;
		}
		if( c1 != c2 )
			return 0;
	}
	// would be strange, if we would get here... ;-)
	return 0;
}

//------------------------------------------------------------------------------
// JILGetCalln
//------------------------------------------------------------------------------

static void JILGetCalln(JILState* pState, JILChar* pDst, JILLong maxLen, JILLong addr)
{
	JILFuncInfo* pFuncInfo;
	JCLString* className;
	JCLString* funcName;
	JILLong type = 0;
	JILLong index = 0;
	JILLong offset;
	className = NEW(JCLString);
	funcName = NEW(JCLString);
	if( JILGetMemory(pState, addr + 1, &type, 1) )
		goto error;
	if( JILGetMemory(pState, addr + 2, &index, 1) )
		goto error;

	JILGetFunctionByIndex(pState, type, index, &pFuncInfo);
	if( pFuncInfo )
	{
		offset = pState->vmpTypeInfoSegment[pFuncInfo->type].offsetName;
		JCLSetString(className, JILCStrGetString(pState, offset));
		JCLSetString(funcName, JILCStrGetString(pState, pFuncInfo->offsetName));
		JCLAppend(className, "::");
		JCLAppend(className, JCLGetString(funcName));
		JCLSetString(funcName, JCLGetString(className));
		JILStrcpy(pDst, maxLen, JCLGetString(funcName));
	}

error:
	DELETE( className );
	DELETE( funcName );
}

//------------------------------------------------------------------------------
// JILGetJsr
//------------------------------------------------------------------------------

static void JILGetJsr(JILState* pState, JILChar* pDst, JILLong maxLen, JILLong addr)
{
	JILLong destAddr = 0;
	if( JILGetMemory(pState, addr + 1, &destAddr, 1) )
		return;
	JILGetFunctionName(pState, pDst, maxLen, destAddr);
}

//------------------------------------------------------------------------------
// JILGetNewctx
//------------------------------------------------------------------------------

static void JILGetNewctx(JILState* pState, JILChar* pDst, JILLong maxLen, JILLong addr)
{
	JILFuncInfo* pFuncInfo = pState->vmpFuncSegment->pData;
	JILLong funcIndex = 0;
	if( JILGetMemory(pState, addr + 2, &funcIndex, 1) )
		return;
	JILGetFunctionName(pState, pDst, maxLen, pFuncInfo[funcIndex].codeAddr);
}

//------------------------------------------------------------------------------
// JILCheckInstructionTables
//------------------------------------------------------------------------------
// This can be called to analyze the instruction table and instruction size
// table for possible errors.

void JILCheckInstructionTables(JILState* ps)
{
	JILLong i;
	JILLong l;
	JILLong e = 0;
	const JILInstrInfo* pi;

	JILMessageLog(ps, "*** Checking instruction table ***\n");
	for( i = 0; i < JILNumOpcodes; i++ )
	{
		pi = JILGetInfoFromOpcode(i);
		l = JILGetInstructionSize(i);
		if( i != pi->opCode )
		{
			JILMessageLog(ps, "%3d opcode=%3d '%-8s' size=%2d   ERROR IN TABLE ORDER\n", i, pi->opCode, pi->name, l);
			e++;
		}
	}
	JILMessageLog(ps, "%d error(s) in instruction table.\n", e);
}
