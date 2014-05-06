/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008 Intel Corporation. All Rights Reserved.

File Name: automatic_vm_file.h

\* ****************************************************************************** */

#include "vm_file.h"

class AutomaticVMFile
{
public:
    // Default constructor
    AutomaticVMFile(void)
    {
        m_file = (vm_file *) 0;
    }

    // Constructor
    explicit
    AutomaticVMFile(const vm_char *fname, const vm_char *mode)
    {
        m_file = (vm_file *) 0;

        // try to open file
        m_file = vm_file_fopen(fname, mode);
    }

    // Destructor
    virtual
    ~AutomaticVMFile(void)
    {
        Release();
    }

    // open the file
    bool Open(const vm_char *fname, const vm_char *mode)
    {
        // close the previous file
        Release();

        // try to open file
        m_file = vm_file_fopen(fname, mode);
        
        return (0 != m_file);
    }

    // Set the file position
    inline
    Ipp64u Seek(Ipp64s position, VM_FILE_SEEK_MODE mode)
    {
        return vm_file_fseek(m_file, position, mode);
    }

    // Read a buffer from the file
    inline
    Ipp32s Read(void *buf, Ipp32u itemsz, Ipp32s nitems)
    {
        return vm_file_fread(buf, itemsz, nitems, m_file);
    }

    // Write a buffer from the file
    inline
    Ipp32s Write(void *buf, Ipp32u itemsz, Ipp32s nitems)
    {
        return vm_file_fwrite(buf, itemsz, nitems, m_file);
    }

    // Get the file size
    inline
    Ipp64u Size(void)
    {
        return m_file->fsize;
    }

    inline
    bool operator == (vm_file *value)
    {
        return (m_file == value);
    }

    inline
    bool operator != (vm_file *value)
    {
        return !(operator == (value));
    }

protected:
    // Release the object
    void Release(void)
    {
        if (m_file)
        {
            vm_file_close(m_file);
        }

        m_file = (vm_file *) 0;
    }

    vm_file *m_file;                                            // (vm_file *) protected file
};

inline
bool operator == (vm_file *value, AutomaticVMFile &file)
{
    return file.operator == (value);
}

inline
bool operator != (vm_file *value, AutomaticVMFile &file)
{
    return file.operator != (value);
}
