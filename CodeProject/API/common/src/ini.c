#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "ini.h"
#include "Error.h"

//Used by ini_parse_string() to keep track of string parsing state.
typedef struct 
{
    const char *ptr;
    size_t num_left;
} ini_parse_string_ctx;

static char* ini_rstrip(char *s);
static char* ini_lskip(const char *s);
static char* ini_find_chars_or_comment(const char *s, const char *chars);
static char* ini_fgets(char *str, int num, void *stream);
static char* ini_reader_string(char *str, int num, void *stream);

int ini_parse_stream(ini_reader reader, void *stream, INI_HANDLER handler, void *user)
{
    INI_SECTION stLastSection = {.keyCnt = 0};
    char line[LINE_CNT_MAX][INI_1LINE_MAXCH] = {{0}, {0}};
    char extraLine[3] = {0};
    char *pStart = NULL, *pEnd = NULL, *pValue = NULL;
    int i, lineno = 0, error = 0, errorLineCnt = 0;
    int szErrLineNO[LINE_CNT_MAX + 1] = {0}; //The szErrLineNO[LINE_CNT_MAX] = errLineNO only means the total lines cnt overflow LINE_CNT_MAX

    if (reader == NULL)
        return ERR_ARGV_1;

    if (handler == NULL)
        return ERR_ARGV_3;

    //Scan through stream line by line
    while (lineno < LINE_CNT_MAX && reader(line[lineno], INI_1LINE_MAXCH, stream) != NULL)
    {
        error = 0;
        pStart = line[lineno++];
        if (strlen(pStart) == INI_1LINE_MAXCH - 1 && strchr(pStart, '\n') == NULL)
        {
            if (reader(extraLine, sizeof(extraLine), stream) != NULL && strcmp(extraLine, "\n") != 0 && strcmp(extraLine, "\r\n") != 0)
            {
                PRINT_FUNCTION_ERRINFO(ERR_OVER_BUF, __FUNCTION__, "ini line%d exceeds INI_1LINE_MAXCH - 1(%d)!", lineno, INI_1LINE_MAXCH - 1);
                szErrLineNO[errorLineCnt++] = lineno;
                return szErrLineNO[0];
            }
        }
#if INI_ALLOW_BOM
        if (lineno == 1 && (unsigned char)pStart[0] == 0xEF && (unsigned char)pStart[1] == 0xBB && (unsigned char)pStart[2] == 0xBF)
            pStart += 3;
#endif
        pStart = ini_rstrip(ini_lskip(pStart));
        if (strchr(INI_START_COMMENT_PREFIXES, *pStart))
        {   //Start-of-line comment
        }
        else if (*pStart == '[')
        {   //a "[section]" line
            pEnd = ini_find_chars_or_comment(pStart + 1, "]");
            if (*pEnd == ']')
            {
                *pEnd = '\0';
                if (stLastSection.keyCnt)
                {
#ifdef INI_HANDLER_LINENO
                    if (!handler(&stLastSection, user, lineno) && !error)
#else
                    if (!handler(&stLastSection, user) && !error)
#endif
                        error = lineno;
                    stLastSection.keyCnt = 0;
                }
                stLastSection.sectionName = pStart + 1;
            }
            else if (!error) 
            {   //No ']' found on section line
                error = lineno;
            }
        }
        else if (*pStart)
        {   //Not a comment, must be a name[=:]value pair
            pEnd = ini_find_chars_or_comment(pStart, "=:");
            if (*pEnd == '=' || *pEnd == ':')
            {
                if (stLastSection.keyCnt < KEY_OF_1SECTION_MAXCNT)
                {
                    *pEnd = '\0';
                    stLastSection.szstKey[stLastSection.keyCnt].name = ini_rstrip(pStart);
                    pValue = pEnd + 1;
#if INI_ALLOW_INLINE_COMMENTS
                    pEnd = ini_find_chars_or_comment(pValue, NULL);
                    if (*pEnd)
                        *pEnd = '\0';
#endif
                    pValue = ini_lskip(pValue);
                    ini_rstrip(pValue);
                    stLastSection.szstKey[stLastSection.keyCnt++].value = pValue;
                }
                else
                {
                    error = lineno;
                }
            }
            else if (!error) 
            {   //No '=' or ':' found on name[=:]value line
                error = lineno;
            }
        }

        if (error)
            szErrLineNO[errorLineCnt++] = error;
#if INI_STOP_ON_FIRST_ERROR
        if (error)
            break;
#endif
    }

    if (stLastSection.keyCnt)
    {
#ifdef INI_HANDLER_LINENO
        if (!handler(&stLastSection, user, lineno) && !error)
#else
        if (!handler(&stLastSection, user) && !error)
#endif
            szErrLineNO[errorLineCnt++] = lineno;
        stLastSection.keyCnt = 0;
    }

    if (lineno >= LINE_CNT_MAX && reader(extraLine, sizeof(extraLine), stream) != NULL)
    {
        PRINT_FUNCTION_ERRINFO(ERR_OVER_BUF, __FUNCTION__, "ini line count exceeds LINE_CNT_MAX(%d)!", LINE_CNT_MAX);
        szErrLineNO[errorLineCnt++] = LINE_CNT_MAX + 1;
    }

    if (errorLineCnt)
    {
        for (i = 0; i < errorLineCnt; i++)
            printf("ini error[%d] in line%d\r\n", i, szErrLineNO[i]);

        return szErrLineNO[0];
    }

    return RET_SUCCESS;
}

