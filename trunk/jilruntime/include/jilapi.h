//------------------------------------------------------------------------------
// File: JILApi.h                                         (c) 2005-2010 jewe.org
//------------------------------------------------------------------------------
//
// JILRUNTIME/JEWELSCRIPT LICENSE AGREEMENT
// ----------------------------------------
//	This license is based on the zlib/libpng license.
//	If the license text from the file "COPYING" should
//	differ from this license text, then this license text
//	shall prevail.
//
//	Copyright (c) 2005-2010 by jewe.org
//
//	This software is provided 'as-is', without any express
//	or implied warranty. In no event will the authors be
//	held liable for any damages arising from the use of this
//	software.
//
//	Permission is granted to anyone to use this software for
//	any purpose, including commercial applications, and to
//	alter it and redistribute it freely, subject to the
//	following restrictions:
//
//	  1. The origin of this software must not be
//	  misrepresented; you must not claim that you
//	  wrote the original software. If you use this
//	  software in a product, an acknowledgment in
//	  the product documentation would be
//	  appreciated but is not required.
//
//	  2. Altered source versions must be plainly
//	  marked as such, and must not be
//	  misrepresented as being the original
//	  software.
//
//	  3. This notice may not be removed or altered
//	  from any source distribution.
//
// Description:
// ------------
/// @file jilapi.h
/// Public application programming interface definitions.
///
/// This file contains the main API functions developers will need to initialize
/// and use a JIL virtual machine.
//------------------------------------------------------------------------------

/*!	@mainpage JILRuntime / JewelScript source code documentation
	This is the source code documentation and focuses solely on the programming
	interface (API) of the script runtime.
	<p>For language documentation, please refer to the JewelScript language reference,
	available for download here: http://blog.jewe.org/?page_id=107#documentation </p>
	<p>Where to get started with the source code documentation:</p>

	<h2>API functions</h2>
	- The main API functions can be found in jilapi.h
	- In addition, you may need the compiler API functions, jilcompilerapi.h
	- If you want to write a new native type for JewelScript, you will need the
		functions found in jilnativetype.h and jilnativetypeex.h

	<h2>Type declarations</h2>
	- All public type definitions can be found in jilapitypes.h

	<h2>Build options</h2>
	- To configure the library's options, check the macros in jilplatform.h

	<h2>Error and exception codes</h2>
	- The libraries error and exception codes can be found in jilexception.h
*/

#ifndef JILAPI_H
#define JILAPI_H

#include "jilnativetypeex.h"

//------------------------------------------------------------------------------
// JILInitialize
//------------------------------------------------------------------------------
/// Initializes the runtime and returns a JILState structure. The
/// virtual machine's stack is fixed in size, it does not automatically grow.
/// For small applets, macros or other functions, a size of 1024 should be
/// sufficient. For larger applets or whole programs without recursive
/// functions a size of 4096 is recommended.
/// If your code is using excessively recursive functions (Ackerman, Fibonacci)
/// the size should be 16384 or larger.
/// <p>You can build the library with 'extended runtime checks' by setting the
/// macro JIL_RUNTIME_CHECKS to 1 in file jilplatform.h, if you need the library
/// to check for stack over/underruns. In this case the VM will generate an
/// exception. Enabling runtime checks will significantly decrease
/// performance.</p>
/// <p>If you're not sure whether your stack size is sufficient, you can first
/// test your JewelScript code with the debug build of the library, which will
/// check the stack for over/underruns and generate an exception. Then, after
/// you successfully ran your code with the debug build, you can safely switch
/// to the release build.</p>
/// <p>You can pass additional options for the runtime and compiler if 'options'
/// points to a character string that contains a comma seperated list of
/// 'name=value' tags. You can pass NULL for this parameter if you don't need to
/// set any specific options. For a complete list of available options, please
/// refer to the JewelScript language reference ('option' keyword), which is
/// available at http://blog.jewe.org/?page_id=107#documentation </p>
/// <p>Scripts can use the <i>option</i> keyword to specify the stack
/// size to use. So applications can keep the default stack size low, while
/// scripts that excessively use the stack can specify a larger stack size.</p>
/// @see JCLSetGlobalOptions

