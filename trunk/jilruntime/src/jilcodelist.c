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

JILEXTERN const JILLong JILInstructionSize[JILNumOpcodes];

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
// g_InstructionInfo
//------------------------------------------------------------------------------

extern const JILInstrInfo g_InstructionInfo[JILNumOpcodes];

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
	if( opcode >= 0 && opcode < JILNumOpcodes )
		return JILInstructionSize[opcode];
	return 0;
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
			if( JILStrEquNoCase(g_InstructionInfo[i].name, pName) )
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
// Find the instruction info from the instruction table by the given virtual
// machine instruction opcode.

const JILInstrInfo* JILGetInfoFromOpcode(JILLong opcode)
{
	JILLong i;
	for( i = 0; i < JILNumOpcodes; i++ )
	{
		if( g_InstructionInfo[i].opCode == opcode )
			return g_InstructionInfo + i;
	}
	return NULL;
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
	const JILInstrInfo* pi;

	JILMessageLog(ps, "*** Checking Instruction tables\n");
	JILMessageLog(ps, "Number of opcodes = %d\n", JILNumOpcodes);

	pi = JILGetInfoFromOpcode( JILNumOpcodes - 1 );
	JILMessageLog(ps, "Last opcode is %d '%s'\n", JILNumOpcodes - 1, pi->name);
	JILMessageLog(ps, "Verifying instruction sizes...\n");

	for(i = 0; i < JILNumOpcodes; i++ )
	{
		JILLong j, k;
		pi = JILGetInfoFromOpcode( i );
		l = JILGetInstructionSize( i );
		k = 1;
		for( j = 0; j < pi->numOperands; j++ )
			k += JILGetOperandSize( pi->opType[j] );
		JILMessageLog(ps, "%3d '%-8s' size(table)=%2d size(computed)=%2d", i, pi->name, l, k);
		if( l != k )
			JILMessageLog(ps, "ERROR!!!\n");
		else
			JILMessageLog(ps, "\n");
	}
	JILMessageLog(ps, "*** Done\n");
}

//------------------------------------------------------------------------------
// g_InstructionInfo
//------------------------------------------------------------------------------
// This is the instruction table, defining an instruction info record for every
// opcode the virtual machine supports.

