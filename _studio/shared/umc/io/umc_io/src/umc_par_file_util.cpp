//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2013 Intel Corporation. All Rights Reserved.
//

#include "umc_par_file_util.h"

#include <stdio.h>

#define VM_MAX_TEMP_LINE 8192
/*
 * read characters from input ASCII file until newline found, null character found,
 * nchars rad or EOF reached. The output strings are unicode in case _UNICODE
 * is defined and ASCII if it is not defined
 */

vm_char* UMC::umc_file_fgets_ascii(vm_char* str, int nchar, vm_file* fd)
{
#if defined(_WIN32) || defined(_WIN64)
  /* read up to VM_MAX_TEMP_LINE bytes from input file, try
   * to satisfy fgets conditions
   */
  Ipp64s fpos;
  Ipp32s rdchar, i, j = 0; // j - current position in the output string
  vm_char* rtv = NULL;

  if (fd == vm_stdin)
#ifdef _UNICODE
  {
    // !!! UNTESTED !!!
    // Code that reads from stdin using fgets
    // It is needed for other functions that use fgets because otherwise its internal buffer is not filled in
    char buffer[VM_MAX_TEMP_LINE];
    str[0] = str[nchar - 1] = 0;
    --nchar;

    while (fgets(buffer, VM_MAX_TEMP_LINE, stdin) && 0 < nchar)
    {
      Ipp32s length = (Ipp32s) _mbstrlen(buffer); // can't be bigger than VM_MAX_TEMP_LINE
      Ipp32u min_length = length < nchar ? length : nchar;
      size_t count;
      errno_t res;

      // copy the row
      res = mbstowcs_s(&count, str, nchar, buffer, length);

      if (0 != res)
        return str;
      else if (count == min_length) // output is not zero terminated
        str[count] = '\0';

      // Using strlen here instead of "length" because _mbstrlen may give
      // smaller number since some characters may be actually multi-byte
      if (strlen(buffer) + 1 < VM_MAX_TEMP_LINE) // EOL found
      {
        if ('\r' == str[count - 2])
        {
          /* remove CR from end of line */
          str[count-2] = '\n';
          str[count-1] = '\0';
        }
        return str;
      }
    }

    return NULL;
  }
#else
    return fgets(str, nchar, stdin);
#endif
  else
  {
    str[0] = str[nchar-1] = 0;
    --nchar;

    if ((fd != NULL) && (fd[0].fd != INVALID_HANDLE_VALUE))
    {
      while ((rdchar = vm_file_fread(fd[0].tbuf, 1, VM_MAX_TEMP_LINE, fd)) != 0)
      {
        for(i = 0; i < rdchar; ++i)
        {
#ifdef _UNICODE
          Ipp32s count = mbtowc(str + j, ((char *)fd[0].tbuf) + i, 1);
          if (count == 0 || count == -1)
            str[j] = '\0';
          else
            i += count - 1;
#else
          str[j] = fd[0].tbuf[i];
#endif
          if((str[j]==0) || (str[j]=='\n') || (j >= nchar))
            break;
          ++j;
        }

        if (i < rdchar)
        {
          /* one of EOS conditions found */
          if ((str[j] == '\n') && (j < nchar)) /* add null character if possible */
            str[++j] = 0;

          if (str[j-2] == '\r')
          {
            /* remove CR from end of line */
            str[j-2] = '\n';
            str[j-1] = '\0';
          }

          /* return file pointer to the first non-string character */
          ++i; // skip end of line character
          fpos = i - rdchar; // - -- because file pointer will move back
          if (fpos != 0)
            vm_file_fseek(fd, fpos, VM_FILE_SEEK_CUR);

          rtv = str;
          break; // leave while loop
        }
      }

      if((rtv == NULL) && (j != 0) && vm_file_feof(fd))
      {
        rtv = str; // end of file during read - input string is valid
        if (j < nchar)
          str[++j] = 0;
      }
    }
  }

  return rtv;
#else
  return vm_file_fgets(str, nchar, fd);
#endif //#if defined(WIN32) || defined(WIN64)
} /* vm_file_fgets_ascii() */
