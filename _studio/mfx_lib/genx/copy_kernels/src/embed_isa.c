//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2016 Intel Corporation. All Rights Reserved.
//

#if defined(_WIN32) || defined(_WIN64)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 16

void make_copyright(FILE* f)
{
    fprintf(f,
"//\n"
"// INTEL CORPORATION PROPRIETARY INFORMATION\n"
"//\n"
"// This software is supplied under the terms of a license agreement or\n"
"// nondisclosure agreement with Intel Corporation and may not be copied\n"
"// or disclosed except in accordance with the terms of that agreement.\n"
"//\n"
"// Copyright(c) 2016 Intel Corporation. All Rights Reserved.\n"
"//\n"
);
}

/* first argument is file.isa to incorporate in C code */
int main(int argc, char** argv)
{
    FILE* f;
    long int size;
    long int size_width;
    int width = WIDTH;
    unsigned char* buf = NULL;
    char* basename;
    char* fname, *sbase, *ebase, *bufname;
    int len,i;

    if(argc != 2)
    {
        printf("Usage: %s somefile.isa", argv[0]);
        exit(-1);
    }

    if((f=fopen(argv[1],"rb")) != NULL)
    {
        fseek(f, 0, SEEK_END);
        size = ftell(f);
        rewind(f);
        buf = (unsigned char*)malloc(size);
        fread(buf,1,size,f);
        fclose(f);
    }

    if( buf )
    {
        //find basename
        fname = argv[1];
        len = strlen(fname);
        ebase = fname+len-1;
        while(ebase >= fname && *ebase != '.') ebase--;
        sbase = ebase;
        while(sbase >= fname && *sbase != '/' && *sbase != '\\') sbase--;
        sbase++;

        len = (int)(ebase-sbase);
        if(len <= 0)
        {
            free(buf);
            exit(-1);
        }

        basename = (char*)malloc(len+5);
        bufname = (char*)malloc(len+1);
        strncpy(bufname, sbase, len);
        bufname[len] = 0;

        strncpy(basename, sbase, len);
        strncpy(basename+len, ".cpp", 4);
        basename[len+4] = 0;

        //cpp file
        if((f=fopen(basename,"wt")) != NULL)
        {
             make_copyright(f);
            //fprintf(f,"#ifndef __%s__\n#define __%s__\n",bufname,bufname);
            fprintf(f,"#include \"../include/%s_isa.h\"\n\n", bufname);
            fprintf(f,"const unsigned char %s[%d] = { \n",bufname, size);
            for(i=0; i<size; i++)
            {
                fprintf(f,"0x%02x", buf[i]);
                if(i != size-1) fprintf(f,",");
                if(i == (width-1)){
                    width += WIDTH;
                    fprintf(f,"\n");
                }
            }
            fprintf(f,"\n};\n");
            //fprintf(f,"#endif\n");
            fclose(f);
        }

        //header file
        strncpy(basename, sbase, len);
        strncpy(basename+len, ".h", 2);
        basename[len+2] = 0;
        if((f=fopen(basename,"wt")) != NULL)
        {
             make_copyright(f);
            //fprintf(f,"#ifndef __%s__\n#define __%s__\n",bufname,bufname);
            fprintf(f,"#ifndef __%s__\n#define __%s__\n",bufname, bufname);
            fprintf(f,"extern const unsigned char %s[%d];\n", bufname, size);
            fprintf(f,"#endif\n");
            //fprintf(f,"#endif\n");
            fclose(f);
        }

        free(bufname);
        free(basename);
        free(buf);
    }

    return 0;
}

#endif /* #if defined(_WIN32) || defined(_WIN64) */
