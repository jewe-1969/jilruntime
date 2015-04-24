//------------------------------------------------------------------------------
// File: JILTypes.h                                         (c) 2003 jewe.org
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
/// @file jiltypes.h
///	Internal definitions for the JILRuntime library.
///
/// This file contains type definitions used internally by the library.
//------------------------------------------------------------------------------

#ifndef JILSTRUCTS_H
#define JILSTRUCTS_H

//------------------------------------------------------------------------------
// include public API definitions
//------------------------------------------------------------------------------

#include "jilnativetypeex.h"

//------------------------------------------------------------------------------
// JILFILE macro
//------------------------------------------------------------------------------

#if JIL_NO_FPRINTF
#define	JILFILE		void
#else
#define JILFILE		FILE
#endif

//------------------------------------------------------------------------------
// misc constants
//------------------------------------------------------------------------------
/// Various constants used in the library.

enum
{
	std_handle_null,		///< Index number of the one-and-only null handle (DO NOT CHANGE)
	kLogGarbageNone = 0,	///< Don't list anything
	kLogGarbageBrief = 1,	///< List leaked objects, but not their child objects, if they can be freed by feeing their parent
	kLogGarbageAll = 2,		///< List all leaked objects
};

//------------------------------------------------------------------------------
// operand types for instruction info
//------------------------------------------------------------------------------
/// These enum values are used for the JILInstrInfo::opType member.

typedef enum
{
	ot_none = 0,	///< No operand
	ot_number,		///< Immediate integer number
	ot_handle,		///< Immediate handle number
	ot_type,		///< Immediate type identifier number
	ot_label,		///< A branch label (used by assembler/disassembler)
	ot_ear,			///< Operand is addressing mode "register direct", i.e. "r7"
	ot_ead,			///< Operand is addressing mode "register indirect, displacement", i.e. "(r5+16)"
	ot_eax,			///< Operand is addressing mode "register indirect, indexed", i.e. "(r7+r5)"
	ot_eas,			///< Operand is addressing mode "stack, displacement", i.e. "(sp+12)"
	ot_regrng,		///< Operand is a register range, i.e. "r3-r7"

	kNumOperandTypes

} JILOperandType;

//------------------------------------------------------------------------------
// Handle flags
//------------------------------------------------------------------------------
/// This enum defines flags for the JILHandle::flags member.

typedef enum
{
	HF_NEWBUCKET	= 1 << 0,	///< handle address is address of a "bucket"
	HF_PERSIST		= 1 << 1,	///< do NOT destroy encapsulated object
	HF_MARKED		= 1 << 2	///< handle is marked in response to garbage collection MARK command

} JILHandleFlags;

//------------------------------------------------------------------------------
// flags for JILFuncInfo
//------------------------------------------------------------------------------
/// These flags describe the functions stored in the runtime's function segment.

typedef enum
{
	fi_method		= 1 << 0,	///< If true is method, if false is global function
	fi_ctor			= 1 << 1,	///< method is constructor
	fi_convertor	= 1 << 2,	///< method is convertor
	fi_accessor		= 1 << 3,	///< method is accessor
	fi_cofunc		= 1 << 4,	///< method is cofunction
	fi_anonymous	= 1 << 5,	///< anonymous local method or function
	fi_explicit		= 1 << 6,	///< constructor / convertor is explicit
	fi_strict		= 1 << 7,	///< method is strict
	fi_virtual		= 1 << 8	///< method is virtual

} JILFuncInfoFlags;

//------------------------------------------------------------------------------
// structs
//------------------------------------------------------------------------------

typedef struct JILChunkHeader		JILChunkHeader;
typedef struct JILObjectDesc		JILObjectDesc;
typedef struct JILNObjectDesc		JILNObjectDesc;
typedef struct JILRestorePoint		JILRestorePoint;
typedef struct JILExceptionInfo		JILExceptionInfo;
typedef struct JCLErrorInfo			JCLErrorInfo;
typedef struct JILInstrInfo			JILInstrInfo;
typedef struct JILString			JILString;
typedef struct JILArray				JILArray;
typedef struct JILList				JILList;
typedef struct JILListItem			JILListItem;
typedef struct JILIterator			JILIterator;
typedef struct JILArrayList			JILArrayList;
typedef struct JILFuncInfo			JILFuncInfo;
typedef struct JILDataHandle		JILDataHandle;
typedef struct JILClosure			JILClosure;
typedef struct JILDelegate			JILDelegate;
typedef struct JILMethodInfo		JILMethodInfo;
typedef struct JILRuntimeException	JILRuntimeException;

//------------------------------------------------------------------------------
// JILHandleData
//------------------------------------------------------------------------------
/// This determines the maximum size of the data a JILHandle can hold.
/// The data size must be at least 64 bits, which this union guarantees.