JILEXTERN JILState*				JILInitialize			(JILLong stackSize, const JILChar* options);

//------------------------------------------------------------------------------
// Terminate
//------------------------------------------------------------------------------
/// Terminates the virtual machine and frees all memory.

JILEXTERN JILError				JILTerminate			(JILState* pState);

//------------------------------------------------------------------------------
// JILRunInitCode
//------------------------------------------------------------------------------
/// Call this after compiling or loading bytecode to run the init-code created
/// by the JewelScript compiler. This should be done once before any other
/// function or method is called to initialize all globals of the program.

JILEXTERN JILError				JILRunInitCode			(JILState* pState);

//------------------------------------------------------------------------------
// JILGetFunction
//------------------------------------------------------------------------------
/// This will return a handle for the specified global function, global class
/// member function or instance method.
/// <p>If 'pObj' is NULL, the function assumes a handle for a global function
/// or global class member function should be returned and will search for the
/// global function 'pName' in 'pClass', or if 'pClass' is NULL, in the global
/// scope.</p>
/// <p>If 'pObj' is a valid handle to an instance of a class (script or native),
/// the function assumes a handle for an instance method should be returned
/// and will search the class specified by 'pObj' for the method 'pName'. In
/// this case 'pClass' is ignored and can be NULL.</p>
/// <p>The result of this function is a handle to the function or NULL in the
/// case of an error (function or method not found). To call the function, use
/// the JILCallFunction() API function. Call NTLFreeHandle() if you don't need
/// the function handle anymore.</p>
/// <p><b>NOTE</b> that this will use the first method or function with a
/// matching name, regardless of return type or argument list!</p>
/// <p>This is a <b>time-consuming operation</b>, you should call this function
/// once during initialization of your application and store the handle in a
/// variable for as long as the function is needed.</p>
/// <p>To obtain handles for all functions of a script-object at once, see
/// JILGetFunctionTable().</p>

JILEXTERN JILHandle*			JILGetFunction			(JILState* pState, JILHandle* pObj, const JILChar* pClass, const JILChar* pName);

//------------------------------------------------------------------------------
// JILCallFunction
//------------------------------------------------------------------------------
/// Execute a function. The handle can be a delegate, an instance class member
/// function (method) or a global class member function, implemented in script
/// or native code. To obtain the required JILHandle of the function, use
/// JILGetFunction().
/// <p>To pass arguments to the function, specify them as parameter list to
/// this function. The parameter list is expected to have the following format:
/// Each parameter to be pushed onto the VM stack uses two arguments - the first
/// argument is an integer describing the type of data, the second argument is
/// the actual data. For example, to push an integer value and a string onto
/// the VM stack, the call would look like this:</p>
/// <pre>JILCallFunction (pVM, pFunc, 2, kArgInt, 235, kArgString, "Hello");</pre>
/// <p>The function will automatically create the needed JILHandle objects for all
/// but the kArgHandle data types. The C string data will be copied, so its safe
/// to pass a pointer to a volatile string variable.</p>
/// <p>When the script function has been executed without error, this function
/// will return with a JILHandle pointer to the script function's result.
/// If the script function does not return a result, a handle of 'type_null'
/// is returned. If an exception has occurred during execution, a handle of
/// an exception object is returned. Use NTLHandleToError() to check if the
/// handle is an exception and get it's error code.</p>
/// <p>To determine the handle's type, use NTLHandleToTypeID(). To convert the
/// handle to an int, float, or const char*, use NTLHandleToInt(),
/// NTLHandleToFloat() and NTLHandleToString(). If the result is an object,
/// you can use NTLHandleToObject() to obtain a pointer to the object.</p>
/// <p>When you are done with the result, you must free it by calling
/// NTLFreeHandle().</p>

