//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

//#include "mfx_h265_defs.h"
#include "mfx_h265_encode.h"
#include "mfx_h265_frame.h"
//#include "mfx_h265_enc.h"
//#include "mfx_h265_optimization.h"
//#include "ippi.h"

TaskIter  findOldestToLookAheadProcess(std::list<Task*> & queue)
{
    if ( queue.empty() ) 
        return queue.end();

    for (TaskIter it = queue.begin(); it != queue.end(); it++ ) {

        if ( !(*it)->m_frameOrigin->wasLAProcessed() ) {
            return it;
        }
    }

    return queue.end();
}

void MFXVideoENCODEH265::LookAheadAnalysis()
{

    TaskIter it = findOldestToLookAheadProcess(m_inputQueue);

    if( it == m_inputQueue.end() ) {
        if( !m_inputQueue.empty() ) {
            m_lookaheadQueue.splice(m_lookaheadQueue.end(), m_inputQueue, m_inputQueue.begin());
        }

        return;
    }

    Task* curr = (*it);
    Task* prev = (it == m_inputQueue.begin()) ? NULL : (*std::prev(it));

    //IntraAnalysis(curr);
    //InterAnalysis(curr, prev);

    curr->m_frameOrigin->setWasLAProcessed();

    if(prev)
        m_lookaheadQueue.splice(m_lookaheadQueue.end(), m_inputQueue, std::prev(it));

    

} // 



#endif // MFX_ENABLE_H265_VIDEO_ENCODE
