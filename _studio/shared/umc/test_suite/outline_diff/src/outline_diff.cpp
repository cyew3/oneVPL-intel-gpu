//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2017 Intel Corporation. All Rights Reserved.
//

#include <memory>
#include "umc_structures.h"
#include "outline_dll.h"
#include "umc_h264_dec.h"

void PrintUsage(const vm_char* app)
{
    vm_string_printf(VM_STRING("%s -i <input_outline> -r <reference_outline> -o <output_outline> \n"), app);
}

struct CommandLine
{
    static const Ipp32u MaxLen = UMC::MAXIMUM_PATH;
    CommandLine(int argc, const vm_char** argv) { Parse(argc, argv); }
    bool IsValid() const { return m_valid; }

    void Parse(int argc, const vm_char** argv);

    bool IsOutputOutlineNeeded() const { return m_OutputOutlineFile[0] != '\0'; }
    bool IsReferenceOutlineNeeded() const { return m_RefOutlineFile[0] != '\0'; }
    bool IsInputOutlineNeeded() const { return m_InputOutlineFile[0] != '\0'; }

    vm_char m_OutputOutlineFile[MaxLen + 1];
    vm_char m_RefOutlineFile[MaxLen + 1];
    vm_char m_InputOutlineFile[MaxLen + 1];

private:
    bool m_valid;
};

void CommandLine::Parse(int argc, const vm_char** argv)
{
    m_valid = false;
    m_OutputOutlineFile[0] = m_RefOutlineFile[0] = VM_STRING('\0');

    for (int i = 1; i < argc; i++)
    {
        if (vm_string_strncmp(argv[i], VM_STRING("-i"), 2) == 0)
        {
            if (i + 1 < argc)
            {
                vm_string_strncpy(m_InputOutlineFile, argv[i + 1], MaxLen);
                i++;
            }
        }

        if (vm_string_strncmp(argv[i], VM_STRING("-r"), 2) == 0)
        {
            if (i + 1 < argc)
            {
                vm_string_strncpy(m_RefOutlineFile, argv[i + 1], MaxLen);
                i++;
            }
        }

        if (vm_string_strncmp(argv[i], VM_STRING("-o"), 2) == 0)
        {
            if (i + 1 < argc)
            {
                vm_string_strncpy(m_OutputOutlineFile, argv[i + 1], MaxLen);
                i++;
            }
        }

    }

    m_valid = IsInputOutlineNeeded() && (IsReferenceOutlineNeeded() || IsOutputOutlineNeeded());
}

class C
{
public:

    C();
    ~C();

    void Init(const vm_char * input, const vm_char * reference, const vm_char * output);
    bool Compare();
    bool Close();

private:
    std::unique_ptr<OutlineReader> m_pReader1;
    std::unique_ptr<OutlineReader> m_pReader2;

    std::unique_ptr<VideoOutlineWriter> m_pWriter;
    std::unique_ptr<CheckerBase> m_checker;

    bool CompareSequence(Ipp32s seq_id);

    OutlineFactoryAbstract  * m_factory;
};

C::C()
{
}

C::~C()
{
    Close();
}

void C::Init(const vm_char * input, const vm_char * reference, const vm_char * output)
{
    m_factory = GetOutlineFactory();
    m_pReader1.reset(m_factory->CreateReader());
    m_pReader1->Init(input);


    if (reference)
    {
        m_pReader2.reset(m_factory->CreateReader());
        m_pReader2->Init(reference);
    }

    if (output)
    {
        m_pWriter.reset(m_factory->CreateVideoOutlineWriter());
        m_pWriter->Init(output);
    }

    m_checker.reset(m_factory->CreateChecker());
}

bool C::Compare()
{
    UMC::H264VideoDecoderParams info1;
    UMC::H264VideoDecoderParams info2;

    try
    {
        for (Ipp32s i = 0; ; i++)
        {
            Entry_Read * entry1 = m_pReader1->GetSequenceEntry(i);

            if (entry1)
                m_pReader1->ReadSequenceStruct(entry1, &info1);

            if (m_pReader2.get())
            {
                Entry_Read * entry2 = m_pReader2->GetSequenceEntry(i);

                if (!entry1 && !entry2)
                {
                    return true;
                }

                if (!entry1 || !entry2)
                {
                    return false;
                }

                m_pReader2->ReadSequenceStruct(entry2, &info2);

                m_checker->CheckSequenceInfo(&info1, &info2);
            }

            if (!entry1)
                return true;

            if (m_pWriter.get())
            {
                m_pWriter->WriteSequence(i, &info1);
            }

            if (!CompareSequence(i))
                return false;
        }
    }
    catch(OutlineException & ex)
    {
        printf("outline error: %s\n", ex.GetDescription());
        return false;
    }
    catch(...)
    {
        return false;
    }
}

bool C::CompareSequence(Ipp32s seq_id)
{
    TestVideoData data1;
    TestVideoData data2;

    for (Ipp32s i = 0; ; i++)
    {
        Entry_Read * entry1 = m_pReader1->GetEntry(seq_id, i);

        if (entry1)
            m_pReader1->ReadEntryStruct(entry1, &data1);

        if (m_pReader2.get())
        {
            Entry_Read * entry2 = m_pReader2->GetEntry(seq_id, i);

            if (!entry1 && !entry2)
            {
                return true;
            }

            if (!entry1 || !entry2)
            {
                return false;
            }

            m_pReader2->ReadEntryStruct(entry2, &data2);

            m_checker->CheckFrameInfo(&data1, &data2);
        }

        if (!entry1)
            return true;

        if (m_pWriter.get())
        {
            m_pWriter->WriteFrame(&data1);
        }
    }
}

bool C::Close()
{
    return true;
}

int vm_main(int argc, const vm_char** argv)
{
    CommandLine cmd = CommandLine(argc, argv);
    if (!cmd.IsValid())
    {
        PrintUsage(argv[0]);
        return 1;
    }

    C c;

    c.Init(cmd.m_InputOutlineFile, cmd.m_RefOutlineFile, cmd.m_OutputOutlineFile);

    if (!c.Compare())
    {
        vm_string_printf(VM_STRING("FAILED\n"));
        return 1;
    }

    vm_string_printf(VM_STRING("OK\n"));
    return 0;
}