JILEXTERN JILHandle*			JILCallFunction			(JILState* pState, JILHandle* pFunction, JILLong numArgs, ...);

//------------------------------------------------------------------------------
// JILLoadBinary
//------------------------------------------------------------------------------
/// Loads bytecode from a binary chunk. This operation will reset the
/// runtime, meaning all currently allocated objects will be freed and the
/// runtime will be re-initialized. This also means the compiler object, which
/// was initialized when calling JILInitialize() will be freed. Hence, after a
/// call to this function, it will not be possible to use any compiler API
/// function. The design idea is to <b>either</b> compile a program from script
/// code, <b>or</b> load a precompiled binary from a file.

JILEXTERN JILError				JILLoadBinary			(JILState* pState, const JILUnknown* pData, JILLong dataSize);

//------------------------------------------------------------------------------
// JILSaveBinary
//------------------------------------------------------------------------------
/// Saves bytecode to a binary chunk. If successful, the function writes the
/// address of a byte buffer to ppData and the size of the buffer, in bytes,
/// to pDataSize. This buffer remains valid until either the runtime
/// is terminated, re-initialized (by calling JILLoadBinary), or
/// JILSaveBinary() is called again.
/// <p>This function is not capable to save a "runtime snapshot" of
/// the virtual machine. While this would be cool, it would require all native
/// objects to support writing their current state into a byte-stream and
/// initializing themselves from a stream.</p>
/// <p>Instead, the purpose of this function is to save a compiled (and not yet
/// executed) program as a binary file, so that it can be loaded back and
/// executed later (maybe even by a different program).
/// This can be useful to very quickly load and execute code (without the
/// additional performance hit of compiling script code), or to ensure that
/// application users can not easily read and modify script code.</p>
/// The binary file created by this function is <b>machine dependant</b>, but
/// <b>not platform dependant</b>. This means byte-code saved by
/// an Intel&reg; Windows&reg; PC should load and run on an Intel
/// Linux or Intel MacOSX&reg; machine (*). On the other hand, it will <b>not</b>
/// load and run on an IBM&reg; PowerPC based machine.</p>
/// (* Provided all used native types are available, and the compiler used
/// to build the library uses 32-bit int and a struct alignment of 8 bytes.)

JILEXTERN JILError				JILSaveBinary			(JILState* pState, JILUnknown** ppData, JILLong* pDataSize);

//------------------------------------------------------------------------------
// JILRegisterNativeType
//------------------------------------------------------------------------------
/// Register a native type library to the runtime environment. After calling
/// this function, the native class can be imported and used by the compiler and
/// runtime.

JILEXTERN JILError				JILRegisterNativeType	(JILState* pState, JILTypeProc proc);

//------------------------------------------------------------------------------
// JILGetExceptionVector
//------------------------------------------------------------------------------
/// Return the address of an exception handler from the virtual machine. If the
/// exception vector is not set to the address of a C/C++ callback function,
/// NULL is returned. @see enum JILExceptionVector

JILEXTERN JILExceptionProc		JILGetExceptionVector	(JILState* pState, JILLong vector);

//------------------------------------------------------------------------------
// JILSetExceptionVector
//------------------------------------------------------------------------------
/// Install a C/C++ exception handler for the virtual machine.
/// @see enum JILExceptionVector

JILEXTERN JILError				JILSetExceptionVector	(JILState* pState, JILLong vector, JILExceptionProc pProc);

//------------------------------------------------------------------------------
// JILGetExceptionString
//------------------------------------------------------------------------------
/// Get a string from an exception number.

JILEXTERN const JILChar*		JILGetExceptionString	(JILState* pState, JILError e);

