#ifndef INI_H
#define INI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <string.h>
#include <ctype.h>

//Visibility symbols, required for Windows DLLs
#ifndef INI_API
#if defined _WIN32 || defined __CYGWIN__
#	ifdef INI_SHARED_LIB
#		ifdef INI_SHARED_LIB_BUILDING
#			define INI_API __declspec(dllexport)
#		else
#			define INI_API __declspec(dllimport)
#		endif
#	else
#		define INI_API
#	endif
#else
#	if defined(__GNUC__) && __GNUC__ >= 4
#		define INI_API __attribute__ ((visibility ("default")))
#	else
#		define INI_API
#	endif
#endif
#endif

#define KEY_OF_1SECTION_MAXCNT 71

typedef struct
{
    char *name;
    char *value;
} INI_KEY;

typedef struct
{
    char *sectionName;
    unsigned int keyCnt;
    INI_KEY szstKey[KEY_OF_1SECTION_MAXCNT];
} INI_SECTION;

//Typedef for prototype of handler function.
#ifdef INI_HANDLER_LINENO
typedef int (*INI_HANDLER)(INI_SECTION *pstSection, void *user, int lineno);
#else
typedef int (*INI_HANDLER)(INI_SECTION *pstSection, void *user);
#endif

//Typedef for prototype of fgets-style reader function.
typedef char* (*ini_reader)(char *str, int num, void *stream);

/* Parse given INI-style file. May have [section]s, name=value pairs
   (whitespace stripped), and comments starting with ';' (semicolon).
   name:value pairs are also supported as a concession to Python's configparser.

   For each parsed section or leading key-value group, call handler function
   with the given user pointer and collected INI_SECTION. Parsed data is valid
   only for the duration of the handler call. Handler should return nonzero on
   success, zero on error.

   Returns 0 on success, positive line number of first parse or handler error
   (doesn't stop on first error), ERR_FILE_OP_FAILED on file open or read error.
*/
INI_API int ini_parse(const char *filePath, INI_HANDLER handler, void *user);

/* Same as ini_parse(), but takes a file pointer instead of a file path. This doesn't
   close the file when it's finished -- the caller must do that. */
INI_API int ini_parse_file(FILE *fp, INI_HANDLER handler, void *user);

/* Same as ini_parse(), but takes an ini_reader function pointer instead of
   a file path. Used for implementing custom or string-based I/O (see also
   ini_parse_string). */
INI_API int ini_parse_stream(ini_reader reader, void *stream, INI_HANDLER handler, void *user);

/* Same as ini_parse(), but takes a zero-terminated string with the INI data
instead of a file. Useful for parsing INI data from a network socket or
already in memory. */
INI_API int ini_parse_string(const char *string, INI_HANDLER handler, void *user);

/* Nonzero to allow a UTF-8 BOM sequence (0xEF 0xBB 0xBF) at the start of
   the file. */
#ifndef INI_ALLOW_BOM
#define INI_ALLOW_BOM 1
#endif

/* Chars that begin a start-of-line comment. Per Python configparser, allow
   both ; and # comments at the start of a line by default. */
#ifndef INI_START_COMMENT_PREFIXES
#define INI_START_COMMENT_PREFIXES ";#"
#endif

/* Nonzero to allow inline comments (with valid inline comment characters
   specified by INI_INLINE_COMMENT_PREFIXES). Set to 0 to turn off and match
   Python 3.2+ configparser behaviour. */
#ifndef INI_ALLOW_INLINE_COMMENTS
#define INI_ALLOW_INLINE_COMMENTS 1
#endif
#ifndef INI_INLINE_COMMENT_PREFIXES
#define INI_INLINE_COMMENT_PREFIXES ";"
#endif

/* Maximum read buffer size for one INI line. A physical line may contain at most
   INI_1LINE_MAXCH - 1 bytes excluding CR/LF; longer lines are rejected. */
#ifndef INI_1LINE_MAXCH
#define INI_1LINE_MAXCH 200
#endif
//Maximum physical line count; additional lines are rejected.
#define LINE_CNT_MAX 919

//Stop parsing on first error (default is to keep parsing).
#ifndef INI_STOP_ON_FIRST_ERROR
#define INI_STOP_ON_FIRST_ERROR 0
#endif

#ifdef __cplusplus
}
#endif

#endif //INI_H
