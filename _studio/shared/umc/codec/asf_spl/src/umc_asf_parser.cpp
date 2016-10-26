//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2013 Intel Corporation. All Rights Reserved.
//

#include "umc_asf_spl.h"
#include "vm_time.h"
#include "ipps.h"

namespace UMC
{

enum
{
#ifndef _WIN32_WCE
    TIME_TO_SLEEP = 5,
#else
    TIME_TO_SLEEP = 0,
#endif
};

Status ASFSplitter::ReadDataFromType(Ipp8u val, Ipp32u &dst, Ipp32u &infoSize)
{
  Status umcRes = UMC_OK;
  Ipp8u val8;
  Ipp16u val16;

  switch (val) {
    case 0:
      dst = 0;
      break;
    case 1:
      umcRes = m_pDataReader->Get8u(&val8);
      UMC_CHECK_STATUS(umcRes)
      infoSize++;
      dst = val8;
      break;
    case 2:
      umcRes = m_pDataReader->Get16uNoSwap(&val16);
      UMC_CHECK_STATUS(umcRes)
      infoSize += 2;
      dst = val16;
      break;
    case 3:
      umcRes = m_pDataReader->Get32uNoSwap(&dst);
      UMC_CHECK_STATUS(umcRes)
      infoSize += 4;
      break;
  }
  return umcRes;
}

Status ASFSplitter::CountNumberOfStreams(Ipp32u &numOfStreams)
{
  Status umcRes = UMC_OK;
  asf_Object headObj, curObj;
  Ipp32u nObjects = 0, i = 0;

  numOfStreams = 0;
  umcRes = m_pDataReader->SetPosition((Ipp64u)0);
  UMC_CHECK_STATUS(umcRes)
  umcRes = ReadNextObject(&headObj);
  UMC_CHECK_STATUS(umcRes)

  if (headObj.objectID != ASF_Header_Object) {
    return UMC_ERR_INVALID_STREAM;
  }

  umcRes = m_pDataReader->Get32uNoSwap(&nObjects);
  UMC_CHECK_STATUS(umcRes)
  umcRes = m_pDataReader->MovePosition(2); /* 2 reserved bytes */
  UMC_CHECK_STATUS(umcRes)

  for (i = 0; i < nObjects; i++) {
    umcRes = ReadNextObject(&curObj);
    UMC_CHECK_STATUS(umcRes)
    if (curObj.objectID == ASF_Stream_Properties_Object) {
      numOfStreams++;
    }
    umcRes = m_pDataReader->SetPosition(curObj.endPos);
    UMC_CHECK_STATUS(umcRes)
  }

  return UMC_OK;
}

Status ASFSplitter::GetESFromPID(Ipp32u &iES, Ipp32u PID)
{
    Ipp32u i = 0;

    if (m_pInfo->m_nOfTracks <= 0)
        return UMC_ERR_INVALID_PARAMS;

    for(i = 0; i < m_pInfo->m_nOfTracks; i++)
    {
        if (m_pES2PIDTbl[i] == PID)
        {
            iES = i;
            return UMC_OK;
        }
    }
    return UMC_ERR_FAILED;
}

/*** read one data packet ***/
Status ASFSplitter::ReadDataPacket()
{
    Status umcRes = UMC_OK;
    Ipp8u value8u = 0;
    Ipp32u infoSize = 0;

    umcRes = m_pDataReader->Get8u(&value8u);
    UMC_CHECK_STATUS(umcRes)
    infoSize++;
    if (value8u >> 7) {   /*** ErrCorrData is present ***/
        asf_ErrCorrectionData *pECData = new asf_ErrCorrectionData;
        pECData->errCorrFlags = value8u;
        if (value8u & 0xF) { /*** should be set to 0x2 ***/
            umcRes = m_pDataReader->Get8u(&pECData->firstBType);
            if (umcRes != UMC_OK) {
                delete pECData;
                UMC_RET_STATUS(umcRes)
            }
            umcRes = m_pDataReader->Get8u(&pECData->secBCycle);
            if (umcRes != UMC_OK) {
                delete pECData;
                UMC_RET_STATUS(umcRes)
            }
            infoSize += 2;
        }

        delete pECData;

        if (((value8u >> 4) & 0x1) != 0) {
            return UMC_ERR_FAILED;  /*** opaque data should be here ***/
        }

        umcRes = m_pDataReader->Get8u(&value8u);

        UMC_CHECK_STATUS(umcRes)

        infoSize++;
    }

    asf_PayloadParseInfo payloadParseInfo;

    payloadParseInfo.mulPld = value8u & 0x1;
    payloadParseInfo.seqType = (value8u >> 1) & 0x3;
    payloadParseInfo.paddLenType = (value8u >> 3) & 0x3;
    payloadParseInfo.packLenType = (value8u >> 5) & 0x3;
    payloadParseInfo.errCorr = (value8u >> 7);

    umcRes = m_pDataReader->Get8u(&value8u);
    UMC_CHECK_STATUS(umcRes)
    infoSize++;

    payloadParseInfo.replDataLenType = value8u & 0x3;
    payloadParseInfo.offIntoMediaObjLenType = (value8u >> 2) & 0x3;
    payloadParseInfo.mediaObjNumLenType = (value8u >> 4) & 0x3;
    payloadParseInfo.streamNumLenType = (value8u >> 6) & 0x3; /*** should be 0x01 always ***/

    umcRes = ReadDataFromType(payloadParseInfo.packLenType, payloadParseInfo.packLen, infoSize);
    UMC_CHECK_STATUS(umcRes)
    if (payloadParseInfo.packLen == 0)
        payloadParseInfo.packLen = m_pHeaderObject->pFPropObject->minDataPackSize;
    umcRes = ReadDataFromType(payloadParseInfo.seqType, payloadParseInfo.sequence, infoSize);
    UMC_CHECK_STATUS(umcRes)
    umcRes = ReadDataFromType(payloadParseInfo.paddLenType, payloadParseInfo.paddLen, infoSize);
    UMC_CHECK_STATUS(umcRes)

    umcRes = m_pDataReader->Get32uNoSwap(&payloadParseInfo.sendTime);
    UMC_CHECK_STATUS(umcRes)
    infoSize += 4;
    umcRes = m_pDataReader->Get16uNoSwap(&payloadParseInfo.duration); /*** in milliseconds ***/
    UMC_CHECK_STATUS(umcRes)
    infoSize += 2;

    if (payloadParseInfo.mulPld) {  /*** multiple payloads ***/
        umcRes = m_pDataReader->Get8u(&value8u);
        UMC_CHECK_STATUS(umcRes)
        infoSize++;

        Ipp8u numOfPlds = value8u & 0x3F;
        Ipp8u pldLenType = value8u >> 6;
        Ipp32u i = 0;

        if (numOfPlds == 0) {
            return UMC_ERR_FAILED;  /*** must not be zero ***/
        }

        for (i = 0; i < numOfPlds && !m_bFlagStop; i++) {
            Ipp32u frameSize = 0;
            Ipp32u framePTS = 0;
            Ipp32u stc_len = 0;
            asf_Payload multiplePld;

            umcRes = m_pDataReader->Get8u(&value8u);
            UMC_CHECK_STATUS(umcRes)
            infoSize++;

            multiplePld.streamNumber = value8u &0x7F;
            multiplePld.keyFrame = value8u >> 7;

            umcRes = ReadDataFromType(payloadParseInfo.mediaObjNumLenType, multiplePld.mediaObjNum, infoSize);
            UMC_CHECK_STATUS(umcRes)
            umcRes = ReadDataFromType(payloadParseInfo.offIntoMediaObjLenType, multiplePld.offIntoMediaObj, infoSize);
            UMC_CHECK_STATUS(umcRes)
            umcRes = ReadDataFromType(payloadParseInfo.replDataLenType, multiplePld.replDataLen, infoSize);
            UMC_CHECK_STATUS(umcRes)

            if (multiplePld.replDataLen == 1) {   /*** compressed payload ***/
                Ipp32u iES = 0;
                Ipp8u PTSdelta = 0;
                /*** PTS of the 1-st sub-payload ***/
                framePTS = multiplePld.offIntoMediaObj;
                UMC_CHECK_STATUS(umcRes)
                umcRes = m_pDataReader->Get8u(&PTSdelta);
                UMC_CHECK_STATUS(umcRes)
                infoSize++;
                umcRes = ReadDataFromType(pldLenType, multiplePld.pldLen, infoSize);
                UMC_CHECK_STATUS(umcRes)
                umcRes = GetESFromPID(iES, multiplePld.streamNumber);
        //            UMC_CHECK_STATUS(umcRes)
                if ((umcRes != UMC_OK) || (!m_pInfo->m_ppTrackInfo[iES]->m_isSelected))
                {   // wrong payload
                    m_pDataReader->MovePosition(multiplePld.pldLen);
                    infoSize += multiplePld.pldLen;
                } else
                {
                    Ipp32u subPldDataLen = 0;
                    while(subPldDataLen < multiplePld.pldLen)
                    {
                        umcRes = m_pDataReader->Get8u(&value8u);
                        UMC_CHECK_STATUS(umcRes)
                        subPldDataLen++;
                        frameSize = value8u;

                        MediaData *pIn = new MediaData;
                        Ipp32u iES = 0;
                        umcRes = m_ppFBuffer[iES]->LockInputBuffer(pIn);
                        while (umcRes == UMC_ERR_NOT_ENOUGH_BUFFER)
                        {
                            if (m_bFlagStop) {
                                delete pIn;
                                return UMC_OK;
                            }
                            vm_time_sleep(TIME_TO_SLEEP);
                            umcRes = m_ppFBuffer[iES]->LockInputBuffer(pIn);
                        }

                        umcRes = m_pDataReader->ReadData((Ipp8u *)pIn->GetDataPointer() + stc_len, &frameSize);
                        if (umcRes != UMC_OK) {
                            delete pIn;            
                            UMC_RET_STATUS(umcRes)
                        }
                        subPldDataLen += frameSize;
                        umcRes = pIn->SetDataSize(frameSize + stc_len);
                        umcRes = pIn->SetTime((Ipp64f)framePTS * 0.001);
                        umcRes = m_ppFBuffer[iES]->UnLockInputBuffer(pIn, UMC_OK);
                        delete pIn;
                        UMC_CHECK_STATUS(umcRes)

                        framePTS += PTSdelta;
                    }
                    infoSize += subPldDataLen;
                }
            } else if (multiplePld.replDataLen >= 8) {

                umcRes = m_pDataReader->Get32uNoSwap(&frameSize);    /*** size of pld's MediaObject  ***/
                UMC_CHECK_STATUS(umcRes)
                umcRes = m_pDataReader->Get32uNoSwap(&framePTS);    /*** in millisec ***/
                UMC_CHECK_STATUS(umcRes)
                infoSize += 8;
                /*~~~ can be more data here ~~~*/
                if (multiplePld.replDataLen > 8) {
                    umcRes = m_pDataReader->MovePosition(multiplePld.replDataLen - 8);    /*** optional extension data  ***/
                    UMC_CHECK_STATUS(umcRes)
                    infoSize += multiplePld.replDataLen - 8;
                }
                umcRes = ReadDataFromType(pldLenType, multiplePld.pldLen, infoSize);
                UMC_CHECK_STATUS(umcRes)

                MediaData *pIn = new MediaData;
                Ipp32u iES = 0;
                umcRes = GetESFromPID(iES, multiplePld.streamNumber);
    //            UMC_CHECK_STATUS(umcRes)
                if ((umcRes != UMC_OK) || (!m_pInfo->m_ppTrackInfo[iES]->m_isSelected))
                {   // wrong payload
                    m_pDataReader->MovePosition(multiplePld.pldLen);
                    infoSize += multiplePld.pldLen;
                    delete pIn;  
                } else
                {
                    umcRes = m_ppFBuffer[iES]->LockInputBuffer(pIn);
                    while (umcRes == UMC_ERR_NOT_ENOUGH_BUFFER)
                    {
                        if (m_bFlagStop) {
                            delete pIn;
                            return UMC_OK;
                        }
                        vm_time_sleep(TIME_TO_SLEEP);
                        umcRes = m_ppFBuffer[iES]->LockInputBuffer(pIn);
                    }

                    umcRes = m_pDataReader->ReadData((Ipp8u *)pIn->GetDataPointer() + stc_len, &multiplePld.pldLen);
                    if (umcRes != UMC_OK) {
                        delete pIn;            
                        UMC_RET_STATUS(umcRes)
                    }

                    infoSize += multiplePld.pldLen;
                    umcRes = pIn->SetDataSize(multiplePld.pldLen + stc_len);
                    umcRes = pIn->SetTime((Ipp64f)framePTS * 0.001);
                    if (multiplePld.offIntoMediaObj + multiplePld.pldLen < frameSize)
                    {   // frame continuation
                        umcRes = m_ppFBuffer[iES]->UnLockInputBuffer(pIn, UMC_ERR_NOT_ENOUGH_DATA);
                    } else
                    {   // frame is finished
                        umcRes = m_ppFBuffer[iES]->UnLockInputBuffer(pIn, UMC_OK);
                    }
                    delete pIn;            
                    UMC_CHECK_STATUS(umcRes)
                }
            } else if (multiplePld.replDataLen != 0) {
                return UMC_ERR_FAILED;
            }

        }

        if (payloadParseInfo.paddLen) {
// read real padding data (should be set to 0)
            umcRes = m_pDataReader->MovePosition(payloadParseInfo.paddLen);
            UMC_CHECK_STATUS(umcRes)
            infoSize += payloadParseInfo.paddLen;
        }

        if (infoSize < m_pHeaderObject->pFPropObject->minDataPackSize) {
            umcRes = m_pDataReader->MovePosition(m_pHeaderObject->pFPropObject->minDataPackSize - infoSize);
            UMC_CHECK_STATUS(umcRes)
        }

    } else
    {   /*** single payload ***/
        Ipp32u frameSize = 0;
        Ipp32u framePTS = 0;
//        Ipp32u pldDataLen = 0;
        Ipp32u stc_len = 0;
        asf_Payload singlePayload;

        umcRes = m_pDataReader->Get8u(&value8u);
        UMC_CHECK_STATUS(umcRes)
        infoSize++;

        singlePayload.streamNumber = value8u &0x7F;
        singlePayload.keyFrame = value8u >> 7;

        umcRes = ReadDataFromType(payloadParseInfo.mediaObjNumLenType, singlePayload.mediaObjNum, infoSize);
        UMC_CHECK_STATUS(umcRes)
        umcRes = ReadDataFromType(payloadParseInfo.offIntoMediaObjLenType, singlePayload.offIntoMediaObj, infoSize);
        UMC_CHECK_STATUS(umcRes)
        umcRes = ReadDataFromType(payloadParseInfo.replDataLenType, singlePayload.replDataLen, infoSize);
        UMC_CHECK_STATUS(umcRes)

        if (singlePayload.replDataLen == 1) {   /*** compressed payload ***/
            return UMC_ERR_NOT_IMPLEMENTED;
        } else if (singlePayload.replDataLen >= 8) {
            umcRes = m_pDataReader->Get32uNoSwap(&frameSize);    /*** size of pld's MediaObject  ***/
            UMC_CHECK_STATUS(umcRes)
            umcRes = m_pDataReader->Get32uNoSwap(&framePTS);    /*** in millisec ***/
            UMC_CHECK_STATUS(umcRes)
            infoSize += 8;
            /*~~~ can be more data here ~~~*/
            if (singlePayload.replDataLen > 8) {
                umcRes = m_pDataReader->MovePosition(singlePayload.replDataLen - 8);    /*** optional extension data  ***/
                UMC_CHECK_STATUS(umcRes)
                infoSize += singlePayload.replDataLen - 8;
            }
        } else if (singlePayload.replDataLen != 0) {
            return UMC_ERR_FAILED;
        }

        singlePayload.pldLen = payloadParseInfo.packLen - infoSize - payloadParseInfo.paddLen;

        MediaData *pIn = new MediaData;
        Ipp32u iES = 0;
        umcRes = GetESFromPID(iES, singlePayload.streamNumber);
        if ((umcRes != UMC_OK) || (!m_pInfo->m_ppTrackInfo[iES]->m_isSelected))
        {   // wrong payload
            umcRes = m_pDataReader->MovePosition(singlePayload.pldLen);
            if (umcRes != UMC_OK) {
                delete pIn;
                UMC_RET_STATUS(umcRes)
            }
        } else
        {
            umcRes = m_ppFBuffer[iES]->LockInputBuffer(pIn);
            while (umcRes == UMC_ERR_NOT_ENOUGH_BUFFER)
            {
                if (m_bFlagStop) {
                    delete pIn;
                    return UMC_OK;
                }
                vm_time_sleep(TIME_TO_SLEEP);
                umcRes = m_ppFBuffer[iES]->LockInputBuffer(pIn);
            }

            umcRes = m_pDataReader->ReadData((Ipp8u *)pIn->GetDataPointer() + stc_len, &singlePayload.pldLen);
            if (umcRes != UMC_OK) {
                delete pIn;
                UMC_RET_STATUS(umcRes)
            }
            umcRes = pIn->SetDataSize(singlePayload.pldLen + stc_len);
            umcRes = pIn->SetTime((Ipp64f)framePTS * 0.001);

            if (singlePayload.offIntoMediaObj + singlePayload.pldLen < frameSize)
            {   // frame continuation
                umcRes = m_ppFBuffer[iES]->UnLockInputBuffer(pIn, UMC_ERR_NOT_ENOUGH_DATA);
            } else
            {   // frame is finished
                umcRes = m_ppFBuffer[iES]->UnLockInputBuffer(pIn, UMC_OK);
            }
            if (umcRes != UMC_OK) {
                delete pIn;
                UMC_RET_STATUS(umcRes)
            }
        }

        delete pIn;
        if (payloadParseInfo.paddLen) {
            umcRes = m_pDataReader->MovePosition(payloadParseInfo.paddLen);
            UMC_CHECK_STATUS(umcRes)
        }

    }

    return umcRes;
}

Status ASFSplitter::ReadDataObject()
{
    Status umcRes = UMC_OK;
    asf_Object  curObject;
    Ipp32u lenGUID = sizeof(asf_GUID);
//    Ipp32u nPack = 0;

    umcRes = ReadNextObject(&curObject);
    UMC_CHECK_STATUS(umcRes)

    if (curObject.objectID != ASF_Data_Object) {
      return UMC_ERR_INVALID_STREAM;
    }

    UMC_NEW (m_pDataObject, asf_DataObject);
    m_pDataObject->objectID = ASF_Data_Object;
    m_pDataObject->objectSize = curObject.size;

    umcRes = m_pDataReader->GetData(&m_pDataObject->fileID, &lenGUID);
    UMC_CHECK_STATUS(umcRes)
    if (m_pDataObject->fileID != m_pHeaderObject->pFPropObject->fileID) {
      return UMC_ERR_INVALID_STREAM;
    }
    umcRes = m_pDataReader->Get64uNoSwap(&m_pDataObject->totalDataPackets);
    UMC_CHECK_STATUS(umcRes)
    umcRes = m_pDataReader->MovePosition(2); /* 2 reserved bytes */
//  UMC_CHECK_STATUS(umcRes)

    return umcRes;
}

Status ASFSplitter::FillAudioMediaInfo(asf_AudioMediaInfo *pAudioMedia)
{
    Status umcRes = UMC_OK;
    Ipp32u len = 0;

    umcRes = m_pDataReader->Get16uNoSwap(&pAudioMedia->formatTag);
    UMC_CHECK_STATUS(umcRes)
    umcRes = m_pDataReader->Get16uNoSwap(&pAudioMedia->numChannels);
    UMC_CHECK_STATUS(umcRes)
    umcRes = m_pDataReader->Get32uNoSwap(&pAudioMedia->sampleRate);
    UMC_CHECK_STATUS(umcRes)
    umcRes = m_pDataReader->Get32uNoSwap(&pAudioMedia->avgBytesPerSec);
    UMC_CHECK_STATUS(umcRes)
    umcRes = m_pDataReader->Get16uNoSwap(&pAudioMedia->blockAlign);
    UMC_CHECK_STATUS(umcRes)
    umcRes = m_pDataReader->Get16uNoSwap(&pAudioMedia->bitsPerSample);
    UMC_CHECK_STATUS(umcRes)
    umcRes = m_pDataReader->Get16uNoSwap(&pAudioMedia->codecSpecDataSize);
    UMC_CHECK_STATUS(umcRes)
    if (pAudioMedia->codecSpecDataSize) {
        len = pAudioMedia->codecSpecDataSize;
        UMC_ALLOC_ARR(pAudioMedia->pCodecSpecData, Ipp8u, len);
        umcRes = m_pDataReader->GetData(pAudioMedia->pCodecSpecData, &len);
        UMC_CHECK_STATUS(umcRes)
    } else
    {
        pAudioMedia->pCodecSpecData = NULL;
    }

    return umcRes;
}

Status ASFSplitter::FillVideoMediaInfo(asf_VideoMediaInfo *pVideoSpecData)
{
    Status umcRes = UMC_OK;
    Ipp32u len = 0;

    umcRes = m_pDataReader->Get32uNoSwap(&pVideoSpecData->width);
    UMC_CHECK_STATUS(umcRes)
    umcRes = m_pDataReader->Get32uNoSwap(&pVideoSpecData->height);
    UMC_CHECK_STATUS(umcRes)
    umcRes = m_pDataReader->MovePosition(1);
    UMC_CHECK_STATUS(umcRes)
    umcRes = m_pDataReader->Get16uNoSwap(&pVideoSpecData->formatDataSize);
    UMC_CHECK_STATUS(umcRes)
    asf_FormatData *pImgFormatData = &pVideoSpecData->FormatData;
    umcRes = m_pDataReader->Get32uNoSwap(&pImgFormatData->formatDataSize);
    UMC_CHECK_STATUS(umcRes)
    umcRes = m_pDataReader->Get32uNoSwap(&pImgFormatData->width);
    UMC_CHECK_STATUS(umcRes)
    umcRes = m_pDataReader->Get32uNoSwap(&pImgFormatData->height);
    UMC_CHECK_STATUS(umcRes)
    umcRes = m_pDataReader->MovePosition(2);  /*** reserved 32bits ***/
    UMC_CHECK_STATUS(umcRes)
    umcRes = m_pDataReader->Get16uNoSwap(&pImgFormatData->bitsPerPixelCount);
    UMC_CHECK_STATUS(umcRes)
    umcRes = m_pDataReader->Get32uNoSwap(&pImgFormatData->compresID);
    UMC_CHECK_STATUS(umcRes)
    umcRes = m_pDataReader->Get32uNoSwap(&pImgFormatData->imgSize);
    UMC_CHECK_STATUS(umcRes)
    umcRes = m_pDataReader->Get32uNoSwap(&pImgFormatData->hrzPixelsPerMeter);
    UMC_CHECK_STATUS(umcRes)
    umcRes = m_pDataReader->Get32uNoSwap(&pImgFormatData->vertPixelsPerMeter);
    UMC_CHECK_STATUS(umcRes)
    umcRes = m_pDataReader->Get32uNoSwap(&pImgFormatData->colorsUsedCount);
    UMC_CHECK_STATUS(umcRes)
    umcRes = m_pDataReader->Get32uNoSwap(&pImgFormatData->importColorsCount);
    UMC_CHECK_STATUS(umcRes)
    if (pImgFormatData->formatDataSize > VIDEO_SPEC_DATA_LEN) {
        len = pImgFormatData->formatDataSize - VIDEO_SPEC_DATA_LEN;
        UMC_ALLOC_ARR(pImgFormatData->pCodecSpecData, Ipp8u, len);
        umcRes = m_pDataReader->GetData(pImgFormatData->pCodecSpecData, &len);
        UMC_CHECK_STATUS(umcRes)
    } else
    {
        pImgFormatData->pCodecSpecData = NULL;
    }
    return umcRes;
}

Status ASFSplitter::FillSpreadAudioData(asf_SpreadAudioData *pSpreadAudioData)
{
    Status umcRes = UMC_OK;
    Ipp32u len = 0;

    umcRes = m_pDataReader->Get8u(&pSpreadAudioData->span);
    UMC_CHECK_STATUS(umcRes)
    umcRes = m_pDataReader->Get16uNoSwap(&pSpreadAudioData->virtPackLen);
    UMC_CHECK_STATUS(umcRes)
    umcRes = m_pDataReader->Get16uNoSwap(&pSpreadAudioData->virtChunkLen);
    UMC_CHECK_STATUS(umcRes)
    umcRes = m_pDataReader->Get16uNoSwap(&pSpreadAudioData->silenceDataLen);
    UMC_CHECK_STATUS(umcRes)
    if (pSpreadAudioData->silenceDataLen) {
        len = pSpreadAudioData->silenceDataLen;
        UMC_ALLOC_ARR(pSpreadAudioData->pSilenceData, Ipp8u, len);
        umcRes = m_pDataReader->GetData(pSpreadAudioData->pSilenceData, &len);
        UMC_CHECK_STATUS(umcRes)
    } else
    {
        pSpreadAudioData->pSilenceData = NULL;
    }
    return umcRes;
}

Status ASFSplitter::ReadBitrateMutualExclObject()
{
  Status umcRes = UMC_OK;
  Ipp32u i = 0;
  asf_BitrateMutualExclObject *pBRMutExclObj = m_pHeaderObject->pBitrateMutualExclObject;
  Ipp32u lenGUID = sizeof(asf_GUID);

  umcRes = m_pDataReader->GetData(&pBRMutExclObj->exclusionType, &lenGUID);
  UMC_CHECK_STATUS(umcRes)
  umcRes = m_pDataReader->Get16uNoSwap(&pBRMutExclObj->streamNumbersCount);
  UMC_CHECK_STATUS(umcRes)

  if (pBRMutExclObj->streamNumbersCount) {
    UMC_NEW_ARR(pBRMutExclObj->pStreamNumbers, Ipp16u, pBRMutExclObj->streamNumbersCount)
    for (i = 0; i < pBRMutExclObj->streamNumbersCount; i++) {
      umcRes = m_pDataReader->Get16uNoSwap(&pBRMutExclObj->pStreamNumbers[i]);
      UMC_CHECK_STATUS(umcRes)
    }
  }

  return umcRes;
}

Status ASFSplitter::ReadErrCorrectObject()
{
  Status umcRes = UMC_OK;
  asf_ErrCorrectObject *pErrCorrObj = m_pHeaderObject->pErrCorrectObject;
  Ipp32u lenGUID = sizeof(asf_GUID);

  umcRes = m_pDataReader->GetData(&pErrCorrObj->errCorrectType, &lenGUID);
  UMC_CHECK_STATUS(umcRes)
  umcRes = m_pDataReader->Get32uNoSwap(&pErrCorrObj->errCorrectDataLen);
  UMC_CHECK_STATUS(umcRes)

  if (pErrCorrObj->errCorrectDataLen) {
    UMC_ALLOC_ZERO_ARR(pErrCorrObj->pErrCorrectData, Ipp8u, pErrCorrObj->errCorrectDataLen)
    umcRes = m_pDataReader->GetData(&pErrCorrObj->pErrCorrectData, &pErrCorrObj->errCorrectDataLen);
    UMC_CHECK_STATUS(umcRes)
  }

  return umcRes;
}

Status ASFSplitter::ReadStreamBitratePropObject()
{
  Status umcRes = UMC_OK;
  Ipp32u i = 0;
  asf_StreamBitratePropObject *pStreamBRObj = m_pHeaderObject->pStreamBitratePropObject;

  umcRes = m_pDataReader->Get16uNoSwap(&pStreamBRObj->bitrateRecordsCount);
  UMC_CHECK_STATUS(umcRes)

  if (pStreamBRObj->bitrateRecordsCount) {
    UMC_NEW_ARR(pStreamBRObj->pBitrateRecords, asf_BitrateRecord, pStreamBRObj->bitrateRecordsCount)
    for (i = 0; i < pStreamBRObj->bitrateRecordsCount; i++) {
      asf_BitrateRecord *pCurRec = &pStreamBRObj->pBitrateRecords[i];
      umcRes = m_pDataReader->Get16uNoSwap(&pCurRec->flags);
      UMC_CHECK_STATUS(umcRes)
      umcRes = m_pDataReader->Get32uNoSwap(&pCurRec->averageBitrate);
      UMC_CHECK_STATUS(umcRes)
    }
  }

  return umcRes;
}

Status ASFSplitter::ReadLangListObject()
{
    return UMC_OK;
}

Status ASFSplitter::ReadMetadataObject()
{
    return UMC_OK;
}

Status ASFSplitter::ReadPaddingObject()
{
    return UMC_OK;
}

Status ASFSplitter::ReadMutualExclObject()
{
    return UMC_OK;
}

Status ASFSplitter::ReadStrPrioritObject()
{
    return UMC_OK;
}

Status ASFSplitter::ReadIndxParamsObject()
{
    return UMC_OK;
}

Status ASFSplitter::ReadCodecListObject()
{
  Status umcRes = UMC_OK;
  asf_CodecListObject *pCodecListObj = m_pHeaderObject->pCodecListObject;
  Ipp32u i = 0, j = 0, len = 0;
  umcRes = m_pDataReader->MovePosition(16); /*** reserved GUID  ***/
  UMC_CHECK_STATUS(umcRes)
  umcRes = m_pDataReader->Get32uNoSwap(&pCodecListObj->codecEntriesCount);
  UMC_CHECK_STATUS(umcRes)

  UMC_NEW_ARR(pCodecListObj->pCodecEntries, asf_CodecEntry, pCodecListObj->codecEntriesCount);
  for (i = 0; i < pCodecListObj->codecEntriesCount; i++) {
    asf_CodecEntry *pCurCodecEntry = &pCodecListObj->pCodecEntries[i];
    umcRes = m_pDataReader->Get16uNoSwap(&pCurCodecEntry->type);
    UMC_CHECK_STATUS(umcRes)
    umcRes = m_pDataReader->Get16uNoSwap(&pCurCodecEntry->codecNameLen);
    UMC_CHECK_STATUS(umcRes)
    if (pCurCodecEntry->codecNameLen) {
      UMC_NEW_ARR(pCurCodecEntry->pCodecName, Ipp16u ,pCurCodecEntry->codecNameLen);
      for (j = 0; j < pCurCodecEntry->codecNameLen; j++) {
        umcRes = m_pDataReader->Get16uNoSwap(&pCurCodecEntry->pCodecName[j]);
        UMC_CHECK_STATUS(umcRes)
      }
    }
    umcRes = m_pDataReader->Get16uNoSwap(&pCurCodecEntry->codecDescrLen);
    UMC_CHECK_STATUS(umcRes)
    if (pCurCodecEntry->codecDescrLen) {
      UMC_NEW_ARR(pCurCodecEntry->pCodecDescription, Ipp16u, pCurCodecEntry->codecDescrLen);
      UMC_CHECK_PTR(pCurCodecEntry->pCodecDescription)
      for (j = 0; j < pCurCodecEntry->codecDescrLen; j++) {
        umcRes = m_pDataReader->Get16uNoSwap(&pCurCodecEntry->pCodecDescription[j]);
        UMC_CHECK_STATUS(umcRes)
      }
    }
    umcRes = m_pDataReader->Get16uNoSwap(&pCurCodecEntry->codecInfoLen);
    UMC_CHECK_STATUS(umcRes)
    if (pCurCodecEntry->codecInfoLen) {
      UMC_NEW_ARR(pCurCodecEntry->pCodecInfo, Ipp8u, pCurCodecEntry->codecInfoLen);
      UMC_CHECK_PTR(pCurCodecEntry->pCodecInfo)
      len = pCurCodecEntry->codecInfoLen;
      umcRes = m_pDataReader->GetData(pCurCodecEntry->pCodecInfo, &len);
      UMC_CHECK_STATUS(umcRes)
    }
  }

  return umcRes;
}

Status ASFSplitter::ReadHeaderExtData(Ipp32u dataSize)
{
  Status umcRes = UMC_OK;
  asf_Object curObj;
  Ipp32u iExtStrProp = 0;
  asf_HeaderExtData *pHeadExtData = m_pHeaderObject->pHeaderExtObject->pHeaderExtData;

  Ipp64u extDataStartPos = m_pDataReader->GetPosition();

  memset(pHeadExtData->ppExtStreamPropObject, 0, ASF_MAX_NUM_OF_STREAMS);
  memset(&curObj, 0, sizeof(curObj));
  while (curObj.endPos < extDataStartPos + dataSize) {
    umcRes = ReadNextObject(&curObj);
    UMC_CHECK_STATUS(umcRes)
    if (curObj.objectID == ASF_Extended_Stream_Properties_Object) {
      UMC_NEW(pHeadExtData->ppExtStreamPropObject[iExtStrProp], asf_ExtStreamPropObject);
      pHeadExtData->ppExtStreamPropObject[iExtStrProp]->objectID = ASF_Extended_Stream_Properties_Object;
      pHeadExtData->ppExtStreamPropObject[iExtStrProp]->objectSize = curObj.size;
      umcRes = ReadExtStreamPropObject(pHeadExtData->ppExtStreamPropObject[iExtStrProp]);
      UMC_CHECK_STATUS(umcRes)
      iExtStrProp++;
    } else
    if (curObj.objectID == ASF_Language_List_Object) {
      umcRes = ReadLangListObject();
      UMC_CHECK_STATUS(umcRes)
    } else
    if (curObj.objectID == ASF_Metadata_Object) {
      umcRes = ReadMetadataObject();
      UMC_CHECK_STATUS(umcRes)
    } else
    if (curObj.objectID == ASF_Padding_Object) {
      umcRes = ReadPaddingObject();
      UMC_CHECK_STATUS(umcRes)
    } else
    if (curObj.objectID == ASF_Advanced_Mutual_Exclusion_Object) {
      umcRes = ReadMutualExclObject();
      UMC_CHECK_STATUS(umcRes)
    } else
    if (curObj.objectID == ASF_Stream_Prioritization_Object) {
      umcRes = ReadStrPrioritObject();
      UMC_CHECK_STATUS(umcRes)
    } else
    if (curObj.objectID == ASF_Index_Parameters_Object) {
      umcRes = ReadIndxParamsObject();
      UMC_CHECK_STATUS(umcRes)
    } else
    if (curObj.objectID == ASF_Stream_Properties_Object) {
    }
    umcRes = m_pDataReader->SetPosition(curObj.endPos);
    UMC_CHECK_STATUS(umcRes)
  }

  return umcRes;
}

Status ASFSplitter::ReadHeaderExtObject()
{
  Status umcRes = UMC_OK;
  asf_HeaderExtObject *pHeadExtObj = m_pHeaderObject->pHeaderExtObject;

  umcRes = m_pDataReader->MovePosition(18); /*** 128:ASF_Reserved_1, 16:0x6   ***/
  UMC_CHECK_STATUS(umcRes)
  umcRes = m_pDataReader->Get32uNoSwap(&pHeadExtObj->headerExtDataSize);
  UMC_CHECK_STATUS(umcRes)
  if (pHeadExtObj->headerExtDataSize) {
    pHeadExtObj->pHeaderExtData = new asf_HeaderExtData;
    memset(pHeadExtObj->pHeaderExtData, 0, sizeof(asf_HeaderExtData));
    umcRes = ReadHeaderExtData(pHeadExtObj->headerExtDataSize);
    UMC_CHECK_STATUS(umcRes)
  }

  return umcRes;
}

Status ASFSplitter::ReadExtStreamPropObject(asf_ExtStreamPropObject *pExtStrProp)
{
  Status umcRes = UMC_OK;

  umcRes = m_pDataReader->Get64uNoSwap(&pExtStrProp->startTime);
  UMC_CHECK_STATUS(umcRes)
  umcRes = m_pDataReader->Get64uNoSwap(&pExtStrProp->endTime);
  UMC_CHECK_STATUS(umcRes)
  umcRes = m_pDataReader->Get32uNoSwap(&pExtStrProp->dataBitrate);
  UMC_CHECK_STATUS(umcRes)
  umcRes = m_pDataReader->Get32uNoSwap(&pExtStrProp->bufSize);
  UMC_CHECK_STATUS(umcRes)
  umcRes = m_pDataReader->Get32uNoSwap(&pExtStrProp->initialBufFullness);
  UMC_CHECK_STATUS(umcRes)
  umcRes = m_pDataReader->Get32uNoSwap(&pExtStrProp->altDataBitrate);
  UMC_CHECK_STATUS(umcRes)
  umcRes = m_pDataReader->Get32uNoSwap(&pExtStrProp->altBufSize);
  UMC_CHECK_STATUS(umcRes)
  umcRes = m_pDataReader->Get32uNoSwap(&pExtStrProp->altInitialBufFullness);
  UMC_CHECK_STATUS(umcRes)
  umcRes = m_pDataReader->Get32uNoSwap(&pExtStrProp->maxObjSize);
  UMC_CHECK_STATUS(umcRes)
  umcRes = m_pDataReader->Get32uNoSwap(&pExtStrProp->flags);
  UMC_CHECK_STATUS(umcRes)
  umcRes = m_pDataReader->Get16uNoSwap(&pExtStrProp->streamNum);
  UMC_CHECK_STATUS(umcRes)
  umcRes = m_pDataReader->Get16uNoSwap(&pExtStrProp->streamLangIDIndex);
  UMC_CHECK_STATUS(umcRes)
  umcRes = m_pDataReader->Get64uNoSwap(&pExtStrProp->avgTimePerFrame);
  UMC_CHECK_STATUS(umcRes)
  umcRes = m_pDataReader->Get16uNoSwap(&pExtStrProp->streamNameCount);
  UMC_CHECK_STATUS(umcRes)
  umcRes = m_pDataReader->Get16uNoSwap(&pExtStrProp->payldExtSysCount);
  UMC_CHECK_STATUS(umcRes)

  return umcRes;
}

Status ASFSplitter::ReadStreamPropObject(Ipp32u strNum)
{
  Status umcRes = UMC_OK;
  Ipp32u lenGUID = sizeof(asf_GUID);
  Ipp64u endPos = 0;
  asf_StreamPropObject *pStrPropObj = m_pHeaderObject->ppStreamPropObject[strNum];

  umcRes = m_pDataReader->GetData(&pStrPropObj->streamType, &lenGUID);
  UMC_CHECK_STATUS(umcRes)
  umcRes = m_pDataReader->GetData(&pStrPropObj->errCorrectType, &lenGUID);
  UMC_CHECK_STATUS(umcRes)
  umcRes = m_pDataReader->Get64uNoSwap(&pStrPropObj->timeOffset);
  UMC_CHECK_STATUS(umcRes)
  umcRes = m_pDataReader->Get32uNoSwap(&pStrPropObj->typeSpecDataLen);
  UMC_CHECK_STATUS(umcRes)
  umcRes = m_pDataReader->Get32uNoSwap(&pStrPropObj->errCorrectDataLen);
  UMC_CHECK_STATUS(umcRes)
  umcRes = m_pDataReader->Get16uNoSwap(&pStrPropObj->flags);
  UMC_CHECK_STATUS(umcRes)
  umcRes = m_pDataReader->MovePosition(4);  /*** reserved 32bits ***/
  UMC_CHECK_STATUS(umcRes)
  endPos = m_pDataReader->GetPosition() + pStrPropObj->typeSpecDataLen;
  if (pStrPropObj->streamType == ASF_Audio_Media)
  {
    UMC_NEW(pStrPropObj->typeSpecData.pAudioSpecData, asf_AudioMediaInfo);
    umcRes = FillAudioMediaInfo(pStrPropObj->typeSpecData.pAudioSpecData);
    UMC_CHECK_STATUS(umcRes)
  } else
  if (pStrPropObj->streamType == ASF_Video_Media) {
    UMC_NEW(pStrPropObj->typeSpecData.pVideoSpecData, asf_VideoMediaInfo);
    umcRes = FillVideoMediaInfo(pStrPropObj->typeSpecData.pVideoSpecData);
    UMC_CHECK_STATUS(umcRes)
  } else
  if (pStrPropObj->streamType == ASF_Command_Media) {
    pStrPropObj->typeSpecData.pAnyData = NULL;
  } else
  if (pStrPropObj->streamType == ASF_JFIF_Media) {
    pStrPropObj->typeSpecData.pAnyData = NULL;
  } else
  if (pStrPropObj->streamType == ASF_Degradable_JPEG_Media) {
    pStrPropObj->typeSpecData.pAnyData = NULL;
  } else
  if (pStrPropObj->streamType == ASF_File_Transfer_Media) {
    pStrPropObj->typeSpecData.pAnyData = NULL;
  } else
  if (pStrPropObj->streamType == ASF_Binary_Media) {
    pStrPropObj->typeSpecData.pAnyData = NULL;
  } else {
    return UMC_ERR_INVALID_STREAM;
  }
  umcRes = m_pDataReader->SetPosition(endPos);
  UMC_CHECK_STATUS(umcRes)

  if ((pStrPropObj->errCorrectType == ASF_No_Error_Correction) ||
      (pStrPropObj->errCorrectType == ASF_Signature_Audio_Correction)) {
  } else
  if (pStrPropObj->errCorrectType == ASF_Audio_Spread) {
    UMC_NEW(pStrPropObj->pErrCorrectData, asf_SpreadAudioData);
    asf_SpreadAudioData *pSpreadAudioData = (asf_SpreadAudioData *)pStrPropObj->pErrCorrectData;
    umcRes = FillSpreadAudioData(pSpreadAudioData);
    UMC_CHECK_STATUS(umcRes)
  } else {
    return UMC_ERR_INVALID_STREAM;
  }

  return umcRes;
}

Status ASFSplitter::ReadFPropObject()
{
  Status umcRes = UMC_OK;
  Ipp32u lenGUID = sizeof(asf_GUID);
  asf_FPropObject *pFPropObj = m_pHeaderObject->pFPropObject;

  umcRes = m_pDataReader->GetData(&pFPropObj->fileID, &lenGUID);
  UMC_CHECK_STATUS(umcRes)
  umcRes = m_pDataReader->Get64uNoSwap(&pFPropObj->fileSize);
  UMC_CHECK_STATUS(umcRes)
  umcRes = m_pDataReader->Get64uNoSwap(&pFPropObj->creationDate);
  UMC_CHECK_STATUS(umcRes)
  umcRes = m_pDataReader->Get64uNoSwap(&pFPropObj->dataPacketsCount);
  UMC_CHECK_STATUS(umcRes)
  umcRes = m_pDataReader->Get64uNoSwap(&pFPropObj->playDuration);
  UMC_CHECK_STATUS(umcRes)
  umcRes = m_pDataReader->Get64uNoSwap(&pFPropObj->sendDuration);
  UMC_CHECK_STATUS(umcRes)
  umcRes = m_pDataReader->Get64uNoSwap(&pFPropObj->preroll);
  UMC_CHECK_STATUS(umcRes)
  umcRes = m_pDataReader->Get32uNoSwap(&pFPropObj->flags);
  UMC_CHECK_STATUS(umcRes)
  umcRes = m_pDataReader->Get32uNoSwap(&pFPropObj->minDataPackSize);
  UMC_CHECK_STATUS(umcRes)
  umcRes = m_pDataReader->Get32uNoSwap(&pFPropObj->maxDataPackSize);
  UMC_CHECK_STATUS(umcRes)
  umcRes = m_pDataReader->Get32uNoSwap(&pFPropObj->maxBitrate);
  UMC_CHECK_STATUS(umcRes)
  return umcRes;
}

Status ASFSplitter::CleanHeaderObject()
{
    Ipp32u i = 0;

    if (m_pHeaderObject) {

        if (m_pHeaderObject->pErrCorrectObject)
        {
            if (m_pHeaderObject->pErrCorrectObject->errCorrectDataLen)
                UMC_FREE(m_pHeaderObject->pErrCorrectObject->pErrCorrectData)
            UMC_DELETE(m_pHeaderObject->pErrCorrectObject);
        }

        if (m_pHeaderObject->pBitrateMutualExclObject)
        {
            if (m_pHeaderObject->pBitrateMutualExclObject->streamNumbersCount)
                UMC_DELETE_ARR(m_pHeaderObject->pBitrateMutualExclObject->pStreamNumbers);
            UMC_DELETE(m_pHeaderObject->pBitrateMutualExclObject);
        }

        if (m_pHeaderObject->pStreamBitratePropObject)
        {
            if (m_pHeaderObject->pStreamBitratePropObject->bitrateRecordsCount)
                UMC_DELETE_ARR(m_pHeaderObject->pStreamBitratePropObject->pBitrateRecords);
            UMC_DELETE(m_pHeaderObject->pStreamBitratePropObject);
        }

        if (m_pHeaderObject->pCodecListObject)
        {
            for (i = 0; i < m_pHeaderObject->pCodecListObject->codecEntriesCount; i++)
            {
                UMC_DELETE_ARR(m_pHeaderObject->pCodecListObject->pCodecEntries[i].pCodecName);
                UMC_DELETE_ARR(m_pHeaderObject->pCodecListObject->pCodecEntries[i].pCodecDescription);
                UMC_DELETE_ARR(m_pHeaderObject->pCodecListObject->pCodecEntries[i].pCodecInfo);
            }
            UMC_DELETE_ARR(m_pHeaderObject->pCodecListObject->pCodecEntries);
            UMC_DELETE(m_pHeaderObject->pCodecListObject);
        }

        if (m_pHeaderObject->ppStreamPropObject)
        {
            for (i = 0; i < m_pInfo->m_nOfTracks; i++)
            {
                if (m_pHeaderObject->ppStreamPropObject[i] == 0)
                    continue;

                if (m_pHeaderObject->ppStreamPropObject[i]->streamType == ASF_Audio_Media)
                {
                    UMC_FREE(m_pHeaderObject->ppStreamPropObject[i]->typeSpecData.pAudioSpecData->pCodecSpecData)
                    UMC_DELETE(m_pHeaderObject->ppStreamPropObject[i]->typeSpecData.pAudioSpecData);
                } else
                if (m_pHeaderObject->ppStreamPropObject[i]->streamType == ASF_Video_Media)
                {
                    UMC_FREE(m_pHeaderObject->ppStreamPropObject[i]->typeSpecData.pVideoSpecData->FormatData.pCodecSpecData)
                    UMC_DELETE(m_pHeaderObject->ppStreamPropObject[i]->typeSpecData.pVideoSpecData);
                }

                if (m_pHeaderObject->ppStreamPropObject[i]->errCorrectType == ASF_Audio_Spread)
                {
                    UMC_FREE(m_pHeaderObject->ppStreamPropObject[i]->pErrCorrectData->pSilenceData)
                }

                UMC_DELETE(m_pHeaderObject->ppStreamPropObject[i]->pErrCorrectData);
                UMC_DELETE(m_pHeaderObject->ppStreamPropObject[i]);
            }
        }

        if (m_pHeaderObject->pHeaderExtObject)
        {
            if (m_pHeaderObject->pHeaderExtObject->headerExtDataSize)
            {
                for (i = 0; i < ASF_MAX_NUM_OF_STREAMS; i++)
                    UMC_DELETE(m_pHeaderObject->pHeaderExtObject->pHeaderExtData->ppExtStreamPropObject[i]);
                UMC_DELETE(m_pHeaderObject->pHeaderExtObject->pHeaderExtData);
            }
        }

        UMC_DELETE(m_pHeaderObject->pHeaderExtObject);
        UMC_DELETE(m_pHeaderObject->pFPropObject);
        UMC_FREE(m_pHeaderObject->ppStreamPropObject);
    }

    return UMC_OK;
}

Status ASFSplitter::ReadHeaderObject()
{
  Status umcRes = UMC_OK;
  asf_Object  curObject;
  Ipp32u nObj = 0;
  bool  existFPropObj = false;
  bool  existHeadExtObj = false;
  bool  existStreamPropObj = false;

  umcRes = m_pDataReader->SetPosition((Ipp64u)0);
  UMC_CHECK_STATUS(umcRes)
  umcRes = ReadNextObject(&curObject);
  UMC_CHECK_STATUS(umcRes)

  if (curObject.objectID != ASF_Header_Object) {
    return UMC_ERR_INVALID_STREAM;
  }

  m_pHeaderObject = new asf_HeaderObject;
  m_pHeaderObject->objectID = ASF_Header_Object;
  m_pHeaderObject->objectSize = curObject.size;

  umcRes = m_pDataReader->Get32uNoSwap(&m_pHeaderObject->nObjects);
  UMC_CHECK_STATUS(umcRes)
  umcRes = m_pDataReader->MovePosition(2); /* 2 reserved bytes */
  UMC_CHECK_STATUS(umcRes)

  UMC_ALLOC_ZERO_ARR(m_pHeaderObject->ppStreamPropObject, asf_StreamPropObject*, m_pInfo->m_nOfTracks)

  Ipp32u streamNum = 0;
  for (nObj = 0; nObj < m_pHeaderObject->nObjects; nObj++) {
    umcRes = ReadNextObject(&curObject);
    UMC_CHECK_STATUS(umcRes)
    if (curObject.objectID == ASF_File_Properties_Object) {
      UMC_NEW(m_pHeaderObject->pFPropObject, asf_FPropObject);
      m_pHeaderObject->pFPropObject->objectID = ASF_File_Properties_Object;
      m_pHeaderObject->pFPropObject->objectSize = curObject.size;
      umcRes = ReadFPropObject();
      UMC_CHECK_STATUS(umcRes)
      existFPropObj = true;
    } else
    if (curObject.objectID == ASF_Header_Extension_Object) {
      UMC_NEW(m_pHeaderObject->pHeaderExtObject, asf_HeaderExtObject);
      m_pHeaderObject->pHeaderExtObject->objectID = ASF_Header_Extension_Object;
      m_pHeaderObject->pHeaderExtObject->objectSize = curObject.size;
      umcRes = ReadHeaderExtObject();
      UMC_CHECK_STATUS(umcRes)
      existHeadExtObj = true;
    } else
    if (curObject.objectID == ASF_Stream_Properties_Object) {
      UMC_NEW(m_pHeaderObject->ppStreamPropObject[streamNum], asf_StreamPropObject);
      m_pHeaderObject->ppStreamPropObject[streamNum]->objectID = ASF_Stream_Properties_Object;
      m_pHeaderObject->ppStreamPropObject[streamNum]->objectSize = curObject.size;
      umcRes = ReadStreamPropObject(streamNum);
      UMC_CHECK_STATUS(umcRes)
      streamNum++;
      existStreamPropObj = true;
    } else
    if (curObject.objectID == ASF_Codec_List_Object) {
      UMC_NEW(m_pHeaderObject->pCodecListObject, asf_CodecListObject);
      m_pHeaderObject->pCodecListObject->objectID = ASF_Codec_List_Object;
      m_pHeaderObject->pCodecListObject->objectSize = curObject.size;
      umcRes = ReadCodecListObject();
      UMC_CHECK_STATUS(umcRes)
    } else
    if (curObject.objectID == ASF_Stream_Bitrate_Properties_Object) {
      UMC_NEW(m_pHeaderObject->pStreamBitratePropObject, asf_StreamBitratePropObject);
      m_pHeaderObject->pStreamBitratePropObject->objectID = ASF_Stream_Properties_Object;
      m_pHeaderObject->pStreamBitratePropObject->objectSize = curObject.size;
      umcRes = ReadStreamBitratePropObject();
      UMC_CHECK_STATUS(umcRes)
    } else
    if (curObject.objectID == ASF_Bitrate_Mutual_Exclusion_Object) {
      UMC_NEW(m_pHeaderObject->pBitrateMutualExclObject, asf_BitrateMutualExclObject);
      m_pHeaderObject->pBitrateMutualExclObject->objectID = ASF_Bitrate_Mutual_Exclusion_Object;
      m_pHeaderObject->pBitrateMutualExclObject->objectSize = curObject.size;
      umcRes = ReadBitrateMutualExclObject();
      UMC_CHECK_STATUS(umcRes)
    } else
    if (curObject.objectID == ASF_Error_Correction_Object) {
      UMC_NEW(m_pHeaderObject->pErrCorrectObject, asf_ErrCorrectObject);
      m_pHeaderObject->pErrCorrectObject->objectID = ASF_Error_Correction_Object;
      m_pHeaderObject->pErrCorrectObject->objectSize = curObject.size;
      umcRes = ReadErrCorrectObject();
      UMC_CHECK_STATUS(umcRes)
    }
    umcRes = m_pDataReader->SetPosition(curObject.endPos);
    UMC_CHECK_STATUS(umcRes)
  }

  return (existFPropObj && existHeadExtObj && existStreamPropObj) ? UMC_OK : UMC_ERR_INVALID_STREAM;
}

Status ASFSplitter::ReadNextObject(asf_Object *pObj)
{
  Status umcRes = UMC_OK;
  Ipp32u lenGUID = sizeof(asf_GUID);

  pObj->startPos = m_pDataReader->GetPosition();
  umcRes = m_pDataReader->GetData(&pObj->objectID, &lenGUID);
  UMC_CHECK_STATUS(umcRes)

  umcRes = m_pDataReader->Get64uNoSwap(&pObj->size);
  UMC_CHECK_STATUS(umcRes)

  pObj->endPos = pObj->startPos + pObj->size;

  return umcRes;
}

} // namespace UMC