//------------------------------------------------------------------------------
// JILGetRuntimeVersion
//------------------------------------------------------------------------------
/// Get the version information of the runtime.
/// @see JILVersionInfo

JILEXTERN const JILVersionInfo*	JILGetRuntimeVersion	(JILState* pState);

//------------------------------------------------------------------------------
// JILGetVersionString
//------------------------------------------------------------------------------
/// Converts the given version number into a null-terminated string.
/// The string will be written into the given buffer. The address of the given
/// buffer will be returned. The buffer should be at least 16 bytes in size.

JILEXTERN const JILChar*		JILGetVersionString		(JILLong version, JILChar* pBuffer);

//------------------------------------------------------------------------------
// JILSetLogCallback
//------------------------------------------------------------------------------
/// Install a callback function for logging of library messages.
/// <p>The compiler uses this callback to output compiler errors and warnings in
/// addition to making them available via the JCLGetErrorText() function.</p>
/// <p>The runtime uses this callback to output diagnostic and error messages.</p>
/// <p>If the option "log-garbage" has been enabled, the gargabe collector will
/// use this callback to output details about objects that have leaked.
/// Additionally, the runtime will check for any leaked objects during it's
/// termination, and use the log output callback to output details about them.
/// This can be useful if developers wish to optimize memory usage in their
/// scripts.</p>
/// <p>An application can use this function to install a log message handler,
/// for example to print the given message to console, to open a message window,
/// or to write it into a log file. If the callback is not set, the library will
/// not output any messages.</p>

JILEXTERN void					JILSetLogCallback		(JILState* pState, JILLogOutputProc proc);

//------------------------------------------------------------------------------
// JILMessageLog
//------------------------------------------------------------------------------
/// Output a formatted string with variable arguments to the runtime's message
/// log callback. Uses standard ANSI format specifiers. @see JILSetLogCallback

JILEXTERN void					JILMessageLog			(JILState* pState, const JILChar* pFormat, ...);

//------------------------------------------------------------------------------
// JILAttachObject
//------------------------------------------------------------------------------
/// Attach any object to the virtual machine's state using this function. The
/// function returns the previous value of the specified object ID. This is
/// especially useful for native code that needs access to your application
/// object. Just attach the pointer to your application object as 'JIL_Application'
/// to the VM state. If your native code is invoked, you can call JILGetObject()
/// to retrieve the pointer to your application.
/// @see enum JILAttachObjectID

JILEXTERN JILUnknown*			JILAttachObject			(JILState* pState, JILLong objectID, JILUnknown* pData);

//------------------------------------------------------------------------------
// JILGetObject
//------------------------------------------------------------------------------
/// Retrieve a pointer to an object attached to the virtual machine state.
/// If not set, this function returns NULL for the specified object ID. You can
/// use this, for example, to get access to your application object from within
/// any type of callback function or native type.
/// @see JILAttachObject

JILEXTERN JILUnknown*			JILGetObject			(JILState* pState, JILLong objectID);

//------------------------------------------------------------------------------
// JILUseFixedMemory
//------------------------------------------------------------------------------
/// Enables the fixed memory management.
/// <p>All runtime memory allocs and deallocs will be made through the runtime's
/// own memory management, which is optimized for fast allocation and deallocation
/// of small memory blocks.</p>
/// <p>Fixed memory management works by trading off memory efficiency for
/// increased performance. That means, objects allocated through this manager
/// may "waste" memory, because a larger block may be allocated than necessary.
/// Therefore it may be better to use normal C-runtime malloc() and free() on
/// machines with low memory resources.</p>
/// <p>Specify the maximum number of objects allowed for the given block sizes.
/// When allocating a block, the library will choose the memory manager with the
/// smallest block size capable of accommodating the block. If the requested
/// block size is for example 24 bytes, the library will use the memory manager
/// for 32 byte blocks.</p>
/// <p>'max16' stands for maximum number of blocks less or equal 16 bytes in size,
/// 'max32' for less or equal 32 bytes in size, and so forth.</p>
/// <p>If you specify 0 (zero) for any of the 'max' parameters, this memory
/// manager will be used in "dynamic growth" mode, meaning there is no limit to
/// the number of blocks of that size (except available RAM of course).</p>
/// <p>Suffice to say that blocks with sizes > 512 bytes will be allocated and
/// freed using the normal C-runtime malloc() and free() functions.</p>
/// <p>Calling this will override the JILState::vmMalloc and
/// JILState::vmFree member variables. If you have set these callbacks to
/// your customized memory allocation / deallocation handlers, do not call this
/// function!</p>
/// @see JILUseFixedMemDynamic, JILMalloc, JILMfree

