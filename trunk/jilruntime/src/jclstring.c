//------------------------------------------------------------------------------
// File: JCLString.c                                        (c) 2004 jewe.org
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
//  A dynamic string object especially for the tasks of parsing / compiling.
//------------------------------------------------------------------------------

#include "jilstdinc.h"

#include "jclstring.h"
#include "jiltools.h"
#include "jilnativetypeex.h" // for JCLReadTextFile

//------------------------------------------------------------------------------
// static variables
//------------------------------------------------------------------------------

static const JILLong kStringAllocGrain = 32;

static const JILLong kFormatWorstCaseBufferSize = 16384;

//------------------------------------------------------------------------------
// static functions
//------------------------------------------------------------------------------

static void Reallocate(JCLString* _this, JILLong newSize, JILLong keepData);
static void Deallocate(JCLString* _this);
static JILBool StrEquNoCase(const JILChar* str1, const JILChar* str2);

//------------------------------------------------------------------------------
// JCLString
//------------------------------------------------------------------------------
// constructor

void create_JCLString(JCLString* _this)
{
	_this->m_String = NULL;
	_this->m_Locator = 0;
	_this->m_Length = 0;
	_this->m_AllocatedLength = 0;
}

//------------------------------------------------------------------------------
// JCLString
//------------------------------------------------------------------------------
// destructor

void destroy_JCLString(JCLString* _this)
{
	Deallocate(_this);
}

//------------------------------------------------------------------------------
// JCLString::Copy
//------------------------------------------------------------------------------
// Copy data from another JCLString to this instance.

void copy_JCLString(JCLString* _this, const JCLString* pSource)
{
	Reallocate(_this, pSource->m_Length, JILFalse);
	if( pSource->m_Length )
	{
		memcpy(_this->m_String, pSource->m_String, pSource->m_Length);
	}
	_this->m_String[_this->m_Length] = 0;
	_this->m_Locator = pSource->m_Locator;
}

//------------------------------------------------------------------------------
// JCLCopyString
//------------------------------------------------------------------------------
// Create a new copy of an instance.

JCLString* JCLCopyString(const JCLString* pSource)
{
	JCLString* pStr = NEW(JCLString);
	copy_JCLString(pStr, pSource);
	return pStr;
}

//------------------------------------------------------------------------------
// JCLSetString
//------------------------------------------------------------------------------
// Assign a c-string to this instance.

void JCLSetString(JCLString* _this, const JILChar* string)
{
	if( string )
	{
		JILLong len = strlen(string);
		Reallocate(_this, len, JILFalse);
		memcpy(_this->m_String, string, len);
		_this->m_String[_this->m_Length] = 0;
	}
	else
	{
		Reallocate(_this, 0, JILFalse);
	}
}

//------------------------------------------------------------------------------
// JCLCompare
//------------------------------------------------------------------------------
// Returns true (1) if the strings are equal, false (0) if they are not.

JILBool JCLCompare(const JCLString* _this, const JCLString* Other)
{
	return (strcmp(JCLGetString(_this), JCLGetString( (JCLString*) Other )) == 0);
}

//------------------------------------------------------------------------------
// JCLCompareNoCase
//------------------------------------------------------------------------------
// JCLCompare strings ignoring the case.

JILBool JCLCompareNoCase(const JCLString* _this, const JCLString* Other)
{
	return StrEquNoCase(JCLGetString(_this), JCLGetString( (JCLString*) Other ));
}

//------------------------------------------------------------------------------
// JCLClear
//------------------------------------------------------------------------------
// JCLClear the string.

void JCLClear(JCLString* _this)
{
	Deallocate( _this );
}

//------------------------------------------------------------------------------
// JCLAppend
//------------------------------------------------------------------------------
// JCLAppend a string to this string.

void JCLAppend(JCLString* _this, const JILChar* source)
{
	if( source )
	{
		JILLong oldLen = _this->m_Length;
		JILLong sLen = strlen(source);
		Reallocate( _this, _this->m_Length + sLen, JILTrue );
		memcpy( _this->m_String + oldLen, source, sLen);
		_this->m_String[_this->m_Length] = 0;
	}
}

//------------------------------------------------------------------------------
// JCLAppendChar
//------------------------------------------------------------------------------
// JCLAppend a character to this string.

void JCLAppendChar(JCLString* _this, JILLong chr)
{
	JILLong oldLen = _this->m_Length;
	Reallocate( _this, oldLen + 1, JILTrue );
	_this->m_String[oldLen] = chr;
	_this->m_String[_this->m_Length] = 0;
}