typedef union
{
	JILLong		_int;
	JILFloat	_float;
	JILUnknown*	_ptr;

} JILHandleData;

//------------------------------------------------------------------------------
// struct JILHandle
//------------------------------------------------------------------------------
/// A small struct describing the type of any data the virtual machine deals
/// with. The virtual machine uses JILHandles for every value, object or other
/// entity it processes when executing instructions. The purpose of the handles
/// is allowing to determine the actual type of data, as well as keeping track
/// of the number of references to that data, in order to automatically free
/// the memory used by the data when it is no longer used.
/// @see opaque structs in JILHandle.h

struct JILHandle
{
	JILLong			type;				///< The type of the value this handle encapsulates, see struct JILTypeInfo
	JILLong			flags;				///< Flags, see enum JILHandleFlags
	JILLong			refCount;			///< Number of references to the value
	JILLong			reserved;			///< Reserved to ensure 8-byte alignment for 64-bit float
	JILHandleData	data[1];			///< The handle's value, handle type dependent, see opaque structs in jilhandle.h
};

//------------------------------------------------------------------------------
// struct NTLInstance
//------------------------------------------------------------------------------
///	Instance data for a Native Type Library (NTL). A NTLInstance pointer will be
///	passed to the NTL with certain calls to it's main entry point function.
///	The NTL can use the NTL Instance to store additional, instance-related data.
///	<p><b>Attention!</b><br>
///	Do not confuse the term 'NTL Instance' with an instance of an object the
///	NTL might allocate when it receives the NTL_NewObject message!
///	Taking the 'string' native type library as an example, each virtual machine
///	state you allocate by calling JILInitialize() will own <b>a single instance</b>
///	of the string native type, this is called the 'NTL Instance'.
///	If an application allocates three virtual machine states and each of them uses
///	the string NTL, then there will be three NTL Instances of it.
///	Each NTL Instance in turn, can create any amount of string objects when it
///	receives the NTL_NewObject message. Even though you can view the creation of
///	string objects also as an instantiation, this is not meant by the term
/// 'NTL Instance'.</p><p>
///	You should use the NTL Instance instead of storing data in global variables
///	in your native type. Using global variables will <b>not work</b> if
/// you want to use multiple instances of the JIL virtual machine in the same
/// application!</p><p>
///	You should not access the members of this struct directly. Instead, you
///	should use these accessor functions, declared in jilnativetype.h:<br>
///	NTLInstanceSetUser()<br>NTLInstanceGetUser()<br>
///	NTLInstanceTypeID()<br>NTLInstanceGetVM()</p>

struct NTLInstance
{
	JILLong			typeID;				///< The type ID of the NTL instance
	JILUnknown*		userData;			///< General purpose user data for the NTL instance
	JILState*		pState;				///< Pointer to VM owning this instance
};

//------------------------------------------------------------------------------
// struct JILMethodInfo
//------------------------------------------------------------------------------
/// This struct defines method indexes of special methods that the runtime needs
/// access to, for example constructor, copy constructor and destructor.

struct JILMethodInfo
{
	JILLong			ctor;				///< Method index of the type's standard constructor or -1 if undefined (future, currently unused)
	JILLong			cctor;				///< Method index of the type's copy-constructor or -1 if undefined
	JILLong			dtor;				///< Method index of the type's destructor or -1 if undefined (future, currently unused)
	JILLong			tostr;				///< Method index of the type's string-convertor, or -1 if undefined
};

//------------------------------------------------------------------------------
// struct JILTypeInfo
//------------------------------------------------------------------------------
/// This struct describes a type in the runtime environment.

struct JILTypeInfo
{
	JILLong			type;				///< The type identifier number (which is the index of this struct in the TypeInfo segment)
	JILLong			base;				///< The type identifier of the base-class, if family == tf_class
	JILLong			family;				///< The type-family, see enum JILTypeFamily
	JILBool			isNative;			///< The type has a native type proc
	JILLong			offsetName;			///< Offset to the name of the type in the CStr segment, if family == tf_class
	JILLong			offsetVtab;			///< Offset to v-table in CStr segment, if family == tf_class and isNative == false
	JILLong			sizeVtab;			///< Size (# of functions) of the class, if family == tf_class
	JILLong			instanceSize;		///< Size (# of handles) of an instance of a class written in VM code, if isNative == false and family == tf_class
	JILLong			interfaceVersion;	///< JIL native type interface version the type uses, if isNative == true
	JILLong			authorVersion;		///< Author's product version, if isNative == true
	JILMethodInfo	methodInfo;			///< Info about special methods, if family == tf_class
	JILTypeProc		typeProc;			///< Pointer to the main entry point function, if isNative == true (ALL types, not only classes, can have a typeProc now!). This pointer will be initialized from a JILTypeListItem when the runtime is initialized and a native type matching the name is registered to the VM.
	NTLInstance		instance;			///< Instance data for this type
	const JILChar*	typeNamePtr;		///< Will point to type name in CStr segment after initialization (for easier debugging)
};