JILEXTERN JILError				JILUseFixedMemory		(JILState* pState,
														 JILLong max16,
														 JILLong max32,
														 JILLong max64,
														 JILLong max128,
														 JILLong max256,
														 JILLong max512);

//------------------------------------------------------------------------------
// JILUseFixedMemDynamic
//------------------------------------------------------------------------------
/// This enables the fixed memory management and sets all memory managers to
/// "dynamic growth" mode. @see JILUseFixedMemory

JILEXTERN JILError				JILUseFixedMemDynamic	(JILState* pState);

//------------------------------------------------------------------------------
// JILMalloc
//------------------------------------------------------------------------------
/// Allocates and returns a block of memory of the specified size.
/// This API function directly calls the JILState::vmMalloc callback.
/// If 'fixed memory management' has been enabled, then it will be used to
/// allocate the block. If the application has customized allocation by setting
/// JILState::vmMalloc to a custom callback function, then the custom allocator
/// will be used. By default, this just maps through to the C-runtime malloc()
/// function.
/// <p><b>Blocks allocated with this function must be freed using JILMfree()!</b>
/// Otherwise it will corrupt the memory management.</p>
/// @see JILUseFixedMemory

JILEXTERN JILUnknown*			JILMalloc				(JILState* pState, JILLong size);

//------------------------------------------------------------------------------
// JILMfree
//------------------------------------------------------------------------------
/// Frees a block of memory previously allocated by JILMalloc().
/// This API function directly calls the JILState::vmFree callback.
/// If 'fixed memory management' has been enabled, then it will be used to
/// free the block. If the application has customized deallocation by setting
/// JILState::vmFree to a custom callback function, then the custom deallocator
/// will be used. By default, this just maps through to the C-runtime free()
/// function.
/// <p><b>Only pass blocks allocated by JILMalloc() to this function!</b>
/// Passing a pointer allocated by any other means will corrupt the memory
/// management and may even produce crashes.</p>
/// @see JILUseFixedMemory

JILEXTERN void					JILMfree				(JILState* pState, JILUnknown* ptr);

//------------------------------------------------------------------------------
// JILSetBlocked
//------------------------------------------------------------------------------
/// This sets the 'blocked' flag of the virtual machine.
/// <p>If this flag is set, calls to JILCallFunction() will be blocked and will
/// fail with error code JIL_ERR_Runtime_Blocked. This can be used to prevent
/// further execution of byte-code if a fatal situation, like an unhandled
/// exception has occurred.</p>
/// <p>The function will return the previous state of the 'blocked' flag.
/// If argument "flag" is -1, the 'blocked' flag of the VM is not altered. This
/// is useful to only query the current state of the flag.</p>

JILEXTERN JILBool				JILSetBlocked			(JILState* pState, JILLong flag);