//------------------------------------------------------------------------------
// JCLInsert
//------------------------------------------------------------------------------
// JCLInsert a string into this string.

void JCLInsert(JCLString* _this, const JCLString* source, JILLong index)
{
	if( source->m_Length )
	{
		JILLong oldLen;
		JILChar* pOldStr;
		// clone, if given object is this
		const JCLString* pSrc = source;
		int bThis = (pSrc == _this);
		if( bThis )
			pSrc = JCLCopyString(_this);
		// now we can continue
		if( index < 0 )
			index = 0;
		if( index > _this->m_Length )
			index = _this->m_Length;
		// save current string
		oldLen = _this->m_Length;
		pOldStr = _this->m_String;
		_this->m_String = NULL;
		_this->m_Length = 0;
		_this->m_AllocatedLength = 0;
		// allocate a new buffer
		Reallocate(_this, oldLen + pSrc->m_Length, JILFalse);
		// copy left part
		if( pOldStr )
			memcpy( _this->m_String, pOldStr, index );
		// copy middle part
		memcpy( _this->m_String + index, pSrc->m_String, pSrc->m_Length );
		// copy right part
		if( pOldStr )
			memcpy( _this->m_String + index + pSrc->m_Length, pOldStr + index, oldLen - index );
		_this->m_String[_this->m_Length] = 0;
		// destruct old string
		if( pOldStr )
			free( pOldStr );
		pOldStr = NULL;
		// destruct clone
		if( bThis )
			DELETE( (JCLString*) pSrc );
	}
}

//------------------------------------------------------------------------------
// JCLRemove
//------------------------------------------------------------------------------
// JCLRemove characters inside this string.

void JCLRemove(JCLString* _this, JILLong index, JILLong length)
{
	if( index < 0 )
		index = 0;
	if( (index < _this->m_Length) && (length > 0) )
	{
		JILLong end;
		if( (index + length) > _this->m_Length )
			length = _this->m_Length - index;
		end = length + index;
		memmove(_this->m_String + index, _this->m_String + end, _this->m_Length - end);
		_this->m_Length -= length;
		_this->m_String[_this->m_Length] = 0;
	}
}

//------------------------------------------------------------------------------
// JCLReplace
//------------------------------------------------------------------------------
// Replace occurrences of substrings in the string.

JILLong JCLReplace(JCLString* _this, const JILChar* search, const JILChar* replace)
{
	JILLong count = 0;
	JILLong searchLength;
	JILLong pos = 0;
	JILChar* pPos;
	JCLString* pReplace = NEW(JCLString);
	if( search && _this->m_String )
	{
		JCLSetString(pReplace, replace);
		searchLength = strlen(search);
		if( searchLength )
		{
			while( pos < (JILLong) strlen(_this->m_String) )
			{
				// using lowlevel strstr, coz it's faster...
				pPos = strstr( _this->m_String + pos, search );
				if( pPos == NULL )
					break;
				pos = (JILLong) (pPos - _this->m_String);
				JCLRemove( _this, pos, searchLength );
				JCLInsert( _this, pReplace, pos );
				pos += JCLGetLength(pReplace);
				count++;
			}
		}
	}
	DELETE(pReplace);
	return count;
}

//------------------------------------------------------------------------------
// JCLSubString
//------------------------------------------------------------------------------
// Set this string to a sub-string of the given string.

void JCLSubString(JCLString* _this, const JCLString* source, JILLong index, JILLong length)
{
	// clone, if given object is this
	const JCLString* pSrc = source;
	int bThis = (pSrc == _this);
	if( bThis )
		pSrc = JCLCopyString(_this);
	// now we can continue
	if( index < 0 )
		index = 0;
	if( (index >= pSrc->m_Length) || (length <= 0) )
	{
		Deallocate(_this);
	}
	else
	{
		if( (index + length) > pSrc->m_Length )
			length = pSrc->m_Length - index;
		Reallocate(_this, length, JILFalse);
		memcpy(_this->m_String, pSrc->m_String + index, length);
		_this->m_String[_this->m_Length] = 0;
	}
	// destruct clone
	if( bThis )
		DELETE( (JCLString*) pSrc );
}

//------------------------------------------------------------------------------
// JCLFill
//------------------------------------------------------------------------------
// JCLFill this string with the number of characters.

