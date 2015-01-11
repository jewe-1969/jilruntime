//------------------------------------------------------------------------------
// File: JILCompilerApi.h                                 (c) 2005-2010 jewe.org
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
/// @file jilcompilerapi.h
/// Contains the JewelScript compiler API.
///
/// This file contains the public interface for developers that want to use the
/// JewelScript compiler. This API introduces a high-level compiler language to
/// make programming the virtual machine easier. The syntax is very similar to
/// that of C/C++ and scripting languages like for example JavaScript.
/// The API allows users of the library to program the virtual machine by
/// passing JewelScript source code down to the library, instead of writing
/// binary instruction data or using the virtual assembly language.
/// The JewelScript code can either be used to write a "standalone" program for
/// the VM, or it could be used to automate a C/C++ program, by writing macro
/// functions in JewelScript code.
///
///	<p>Note that as of version 0.9 of the library, the compiler API has been
/// merged with the runtime API. You no longer create the compiler seperately;
/// it is created automatically for you as soon as you call JILInitialize().</p>
///	<p>Consequently, all functions in this file get passed a pointer to the
///	JILState runtime object, instead the old 'JCLState' pointer.</p>
/// 
//------------------------------------------------------------------------------

#ifndef JILCOMPILERAPI_H
#define JILCOMPILERAPI_H

#include "jilapitypes.h"
#include "jilexception.h"

//------------------------------------------------------------------------------
// JCLCompile
//------------------------------------------------------------------------------
/// Call subsequently to compile functions or whole files. The code snippet
/// passed to this function must be syntactically and semantically correct. Note
/// that the code generated will not be ready for execution unless JCLLink()
/// has been called. The parameter pName has no specific meaning, except it will
/// be used as the file name, when the compiler issues a compiler or warning
/// message. If pName is NULL, "anonymous character string" will be used.

JILEXTERN JILError				JCLCompile				(JILState* pState, const JILChar* pName, const JILChar* pText);

//------------------------------------------------------------------------------
// JCLLoadAndCompile
//------------------------------------------------------------------------------
/// This can be used to directly load and compile a file from disk. Pass the
/// file name to the pName parameter. Note that the code generated will not be
/// ready for execution unless JCLLink() has been called.

JILEXTERN JILError				JCLLoadAndCompile		(JILState* pState, const JILChar* pName);

//------------------------------------------------------------------------------
// JCLLink
//------------------------------------------------------------------------------
/// Call this when you are done parsing all source code. If no error occurred,
/// the virtual machine will be able to run the program built from the
/// source code.
/// <p>You may call JCLCompile(), JCLLoadAndCompile() or JCLAddAnonFunction()
/// again after linking, but you also need to call JCLLink() again afterwards
/// for the changes to take effect.</p>
/// <p>After linking, and before calling any script function, you should call
/// JILRunInitCode() to initialize all global variables</p>

JILEXTERN JILError				JCLLink					(JILState* pState);

//------------------------------------------------------------------------------
// JCLGetErrorText
//------------------------------------------------------------------------------
/// Call this after JCLCompile() or JCLLink() to retrieve any warning or error
/// messages the compiler might have generated. If more than one error or
/// warning have been detected, this can be called subsequently to retrieve all
/// messages. If the function returns NULL, all messages have been returned.

JILEXTERN const JILChar*		JCLGetErrorText			(JILState* pState);

//------------------------------------------------------------------------------
// JCLCompileAndRun
//------------------------------------------------------------------------------
/// This compiles the given code into an anonymous function, links the program
/// and executes the generated function. This is intended to provide an easy
/// to use "direct mode" functionality, which could be used, for example, by a
/// command processor to compile and execute the user's input.
/// The specified script code should only contain the function's body, which is
/// everything between and not including the function definition's curly braces.
/// @param pState   The compiler object
///	@param pText	The function body

JILEXTERN JILError				JCLCompileAndRun		(JILState* pState, const JILChar* pText);

//------------------------------------------------------------------------------
// JCLAddAnonFunction
//------------------------------------------------------------------------------
/// Similar to the previous function, this compiles the given script code pointed
/// to by pText into an anonymous function. However, unlike the previous function,
/// the generated code will not be immediately executed.
/// Instead, the handle of the created function will be returned, so
/// the function can be executed with JILCallFunction() anytime. Use NTLFreeHandle()
/// when you don't need the function handle anymore.
/// If an error occurs, the result of this function is NULL.
/// <p>The specified script code should only contain the function's body, which
/// is everything between and not including the function definition's curly
/// braces.</p>
/// <p>To specify arguments to the created function, specify their declarations as
/// a comma seperated list in 'pArgs'.</p>
/// Make sure to re-run the init code by calling JILRunInitCode() before trying
/// to call the returned function.
/// @param pState   The compiler object
/// @param pRes		Type name of the result, or empty string
/// @param pArgs    List of argument declarations, or empty string
///	@param pText	The function body

JILEXTERN JILHandle*			JCLAddAnonFunction		(JILState* pState, const JILChar* pRes, const JILChar* pArgs, const JILChar* pText);

//------------------------------------------------------------------------------
// JCLSetFatalErrorHandler
//------------------------------------------------------------------------------
/// Installs a callback function that will be called in case the compiler
/// detects a <b>fatal error</b>. A fatal error is an error from which the
/// compiler cannot recover and that is likely to crash the Application if the
/// compiler would proceed. The purpose of this callback is to give the
/// Application the opportunity to graciously terminate itself.
/// @see JCLFatalErrorHandler

JILEXTERN void					JCLSetFatalErrorHandler	(JILState* pState, JCLFatalErrorHandler proc);