//------------------------------------------------------------------------------
// JILSetFileInputProc
//------------------------------------------------------------------------------
/// Sets a callback function for handling opening files and reading them.
/// <p>The compiler can automatically load source code from the local file
/// system, either when it encounters an "import" statement while compiling code,
/// or when the API function JCLLoadAndCompile() is used.
/// In addition, native types can use the NTLLoadResource() function to load
/// any text or binary data resources they may need.</p>
/// <p>By default, the library will use the ANSI functions fopen() and fread()
/// to load these files. If you need a different method of loading the file,
/// e.g. from a compressed archive, or a network stream, you can install a
/// callback function by calling this API function.</p>
/// <p>The callback function's prototype looks as follows:</p>
/// <pre>JILError JILFileInputProc(JILState* pState, JILLong mode, JILChar* pBuffer, JILLong size, JILUnknown** ppFile);</pre>
/// <p>In order to to load a file, the library will call this callback
/// multiple times. One time to open the file, multiple times to read bytes
/// from it, and one time close the file. The parameter "mode" will determine
/// what kind of action the callback is supposed to perform:</p>
/// <p><i>JILFileInputOpen</i></p>
/// <p>The "pBuffer" parameter points to the filename of the file to open. The
/// "ppFile" pointer can be used to store any "file handle" or other pointer
/// associated with the file, and will be passed back to the callback in
/// successive calls. If opening the specified file fails, the callback should
/// return JIL_ERR_Generic_Error, otherwise JIL_No_Exception.</p>
/// <p><i>JILFileInputRead</i></p>
/// <p>The "pBuffer" parameter points to the buffer the function is supposed to
/// fill with bytes, "size" specifies the number of bytes that can
/// be written to "pBuffer" at most. The callback should return the number of
/// bytes actually read and stored as a result.</p>
/// <p><i>JILFileInputClose</i></p>
/// <p>The callback should close the file and return either an error code, or
/// JIL_No_Exception.</p>
/// <p>If you need access to the application or an additional user data
/// pointer, you can always call JILGetObject() from within your callback.
/// For a code example on how to implement this callback, you can look at the
/// DefaultFileInputProc(), defined in jilruntime.c.</p>

JILEXTERN void					JILSetFileInputProc		(JILState* pState, JILFileInputProc proc);

//------------------------------------------------------------------------------
// JILGetFunctionTable
//------------------------------------------------------------------------------
/// Returns a pointer to a JILFunctionTable object, which contains an array of
/// JILHandle pointers for all functions and methods of the given
/// (script-)object. This can be useful if your application needs to call all
/// functions of a class implemented in script, or if your script class contains
/// multiple functions with the same name.
/// To call these functions, pass one of the handles in the returned array to
/// JILCallFunction().
/// All of the class's global functions and methods will be added to the array,
/// including convertor methods and accessors. Cofunctions can not be called
/// using JILCallFunction(), and therefore a null-handle is added to the array
/// instead of a handle to the cofunction. The order of handles in the array is
/// in sync with the declaration of the class. This means, the first
/// declared function is at index 0, the second at index 1, and so on.
/// The object handle passed to this function can be an instance of either
/// a script class, or a native class. If an error occurs, the result of this
/// function is NULL.
/// <p>This is a <b>time-consuming</b> operation, you should call this function
/// once during initialization of your application and store the table in a
/// variable for as long as the table is needed.</p>
/// If you don't need the function table anymore, call JILFreeFunctionTable().

JILEXTERN JILFunctionTable*		JILGetFunctionTable		(JILState* pState, JILHandle* pObj);

//------------------------------------------------------------------------------
// JILFreeFunctionTable
//------------------------------------------------------------------------------
/// Frees all the handles in the given function table and the table itself.
/// You should not access the specified pointer anymore after this function
/// returns.

JILEXTERN void					JILFreeFunctionTable	(JILState* pState, JILFunctionTable* pTable);

//------------------------------------------------------------------------------
// JILMarkFunctionTable
//------------------------------------------------------------------------------
/// Marks all the handles in the function table. Call this in response to a
/// NTL_MarkHandles message, if your application is using the garbage collector.
/// @see JILCollectGarbage

JILEXTERN JILError				JILMarkFunctionTable	(JILState* pState, JILFunctionTable* pTable);

