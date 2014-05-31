//------------------------------------------------------------------------------
// File: JILPlatform.h                                    (c) 2005-2010 jewe.org
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
/// Platform specific definitions for the JILRuntime library.
///
/// This file defines some platform specific macros that the JILRuntime library
/// and some native types are using. This file is automatically included when
/// including file "jilapi.h".
//------------------------------------------------------------------------------

#ifndef JILPLATFORM_H
#define JILPLATFORM_H

//------------------------------------------------------------------------------
// DEFINESCRIPT
//------------------------------------------------------------------------------
/// This macro is useful to define script code in your C/C++ code. Just pass
/// your script code, without quotes, as an argument to this macro. The macro
/// will wrap the code in quotes.

#ifndef DEFINESCRIPT
#define DEFINESCRIPT(CODE)		#CODE
#endif

//------------------------------------------------------------------------------
// TAG
//------------------------------------------------------------------------------
/// This macro is useful for adding documentation tags to script function declarations.
/// Starting with version 1.1, the compiler allows classes and functions to have
/// documentation tags which can be used by an integrated HTML documentation
/// generator. If you are concerned about the size of your native type library and
/// don't want to include the documentation tags in release builds, you can use
/// the macro below to define a tag that is only there in debug builds.
/// This macro does not work inside the above DEFINESCRIPT macro. You need to declare
/// your functions in quotes and append the TAG macro, like this:
/// const JILChar* kClassColor = "method Color();" TAG("Constructs a default color.");

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
/// If you are compiling this project for a processor architecture that uses
/// little endianess (IA-32, AMD-64) you must set this macro to 1.
/// If target architecture uses big endian (PowerPC, SPARC, M68000), set it to 0.

#ifndef JIL_USE_LITTLE_ENDIAN
#define JIL_USE_LITTLE_ENDIAN	1
#endif

//------------------------------------------------------------------------------
// JIL_USE_LOCAL_FILESYS
//------------------------------------------------------------------------------
/// Enable or disable access to the local file system.
/// The import statement by default allows to directly load and compile
/// additional script files from the local filesystem, if no ANSI file support
/// is available for your platform, or to prevent file import set this to 0.
/// Note that there is also a JewelScript compiler option that can prevent local
/// file import (while still allowing it to be re-enabled).
/// This switch can also be used to disable JCLGenerateDocs() and JCLGenerateBindings().

#ifndef JIL_USE_LOCAL_FILESYS
#define JIL_USE_LOCAL_FILESYS	1
#endif

//------------------------------------------------------------------------------
// JIL_USE_BINDING_CODEGEN
//------------------------------------------------------------------------------
/// Enable or disable the integrated C++ binding code generator. You should only
/// set this macro to true for tools where you need the code generator
/// functionality to reduce the size of the library and avoid that files can
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
/// Enable or disable the integrated HTML documentation generator. You should only
/// set this macro to true for tools where you need the code generator
/// functionality to reduce the size of the library and avoid that files can
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
/// Enable or disable usage of the ANSI FILE handle and fprintf().
/// The code-listing functionality accepts an ANSI FILE handle and uses the
/// fprintf() function to list code, if your platform doesn't support the
/// FILE handle and/or fprintf(), you can disable code-listing by setting this
/// to 1.

#ifndef JIL_NO_FPRINTF
#define JIL_NO_FPRINTF			0
#endif

//------------------------------------------------------------------------------
// JIL_USE_INSTRUCTION_COUNTER
//------------------------------------------------------------------------------
/// Enable or disable the VM's instruction counter. The instruction counter is
/// simple a 32-bit integer that gets increased for every VM instruction that
/// is executed. It is intended for performance measurement / benchmarking only.
/// Of course, the extra increment for each instruction adds a slight drop in
/// execution speed to the VM, so if you don't need the instruction counter, you
/// can disable it.

#ifndef JIL_USE_INSTRUCTION_COUNTER
#define JIL_USE_INSTRUCTION_COUNTER		1
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
#else

//------------------------------------------------------------------------------
// LINUX
//------------------------------------------------------------------------------

#ifdef LINUX
	#define JIL_PATHSEPARATOR		'/'
	#define JIL_PATHSEPARATORSTR	"/"
	#define	JIL_VSNPRINTF			vsnprintf
#else

//------------------------------------------------------------------------------
// MACOSX
//------------------------------------------------------------------------------

#ifdef MACOSX
	#define JIL_PATHSEPARATOR		'/'
	#define JIL_PATHSEPARATORSTR	"/"
	#define	JIL_VSNPRINTF			vsnprintf
#else

//------------------------------------------------------------------------------
// Any other platform
//------------------------------------------------------------------------------

	#define JIL_PATHSEPARATOR		'/'
	#define JIL_PATHSEPARATORSTR	"/"
	#define	JIL_VSNPRINTF			vsnprintf
#endif // MACOSX

#endif // LINUX
#endif // WIN32
#endif // JILPLATFORM_H