void JCLFill(JCLString* _this, JILLong chr, JILLong size)
{
	if( size )
	{
		Reallocate(_this, size, JILFalse);
		memset(_this->m_String, chr, size);
		_this->m_String[size] = 0;
	}
}

//------------------------------------------------------------------------------
// JCLTrim
//------------------------------------------------------------------------------
// JCLTrim all control characters and space from beginning and end of this string.

void JCLTrim(JCLString* _this)
{
	if( _this->m_Length )
	{
		// trim start
		JILLong start;
		JILLong end;
		JILByte* pString = (JILByte*) _this->m_String;
		for( start = 0; start < _this->m_Length; start++ )
		{
			if( pString[start] > 32 )
				break;
		}
		// trim end
		for( end = _this->m_Length - 1; end >= 0; end-- )
		{
			if( pString[end] > 32 )
				break;
		}
		end++;
		if( end <= start )
		{
			Deallocate(_this);
		}
		else
		{
			JCLSubString(_this, _this, start, end - start);
		}
	}
}

//------------------------------------------------------------------------------
// JCLRandomIdentifier
//------------------------------------------------------------------------------
// Generate a random identifier string of the given length.

void JCLRandomIdentifier(JCLString* _this, JILLong length)
{
	JILLong i, c;
	JCLClear(_this);
	if( length > 0 )
	{
		Reallocate(_this, length, JILFalse);
		for( i = 0; i < length; i++ )
		{
			if( !i )
			{
				c = rand() % 52;	// A-z a-z
				if( c < 26 )
					c += 65;		// A-Z
				else
					c += 97 - 26;	// a-z
			}
			else
			{
				c = rand() % 62;	// 0-9 A-Z a-z
				if( c < 10 )
					c += 48;		// 0-9
				else if( c < 36 )
					c += 65 - 10;	// A-Z
				else
					c += 97 - 36;	// a-z
			}
			_this->m_String[i] = c;
		}
		_this->m_String[length] = 0;
	}
}

//------------------------------------------------------------------------------
// JCLFormat
//------------------------------------------------------------------------------
// Write formatted data into the string. Use it like sprintf(). Note that the
// output of the function is limited. See kFormatWorstCaseBufferSize

JILLong JCLFormat(JCLString* _this, const JILChar* pFormat, ...)
{
#if JIL_NO_FPRINTF
	JCLSetString(_this, pFormat);
	return JCLGetLength(_this);
#else
	size_t len;
	va_list arguments;
	va_start( arguments, pFormat );

	// make string real big
	Reallocate(_this, kFormatWorstCaseBufferSize, JILFalse);

	// print into the string buffer
	len = JIL_VSNPRINTF( _this->m_String, kFormatWorstCaseBufferSize, pFormat, arguments );
	_this->m_String[len] = 0;

	// shrink string to actual length
	Reallocate(_this, len, JILTrue);

	va_end( arguments );
	return len;
#endif
}

//------------------------------------------------------------------------------
// JCLFormatTime
//------------------------------------------------------------------------------
// Write formatted time and date into the string. Use it like strftime().
// Note that the output of the function is limited. See kFormatWorstCaseBufferSize

JILLong JCLFormatTime(JCLString* _this, const JILChar* pFormat, time_t time)
{
#if JIL_NO_FPRINTF
	JCLSetString(_this, pFormat);
	return JCLGetLength(_this);
#else
	size_t len;
	// make string real big
	Reallocate(_this, kFormatWorstCaseBufferSize, JILFalse);
	// format the time
	len = strftime(_this->m_String, kFormatWorstCaseBufferSize, pFormat, gmtime(&time));
	_this->m_String[len] = 0;
	// shrink string to actual length
	Reallocate(_this, len, JILTrue);
	return len;
#endif
}

//------------------------------------------------------------------------------
// JCLFindChar
//------------------------------------------------------------------------------
// Find the given JILChar in the string from an index. The index is zero based.
// Returns the zero based index of the character if found. Returns -1 if not
// found.

JILLong JCLFindChar(const JCLString* _this, JILLong chr, JILLong index)
{
	JILLong result = -1;
	if( index < 0 )
		index = 0;
	if( index < _this->m_Length )
	{
		JILChar* pos = strchr( _this->m_String + index, chr );
		if( pos )
		{
			result = (JILLong) (pos - _this->m_String);
		}
	}
	return result;
}

//------------------------------------------------------------------------------
// JCLFindCharReverse
//------------------------------------------------------------------------------
// Like JCLFindChar but searches the string reverse, from end to beginning.