//------------------------------------------------------------------------------
// struct JILTypeListItem
//------------------------------------------------------------------------------
/// This struct describes a native type library that is registered to the
/// runtime environment.

struct JILTypeListItem
{
	JILChar*		pClassName;	///< Points to the native type libraries class name
	JILTypeProc		typeProc;	///< Pointer to the main entry point function
};

//------------------------------------------------------------------------------
// struct JILInstrInfo
//------------------------------------------------------------------------------
/// This struct is used to build the instruction info table, which is used to
/// generate a clear text listing of the VM bytecode (it is also used by the
/// byte code optimizer), JILGetInstructionInfo().

struct JILInstrInfo
{
	JILLong			opCode;			///< Virtual machine instruction number
	JILLong			instrSize;		///< Instruction size (number of ints)
	JILLong			numOperands;	///< Number of operands of this instruction
	JILLong			opType[4];		///< Enumeration value describing the operand, see enum JILOperandType
	const JILChar*	name;			///< Clear text name of the instruction (mnemonic)
};

//------------------------------------------------------------------------------
// struct JILChunkHeader
//------------------------------------------------------------------------------
/// Structure of the header of a binary JIL program.
/// @see JILLoadBinary and JILSaveBinary

struct JILChunkHeader
{
	JILChar			cnkMagic[16];		///< Magic identifier
	JILLong			cnkSize;			///< Chunk size
	JILLong			cnkTypeSegSize;		///< Size of TypeInfo segment
	JILLong			cnkDataSegSize;		///< Size of data segment
	JILLong			cnkCodeSegSize;		///< Size of code segment
	JILLong			cnkFuncSegSize;		///< Size of function segment
	JILLong			cnkCStrSegSize;		///< Size of string constant segment
	JILLong			cnkSymTabSize;		///< Size of symbol table
};

//------------------------------------------------------------------------------
// struct JILRestorePoint
//------------------------------------------------------------------------------
/// This is used by JILCreateRestorePoint() and JILGotoRestorePoint(), defined
/// in file jilprogramming.h.

struct JILRestorePoint
{
	JILLong			reMagic;			///< Magic identifier
	JILLong			reUsedTypeSegSize;	///< Saved TypeInfo segment size
	JILLong			reUsedDataSegSize;	///< Saved data segment size
	JILLong			reUsedCodeSegSize;	///< Saved code segment size
	JILLong			reUsedCStrSegSize;	///< Saved cstr segment size
	JILLong			reUsedSymTabSize;	///< Saved symbol table size
};

//------------------------------------------------------------------------------
// struct JILStackFrame
//------------------------------------------------------------------------------
/// This helper object is created on the native stack in preparation of
/// a call to a JIL function. It saves the current VM context and allows for
/// multiple, nested calls to the JILCallFunction() function.

struct JILStackFrame
{
	JILContext*		ctx;				///< Saved previous context
	JILLong			pc;					///< Saved program counter
	JILLong			cstp;				///< Saved call stack pointer
	JILLong			dstp;				///< Saved data stack pointer
};

//------------------------------------------------------------------------------
// struct JILExceptionInfo
//------------------------------------------------------------------------------
/// This struct is used internally to create the exception string table.

struct JILExceptionInfo
{
	JILLong			e;
	const JILChar*	s;
};

//------------------------------------------------------------------------------
// struct JCLErrorInfo
//------------------------------------------------------------------------------
// This struct is used internally to create the errors and warnings table.

struct JCLErrorInfo
{
	JILLong			l;
	JILLong			e;
	const JILChar*	s;
};

//------------------------------------------------------------------------------
// struct JILSymTabEntry
//------------------------------------------------------------------------------
/// This struct is used internally by the symbol table.

struct JILSymTabEntry
{
	JILSymTabEntry*		pPrev;		///< Pointer to previous entry
	JILSymTabEntry*		pNext;		///< Pointer to next entry
	JILLong				sizeName;	///< Size, in bytes, of the symbol name (including termination)
	JILChar*			pName;		///< Pointer to the symbol name (dynamically allocated)
	JILLong				sizeData;	///< Size, in bytes, of the data block associated with the symbol name
	JILUnknown*			pData;		///< Pointer to the data block (dynamically allocated)
};

//------------------------------------------------------------------------------
// struct JILFuncInfo
//------------------------------------------------------------------------------
/// This struct describes a function and carries information required
/// during runtime.

struct JILFuncInfo
{
	JILLong				type;		///< The type identifier number of the class the function belongs to
	JILLong				flags;		///< The function type, this is a bitfield, @see enum JILFuncInfoFlags
	JILLong				codeAddr;	///< The code address of the function
	JILLong				codeSize;	///< The size of the function in instruction words, or 0 if native function
	JILLong				args;		///< The number of arguments the function expects on the stack
	JILLong				memberIdx;	///< The member index of the function or method
	JILLong				offsetName;	///< Offset to the function name in the CStr segment
};