//See documentation in header file.
int ini_parse_file(FILE *fp, INI_HANDLER handler, void *user)
{
    int ret;

    if (fp == NULL)
        return ERR_ARGV_1;

    if (handler == NULL)
        return ERR_ARGV_2;

    ret = ini_parse_stream(ini_fgets, fp, handler, user);
    if (ferror(fp))
        return ERR_FILE_OP_FAILED;

    return ret;
}

//See documentation in header file.
int ini_parse(const char *filePath, INI_HANDLER handler, void *user)
{
    FILE *fp = NULL;
    int ret;

    if (filePath == NULL)
        return ERR_ARGV_1;

    if (handler == NULL)
        return ERR_ARGV_2;

    if ( (fp = fopen(filePath, "r")) == NULL )
        return ERR_FILE_OP_FAILED;

    ret = ini_parse_file(fp, handler, user);
    fclose(fp);
    return ret;
}

//See documentation in header file.
int ini_parse_string(const char *string, INI_HANDLER handler, void *user)
{
    ini_parse_string_ctx stCtx;

    if (string == NULL)
        return ERR_ARGV_1;

    if (handler == NULL)
        return ERR_ARGV_2;

    stCtx.ptr = string;
    stCtx.num_left = strlen(string);
    return ini_parse_stream(ini_reader_string, &stCtx, handler, user);
}

//Strip whitespace chars off end of given string, in place. Return s.
static char* ini_rstrip(char *s)
{
    char *p = s + strlen(s);

    while (p > s && isspace((unsigned char)(*--p)))
        *p = '\0';

    return s;
}

//Return pointer to first non-whitespace char in given string.
static char* ini_lskip(const char *s)
{
    while (*s && isspace((unsigned char)(*s)))
        s++;

    return (char*)s;
}

/* Return pointer to first char (of chars) or inline comment in given string,
   or pointer to NUL at end of string if neither found. Inline comment must
   be prefixed by a whitespace character to register as a comment. */
static char* ini_find_chars_or_comment(const char *s, const char *chars)
{
#if INI_ALLOW_INLINE_COMMENTS
    int was_space = 0;

    while (*s && (!chars || !strchr(chars, *s)) && !(was_space && strchr(INI_INLINE_COMMENT_PREFIXES, *s)))
    {
        was_space = isspace((unsigned char)(*s));
        s++;
    }
#else
    while (*s && (!chars || !strchr(chars, *s)))
    {
        s++;
    }
#endif
    return (char*)s;
}

static char* ini_fgets(char *str, int num, void *stream)
{
    return fgets(str, num, (FILE*)stream);
}

/*
//Similar to strncpy, but ensures dest (size bytes) is NUL-terminated, and doesn't pad with NULs.
static char* ini_strncpy0(char *dest, const char *src, size_t size)
{
    //Could use strncpy internally, but it causes gcc warnings (see issue #91)
    size_t i;
    for (i = 0; i < size - 1 && src[i]; i++)
        dest[i] = src[i];
    dest[i] = '\0';
    return dest;
}
*/

/* An ini_reader function to read the next line from a string buffer. This
   is the fgets() equivalent used by ini_parse_string(). */
static char* ini_reader_string(char *str, int num, void *stream)
{
    ini_parse_string_ctx *pstCtx = (ini_parse_string_ctx*)stream;
    const char *ctx_ptr = pstCtx->ptr;
    size_t ctx_num_left = pstCtx->num_left;
    char *strp = str;
    char c;

    if (ctx_num_left == 0 || num < 2)
        return NULL;

    while (num > 1 && ctx_num_left != 0) 
    {
        c = *ctx_ptr++;
        ctx_num_left--;
        *strp++ = c;
        if (c == '\n')
            break;

        num--;
    }
    *strp = '\0';
    pstCtx->ptr = ctx_ptr;
    pstCtx->num_left = ctx_num_left;
    return str;
}
