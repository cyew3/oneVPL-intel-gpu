// Copyright (c) 2003-2020 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef __VM_STRINGS_H__
#define __VM_STRINGS_H__

#include <stdint.h>
#include "ippdefs.h"

#if defined(LINUX32)

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <stdarg.h>
# include <ctype.h>
# include <dirent.h>
# include <errno.h>

typedef char vm_char;

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

//<WALKAROUND BEGIN>
typedef size_t  rsize_t;
typedef int                           errno_t;

#define RCNEGATE(x)  (x)

#define RSIZE_MAX_STR      ( 4UL << 10 )      /* 4KB */

#ifndef ESNULLP
#define ESNULLP         ( 400 )       /* null ptr                    */
#endif

#ifndef ESZEROL
#define ESZEROL         ( 401 )       /* length is zero              */
#endif

#ifndef ESLEMIN
#define ESLEMIN         ( 402 )       /* length is below min         */
#endif

#ifndef ESLEMAX
#define ESLEMAX         ( 403 )       /* length exceeds max          */
#endif

#ifndef ESOVRLP
#define ESOVRLP         ( 404 )       /* overlap undefined           */
#endif

#ifndef ESEMPTY
#define ESEMPTY         ( 405 )       /* empty string                */
#endif

#ifndef ESNOSPC
#define ESNOSPC         ( 406 )       /* not enough space for s2     */
#endif

#ifndef ESUNTERM
#define ESUNTERM        ( 407 )       /* unterminated string         */
#endif

#ifndef ESNODIFF
#define ESNODIFF        ( 408 )       /* no difference               */
#endif

#ifndef ESNOTFND
#define ESNOTFND        ( 409 )       /* not found                   */
#endif

/* Additional for safe snprintf_s interfaces                         */
#ifndef ESBADFMT
#define ESBADFMT        ( 410 )       /* bad format string           */
#endif

#ifndef ESFMTTYP
#define ESFMTTYP        ( 411 )       /* bad format type             */
#endif

/* EOK may or may not be defined in errno.h */
#ifndef EOK
#define EOK             ( 0 )
#endif

#define sl_default_handler ignore_handler_s

typedef void (*constraint_handler_t) (const char * /* msg */,
                                      void *       /* ptr */,
                                      errno_t      /* error */);

#define sldebug_printf(...)

static constraint_handler_t str_handler = NULL;

extern void ignore_handler_s(const char *msg, void *ptr, errno_t error);

extern void
invoke_safe_str_constraint_handler (const char *msg,
                                    void *ptr,
                                    errno_t error);

static inline void handle_error(char *orig_dest, rsize_t orig_dmax,
                                char *err_msg, errno_t err_code)
{
#ifdef SAFECLIB_STR_NULL_SLACK
    /* null string to eliminate partial copy */
    while (orig_dmax) { *orig_dest = '\0'; orig_dmax--; orig_dest++; }
#else
    (void)orig_dmax;

    *orig_dest = '\0';
#endif

    invoke_safe_str_constraint_handler(err_msg, NULL, err_code);
    return;
}

extern constraint_handler_t
set_str_constraint_handler_s(constraint_handler_t handler);


/* string compare */
extern errno_t
strcasecmp_s(const char *dest, rsize_t dmax,
             const char *src, int *indicator);


/* find a substring _ case insensitive */
extern errno_t
strcasestr_s(char *dest, rsize_t dmax,
             const char *src, rsize_t slen, char **substring);


/* string concatenate */
extern errno_t
strcat_s(char *dest, rsize_t dmax, const char *src);


/* string compare */
extern errno_t
strcmp_s(const char *dest, rsize_t dmax,
         const char *src, int *indicator);


/* fixed field string compare */
extern errno_t
strcmpfld_s(const char *dest, rsize_t dmax,
            const char *src, int *indicator);


/* string copy */
extern errno_t
strcpy_s(char *dest, rsize_t dmax, const char *src);

/* string copy */
extern char *
stpcpy_s(char *dest, rsize_t dmax, const char *src, errno_t *err);

