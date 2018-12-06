// Copyright (c) 2003-2018 Intel Corporation
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

/*
 * VM 64-bits buffered file operations library
 *       common implementation
 */
#include "vm_file.h"
#if defined(LINUX32) || defined(__APPLE__)
# define SLASH '/'
#else
# define SLASH '\\'
# if _MSC_VER >= 1400
#   pragma warning( disable:4996 )
# endif
#endif
#if defined (__ICL)
/* non-pointer conversion from "unsigned __int64" to "int32_t={signed int}" may lose significant bits */
#pragma warning(disable:2259)
#endif
/*
 * return full file suffix or nchars of suffix if nchars is to small to fit the suffix
 * !!! if more then one suffix applied then only first from the end of filename will be found
 */
void vm_file_getsuffix(vm_char *filename, vm_char *suffix, int nchars) {
  /* go to end of line and then go up until we will meet the suffix sign . or
   * to begining of line if no suffix found */
  int32_t len, i = 0;
  len = (int32_t) vm_string_strlen(filename);
  suffix[0] = '\0';
  while(len && (filename[len--] != '.'));
  if (len) {
    len += 2;
    for( ; filename[len]; ++len) {
      suffix[i] = filename[len];
      if (++i >= nchars)
        break;
      }
    suffix[i] = '\0';
    }
  }

#define ADDPARM(A)                    \
  if ((uint32_t)nchars > vm_string_strlen(A)) {   \
    vm_string_strcat_s(filename, nchars, A);              \
    offs = (uint32_t) vm_string_strlen(filename);          \
    nchars -= offs;                   \
    if (nchars)                       \
      filename[offs] = SLASH;         \
    ++offs;                           \
    --nchars;                         \
    filename[offs] = '\0';            \
    }
