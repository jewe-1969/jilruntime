//------------------------------------------------------------------------------
// File: JILPlatform.h                                    (c) 2005-2015 jewe.org
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
/// @file jilplatform.h
/// Platform specific macros and options for the JILRuntime library.
///
/// This file defines some platform specific macros and build options for the
/// JILRuntime libarary. It is automatically included when including "jilapi.h".
//------------------------------------------------------------------------------

#ifndef JILPLATFORM_H
#define JILPLATFORM_H

//------------------------------------------------------------------------------
// DEFINESCRIPT
//------------------------------------------------------------------------------
/// @def DEFINESCRIPT
/// This macro is useful to define script code in your C/C++ code. Just pass
/// your script code, without quotes, as an argument to this macro. The macro
/// will stringify the code.

#ifndef DEFINESCRIPT
#define DEFINESCRIPT(CODE)		#CODE
#endif

//------------------------------------------------------------------------------
// TAG
//------------------------------------------------------------------------------
/// @def TAG
/// This macro is useful for adding documentation tags to script function declarations.
/// Starting with version 1.1, the compiler allows classes and functions to have
/// documentation tags, which can be used by an integrated HTML documentation
/// generator. If you are concerned about the size of your native type library and
/// don't want to include the documentation tags in release builds, you can use
/// the macro below to define a tag that only exists in debug builds.
/// This macro does NOT work inside the above DEFINESCRIPT macro. You need to declare
/// your functions in quotes and append the TAG macro, like this:
/// <pre>const JILChar* kClassColor = "method Color();" TAG("Constructs a default color.");</pre>

#ifndef TAG
#ifdef _DEBUG
#define TAG(COMMENT)			"[\"" COMMENT "\"]"
#else
#define TAG(COMMENT)
#endif
#endif

//------------------------------------------------------------------------------
// JIL_USE_LITTLE_ENDIAN
//------------------------------------------------------------------------------
/// @def JIL_USE_LITTLE_ENDIAN
/// If you are compiling this project for a processor architecture that uses
/// little endianess (IA-32, AMD-64) you should set this macro to 1.
/// If target architecture uses big endian (PowerPC, SPARC, M68000) you should
/// set it to 0. At present, this macro is more or less unused.

#ifndef JIL_USE_LITTLE_ENDIAN
#define JIL_USE_LITTLE_ENDIAN	1
#endif

//------------------------------------------------------------------------------
// JIL_USE_LOCAL_FILESYS
//------------------------------------------------------------------------------
/// @def JIL_USE_LOCAL_FILESYS
/// Enable or disable access to the local file system.
/// The import statement by default allows to directly load and compile
/// additional script files from the local filesystem. If no ANSI file support
/// is available for your platform, or to prevent file import ,set this to 0.
/// Note that there is also a JewelScript compiler option that can prevent local
/// file import (while still allowing it to be re-enabled).
/// This switch can also be used to disable JCLGenerateDocs() and JCLGenerateBindings().

#ifndef JIL_USE_LOCAL_FILESYS
#define JIL_USE_LOCAL_FILESYS	1
#endif

//------------------------------------------------------------------------------
// JIL_USE_BINDING_CODEGEN
//------------------------------------------------------------------------------
/// @def JIL_USE_BINDING_CODEGEN
/// Enable or disable the integrated C++ binding code generator. You should only
/// set this macro to 1 for tools where you need the code generator
/// functionality, to reduce the size of the library, and avoid that files can
/// be created on the local file system where this isn't desired.

#ifndef JIL_USE_BINDING_CODEGEN
#ifdef _DEBUG
#define JIL_USE_BINDING_CODEGEN	1
#else
#define JIL_USE_BINDING_CODEGEN	0
#endif
#endif

//------------------------------------------------------------------------------
// JIL_USE_HTML_CODEGEN
//------------------------------------------------------------------------------
/// @def JIL_USE_HTML_CODEGEN
/// Enable or disable the integrated HTML documentation generator. You should only
/// set this macro to 1 for tools where you need the code generator
/// functionality, to reduce the size of the library, and avoid that files can
/// be created on the local file system where this isn't desired.

#ifndef JIL_USE_HTML_CODEGEN
#ifdef _DEBUG
#define JIL_USE_HTML_CODEGEN	1
#else
#define JIL_USE_HTML_CODEGEN	0
#endif
#endif

//------------------------------------------------------------------------------
// JIL_NO_FPRINTF
//------------------------------------------------------------------------------
/// @def JIL_NO_FPRINTF
/// Enable or disable usage of the ANSI FILE handle and fprintf().
/// Despite the name, ALL functionality that deals with ANSI FILE handles will
/// be disabled by this macro.
/// Setting this to 1 may be useful for security reasons, or if your platform
/// or C-runtime doesn't fully support file I/O.

#ifndef JIL_NO_FPRINTF
#define JIL_NO_FPRINTF			0
#endif