//------------------------------------------------------------------------------
// struct JILDataHandle
//------------------------------------------------------------------------------
/// This struct is used to store data handles in the data segment. When the VM
/// is initialized, it automatically creates runtime handles out of the data
/// handles. Data handles are used to store global literals. The moveh
/// instruction can directly load these literals.

struct JILDataHandle
{
	JILLong				type;		///< The type of data this handle encapsulates, see struct JILTypeInfo
	JILLong				index;		///< The index number this handle should have as a runtime handle
	JILHandleData		data[1];	///< The handle data, type dependant
};

//------------------------------------------------------------------------------
// struct JILMemStats
//------------------------------------------------------------------------------
/// This is a small object that is passed to the Fixed Memory management
/// functions to keep track of number of allocs, number of bytes allocated, and
/// so on. The statistics is only generated in DEBUG builds, and only when
/// Fixed Memory Management is enabled. FixMem allocates memory in bigger blocks,
/// called buckets. It never frees buckets, but keeps them around for fast reuse
/// of free memory. This value should give a rough picture how much memory
/// the runtime really has used. Note that FixMem only is used when memory is
/// allocated during run-time, not when the compiler or other parts of the
/// library allocate it. The built-in native type libraries use FixMem too.
/// But of course it is up to the implementor, whether or not a specific
/// type library uses FixMem or just new / malloc.
/// @see JILUseFixedMemory ()

struct JILMemStats
{
	JILLong				numAlloc;		///< The total number of memory allocations
	JILLong				numFree;		///< Total number of memory frees
	JILLong				bytesUsed;		///< Currently allocated number of bytes
	JILLong				maxBytesUsed;	///< Maximum number of allocated bytes at any time
	JILLong				numBuckets;		///< Total number of buckets used
	JILLong				bucketBytes;	///< Total number of bytes allocated for buckets
};

//------------------------------------------------------------------------------
// struct JILClosure
//------------------------------------------------------------------------------
/// Additional data for 'closure' delegates, which are anonymous local functions
/// and methods that have access to the parent function's stack.

struct JILClosure
{
	JILLong				stackSize;		///< size of the parent function's stack
	JILHandle**			ppStack;		///< snapshot of the parent function's stack
};

//------------------------------------------------------------------------------
// struct JILDelegate
//------------------------------------------------------------------------------
/// This small struct describes a delegate, which represents a first class
/// function in JewelScript. A delegate can be seen as a reference to a global
/// or instance member function. The language allows to assign any type of
/// global or instance member function to a delegate variable.

struct JILDelegate
{
	JILLong				index;			///< method index in case of an instance member function, otherwise global function index
	JILHandle*			pObject;		///< 'this' reference in case of an instance member function, otherwise NULL
	JILClosure*			pClosure;		///< if the delegate is a closure, contains the parent function's stack, else NULL
};

//------------------------------------------------------------------------------
// struct JILRuntimeException
//------------------------------------------------------------------------------
/// When the runtime detects an error during JILCallFunction() it will generate
/// and return an instance of this class. This object is NOT used when user code
/// throws an exception, but in all cases where the error is generated internally.

struct JILRuntimeException
{
	JILLong				error;			///< the error code number of the exception
	JILString*			pMessage;		///< a string describing the exception
};

//------------------------------------------------------------------------------
// struct JILDelegate
//------------------------------------------------------------------------------
/// Record used by the garbage collector event system.

struct JILGCEventRecord
{
	JILUnknown*			pUserPtr;		///< pointer to native "user" object
	JILGCEventHandler	eventProc;		///< native callback function
	JILGCEventRecord*	pNext;			///< pointer to next record
};

//------------------------------------------------------------------------------
// struct JILFileHandle
//------------------------------------------------------------------------------
/// Encapsulates a file object for use with the VM's file input proc.

struct JILFileHandle
{
	JILUnknown*			pStream;		///< pointer to FILE object
	JILState*			pState;			///< pointer to vm state
};

//------------------------------------------------------------------------------
// callback for EnumSymbolTableEntries()
//------------------------------------------------------------------------------
/// This is a callback that can be used to process multiple symbol table entries.
/// @see EnumSymbolTableEntries in jilsymboltable.h

typedef JILError (*JILSymTabEnumerator)(JILState*, JILLong, JILSymTabEntry*, JILUnknown*);

//------------------------------------------------------------------------------
// JILOptionHandler
//------------------------------------------------------------------------------

typedef JILError (*JILOptionHandler)(JILState*, const JILChar*, const JILChar*);

#endif	// #ifndef JILTYPES_H