/* fixed char array copy */
extern errno_t
strcpyfld_s(char *dest, rsize_t dmax, const char *src, rsize_t slen);


/* copy from a null terminated string to fixed char array */
extern errno_t
strcpyfldin_s(char *dest, rsize_t dmax, const char *src, rsize_t slen);


/* copy from a char array to null terminated string */
extern errno_t
strcpyfldout_s(char *dest, rsize_t dmax, const char *src, rsize_t slen);


/* computes excluded prefix length */
extern errno_t
strcspn_s(const char *dest, rsize_t dmax,
          const char *src,  rsize_t slen, rsize_t *count);


/* returns a pointer to the first occurrence of c in dest */
extern errno_t
strfirstchar_s(char *dest, rsize_t dmax, char c, char **first);


/* returns index of first difference */
extern  errno_t
strfirstdiff_s(const char *dest, rsize_t dmax,
               const char *src, rsize_t *index);


/* returns  a pointer to the last occurrence of c in s1 */
extern errno_t
strlastchar_s(char *str, rsize_t smax, char c, char **first);


/* returns index of last difference */
extern  errno_t
strlastdiff_s(const char *dest, rsize_t dmax,
              const char *src, rsize_t *index);


/* left justify */
extern errno_t
strljustify_s(char *dest, rsize_t dmax);


/* fitted string concatenate */
extern errno_t
strncat_s(char *dest, rsize_t dmax, const char *src, rsize_t slen);


/* fitted string copy */
extern errno_t
strncpy_s(char *dest, rsize_t dmax, const char *src, rsize_t slen);


/* string length */
extern rsize_t
strnlen_s (const char *s, rsize_t smax);


/* string terminate */
extern rsize_t
strnterminate_s (char *s, rsize_t smax);


/* get pointer to first occurrence from set of char */
extern errno_t
strpbrk_s(char *dest, rsize_t dmax,
          char *src,  rsize_t slen, char **first);


extern errno_t
strfirstsame_s(const char *dest, rsize_t dmax,
               const char *src,  rsize_t *index);

extern errno_t
strlastsame_s(const char *dest, rsize_t dmax,
              const char *src, rsize_t *index);


/* searches for a prefix */
extern errno_t
strprefix_s(const char *dest, rsize_t dmax, const char *src);


/* removes leading and trailing white space */
extern errno_t
strremovews_s(char *dest, rsize_t dmax);


/* computes inclusive prefix length */
extern errno_t
strspn_s(const char *dest, rsize_t dmax,
         const char *src,  rsize_t slen, rsize_t *count);


/* find a substring */
extern errno_t
strstr_s(char *dest, rsize_t dmax,
         const char *src, rsize_t slen, char **substring);


/* string tokenizer */
extern char *
strtok_s(char *s1, rsize_t *s1max, const char *src, char **ptr);


/* convert string to lowercase */
extern errno_t
strtolowercase_s(char *str, rsize_t slen);


/* convert string to uppercase */
extern errno_t
strtouppercase_s(char *str, rsize_t slen);


/* zero an entire string with nulls */
extern errno_t
strzero_s(char *dest, rsize_t dmax);

extern errno_t
memcpy_s (void *dest, rsize_t dmax, const void *src, rsize_t smax);

#define FMT_CHAR    'c'
#define FMT_WCHAR   'C'
#define FMT_SHORT   'h'
#define FMT_INT		'd'
#define FMT_LONG	'l'
#define FMT_STRING	's'
#define FMT_WSTRING	'S'
#define FMT_DOUBLE	'g'
#define FMT_LDOUBLE	'G'
#define FMT_VOID    'p'
#define FMT_PCHAR	'1'
#define FMT_PSHORT	'2'
#define FMT_PINT	'3'
#define FMT_PLONG	'4'

#define MAX_FORMAT_ELEMENTS    16

#define CHK_FORMAT(X,Y)   (((X)==(Y))?1:0)