JILLong JCLFindCharReverse(const JCLString* _this, JILLong chr, JILLong index)
{
	JILLong result = -1;
	if( _this->m_Length && index >= 0 )
	{
		JILChar* pos;
		if( index >= _this->m_Length )
			index = _this->m_Length - 1;
		for( pos = _this->m_String + index; pos >= _this->m_String; pos-- )
		{
			if( *pos == chr )
			{
				result = (JILLong) (pos - _this->m_String);
				break;
			}
		}
	}
	return result;
}

//------------------------------------------------------------------------------
// JCLFindString
//------------------------------------------------------------------------------
// Find substring and return position as zero based index.

JILLong JCLFindString(const JCLString* _this, const JILChar* src, JILLong index)
{
	JILLong result = -1;
	if( index < 0 )
		index = 0;
	if( index < _this->m_Length )
	{
		JILChar* pos = strstr( _this->m_String + index, src );
		if( pos )
		{
			result = (JILLong) (pos - _this->m_String);
		}
	}
	return result;
}

//------------------------------------------------------------------------------
// JCLReadTextFile
//------------------------------------------------------------------------------
// Read the entire contents of a text file into this string.

JILLong JCLReadTextFile(JCLString* _this, const JILChar* pName, JILState* pVM)
{
	JILFileHandle* pFile = NULL;
	JILError err;
	JILLong fileSize = 0;

	// open the file
	err = NTLFileOpen(pVM, pName, &pFile);
	if( err )
		goto exit;
	// get the file length
	fileSize = NTLFileLength(pFile);
	// set file pointer back to start!
	if( NTLFileSeek(pFile, 0) )
		goto exit;
	// reallocate string
	Reallocate(_this, fileSize, JILFalse);
	// load file into string buffer
	if( NTLFileRead(pFile, _this->m_String, fileSize) != fileSize )
		goto exit;
	_this->m_String[fileSize] = 0;
	// close file
	err = NTLFileClose(pFile);
	if( err )
		goto exit;
	// success!
	return fileSize;

exit:
	Deallocate(_this);
	NTLFileClose(pFile);
	pFile = NULL;
	return -1;
}

//------------------------------------------------------------------------------
// JCLBeginsWith
//------------------------------------------------------------------------------
// checks if the string from the current position begins with the given
// substring and returns true if so.

JILLong JCLBeginsWith(const JCLString* _this, const JILChar* string)
{
	JILLong result = JILFalse;
	if( _this->m_Locator < _this->m_Length )
	{
		result = (strncmp(_this->m_String + _this->m_Locator, string, strlen(string)) == 0);
	}
	return result;
}

//------------------------------------------------------------------------------
// JCLEscapeXml
//------------------------------------------------------------------------------
// Replace invalid characters in the given string by their XML entities and store
// the result in this string.

void JCLEscapeXml(JCLString* _this, const JCLString* pString)
{
	copy_JCLString(_this, pString);
	JCLReplace(_this, "&",	"&amp;");
	JCLReplace(_this, "\"",	"&quot;");
	JCLReplace(_this, "'",	"&apos;");
	JCLReplace(_this, ">",	"&gt;");
	JCLReplace(_this, "<",	"&lt;");
}

//------------------------------------------------------------------------------
// JCLSetLocator
//------------------------------------------------------------------------------
// Set the locator for the following string parser functions.

void JCLSetLocator(JCLString* _this, JILLong position)
{
	if( position > _this->m_Length )
		position = _this->m_Length;
	if( position < 0 )
		position = 0;
	_this->m_Locator = position;
}

//------------------------------------------------------------------------------
// JCLSpanIncluding
//------------------------------------------------------------------------------
// returns as many characters from the current position, as INCLUDED in the
// given character set. If the character at the current position is not included
// in the character set, an empty string is returned.

JILLong JCLSpanIncluding(JCLString* _this, const JILChar* charSet, JCLString* result)
{
	JILLong length = 0;
	if( _this->m_Locator < _this->m_Length )
	{
		length = strspn( _this->m_String + _this->m_Locator, charSet );
	}
	JCLSubString(result, _this, _this->m_Locator, length);
	_this->m_Locator += length;
	return length;
}

//------------------------------------------------------------------------------
// JCLSpanExcluding
//------------------------------------------------------------------------------
// returns as many characters from the current position, as NOT INCLUDED in the
// given character set. If the character at the current position is included
// in the character set, an empty string is returned.

