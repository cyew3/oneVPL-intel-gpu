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

#ifndef __UMC_INDEX_H__
#define __UMC_INDEX_H__

#include "umc_linked_list.h"
#include "umc_mutex.h"

namespace UMC
{

struct IndexEntry
{
    // Constructor
    IndexEntry()
    {
        stPosition = 0;
        dPts = -1.0;
        dDts = -1.0;
        uiSize = 0;
        uiFlags = 0;
    }

    // Returns DTS if present otherwise PTS
    inline double GetTimeStamp(void)
    {
        return dDts < 0.0 ? dPts : dDts;
    }

    // Aboslute position of the sample
    size_t stPosition;

    // Presentation time stamp in seconds
    double dPts;

    // Decoding time stamp in seconds
    double dDts;

    // Sample size in byte
    uint32_t uiSize;

    // Flags (frame type for video samples)
    uint32_t uiFlags;
};

struct IndexFragment
{
    // Constructor
    IndexFragment()
    {
        pEntryArray = NULL;
        iNOfEntries = 0;
    }

    // The pointer to array of entries
    IndexEntry *pEntryArray;

    // number of entries in the array
    int32_t iNOfEntries;

};

class TrackIndex
{
public:

    TrackIndex();
    ~TrackIndex();

    // Returns number of entries in ALL fragments
    uint32_t NOfEntries(void);

    // Provides FIRST entry from the FIRST fragment
    Status First(IndexEntry &entry);

    // Provides LAST entry from the LAST fragment
    Status Last(IndexEntry &entry);

    // Provides next entry
    // If last returned entry is the last in the fragment,
    // first entry from the NEXT fragment will be returned
    Status Next(IndexEntry &entry);

    // Provides previous entry
    // If last returned entry is the first in the fragment,
    // last entry from the PREVIOUS fragment will be returned
    Status Prev(IndexEntry &entry);

    // Provides next key entry
    // If last returned entry is the last in the fragment,
    // first entry from the NEXT fragment will be returned
    Status NextKey(IndexEntry &entry);

    // Provides previous key entry
    // If last returned entry is the first in the fragment,
    // last entry from the PREVIOUS fragment will be returned
    Status PrevKey(IndexEntry &entry);

    // Provides last returned entry
    Status Get(IndexEntry &entry);

    // Provides entry at the specified position (through ALL fragments)
    Status Get(IndexEntry &entry, int32_t pos);

    // Provides entry with timestamp is less or equal to specified (through ALL fragments)
    Status Get(IndexEntry &entry, double time);

    // Add whole fragment to the end of index
    Status Add(IndexFragment &newFrag);

    // Removes last fragment
    Status Remove(void);

/*
 * These functions are not necessary for AVI and MPEG4 splitters,
 * so they are temporary commented

    // Modifies last returned entry
    Status Modify(IndexEntry &entry);

    // Modifies entry at the specified position
    Status Modify(IndexEntry &entry, int32_t pos);
*/

protected:

    // Returns pointer to next entry
    // If last returned entry is the last in the fragment,
    // first entry from the NEXT fragment will be returned
    // Input parameter and current state are not checked
    // State variables will be modified
    IndexEntry *NextEntry(void);

    // Returns pointer to previous entry
    // If last returned entry is the first in the fragment,
    // last entry from the PREVIOUS fragment will be returned
    // Input parameter and current state are not checked
    // State variables will be modified
    IndexEntry *PrevEntry(void);

    // Returns element at a specified position
    // Input parameter and current state are not checked
    // State variables will be modified
    IndexEntry *GetEntry(int32_t pos);

    // Returns element with timestamp is less or equal to specified
    // Input parameter and current state are not checked
    // State variables will be modified
    IndexEntry *GetEntry(double time);

    // Linked list of index fragments
    LinkedList<IndexFragment> m_FragmentList;

    // Total number of entries in all fragments
    uint32_t m_uiTotalEntries;

    // Copy of fragment which contains last returned entry
    IndexFragment m_ActiveFrag;

    // absolute position in index of the first entry of active fragment
    int32_t m_iFirstEntryPos;

    // absolute position in index of the last entry of active fragment
    int32_t m_iLastEntryPos;

    // relative position inside active fragment of the last returned entry
    int32_t m_iLastReturned;

    // synchro object
    vm_mutex m_Mutex;

}; // class TrackIndex

} // namespace UMC

#endif // __UMC_INDEX_H__