//------------------------------------------------------------------------------
// JIL_USE_INSTRUCTION_COUNTER
//------------------------------------------------------------------------------
/// @def JIL_USE_INSTRUCTION_COUNTER
/// Enable or disable the VM's instruction counter. The instruction counter is
/// a simple 64-bit integer that gets increased for every VM instruction that
/// is executed. It is intended for performance measurement / benchmarking.
/// Of course, the extra increment for every instruction adds an insignificant
/// drop in execution speed to the VM, so if you don't need the instruction
/// counter, you can disable it.

#ifndef JIL_USE_INSTRUCTION_COUNTER
#define JIL_USE_INSTRUCTION_COUNTER		1
#endif

//------------------------------------------------------------------------------
// JIL_RUNTIME_CHECKS
//------------------------------------------------------------------------------
/// @def JIL_RUNTIME_CHECKS
/// Enable or disable extended runtime checks while executing byte-code. If this
/// macro is undefined, it will be enabled in DEBUG builds of the library, but
/// disabled in RELEASE builds for maximum performance.
/// Disabling this option greatly affects the VM's ability to detect runtime
/// errors and may lead to crashes if the byte-code is faulty.
/// Enabling this option greatly affects the VM's performance.

#ifndef JIL_RUNTIME_CHECKS
#ifdef _DEBUG
#define JIL_RUNTIME_CHECKS		1
#else
#define JIL_RUNTIME_CHECKS		0
#endif
#endif

//------------------------------------------------------------------------------
// JIL_TRACE_RELEASE
//------------------------------------------------------------------------------
/// @def JIL_TRACE_RELEASE
/// Enable the TRACE exception in release builds of the virtual machine. This is
/// disabled by default for performance reasons. If this is enabled, the release
/// build of the virtual machine will invoke a TRACE exception before every
/// VM instruction that is about to be executed.
/// If you don't have very good reasons to enable this, you should leave it off.

#ifndef JIL_TRACE_RELEASE
#define JIL_TRACE_RELEASE		0
#endif

//------------------------------------------------------------------------------
// JIL_STRING_POOLING
//------------------------------------------------------------------------------
/// @def JIL_STRING_POOLING
/// Allows to disable string pooling when compiling programs. This will result
/// in larger executables, but may speed up compiling on slower machines.

#ifndef JIL_STRING_POOLING
#define JIL_STRING_POOLING		1
#endif

//------------------------------------------------------------------------------
// JIL_MACHINE_NO_64_BIT
//------------------------------------------------------------------------------
/// @def JIL_MACHINE_NO_64_BIT
/// Enable this for machines that do not support 64 bit integer / floating-point
/// math, or where this is would be too slow. The library will use 32 bit for
/// JILUInt64 and JILFloat if this macro is defined (EXPERIMENTAL).

#ifndef JIL_MACHINE_NO_64_BIT
#define JIL_MACHINE_NO_64_BIT	0
#endif

//------------------------------------------------------------------------------
// external declarations
//------------------------------------------------------------------------------

#ifdef __cplusplus
	#define	JILEXTERN			extern "C"
	#define BEGIN_JILEXTERN		extern "C" {
	#define END_JILEXTERN		}
#else
	#define	JILEXTERN			extern
	#define BEGIN_JILEXTERN
	#define END_JILEXTERN
#endif

//------------------------------------------------------------------------------
// inline keyword
//------------------------------------------------------------------------------

#if _MSC_VER >= 1200	// MS VC 6.0 or higher
	#define	JILINLINE	__forceinline
#else
	#define	JILINLINE	static
#endif

//------------------------------------------------------------------------------
// WINDOWS
//------------------------------------------------------------------------------

#ifdef WIN32
	#define JIL_PATHSEPARATOR		'\\'
	#define JIL_PATHSEPARATORSTR	"\\"
	#define	JIL_VSNPRINTF			_vsnprintf
	#define JIL_STRREV				_strrev
#else

//------------------------------------------------------------------------------
// LINUX
//------------------------------------------------------------------------------

#ifdef LINUX
	#define JIL_PATHSEPARATOR		'/'
	#define JIL_PATHSEPARATORSTR	"/"
	#define	JIL_VSNPRINTF			vsnprintf
	#define JIL_STRREV				strrev
#else

//------------------------------------------------------------------------------
// MACOSX
//------------------------------------------------------------------------------

#ifdef MACOSX
	#define JIL_PATHSEPARATOR		'/'
	#define JIL_PATHSEPARATORSTR	"/"
	#define	JIL_VSNPRINTF			vsnprintf
	#define JIL_STRREV				strrev
#else

//------------------------------------------------------------------------------
// Any other platform
//------------------------------------------------------------------------------

	#define JIL_PATHSEPARATOR		'/'
	#define JIL_PATHSEPARATORSTR	"/"
	#define	JIL_VSNPRINTF			vsnprintf
	#define JIL_STRREV				strrev
#endif // MACOSX

#endif // LINUX
#endif // WIN32
#endif // JILPLATFORM_H