//------------------------------------------------------------------------------
// JILCollectGarbage
//------------------------------------------------------------------------------
/// Run the garbage collector to find and free any leaked objects due to
/// reference cycles. Depending on the number of objects in use, this can take
/// a considerable amount of time.
/// <p>The runtime will never automatically run the garbage collector. If you
/// intend to use garbage collection, it is up to your application to find a
/// suitable time to call this function, i.e. when the application has been
/// idle for a few minutes, or it's window is minimized.</p>
/// <p>If your application is going to call this function, it MUST make sure
/// that it registers itself to the runtime by calling JILRegisterGCEvent().
/// This is important, because your application needs to be notified about the
/// garbage collector's JIL_GCEvent_Mark message, and in response call NTLMarkHandle()
/// for all JILHandle pointers that your application class has stored.</p>
/// <p>Not responding to the JIL_GCEvent_Mark message can result in destruction of
/// objects your application still uses, since the garbage collector will
/// assume they are no longer in use.</p>
/// <p>It is important to understand that this only applies to classes of your
/// application that permanently store JILHandle pointers, and that are NOT
/// native types itself. Native types should handle marking when they receive
/// the NTL_MarkHandles message in their type proc.</p>
/// The function will return JIL_No_Exception unless a native function did not
/// handle the marking message correctly or returned an error.
/// @see JILRegisterGCEvent, JILUnregisterGCEvent

JILEXTERN JILError				JILCollectGarbage		(JILState* pState);

//------------------------------------------------------------------------------
// JILRegisterGCEvent
//------------------------------------------------------------------------------
/// If any of your application's classes store pointers to JILHandle objects,
/// but are NOT registered as native types to the runtime, these classes must
/// register themselves for GC events. This is important so these classes get
/// notified when the garbage collector sends the JIL_GCEvent_Mark message.
/// Pass a pointer to a static function and a pointer to your object to this
/// function. When the garbage collector is run, your callback will get called
/// and the pointer to your object will be passed to it. Call NTLMarkHandle()
/// for every JILHandle pointer that your class still uses.
/// <p>If your class gets destroyed, you need to unregister from the GC event.
/// Call JILUnregisterGCEvent() from the destructor of your class.</p>
/// You can add the same static event handler function multiple times in
/// combination with different user object pointers. But you can only register
/// the same user pointer a single time.

JILEXTERN JILError				JILRegisterGCEvent		(JILState* pState, JILGCEventHandler proc, JILUnknown* pUser);

//------------------------------------------------------------------------------
// JILUnregisterGCEvent
//------------------------------------------------------------------------------
/// Unregisters the event handler for the given user object pointer.
/// <p>Call this from the destructor of your class (the instance of your class
/// that pUser points to) to unregister from event notification.</p>

JILEXTERN JILError				JILUnregisterGCEvent	(JILState* pState, JILUnknown* pUser);

//------------------------------------------------------------------------------
// JILGetTimeLastGC
//------------------------------------------------------------------------------
/// This can be used to request the time the last garbage collection has been
/// performed. The result is the time in ANSI clock() ticks. If the GC has
/// never been run before calling this, the result is 0.

JILEXTERN JILFloat				JILGetTimeLastGC		(JILState* pState);

//------------------------------------------------------------------------------
// JILGetImplementors
//------------------------------------------------------------------------------
/// Returns all classes that implement the specified interface type-id.
/// The function will write the type-ids of all classes that implement the
/// given interface to 'pOutArray'. Specify the size of the array in 'outSize'.
/// The function will return the number of type-ids actually written into the
/// array. If the function returns 0, there are no classes implementing the
/// specified interface. If the function returns a negative value, the given
/// type-id is not an interface or any other error has occurred.

JILEXTERN JILLong				JILGetImplementors		(JILState* pState, JILLong* pOutArray, JILLong outSize, JILLong interfaceId);

#endif	// #ifndef JILAPI_H