const JILInstrInfo g_InstructionInfo[JILNumOpcodes] =
{
	op_nop,			0,	ot_none,	ot_none,	ot_none,	ot_none,	"nop",
	op_alloc,		2,	ot_type,	ot_ear,		ot_none,	ot_none,	"alloc",
	op_alloca,		3,	ot_type,	ot_number,	ot_ear,		ot_none,	"alloca",
	op_allocn,		2,	ot_type,	ot_ear,		ot_none,	ot_none,	"allocn",
	op_bra,			1,	ot_label,	ot_none,	ot_none,	ot_none,	"bra",
	op_brk,			0,	ot_none,	ot_none,	ot_none,	ot_none,	"brk",
	op_callm,		2,	ot_type,	ot_number,	ot_none,	ot_none,	"callm",
	op_calls,		1,	ot_number,	ot_none,	ot_none,	ot_none,	"calls",
	op_calln,		2,	ot_type,	ot_number,	ot_none,	ot_none,	"calln",
	op_cvf,			2,	ot_ear,		ot_ear,		ot_none,	ot_none,	"cvf",
	op_cvl,			2,	ot_ear,		ot_ear,		ot_none,	ot_none,	"cvl",
	op_popm,		1,	ot_number,	ot_none,	ot_none,	ot_none,	"popm",
	op_popr,		1,	ot_regrng,	ot_none,	ot_none,	ot_none,	"popr",
	op_pushm,		1,	ot_number,	ot_none,	ot_none,	ot_none,	"pushm",
	op_pushr,		1,	ot_regrng,	ot_none,	ot_none,	ot_none,	"pushr",
	op_ret,			0,	ot_none,	ot_none,	ot_none,	ot_none,	"ret",
	op_size,		2,	ot_ear,		ot_ear,		ot_none,	ot_none,	"size",
	op_type,		2,	ot_ear,		ot_ear,		ot_none,	ot_none,	"type",
	op_dec_r,		1,	ot_ear,		ot_none,	ot_none,	ot_none,	"dec",
	op_dec_d,		1,	ot_ead,		ot_none,	ot_none,	ot_none,	"dec",
	op_dec_x,		1,	ot_eax,		ot_none,	ot_none,	ot_none,	"dec",
	op_dec_s,		1,	ot_eas,		ot_none,	ot_none,	ot_none,	"dec",
	op_inc_r,		1,	ot_ear,		ot_none,	ot_none,	ot_none,	"inc",
	op_inc_d,		1,	ot_ead,		ot_none,	ot_none,	ot_none,	"inc",
	op_inc_x,		1,	ot_eax,		ot_none,	ot_none,	ot_none,	"inc",
	op_inc_s,		1,	ot_eas,		ot_none,	ot_none,	ot_none,	"inc",
	op_moveh_r,		2,	ot_handle,	ot_ear,		ot_none,	ot_none,	"moveh",
	op_moveh_d,		2,	ot_handle,	ot_ead,		ot_none,	ot_none,	"moveh",
	op_moveh_x,		2,	ot_handle,	ot_eax,		ot_none,	ot_none,	"moveh",
	op_moveh_s,		2,	ot_handle,	ot_eas,		ot_none,	ot_none,	"moveh",
	op_neg_r,		1,	ot_ear,		ot_none,	ot_none,	ot_none,	"neg",
	op_neg_d,		1,	ot_ead,		ot_none,	ot_none,	ot_none,	"neg",
	op_neg_x,		1,	ot_eax,		ot_none,	ot_none,	ot_none,	"neg",
	op_neg_s,		1,	ot_eas,		ot_none,	ot_none,	ot_none,	"neg",
	op_not_r,		1,	ot_ear,		ot_none,	ot_none,	ot_none,	"not",
	op_not_d,		1,	ot_ead,		ot_none,	ot_none,	ot_none,	"not",
	op_not_x,		1,	ot_eax,		ot_none,	ot_none,	ot_none,	"not",
	op_not_s,		1,	ot_eas,		ot_none,	ot_none,	ot_none,	"not",
	op_tsteq_r,		2,	ot_ear,		ot_label,	ot_none,	ot_none,	"tsteq",
	op_tsteq_d,		2,	ot_ead,		ot_label,	ot_none,	ot_none,	"tsteq",
	op_tsteq_x,		2,	ot_eax,		ot_label,	ot_none,	ot_none,	"tsteq",
	op_tsteq_s,		2,	ot_eas,		ot_label,	ot_none,	ot_none,	"tsteq",
	op_tstne_r,		2,	ot_ear,		ot_label,	ot_none,	ot_none,	"tstne",
	op_tstne_d,		2,	ot_ead,		ot_label,	ot_none,	ot_none,	"tstne",
	op_tstne_x,		2,	ot_eax,		ot_label,	ot_none,	ot_none,	"tstne",
	op_tstne_s,		2,	ot_eas,		ot_label,	ot_none,	ot_none,	"tstne",
	op_add_rr,		2,	ot_ear,		ot_ear,		ot_none,	ot_none,	"add",
	op_add_rd,		2,	ot_ear,		ot_ead,		ot_none,	ot_none,	"add",
	op_add_rx,		2,	ot_ear,		ot_eax,		ot_none,	ot_none,	"add",
	op_add_rs,		2,	ot_ear,		ot_eas,		ot_none,	ot_none,	"add",
	op_add_dr,		2,	ot_ead,		ot_ear,		ot_none,	ot_none,	"add",
	op_add_xr,		2,	ot_eax,		ot_ear,		ot_none,	ot_none,	"add",
	op_add_sr,		2,	ot_eas,		ot_ear,		ot_none,	ot_none,	"add",
	op_and_rr,		2,	ot_ear,		ot_ear,		ot_none,	ot_none,	"and",
	op_and_rd,		2,	ot_ear,		ot_ead,		ot_none,	ot_none,	"and",
	op_and_rx,		2,	ot_ear,		ot_eax,		ot_none,	ot_none,	"and",
	op_and_rs,		2,	ot_ear,		ot_eas,		ot_none,	ot_none,	"and",
	op_and_dr,		2,	ot_ead,		ot_ear,		ot_none,	ot_none,	"and",
	op_and_xr,		2,	ot_eax,		ot_ear,		ot_none,	ot_none,	"and",
	op_and_sr,		2,	ot_eas,		ot_ear,		ot_none,	ot_none,	"and",
	op_asl_rr,		2,	ot_ear,		ot_ear,		ot_none,	ot_none,	"asl",
	op_asl_rd,		2,	ot_ear,		ot_ead,		ot_none,	ot_none,	"asl",
	op_asl_rx,		2,	ot_ear,		ot_eax,		ot_none,	ot_none,	"asl",
	op_asl_rs,		2,	ot_ear,		ot_eas,		ot_none,	ot_none,	"asl",
	op_asl_dr,		2,	ot_ead,		ot_ear,		ot_none,	ot_none,	"asl",
	op_asl_xr,		2,	ot_eax,		ot_ear,		ot_none,	ot_none,	"asl",
	op_asl_sr,		2,	ot_eas,		ot_ear,		ot_none,	ot_none,	"asl",
	op_asr_rr,		2,	ot_ear,		ot_ear,		ot_none,	ot_none,	"asr",
	op_asr_rd,		2,	ot_ear,		ot_ead,		ot_none,	ot_none,	"asr",
	op_asr_rx,		2,	ot_ear,		ot_eax,		ot_none,	ot_none,	"asr",
	op_asr_rs,		2,	ot_ear,		ot_eas,		ot_none,	ot_none,	"asr",
	op_asr_dr,		2,	ot_ead,		ot_ear,		ot_none,	ot_none,	"asr",
	op_asr_xr,		2,	ot_eax,		ot_ear,		ot_none,	ot_none,	"asr",
	op_asr_sr,		2,	ot_eas,		ot_ear,		ot_none,	ot_none,	"asr",
	op_div_rr,		2,	ot_ear,		ot_ear,		ot_none,	ot_none,	"div",
	op_div_rd,		2,	ot_ear,		ot_ead,		ot_none,	ot_none,	"div",
	op_div_rx,		2,	ot_ear,		ot_eax,		ot_none,	ot_none,	"div",
	op_div_rs,		2,	ot_ear,		ot_eas,		ot_none,	ot_none,	"div",
	op_div_dr,		2,	ot_ead,		ot_ear,		ot_none,	ot_none,	"div",
	op_div_xr,		2,	ot_eax,		ot_ear,		ot_none,	ot_none,	"div",
	op_div_sr,		2,	ot_eas,		ot_ear,		ot_none,	ot_none,	"div",
	op_lsl_rr,		2,	ot_ear,		ot_ear,		ot_none,	ot_none,	"lsl",
	op_lsl_rd,		2,	ot_ear,		ot_ead,		ot_none,	ot_none,	"lsl",
	op_lsl_rx,		2,	ot_ear,		ot_eax,		ot_none,	ot_none,	"lsl",
	op_lsl_rs,		2,	ot_ear,		ot_eas,		ot_none,	ot_none,	"lsl",
	op_lsl_dr,		2,	ot_ead,		ot_ear,		ot_none,	ot_none,	"lsl",
	op_lsl_xr,		2,	ot_eax,		ot_ear,		ot_none,	ot_none,	"lsl",
	op_lsl_sr,		2,	ot_eas,		ot_ear,		ot_none,	ot_none,	"lsl",
	op_lsr_rr,		2,	ot_ear,		ot_ear,		ot_none,	ot_none,	"lsr",
	op_lsr_rd,		2,	ot_ear,		ot_ead,		ot_none,	ot_none,	"lsr",
	op_lsr_rx,		2,	ot_ear,		ot_eax,		ot_none,	ot_none,	"lsr",
	op_lsr_rs,		2,	ot_ear,		ot_eas,		ot_none,	ot_none,	"lsr",
	op_lsr_dr,		2,	ot_ead,		ot_ear,		ot_none,	ot_none,	"lsr",
	op_lsr_xr,		2,	ot_eax,		ot_ear,		ot_none,	ot_none,	"lsr",
	op_lsr_sr,		2,	ot_eas,		ot_ear,		ot_none,	ot_none,	"lsr",
	op_mod_rr,		2,	ot_ear,		ot_ear,		ot_none,	ot_none,	"mod",
	op_mod_rd,		2,	ot_ear,		ot_ead,		ot_none,	ot_none,	"mod",
	op_mod_rx,		2,	ot_ear,		ot_eax,		ot_none,	ot_none,	"mod",
	op_mod_rs,		2,	ot_ear,		ot_eas,		ot_none,	ot_none,	"mod",
	op_mod_dr,		2,	ot_ead,		ot_ear,		ot_none,	ot_none,	"mod",
	op_mod_xr,		2,	ot_eax,		ot_ear,		ot_none,	ot_none,	"mod",
	op_mod_sr,		2,	ot_eas,		ot_ear,		ot_none,	ot_none,	"mod",
	op_mul_rr,		2,	ot_ear,		ot_ear,		ot_none,	ot_none,	"mul",
	op_mul_rd,		2,	ot_ear,		ot_ead,		ot_none,	ot_none,	"mul",
	op_mul_rx,		2,	ot_ear,		ot_eax,		ot_none,	ot_none,	"mul",
	op_mul_rs,		2,	ot_ear,		ot_eas,		ot_none,	ot_none,	"mul",
	op_mul_dr,		2,	ot_ead,		ot_ear,		ot_none,	ot_none,	"mul",
	op_mul_xr,		2,	ot_eax,		ot_ear,		ot_none,	ot_none,	"mul",
	op_mul_sr,		2,	ot_eas,		ot_ear,		ot_none,	ot_none,	"mul",
	op_or_rr,		2,	ot_ear,		ot_ear,		ot_none,	ot_none,	"or",
	op_or_rd,		2,	ot_ear,		ot_ead,		ot_none,	ot_none,	"or",
	op_or_rx,		2,	ot_ear,		ot_eax,		ot_none,	ot_none,	"or",
	op_or_rs,		2,	ot_ear,		ot_eas,		ot_none,	ot_none,	"or",
	op_or_dr,		2,	ot_ead,		ot_ear,		ot_none,	ot_none,	"or",
	op_or_xr,		2,	ot_eax,		ot_ear,		ot_none,	ot_none,	"or",
	op_or_sr,		2,	ot_eas,		ot_ear,		ot_none,	ot_none,	"or",
	op_sub_rr,		2,	ot_ear,		ot_ear,		ot_none,	ot_none,	"sub",
	op_sub_rd,		2,	ot_ear,		ot_ead,		ot_none,	ot_none,	"sub",
	op_sub_rx,		2,	ot_ear,		ot_eax,		ot_none,	ot_none,	"sub",
	op_sub_rs,		2,	ot_ear,		ot_eas,		ot_none,	ot_none,	"sub",
	op_sub_dr,		2,	ot_ead,		ot_ear,		ot_none,	ot_none,	"sub",
	op_sub_xr,		2,	ot_eax,		ot_ear,		ot_none,	ot_none,	"sub",
	op_sub_sr,		2,	ot_eas,		ot_ear,		ot_none,	ot_none,	"sub",
	op_xor_rr,		2,	ot_ear,		ot_ear,		ot_none,	ot_none,	"xor",
	op_xor_rd,		2,	ot_ear,		ot_ead,		ot_none,	ot_none,	"xor",
	op_xor_rx,		2,	ot_ear,		ot_eax,		ot_none,	ot_none,	"xor",
	op_xor_rs,		2,	ot_ear,		ot_eas,		ot_none,	ot_none,	"xor",
	op_xor_dr,		2,	ot_ead,		ot_ear,		ot_none,	ot_none,	"xor",
	op_xor_xr,		2,	ot_eax,		ot_ear,		ot_none,	ot_none,	"xor",
	op_xor_sr,		2,	ot_eas,		ot_ear,		ot_none,	ot_none,	"xor",
	op_move_rr,		2,	ot_ear,		ot_ear,		ot_none,	ot_none,	"move",
	op_move_rd,		2,	ot_ear,		ot_ead,		ot_none,	ot_none,	"move",
	op_move_rx,		2,	ot_ear,		ot_eax,		ot_none,	ot_none,	"move",
	op_move_rs,		2,	ot_ear,		ot_eas,		ot_none,	ot_none,	"move",
	op_move_dr,		2,	ot_ead,		ot_ear,		ot_none,	ot_none,	"move",
	op_move_dd,		2,	ot_ead,		ot_ead,		ot_none,	ot_none,	"move",
	op_move_dx,		2,	ot_ead,		ot_eax,		ot_none,	ot_none,	"move",
	op_move_ds,		2,	ot_ead,		ot_eas,		ot_none,	ot_none,	"move",
	op_move_xr,		2,	ot_eax,		ot_ear,		ot_none,	ot_none,	"move",
	op_move_xd,		2,	ot_eax,		ot_ead,		ot_none,	ot_none,	"move",
	op_move_xx,		2,	ot_eax,		ot_eax,		ot_none,	ot_none,	"move",
	op_move_xs,		2,	ot_eax,		ot_eas,		ot_none,	ot_none,	"move",
	op_move_sr,		2,	ot_eas,		ot_ear,		ot_none,	ot_none,	"move",
	op_move_sd,		2,	ot_eas,		ot_ead,		ot_none,	ot_none,	"move",
	op_move_sx,		2,	ot_eas,		ot_eax,		ot_none,	ot_none,	"move",
	op_move_ss,		2,	ot_eas,		ot_eas,		ot_none,	ot_none,	"move",

	// extensions 2004/10/30
	op_ldz_r,		1,	ot_ear,		ot_none,	ot_none,	ot_none,	"ldz",

	// extensions 2004/11/10
	op_copy_rr,		2,	ot_ear,		ot_ear,		ot_none,	ot_none,	"copy",
	op_copy_rd,		2,	ot_ear,		ot_ead,		ot_none,	ot_none,	"copy",
	op_copy_rx,		2,	ot_ear,		ot_eax,		ot_none,	ot_none,	"copy",
	op_copy_rs,		2,	ot_ear,		ot_eas,		ot_none,	ot_none,	"copy",
	op_copy_dr,		2,	ot_ead,		ot_ear,		ot_none,	ot_none,	"copy",
	op_copy_dd,		2,	ot_ead,		ot_ead,		ot_none,	ot_none,	"copy",
	op_copy_dx,		2,	ot_ead,		ot_eax,		ot_none,	ot_none,	"copy",
	op_copy_ds,		2,	ot_ead,		ot_eas,		ot_none,	ot_none,	"copy",
	op_copy_xr,		2,	ot_eax,		ot_ear,		ot_none,	ot_none,	"copy",
	op_copy_xd,		2,	ot_eax,		ot_ead,		ot_none,	ot_none,	"copy",
	op_copy_xx,		2,	ot_eax,		ot_eax,		ot_none,	ot_none,	"copy",
	op_copy_xs,		2,	ot_eax,		ot_eas,		ot_none,	ot_none,	"copy",
	op_copy_sr,		2,	ot_eas,		ot_ear,		ot_none,	ot_none,	"copy",
	op_copy_sd,		2,	ot_eas,		ot_ead,		ot_none,	ot_none,	"copy",
	op_copy_sx,		2,	ot_eas,		ot_eax,		ot_none,	ot_none,	"copy",
	op_copy_ss,		2,	ot_eas,		ot_eas,		ot_none,	ot_none,	"copy",
	op_pop_r,		1,	ot_ear,		ot_none,	ot_none,	ot_none,	"pop",
	op_pop_d,		1,	ot_ead,		ot_none,	ot_none,	ot_none,	"pop",
	op_pop_x,		1,	ot_eax,		ot_none,	ot_none,	ot_none,	"pop",
	op_pop_s,		1,	ot_eas,		ot_none,	ot_none,	ot_none,	"pop",
	op_push_r,		1,	ot_ear,		ot_none,	ot_none,	ot_none,	"push",
	op_push_d,		1,	ot_ead,		ot_none,	ot_none,	ot_none,	"push",
	op_push_x,		1,	ot_eax,		ot_none,	ot_none,	ot_none,	"push",
	op_push_s,		1,	ot_eas,		ot_none,	ot_none,	ot_none,	"push",

	// extensions 2004/11/28
	op_copyh_r,		2,	ot_handle,	ot_ear,		ot_none,	ot_none,	"copyh",
	op_copyh_d,		2,	ot_handle,	ot_ead,		ot_none,	ot_none,	"copyh",
	op_copyh_x,		2,	ot_handle,	ot_eax,		ot_none,	ot_none,	"copyh",
	op_copyh_s,		2,	ot_handle,	ot_eas,		ot_none,	ot_none,	"copyh",

	op_cseq_rr,		3,	ot_ear,		ot_ear,		ot_ear,		ot_none,	"cseq",
	op_cseq_rd,		3,	ot_ear,		ot_ead,		ot_ear,		ot_none,	"cseq",
	op_cseq_rx,		3,	ot_ear,		ot_eax,		ot_ear,		ot_none,	"cseq",
	op_cseq_rs,		3,	ot_ear,		ot_eas,		ot_ear,		ot_none,	"cseq",
	op_cseq_dr,		3,	ot_ead,		ot_ear,		ot_ear,		ot_none,	"cseq",
	op_cseq_xr,		3,	ot_eax,		ot_ear,		ot_ear,		ot_none,	"cseq",
	op_cseq_sr,		3,	ot_eas,		ot_ear,		ot_ear,		ot_none,	"cseq",
	op_csne_rr,		3,	ot_ear,		ot_ear,		ot_ear,		ot_none,	"csne",
	op_csne_rd,		3,	ot_ear,		ot_ead,		ot_ear,		ot_none,	"csne",
	op_csne_rx,		3,	ot_ear,		ot_eax,		ot_ear,		ot_none,	"csne",
	op_csne_rs,		3,	ot_ear,		ot_eas,		ot_ear,		ot_none,	"csne",
	op_csne_dr,		3,	ot_ead,		ot_ear,		ot_ear,		ot_none,	"csne",
	op_csne_xr,		3,	ot_eax,		ot_ear,		ot_ear,		ot_none,	"csne",
	op_csne_sr,		3,	ot_eas,		ot_ear,		ot_ear,		ot_none,	"csne",
	op_csgt_rr,		3,	ot_ear,		ot_ear,		ot_ear,		ot_none,	"csgt",
	op_csgt_rd,		3,	ot_ear,		ot_ead,		ot_ear,		ot_none,	"csgt",
	op_csgt_rx,		3,	ot_ear,		ot_eax,		ot_ear,		ot_none,	"csgt",
	op_csgt_rs,		3,	ot_ear,		ot_eas,		ot_ear,		ot_none,	"csgt",
	op_csgt_dr,		3,	ot_ead,		ot_ear,		ot_ear,		ot_none,	"csgt",
	op_csgt_xr,		3,	ot_eax,		ot_ear,		ot_ear,		ot_none,	"csgt",
	op_csgt_sr,		3,	ot_eas,		ot_ear,		ot_ear,		ot_none,	"csgt",
	op_csge_rr,		3,	ot_ear,		ot_ear,		ot_ear,		ot_none,	"csge",
	op_csge_rd,		3,	ot_ear,		ot_ead,		ot_ear,		ot_none,	"csge",
	op_csge_rx,		3,	ot_ear,		ot_eax,		ot_ear,		ot_none,	"csge",
	op_csge_rs,		3,	ot_ear,		ot_eas,		ot_ear,		ot_none,	"csge",
	op_csge_dr,		3,	ot_ead,		ot_ear,		ot_ear,		ot_none,	"csge",
	op_csge_xr,		3,	ot_eax,		ot_ear,		ot_ear,		ot_none,	"csge",
	op_csge_sr,		3,	ot_eas,		ot_ear,		ot_ear,		ot_none,	"csge",
	op_cslt_rr,		3,	ot_ear,		ot_ear,		ot_ear,		ot_none,	"cslt",
	op_cslt_rd,		3,	ot_ear,		ot_ead,		ot_ear,		ot_none,	"cslt",
	op_cslt_rx,		3,	ot_ear,		ot_eax,		ot_ear,		ot_none,	"cslt",
	op_cslt_rs,		3,	ot_ear,		ot_eas,		ot_ear,		ot_none,	"cslt",
	op_cslt_dr,		3,	ot_ead,		ot_ear,		ot_ear,		ot_none,	"cslt",
	op_cslt_xr,		3,	ot_eax,		ot_ear,		ot_ear,		ot_none,	"cslt",
	op_cslt_sr,		3,	ot_eas,		ot_ear,		ot_ear,		ot_none,	"cslt",
	op_csle_rr,		3,	ot_ear,		ot_ear,		ot_ear,		ot_none,	"csle",
	op_csle_rd,		3,	ot_ear,		ot_ead,		ot_ear,		ot_none,	"csle",
	op_csle_rx,		3,	ot_ear,		ot_eax,		ot_ear,		ot_none,	"csle",
	op_csle_rs,		3,	ot_ear,		ot_eas,		ot_ear,		ot_none,	"csle",
	op_csle_dr,		3,	ot_ead,		ot_ear,		ot_ear,		ot_none,	"csle",
	op_csle_xr,		3,	ot_eax,		ot_ear,		ot_ear,		ot_none,	"csle",
	op_csle_sr,		3,	ot_eas,		ot_ear,		ot_ear,		ot_none,	"csle",

	// extensions 2004/12/06
	op_snul_rr,		2,	ot_ear,		ot_ear,		ot_none,	ot_none,	"snul",
	op_snnul_rr,	2,	ot_ear,		ot_ear,		ot_none,	ot_none,	"snnul",
	op_unot_r,		1,	ot_ear,		ot_none,	ot_none,	ot_none,	"unot",
	op_unot_d,		1,	ot_ead,		ot_none,	ot_none,	ot_none,	"unot",
	op_unot_x,		1,	ot_eax,		ot_none,	ot_none,	ot_none,	"unot",
	op_unot_s,		1,	ot_eas,		ot_none,	ot_none,	ot_none,	"unot",

	// extensions 2004/12/16
	op_streq_rr,	3,	ot_ear,		ot_ear,		ot_ear,		ot_none,	"streq",
	op_streq_rd,	3,	ot_ear,		ot_ead,		ot_ear,		ot_none,	"streq",
	op_streq_rx,	3,	ot_ear,		ot_eax,		ot_ear,		ot_none,	"streq",
	op_streq_rs,	3,	ot_ear,		ot_eas,		ot_ear,		ot_none,	"streq",
	op_streq_dr,	3,	ot_ead,		ot_ear,		ot_ear,		ot_none,	"streq",
	op_streq_xr,	3,	ot_eax,		ot_ear,		ot_ear,		ot_none,	"streq",
	op_streq_sr,	3,	ot_eas,		ot_ear,		ot_ear,		ot_none,	"streq",
	op_strne_rr,	3,	ot_ear,		ot_ear,		ot_ear,		ot_none,	"strne",
	op_strne_rd,	3,	ot_ear,		ot_ead,		ot_ear,		ot_none,	"strne",
	op_strne_rx,	3,	ot_ear,		ot_eax,		ot_ear,		ot_none,	"strne",
	op_strne_rs,	3,	ot_ear,		ot_eas,		ot_ear,		ot_none,	"strne",
	op_strne_dr,	3,	ot_ead,		ot_ear,		ot_ear,		ot_none,	"strne",
	op_strne_xr,	3,	ot_eax,		ot_ear,		ot_ear,		ot_none,	"strne",
	op_strne_sr,	3,	ot_eas,		ot_ear,		ot_ear,		ot_none,	"strne",
	op_stradd_rr,	2,	ot_ear,		ot_ear,		ot_none,	ot_none,	"stradd",
	op_stradd_rd,	2,	ot_ear,		ot_ead,		ot_none,	ot_none,	"stradd",
	op_stradd_rx,	2,	ot_ear,		ot_eax,		ot_none,	ot_none,	"stradd",
	op_stradd_rs,	2,	ot_ear,		ot_eas,		ot_none,	ot_none,	"stradd",
	op_stradd_dr,	2,	ot_ead,		ot_ear,		ot_none,	ot_none,	"stradd",
	op_stradd_xr,	2,	ot_eax,		ot_ear,		ot_none,	ot_none,	"stradd",
	op_stradd_sr,	2,	ot_eas,		ot_ear,		ot_none,	ot_none,	"stradd",
	op_arrcp_rr,	2,	ot_ear,		ot_ear,		ot_none,	ot_none,	"arrcp",
	op_arrcp_rd,	2,	ot_ear,		ot_ead,		ot_none,	ot_none,	"arrcp",
	op_arrcp_rx,	2,	ot_ear,		ot_eax,		ot_none,	ot_none,	"arrcp",
	op_arrcp_rs,	2,	ot_ear,		ot_eas,		ot_none,	ot_none,	"arrcp",
	op_arrcp_dr,	2,	ot_ead,		ot_ear,		ot_none,	ot_none,	"arrcp",
	op_arrcp_xr,	2,	ot_eax,		ot_ear,		ot_none,	ot_none,	"arrcp",
	op_arrcp_sr,	2,	ot_eas,		ot_ear,		ot_none,	ot_none,	"arrcp",
	op_arrmv_rr,	2,	ot_ear,		ot_ear,		ot_none,	ot_none,	"arrmv",
	op_arrmv_rd,	2,	ot_ear,		ot_ead,		ot_none,	ot_none,	"arrmv",
	op_arrmv_rx,	2,	ot_ear,		ot_eax,		ot_none,	ot_none,	"arrmv",
	op_arrmv_rs,	2,	ot_ear,		ot_eas,		ot_none,	ot_none,	"arrmv",
	op_arrmv_dr,	2,	ot_ead,		ot_ear,		ot_none,	ot_none,	"arrmv",
	op_arrmv_xr,	2,	ot_eax,		ot_ear,		ot_none,	ot_none,	"arrmv",
	op_arrmv_sr,	2,	ot_eas,		ot_ear,		ot_none,	ot_none,	"arrmv",

	// extensions 2005/02/19
	op_addl_rr,		2,	ot_ear,		ot_ear,		ot_none,	ot_none,	"addl",
	op_addl_rd,		2,	ot_ear,		ot_ead,		ot_none,	ot_none,	"addl",
	op_addl_rx,		2,	ot_ear,		ot_eax,		ot_none,	ot_none,	"addl",
	op_addl_rs,		2,	ot_ear,		ot_eas,		ot_none,	ot_none,	"addl",
	op_addl_dr,		2,	ot_ead,		ot_ear,		ot_none,	ot_none,	"addl",
	op_addl_xr,		2,	ot_eax,		ot_ear,		ot_none,	ot_none,	"addl",
	op_addl_sr,		2,	ot_eas,		ot_ear,		ot_none,	ot_none,	"addl",
	op_subl_rr,		2,	ot_ear,		ot_ear,		ot_none,	ot_none,	"subl",
	op_subl_rd,		2,	ot_ear,		ot_ead,		ot_none,	ot_none,	"subl",
	op_subl_rx,		2,	ot_ear,		ot_eax,		ot_none,	ot_none,	"subl",
	op_subl_rs,		2,	ot_ear,		ot_eas,		ot_none,	ot_none,	"subl",
	op_subl_dr,		2,	ot_ead,		ot_ear,		ot_none,	ot_none,	"subl",
	op_subl_xr,		2,	ot_eax,		ot_ear,		ot_none,	ot_none,	"subl",
	op_subl_sr,		2,	ot_eas,		ot_ear,		ot_none,	ot_none,	"subl",
	op_mull_rr,		2,	ot_ear,		ot_ear,		ot_none,	ot_none,	"mull",
	op_mull_rd,		2,	ot_ear,		ot_ead,		ot_none,	ot_none,	"mull",
	op_mull_rx,		2,	ot_ear,		ot_eax,		ot_none,	ot_none,	"mull",
	op_mull_rs,		2,	ot_ear,		ot_eas,		ot_none,	ot_none,	"mull",
	op_mull_dr,		2,	ot_ead,		ot_ear,		ot_none,	ot_none,	"mull",
	op_mull_xr,		2,	ot_eax,		ot_ear,		ot_none,	ot_none,	"mull",
	op_mull_sr,		2,	ot_eas,		ot_ear,		ot_none,	ot_none,	"mull",
	op_divl_rr,		2,	ot_ear,		ot_ear,		ot_none,	ot_none,	"divl",
	op_divl_rd,		2,	ot_ear,		ot_ead,		ot_none,	ot_none,	"divl",
	op_divl_rx,		2,	ot_ear,		ot_eax,		ot_none,	ot_none,	"divl",
	op_divl_rs,		2,	ot_ear,		ot_eas,		ot_none,	ot_none,	"divl",
	op_divl_dr,		2,	ot_ead,		ot_ear,		ot_none,	ot_none,	"divl",
	op_divl_xr,		2,	ot_eax,		ot_ear,		ot_none,	ot_none,	"divl",
	op_divl_sr,		2,	ot_eas,		ot_ear,		ot_none,	ot_none,	"divl",
	op_modl_rr,		2,	ot_ear,		ot_ear,		ot_none,	ot_none,	"modl",
	op_modl_rd,		2,	ot_ear,		ot_ead,		ot_none,	ot_none,	"modl",
	op_modl_rx,		2,	ot_ear,		ot_eax,		ot_none,	ot_none,	"modl",
	op_modl_rs,		2,	ot_ear,		ot_eas,		ot_none,	ot_none,	"modl",
	op_modl_dr,		2,	ot_ead,		ot_ear,		ot_none,	ot_none,	"modl",
	op_modl_xr,		2,	ot_eax,		ot_ear,		ot_none,	ot_none,	"modl",
	op_modl_sr,		2,	ot_eas,		ot_ear,		ot_none,	ot_none,	"modl",
	op_decl_r,		1,	ot_ear,		ot_none,	ot_none,	ot_none,	"decl",
	op_decl_d,		1,	ot_ead,		ot_none,	ot_none,	ot_none,	"decl",
	op_decl_x,		1,	ot_eax,		ot_none,	ot_none,	ot_none,	"decl",
	op_decl_s,		1,	ot_eas,		ot_none,	ot_none,	ot_none,	"decl",
	op_incl_r,		1,	ot_ear,		ot_none,	ot_none,	ot_none,	"incl",
	op_incl_d,		1,	ot_ead,		ot_none,	ot_none,	ot_none,	"incl",
	op_incl_x,		1,	ot_eax,		ot_none,	ot_none,	ot_none,	"incl",
	op_incl_s,		1,	ot_eas,		ot_none,	ot_none,	ot_none,	"incl",
	op_negl_r,		1,	ot_ear,		ot_none,	ot_none,	ot_none,	"negl",
	op_negl_d,		1,	ot_ead,		ot_none,	ot_none,	ot_none,	"negl",
	op_negl_x,		1,	ot_eax,		ot_none,	ot_none,	ot_none,	"negl",
	op_negl_s,		1,	ot_eas,		ot_none,	ot_none,	ot_none,	"negl",
	op_cseql_rr,	3,	ot_ear,		ot_ear,		ot_ear,		ot_none,	"cseql",
	op_cseql_rd,	3,	ot_ear,		ot_ead,		ot_ear,		ot_none,	"cseql",
	op_cseql_rx,	3,	ot_ear,		ot_eax,		ot_ear,		ot_none,	"cseql",
	op_cseql_rs,	3,	ot_ear,		ot_eas,		ot_ear,		ot_none,	"cseql",
	op_cseql_dr,	3,	ot_ead,		ot_ear,		ot_ear,		ot_none,	"cseql",
	op_cseql_xr,	3,	ot_eax,		ot_ear,		ot_ear,		ot_none,	"cseql",
	op_cseql_sr,	3,	ot_eas,		ot_ear,		ot_ear,		ot_none,	"cseql",
	op_csnel_rr,	3,	ot_ear,		ot_ear,		ot_ear,		ot_none,	"csnel",
	op_csnel_rd,	3,	ot_ear,		ot_ead,		ot_ear,		ot_none,	"csnel",
	op_csnel_rx,	3,	ot_ear,		ot_eax,		ot_ear,		ot_none,	"csnel",
	op_csnel_rs,	3,	ot_ear,		ot_eas,		ot_ear,		ot_none,	"csnel",
	op_csnel_dr,	3,	ot_ead,		ot_ear,		ot_ear,		ot_none,	"csnel",
	op_csnel_xr,	3,	ot_eax,		ot_ear,		ot_ear,		ot_none,	"csnel",
	op_csnel_sr,	3,	ot_eas,		ot_ear,		ot_ear,		ot_none,	"csnel",
	op_csgtl_rr,	3,	ot_ear,		ot_ear,		ot_ear,		ot_none,	"csgtl",
	op_csgtl_rd,	3,	ot_ear,		ot_ead,		ot_ear,		ot_none,	"csgtl",
	op_csgtl_rx,	3,	ot_ear,		ot_eax,		ot_ear,		ot_none,	"csgtl",
	op_csgtl_rs,	3,	ot_ear,		ot_eas,		ot_ear,		ot_none,	"csgtl",
	op_csgtl_dr,	3,	ot_ead,		ot_ear,		ot_ear,		ot_none,	"csgtl",
	op_csgtl_xr,	3,	ot_eax,		ot_ear,		ot_ear,		ot_none,	"csgtl",
	op_csgtl_sr,	3,	ot_eas,		ot_ear,		ot_ear,		ot_none,	"csgtl",
	op_csgel_rr,	3,	ot_ear,		ot_ear,		ot_ear,		ot_none,	"csgel",
	op_csgel_rd,	3,	ot_ear,		ot_ead,		ot_ear,		ot_none,	"csgel",
	op_csgel_rx,	3,	ot_ear,		ot_eax,		ot_ear,		ot_none,	"csgel",
	op_csgel_rs,	3,	ot_ear,		ot_eas,		ot_ear,		ot_none,	"csgel",
	op_csgel_dr,	3,	ot_ead,		ot_ear,		ot_ear,		ot_none,	"csgel",
	op_csgel_xr,	3,	ot_eax,		ot_ear,		ot_ear,		ot_none,	"csgel",
	op_csgel_sr,	3,	ot_eas,		ot_ear,		ot_ear,		ot_none,	"csgel",
	op_csltl_rr,	3,	ot_ear,		ot_ear,		ot_ear,		ot_none,	"csltl",
	op_csltl_rd,	3,	ot_ear,		ot_ead,		ot_ear,		ot_none,	"csltl",
	op_csltl_rx,	3,	ot_ear,		ot_eax,		ot_ear,		ot_none,	"csltl",
	op_csltl_rs,	3,	ot_ear,		ot_eas,		ot_ear,		ot_none,	"csltl",
	op_csltl_dr,	3,	ot_ead,		ot_ear,		ot_ear,		ot_none,	"csltl",
	op_csltl_xr,	3,	ot_eax,		ot_ear,		ot_ear,		ot_none,	"csltl",
	op_csltl_sr,	3,	ot_eas,		ot_ear,		ot_ear,		ot_none,	"csltl",
	op_cslel_rr,	3,	ot_ear,		ot_ear,		ot_ear,		ot_none,	"cslel",
	op_cslel_rd,	3,	ot_ear,		ot_ead,		ot_ear,		ot_none,	"cslel",
	op_cslel_rx,	3,	ot_ear,		ot_eax,		ot_ear,		ot_none,	"cslel",
	op_cslel_rs,	3,	ot_ear,		ot_eas,		ot_ear,		ot_none,	"cslel",
	op_cslel_dr,	3,	ot_ead,		ot_ear,		ot_ear,		ot_none,	"cslel",
	op_cslel_xr,	3,	ot_eax,		ot_ear,		ot_ear,		ot_none,	"cslel",
	op_cslel_sr,	3,	ot_eas,		ot_ear,		ot_ear,		ot_none,	"cslel",

	op_addf_rr,		2,	ot_ear,		ot_ear,		ot_none,	ot_none,	"addf",
	op_addf_rd,		2,	ot_ear,		ot_ead,		ot_none,	ot_none,	"addf",
	op_addf_rx,		2,	ot_ear,		ot_eax,		ot_none,	ot_none,	"addf",
	op_addf_rs,		2,	ot_ear,		ot_eas,		ot_none,	ot_none,	"addf",
	op_addf_dr,		2,	ot_ead,		ot_ear,		ot_none,	ot_none,	"addf",
	op_addf_xr,		2,	ot_eax,		ot_ear,		ot_none,	ot_none,	"addf",
	op_addf_sr,		2,	ot_eas,		ot_ear,		ot_none,	ot_none,	"addf",
	op_subf_rr,		2,	ot_ear,		ot_ear,		ot_none,	ot_none,	"subf",
	op_subf_rd,		2,	ot_ear,		ot_ead,		ot_none,	ot_none,	"subf",
	op_subf_rx,		2,	ot_ear,		ot_eax,		ot_none,	ot_none,	"subf",
	op_subf_rs,		2,	ot_ear,		ot_eas,		ot_none,	ot_none,	"subf",
	op_subf_dr,		2,	ot_ead,		ot_ear,		ot_none,	ot_none,	"subf",
	op_subf_xr,		2,	ot_eax,		ot_ear,		ot_none,	ot_none,	"subf",
	op_subf_sr,		2,	ot_eas,		ot_ear,		ot_none,	ot_none,	"subf",
	op_mulf_rr,		2,	ot_ear,		ot_ear,		ot_none,	ot_none,	"mulf",
	op_mulf_rd,		2,	ot_ear,		ot_ead,		ot_none,	ot_none,	"mulf",
	op_mulf_rx,		2,	ot_ear,		ot_eax,		ot_none,	ot_none,	"mulf",
	op_mulf_rs,		2,	ot_ear,		ot_eas,		ot_none,	ot_none,	"mulf",
	op_mulf_dr,		2,	ot_ead,		ot_ear,		ot_none,	ot_none,	"mulf",
	op_mulf_xr,		2,	ot_eax,		ot_ear,		ot_none,	ot_none,	"mulf",
	op_mulf_sr,		2,	ot_eas,		ot_ear,		ot_none,	ot_none,	"mulf",
	op_divf_rr,		2,	ot_ear,		ot_ear,		ot_none,	ot_none,	"divf",
	op_divf_rd,		2,	ot_ear,		ot_ead,		ot_none,	ot_none,	"divf",
	op_divf_rx,		2,	ot_ear,		ot_eax,		ot_none,	ot_none,	"divf",
	op_divf_rs,		2,	ot_ear,		ot_eas,		ot_none,	ot_none,	"divf",
	op_divf_dr,		2,	ot_ead,		ot_ear,		ot_none,	ot_none,	"divf",
	op_divf_xr,		2,	ot_eax,		ot_ear,		ot_none,	ot_none,	"divf",
	op_divf_sr,		2,	ot_eas,		ot_ear,		ot_none,	ot_none,	"divf",
	op_modf_rr,		2,	ot_ear,		ot_ear,		ot_none,	ot_none,	"modf",
	op_modf_rd,		2,	ot_ear,		ot_ead,		ot_none,	ot_none,	"modf",
	op_modf_rx,		2,	ot_ear,		ot_eax,		ot_none,	ot_none,	"modf",
	op_modf_rs,		2,	ot_ear,		ot_eas,		ot_none,	ot_none,	"modf",
	op_modf_dr,		2,	ot_ead,		ot_ear,		ot_none,	ot_none,	"modf",
	op_modf_xr,		2,	ot_eax,		ot_ear,		ot_none,	ot_none,	"modf",
	op_modf_sr,		2,	ot_eas,		ot_ear,		ot_none,	ot_none,	"modf",
	op_decf_r,		1,	ot_ear,		ot_none,	ot_none,	ot_none,	"decf",
	op_decf_d,		1,	ot_ead,		ot_none,	ot_none,	ot_none,	"decf",
	op_decf_x,		1,	ot_eax,		ot_none,	ot_none,	ot_none,	"decf",
	op_decf_s,		1,	ot_eas,		ot_none,	ot_none,	ot_none,	"decf",
	op_incf_r,		1,	ot_ear,		ot_none,	ot_none,	ot_none,	"incf",
	op_incf_d,		1,	ot_ead,		ot_none,	ot_none,	ot_none,	"incf",
	op_incf_x,		1,	ot_eax,		ot_none,	ot_none,	ot_none,	"incf",
	op_incf_s,		1,	ot_eas,		ot_none,	ot_none,	ot_none,	"incf",
	op_negf_r,		1,	ot_ear,		ot_none,	ot_none,	ot_none,	"negf",
	op_negf_d,		1,	ot_ead,		ot_none,	ot_none,	ot_none,	"negf",
	op_negf_x,		1,	ot_eax,		ot_none,	ot_none,	ot_none,	"negf",
	op_negf_s,		1,	ot_eas,		ot_none,	ot_none,	ot_none,	"negf",
	op_cseqf_rr,	3,	ot_ear,		ot_ear,		ot_ear,		ot_none,	"cseqf",
	op_cseqf_rd,	3,	ot_ear,		ot_ead,		ot_ear,		ot_none,	"cseqf",
	op_cseqf_rx,	3,	ot_ear,		ot_eax,		ot_ear,		ot_none,	"cseqf",
	op_cseqf_rs,	3,	ot_ear,		ot_eas,		ot_ear,		ot_none,	"cseqf",
	op_cseqf_dr,	3,	ot_ead,		ot_ear,		ot_ear,		ot_none,	"cseqf",
	op_cseqf_xr,	3,	ot_eax,		ot_ear,		ot_ear,		ot_none,	"cseqf",
	op_cseqf_sr,	3,	ot_eas,		ot_ear,		ot_ear,		ot_none,	"cseqf",
	op_csnef_rr,	3,	ot_ear,		ot_ear,		ot_ear,		ot_none,	"csnef",
	op_csnef_rd,	3,	ot_ear,		ot_ead,		ot_ear,		ot_none,	"csnef",
	op_csnef_rx,	3,	ot_ear,		ot_eax,		ot_ear,		ot_none,	"csnef",
	op_csnef_rs,	3,	ot_ear,		ot_eas,		ot_ear,		ot_none,	"csnef",
	op_csnef_dr,	3,	ot_ead,		ot_ear,		ot_ear,		ot_none,	"csnef",
	op_csnef_xr,	3,	ot_eax,		ot_ear,		ot_ear,		ot_none,	"csnef",
	op_csnef_sr,	3,	ot_eas,		ot_ear,		ot_ear,		ot_none,	"csnef",
	op_csgtf_rr,	3,	ot_ear,		ot_ear,		ot_ear,		ot_none,	"csgtf",
	op_csgtf_rd,	3,	ot_ear,		ot_ead,		ot_ear,		ot_none,	"csgtf",
	op_csgtf_rx,	3,	ot_ear,		ot_eax,		ot_ear,		ot_none,	"csgtf",
	op_csgtf_rs,	3,	ot_ear,		ot_eas,		ot_ear,		ot_none,	"csgtf",
	op_csgtf_dr,	3,	ot_ead,		ot_ear,		ot_ear,		ot_none,	"csgtf",
	op_csgtf_xr,	3,	ot_eax,		ot_ear,		ot_ear,		ot_none,	"csgtf",
	op_csgtf_sr,	3,	ot_eas,		ot_ear,		ot_ear,		ot_none,	"csgtf",
	op_csgef_rr,	3,	ot_ear,		ot_ear,		ot_ear,		ot_none,	"csgef",
	op_csgef_rd,	3,	ot_ear,		ot_ead,		ot_ear,		ot_none,	"csgef",
	op_csgef_rx,	3,	ot_ear,		ot_eax,		ot_ear,		ot_none,	"csgef",
	op_csgef_rs,	3,	ot_ear,		ot_eas,		ot_ear,		ot_none,	"csgef",
	op_csgef_dr,	3,	ot_ead,		ot_ear,		ot_ear,		ot_none,	"csgef",
	op_csgef_xr,	3,	ot_eax,		ot_ear,		ot_ear,		ot_none,	"csgef",
	op_csgef_sr,	3,	ot_eas,		ot_ear,		ot_ear,		ot_none,	"csgef",
	op_csltf_rr,	3,	ot_ear,		ot_ear,		ot_ear,		ot_none,	"csltf",
	op_csltf_rd,	3,	ot_ear,		ot_ead,		ot_ear,		ot_none,	"csltf",
	op_csltf_rx,	3,	ot_ear,		ot_eax,		ot_ear,		ot_none,	"csltf",
	op_csltf_rs,	3,	ot_ear,		ot_eas,		ot_ear,		ot_none,	"csltf",
	op_csltf_dr,	3,	ot_ead,		ot_ear,		ot_ear,		ot_none,	"csltf",
	op_csltf_xr,	3,	ot_eax,		ot_ear,		ot_ear,		ot_none,	"csltf",
	op_csltf_sr,	3,	ot_eas,		ot_ear,		ot_ear,		ot_none,	"csltf",
	op_cslef_rr,	3,	ot_ear,		ot_ear,		ot_ear,		ot_none,	"cslef",
	op_cslef_rd,	3,	ot_ear,		ot_ead,		ot_ear,		ot_none,	"cslef",
	op_cslef_rx,	3,	ot_ear,		ot_eax,		ot_ear,		ot_none,	"cslef",
	op_cslef_rs,	3,	ot_ear,		ot_eas,		ot_ear,		ot_none,	"cslef",
	op_cslef_dr,	3,	ot_ead,		ot_ear,		ot_ear,		ot_none,	"cslef",
	op_cslef_xr,	3,	ot_eax,		ot_ear,		ot_ear,		ot_none,	"cslef",
	op_cslef_sr,	3,	ot_eas,		ot_ear,		ot_ear,		ot_none,	"cslef",

	// extensions 2005-03-28
	op_pop,			0,	ot_none,	ot_none,	ot_none,	ot_none,	"pop",
	op_push,		0,	ot_none,	ot_none,	ot_none,	ot_none,	"push",

	// extensions 2005-11-12
	op_rtchk_r,		2,	ot_type,	ot_ear,		ot_none,	ot_none,	"rtchk",
	op_rtchk_d,		2,	ot_type,	ot_ead,		ot_none,	ot_none,	"rtchk",
	op_rtchk_x,		2,	ot_type,	ot_eax,		ot_none,	ot_none,	"rtchk",
	op_rtchk_s,		2,	ot_type,	ot_eas,		ot_none,	ot_none,	"rtchk",

	// extensions 2005-11-25
	op_jsr,			1,	ot_number,	ot_none,	ot_none,	ot_none,	"jsr",
	op_jsr_r,		1,	ot_ear,		ot_none,	ot_none,	ot_none,	"jsr",
	op_jsr_d,		1,	ot_ead,		ot_none,	ot_none,	ot_none,	"jsr",
	op_jsr_x,		1,	ot_eax,		ot_none,	ot_none,	ot_none,	"jsr",
	op_jsr_s,		1,	ot_eas,		ot_none,	ot_none,	ot_none,	"jsr",

	// extensions 2006-07-01
	op_newctx,		4,	ot_type,	ot_number,	ot_number,	ot_ear,		"newctx",
	op_resume_r,	1,	ot_ear,		ot_none,	ot_none,	ot_none,	"resume",
	op_resume_d,	1,	ot_ead,		ot_none,	ot_none,	ot_none,	"resume",
	op_resume_x,	1,	ot_eax,		ot_none,	ot_none,	ot_none,	"resume",
	op_resume_s,	1,	ot_eas,		ot_none,	ot_none,	ot_none,	"resume",
	op_yield,		0,	ot_none,	ot_none,	ot_none,	ot_none,	"yield",

	// extensions 2006-07-12, "weak references"
	op_wref_rr,		2,	ot_ear,		ot_ear,		ot_none,	ot_none,	"wref",
	op_wref_rd,		2,	ot_ear,		ot_ead,		ot_none,	ot_none,	"wref",
	op_wref_rx,		2,	ot_ear,		ot_eax,		ot_none,	ot_none,	"wref",
	op_wref_rs,		2,	ot_ear,		ot_eas,		ot_none,	ot_none,	"wref",
	op_wref_dr,		2,	ot_ead,		ot_ear,		ot_none,	ot_none,	"wref",
	op_wref_dd,		2,	ot_ead,		ot_ead,		ot_none,	ot_none,	"wref",
	op_wref_dx,		2,	ot_ead,		ot_eax,		ot_none,	ot_none,	"wref",
	op_wref_ds,		2,	ot_ead,		ot_eas,		ot_none,	ot_none,	"wref",
	op_wref_xr,		2,	ot_eax,		ot_ear,		ot_none,	ot_none,	"wref",
	op_wref_xd,		2,	ot_eax,		ot_ead,		ot_none,	ot_none,	"wref",
	op_wref_xx,		2,	ot_eax,		ot_eax,		ot_none,	ot_none,	"wref",
	op_wref_xs,		2,	ot_eax,		ot_eas,		ot_none,	ot_none,	"wref",
	op_wref_sr,		2,	ot_eas,		ot_ear,		ot_none,	ot_none,	"wref",
	op_wref_sd,		2,	ot_eas,		ot_ead,		ot_none,	ot_none,	"wref",
	op_wref_sx,		2,	ot_eas,		ot_eax,		ot_none,	ot_none,	"wref",
	op_wref_ss,		2,	ot_eas,		ot_eas,		ot_none,	ot_none,	"wref",

	// extensions 2007-09-13
	op_cmpref_rr,	3,	ot_ear,		ot_ear,		ot_ear,		ot_none,	"cmpref",

	// extensions 2008-04-13
	op_newdg,		3,	ot_type,	ot_number,	ot_ear,		ot_none,	"newdg",
	op_newdgm,		4,	ot_type,	ot_number,	ot_ear,		ot_ear,		"newdgm",
	op_calldg_r,	1,	ot_ear,		ot_none,	ot_none,	ot_none,	"calldg",
	op_calldg_d,	1,	ot_ead,		ot_none,	ot_none,	ot_none,	"calldg",
	op_calldg_x,	1,	ot_eax,		ot_none,	ot_none,	ot_none,	"calldg",
	op_calldg_s,	1,	ot_eas,		ot_none,	ot_none,	ot_none,	"calldg",

	// extensions 2010-02-15
	op_throw,		0,	ot_none,	ot_none,	ot_none,	ot_none,	"throw",

	// extensions 2014-04-12
	op_alloci,		2,	ot_type,	ot_ear,		ot_none,	ot_none,	"alloci",
	op_calli,		2,	ot_type,	ot_number,	ot_none,	ot_none,	"calli"
};