JILLong JCLSpanExcluding(JCLString* _this, const JILChar* charSet, JCLString* result)
{
	JILLong length = 0;
	if( _this->m_Locator < _this->m_Length )
	{
		length = strcspn( _this->m_String + _this->m_Locator, charSet );
	}
	JCLSubString(result, _this, _this->m_Locator, length);
	_this->m_Locator += length;
	return length;
}

//------------------------------------------------------------------------------
// JCLSpanBetween
//------------------------------------------------------------------------------
// returns the string between the given start character and end character, not
// including them. If no matching start / end pair is found from the current
// position to the end of the string, -1 will be returned. If the string
// contains multiple NESTED start / end pairs, the largest hierarchical correct
// string is returned. If the function is successful, the current position will
// be moved BEHIND the found end character. If the start and end characters are
// the SAME, no hierarchy checking is possible, so the method will return the
// string between two occurences of this character.

JILLong JCLSpanBetween(JCLString* _this, JILChar startChr, JILChar endChr, JCLString* result)
{
	JILLong length = 0;
	JILLong hier = 0;
	JILLong size = JCLGetLength(_this);
	JILLong startPos = 0;
	JILLong endPos = 0;
	JILLong i;
	JILChar chr;
	for( i = _this->m_Locator; i < size; i++ )
	{
		chr = _this->m_String[i];
		if( chr == startChr )
		{
			if( !hier)
			{
				startPos = i + 1;
			}
			else
			{
				if( chr == endChr )
				{
					endPos = i;
					break;
				}
			}
			hier++;
		}
		else if( chr == endChr )
		{
			hier--;
			if( !hier )
			{
				endPos = i;
				break;
			}
		}
	}
	if( endPos < startPos )
	{
		JCLClear(result);
		length = -1;
	}
	else
	{
		length = endPos - startPos;
		JCLSubString(result, _this, startPos, length);
		_this->m_Locator = endPos + 1;
		if( _this->m_Locator > _this->m_Length )
			_this->m_Locator = _this->m_Length;
	}
	return length;
}

//------------------------------------------------------------------------------
// JCLSpanNumber
//------------------------------------------------------------------------------
// Extracts as many characters from the current position, that can be converted
// into a floating point, hexadecimal, octal or decimal number. The function
// returns the number of characters extracted, the number as a sub-string
// and the type of the number: 0 for integer and 1 for floating point.

JILLong JCLSpanNumber(JCLString* _this, JCLString* result, JILLong* type)
{
	JILLong length = 0;

	JCLClear(result);
	*type = 0;

	if( _this->m_Locator < _this->m_Length )
	{
		JILChar* endptr;
		JILChar* endptrf;
		JILChar* str;
		JILLong l;
		JILFloat f;
		JILChar* startptr = _this->m_String + _this->m_Locator;
		JILLong radix = 0;
		// ignore leading whitespace
		str = startptr + strspn(startptr, " \t");
		// detect hex/oct/bin number
		if( str[0] == '0' )
		{
			switch( str[1] )
			{
				case 'b': radix = 2; break;
				case 'n': radix = 4; break;
				case 'o': radix = 8; break;
				case 'x': radix = 16; break;
			}
		}
		if( radix )
		{
			l = strtol( str + 2, &endptr, radix );
			JCLFormat( result, "%d", l );
			length = (JILLong) (endptr - startptr);
			_this->m_Locator += length;
		}
		else
		{
			// to avoid overflows with huge numbers, try strtod() AND strtol()
			f = strtod( str, &endptrf );
			l = strtol( str, &endptr, 10 );
			// must use float if values differ or stopped at '.' or 'e'
			if( ((JILFloat)l != f) || *endptr == '.' || *endptr == 'e' || *endptr == 'E' )
			{
				*type = 1;
				// calculate length
				length = (JILLong) (endptrf - startptr);
				// extract sub string
				JCLSubString(result, _this, _this->m_Locator, length);
				// skip extracted portion
				_this->m_Locator += length;
			}
			else
			{
				JCLFormat( result, "%d", l );
				length = (JILLong) (endptr - startptr);
				_this->m_Locator += length;
			}
		}
	}
	return length;
}

//------------------------------------------------------------------------------
// JCLSeekWhile
//------------------------------------------------------------------------------
// advance in the string for as many characters, as INCLUDED in the given
// character set.

JILLong JCLSeekWhile(JCLString* _this, const JILChar* charSet)
{
	JILLong length = 0;
	if( _this->m_Locator < _this->m_Length )
	{
		length = strspn( _this->m_String + _this->m_Locator, charSet );
	}
	_this->m_Locator += length;
	return length;
}