//------------------------------------------------------------------------------
// JCLSetGlobalOptions
//------------------------------------------------------------------------------
/// Call this before compiling a script file to customize any global options.
/// Pass in a string defining a comma seperated list of options in a
/// "name=value" manner (same format as the JewelScript option keyword), where
/// "name" specifies a compiler defined keyword for the option to change, and
/// value specifies an integer value or character string. Instead of
/// specifying values as integer numbers, you can also specify the keywords
/// true/false, on/off, or yes/no, which are "syntactical sugar" for the integer
/// value 1 and 0 respectively. The case of the option keywords is significant.
/// In the case of an error, the function will return an error or warning code.
/// For a complete list of available options and their possible values, please
/// refer to the "JewelScript Language Reference", the language documentation,
/// available at http://blog.jewe.org/?p=53

JILEXTERN JILError				JCLSetGlobalOptions		(JILState* pState, const JILChar* pOptionString);

//------------------------------------------------------------------------------
// JCLGenerateBindings
//------------------------------------------------------------------------------
/// <p>This function will create C++ binding code files for all currently
/// known classes that have been declared using the "native" keyword.</p>
/// Note that this can be disabled using macro JIL_USE_BINDING_CODEGEN in jilplatform.h

JILEXTERN JILError				JCLGenerateBindings		(JILState* pState, const JILChar* pPath);

//------------------------------------------------------------------------------
// JCLGenerateDocs
//------------------------------------------------------------------------------
/// <p>This function will extract all annotations ("tags") from all currently
/// compiled classes and functions and generate a HTML page from it. The intention
/// behind this function is to allow developers (or users) to very easily get
/// an overview of the all classes, methods and functions available to script
/// programmers. It is not meant as an in-depth documentation generator, but may
/// serve as the starting point of it.</p>
/// Note that this can be disabled using macro JIL_USE_HTML_CODEGEN in jilplatform.h

JILEXTERN JILError				JCLGenerateDocs			(JILState* pState, const JILChar* pPath, const JILChar* pParams);

//------------------------------------------------------------------------------
// JCLExportTypeInfo
//------------------------------------------------------------------------------
/// Exports all type information currently known to the compiler to disk.
/// The function will write an XML file to the specified path. The file could be
/// used by an external documentation generation tool, or by an IDE to get a
/// hierarchical model of the script application currently being developed.
/// The XML file will not contain documentation tags in release builds, as they
/// are stripped from the source in release builds.

JILEXTERN JILError				JCLExportTypeInfo		(JILState* pState, const JILChar* pFilename);

//------------------------------------------------------------------------------
// JCLAddImportPath
//------------------------------------------------------------------------------
/// <p>Adds an import path to the compiler's list of import pathes. By default, the
/// import-statement will look for files (and folders, if there are dots in the
/// import-specifier) in the current working directory of the application.
/// However, when wanting to share script classes among multiple projects, this
/// isn't enough, because all classes must be in the same directory or in one of
/// it's subfolders.</p>
/// <p>With this function it is possible to add additional root-directories for the
/// statement to look in. For example, the following call</p>
/// <pre>JCLAddImportPath(pVM, "System", "C:\\JewelScript\\Library");</pre>
/// <p>will enable script developers to import classes from that directory whenever
/// the import-specifier begins with "System":</p>
/// <pre>import System.Console.TextField;</pre>
/// <p>This statement will effectively try to load the script file 
/// "C:\JewelScript\Library\Console\TextField.jc".</p>
/// <p>Note that the case of the import-specifier matters, even if the underlying
/// file system is not case sensitive. The string passed for 'pName' must be a
/// valid identifier name, meaning no characters are allowed that would be
/// illegal for a class name.</p>
/// <p>The function fails if 'pName' contains invalid characters, the name has
/// already been added to the list, or the compiler object has been freed.</p>

JILEXTERN JILError				JCLAddImportPath		(JILState* pState, const JILChar* pName, const JILChar* pPath);

//------------------------------------------------------------------------------
// JCLForwardClass
//------------------------------------------------------------------------------
/// <p>Forward declares the class specified by the given string. You can use this
/// function to allocate a type-ID number for a native or script class. This may
/// be useful if you need the type-ID number of the class ahead of time, meaning
/// before the class is actually imported or declared / defined by the user.
/// This is essentially the same as compiling the statement:</p>
/// <pre>class ClassName;</pre>
/// If the specified class can not be forward declared, an error is returned.
/// If the class is already declared or defined, the call is ignored.
/// To check beforehand if the class is already known to the compiler, you can
/// use the NTLTypeNameToTypeID() function.

JILEXTERN JILError				JCLForwardClass			(JILState* pState, const JILChar* pClassName);

//------------------------------------------------------------------------------
// JCLImportClass
//------------------------------------------------------------------------------
/// <p>Imports the specified class to the compiler. You can use this function to
/// programmatically import native or script classes into the compiler.
/// This is essentially the same as compiling the statement:</p>
/// <pre>import ClassName;</pre>
/// If the specified class cannot be loaded or compiled, an error is returned.
/// If the class is already imported, the call is silently ignored.

JILEXTERN JILError				JCLImportClass			(JILState* pState, const JILChar* pClassName);

//------------------------------------------------------------------------------
// JCLFreeCompiler
//------------------------------------------------------------------------------
/// <p>You may call this to destroy all objects allocated by the compiler if you
/// need to keep the memory usage low. The runtime will not be affected by this.
/// You can safely call this function when you are done compiling and linking
/// your code, want to run your program, and don't need the compiler anymore.</p>
/// Be aware, that after calling this function, you won't be able to use any of
/// the functions declared in this file aymore.

JILEXTERN JILError				JCLFreeCompiler			(JILState* pState);

#endif	// #ifndef JILCOMPILERAPI_H