#ifdef __KERNEL__
# include <linux/errno.h>
#else
#include <errno.h>
#endif /* __KERNEL__ */

/*
 * Safe Lib specific errno codes.  These can be added to the errno.h file
 * if desired.
 */
#ifndef ESNULLP
#define ESNULLP         ( 400 )       /* null ptr                    */
#endif

#ifndef ESZEROL
#define ESZEROL         ( 401 )       /* length is zero              */
#endif

#ifndef ESLEMIN
#define ESLEMIN         ( 402 )       /* length is below min         */
#endif

#ifndef ESLEMAX
#define ESLEMAX         ( 403 )       /* length exceeds max          */
#endif

#ifndef ESOVRLP
#define ESOVRLP         ( 404 )       /* overlap undefined           */
#endif

#ifndef ESEMPTY
#define ESEMPTY         ( 405 )       /* empty string                */
#endif

#ifndef ESNOSPC
#define ESNOSPC         ( 406 )       /* not enough space for s2     */
#endif

#ifndef ESUNTERM
#define ESUNTERM        ( 407 )       /* unterminated string         */
#endif

#ifndef ESNODIFF
#define ESNODIFF        ( 408 )       /* no difference               */
#endif

#ifndef ESNOTFND
#define ESNOTFND        ( 409 )       /* not found                   */
#endif

/* Additional for safe snprintf_s interfaces                         */
#ifndef ESBADFMT
#define ESBADFMT        ( 410 )       /* bad format string           */
#endif

#ifndef ESFMTTYP
#define ESFMTTYP        ( 411 )       /* bad format type             */
#endif

/* EOK may or may not be defined in errno.h */
#ifndef EOK
#define EOK             ( 0 )
#endif

#define SNPRFNEGATE(x) (-1*(x))

extern int snprintf_s_i(char *dest, rsize_t dmax, const char *format, int a);
extern int snprintf_s_si(char *dest, rsize_t dmax, const char *format, char *s, int a);
extern int snprintf_s_l(char *dest, rsize_t dmax, const char *format, long a);
extern int snprintf_s_sl(char *dest, rsize_t dmax, const char *format, char *s, long a);
extern int snprintf_s_ss(char *dest, rsize_t dmax, const char *format, const char *s1, const char *s2);
extern unsigned int
check_integer_format(const char format);
extern unsigned int
parse_format(const char *format, char pformatList[], unsigned int maxFormats);
//<WALKAROUND END>

#ifdef __cplusplus
}
#endif /* __cplusplus */
# include "vm_file.h"
#define VM_STRING(x) x

#define vm_string_printf    printf
#define vm_string_sprintf   sprintf
#define vm_string_snprintf  snprintf
#define vm_string_fprintf   vm_file_fprintf
#define vm_string_vfprintf  vm_file_vfprintf

#define vm_string_vsprintf  vsprintf
#define vm_string_vsnprintf vsnprintf

#if !defined(ANDROID)

#define vm_string_strcat    strcat
#define vm_string_strcpy    strcpy
#define vm_string_strncpy   strncpy
#define vm_string_strcspn   strcspn
#define vm_string_strspn    strspn

#define vm_string_strlen    strlen
#define vm_string_strcmp    strcmp
#define vm_string_strncmp   strncmp
#define vm_string_stricmp   strcmp
#define vm_string_strnicmp  strncmp
#define vm_string_strrchr   strrchr

#define vm_string_sprintf_s sprintf
#define vm_string_strcat_s  strcat_s
#define vm_string_strncat   strncat_s
#define vm_string_strcpy_s  strcpy_s
#define vm_string_strncpy_s strncpy_s
#define vm_string_strnlen_s strnlen_s
#else // ANDROID

#define vm_string_strcat    strcat
#define vm_string_strcpy    strcpy
#define vm_string_strncpy   strncpy
#define vm_string_strcspn   strcspn
#define vm_string_strspn    strspn

#define vm_string_strlen    strlen
#define vm_string_strcmp    strcmp
#define vm_string_strncmp   strncmp
#define vm_string_stricmp   strcasecmp
#define vm_string_strnicmp  strncasecmp
#define vm_string_strrchr   strrchr