//------------------------------------------------------------------------------
// JCLSeekUntil
//------------------------------------------------------------------------------
// advance in the string for as many characters, as NOT INCLUDED in the given
// character set.

JILLong JCLSeekUntil(JCLString* _this, const JILChar* charSet)
{
	JILLong length = 0;
	if( _this->m_Locator < _this->m_Length )
	{
		length = strcspn( _this->m_String + _this->m_Locator, charSet );
	}
	_this->m_Locator += length;
	return length;
}

//------------------------------------------------------------------------------
// JCLSeekForward
//------------------------------------------------------------------------------
// advance for the specified number of characters. negative values are allowed.

JILLong JCLSeekForward(JCLString* _this, JILLong length)
{
	if( (_this->m_Locator + length) < 0 )
		length = -_this->m_Locator;
	if( (_this->m_Locator + length) > _this->m_Length )
		length = _this->m_Length - _this->m_Locator;
	_this->m_Locator += length;
	return length;
}

//------------------------------------------------------------------------------
// JCLSeekString
//------------------------------------------------------------------------------
// advance to the occurrence of the given substring. If the given substring is
// not found, -1 is returned and the locator is not moved. Otherwise the number
// of characters skipped is returned and the locator advances to the beginning
// of the given substring in the string.

JILLong JCLSeekString(JCLString* _this, const JILChar* string)
{
	JILLong result = JCLFindString(_this, string, _this->m_Locator);
	if( result < _this->m_Locator )
	{
		result = -1;
	}
	else
	{
		result -= _this->m_Locator;
		_this->m_Locator += result;
	}
	return result;
}

//------------------------------------------------------------------------------
// JCLContainsOneOf
//------------------------------------------------------------------------------
// checks if the string from the current position contains one of the characters
// in the given character set.

JILLong JCLContainsOneOf(JCLString* _this, const JILChar* charSet)
{
	JILLong length = 0;
	if( _this->m_Locator < _this->m_Length )
	{
		length = strcspn( _this->m_String + _this->m_Locator, charSet );
	}
	length += _this->m_Locator;
	return (length != _this->m_Length);
}

//------------------------------------------------------------------------------
// JCLContainsOnly
//------------------------------------------------------------------------------
// checks if the string from the current position consists entirely of the
// characters in the given character set.

JILLong JCLContainsOnly(JCLString* _this, const JILChar* charSet)
{
	JILLong length = 0;
	if( _this->m_Locator < _this->m_Length )
	{
		length = strspn( _this->m_String + _this->m_Locator, charSet );
	}
	length += _this->m_Locator;
	return (length == _this->m_Length);
}

//------------------------------------------------------------------------------
// Reallocate
//------------------------------------------------------------------------------
// throw away string buffer and allocate a new one

static void Reallocate(JCLString* _this, JILLong newSize, JILLong keepData)
{
	JILLong newAllocatedLength = (((newSize + 1) / kStringAllocGrain) + 1) * kStringAllocGrain;
	// first we check, if reallocation is necessary
	if( newAllocatedLength != _this->m_AllocatedLength )
	{
		JILChar* pNewBuffer = NULL;
		pNewBuffer = malloc( newAllocatedLength );
		if( pNewBuffer )
		{
			if( keepData && _this->m_String )
			{
				JILLong numKeep = (newSize < _this->m_Length) ? newSize : _this->m_Length;
				JILStrncpy( pNewBuffer, newAllocatedLength, _this->m_String, numKeep );
			}
			// destroy old buffer
			if( _this->m_String )
				free( _this->m_String );
			_this->m_String = pNewBuffer;
			_this->m_AllocatedLength = newAllocatedLength;
		}
	}
	_this->m_Length = newSize;
	if( _this->m_Locator > newSize )
		_this->m_Locator = newSize;
}

//------------------------------------------------------------------------------
// Deallocate
//------------------------------------------------------------------------------
// free memory and set string to null string

static void Deallocate(JCLString* _this)
{
	if( _this->m_String )
	{
		free( _this->m_String );
		_this->m_String = NULL;
	}
	_this->m_Length = 0;
	_this->m_AllocatedLength = 0;
	_this->m_Locator = 0;
}

//------------------------------------------------------------------------------
// StrEquNoCase
//------------------------------------------------------------------------------
// Compares two strings, but ignores the case of characters "A" to "Z".

static JILBool StrEquNoCase(const JILChar* str1, const JILChar* str2)
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