#endif // ANDROID

#define vm_string_atol      atol
#define vm_string_atoi      atoi
#define vm_string_atof      atof

#define vm_string_strtod    strtod
#define vm_string_strtol    strtol

#define vm_string_strstr    strstr
#define vm_string_sscanf    sscanf
#define vm_string_vsscanf   vsscanf
#define vm_string_strchr    strchr

#define vm_finddata_t struct _finddata_t

typedef DIR* vm_findptr;

/*
 * findfirst, findnext, findclose direct emulation
 * for old ala Windows applications
 */
struct _finddata_t
{
  uint32_t attrib;
  long long size;
  vm_char  name[260];
};

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

int32_t vm_string_vprintf(const vm_char *format, va_list argptr);
vm_findptr vm_string_findfirst(vm_char* filespec, vm_finddata_t* fileinfo);
int32_t vm_string_findnext(vm_findptr handle, vm_finddata_t* fileinfo);
int32_t vm_string_findclose(vm_findptr handle);
void vm_string_splitpath(const vm_char *path, char *drive, char *dir, char *fname, char *ext);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#elif defined(_WIN32) || defined(_WIN64) || defined(_WIN32_WCE)

//#include <io.h>
#include <tchar.h>

#if (_MSC_VER >= 1400)
#pragma warning( disable: 4996 )
#endif

#define VM_STRING(x) __T(x)
typedef TCHAR vm_char;
#define vm_string_printf    _tprintf
#define vm_string_fprintf   vm_file_fprintf
#define vm_string_fprintf_s _ftprintf_s
#define vm_string_sprintf   _stprintf
#define vm_string_sprintf_s _stprintf_s
#define vm_string_snprintf  _sntprintf
#define vm_string_vprintf   _vtprintf
#define vm_string_vfprintf  vm_file_vfprintf
#define vm_string_vsprintf  _vstprintf
#define vm_string_vsnprintf _vsntprintf

#define vm_string_strcat    _tcscat
#define vm_string_strcat_s  _tcscat_s
#define vm_string_strncat   _tcsncat
#define vm_string_strcpy    _tcscpy
#define vm_string_strcpy_s  _tcscpy_s
#define vm_string_strcspn   _tcscspn
#define vm_string_strspn    _tcsspn

#define vm_string_strlen    _tcslen
#define vm_string_strcmp    _tcscmp
#define vm_string_strncmp   _tcsnccmp
#define vm_string_stricmp   _tcsicmp
#define vm_string_strnicmp   _tcsncicmp
#define vm_string_strncpy   _tcsncpy
#define vm_string_strncpy_s _tcsncpy_s
#define vm_string_strrchr   _tcsrchr

#define vm_string_atoi      _ttoi
#define vm_string_atof      _tstof
#define vm_string_atol      _ttol
#define vm_string_strtod    _tcstod
#define vm_string_strtol    _tcstol
#define vm_string_strstr    _tcsstr
#define vm_string_sscanf    _stscanf
#define vm_string_vsscanf(str, format, args) _stscanf(str, format, *(void**)args)
#define vm_string_strchr    _tcschr
#define vm_string_strtok    _tcstok

#ifndef _WIN32_WCE
# define vm_findptr intptr_t
# define vm_finddata_t struct _tfinddata_t
# define vm_string_splitpath _tsplitpath
# define vm_string_findclose _findclose

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

vm_findptr vm_string_findfirst(vm_char* filespec, vm_finddata_t* fileinfo);
int32_t vm_string_findnext(vm_findptr handle, vm_finddata_t* fileinfo);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* no findfirst etc for _WIN32_WCE */

#define vm_string_makepath  _tmakepath

#endif /* WINDOWS */

#define __VM_STRING(str) VM_STRING(str)

#if !defined(_WIN32) && !defined(_WIN64)

typedef int error_t;

#endif 

#endif /* __VM_STRINGS_H__ */
