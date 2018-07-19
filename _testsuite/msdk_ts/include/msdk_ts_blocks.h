// EACH block should be declared here
#ifndef msdk_ts_DECLARE_BLOCK
    //break build
    #error improper usage of blocks declaration header
    #define msdk_ts_DECLARE_BLOCK(name) 
#endif

/* Global Functions */
msdk_ts_DECLARE_BLOCK(b_MFXInit);
msdk_ts_DECLARE_BLOCK(b_MFXClose);
msdk_ts_DECLARE_BLOCK(b_MFXQueryIMPL);
msdk_ts_DECLARE_BLOCK(b_MFXQueryVersion);
msdk_ts_DECLARE_BLOCK(b_MFXJoinSession);
msdk_ts_DECLARE_BLOCK(b_MFXDisjoinSession);
msdk_ts_DECLARE_BLOCK(b_MFXCloneSession);
msdk_ts_DECLARE_BLOCK(b_MFXSetPriority);
msdk_ts_DECLARE_BLOCK(b_MFXGetPriority);

/* VideoCORE */
msdk_ts_DECLARE_BLOCK(b_MFXVideoCORE_SetBufferAllocator);
msdk_ts_DECLARE_BLOCK(b_MFXVideoCORE_SetFrameAllocator);
msdk_ts_DECLARE_BLOCK(b_MFXVideoCORE_SetHandle);
msdk_ts_DECLARE_BLOCK(b_MFXVideoCORE_GetHandle);
msdk_ts_DECLARE_BLOCK(b_MFXVideoCORE_SyncOperation);

/* VideoENCODE */
msdk_ts_DECLARE_BLOCK(b_MFXVideoENCODE_Query);
msdk_ts_DECLARE_BLOCK(b_MFXVideoENCODE_QueryIOSurf);
msdk_ts_DECLARE_BLOCK(b_MFXVideoENCODE_Init);
msdk_ts_DECLARE_BLOCK(b_MFXVideoENCODE_Reset);
msdk_ts_DECLARE_BLOCK(b_MFXVideoENCODE_Close);

msdk_ts_DECLARE_BLOCK(b_MFXVideoENCODE_GetVideoParam);
msdk_ts_DECLARE_BLOCK(b_MFXVideoENCODE_GetEncodeStat);
msdk_ts_DECLARE_BLOCK(b_MFXVideoENCODE_EncodeFrameAsync);

/* VideoDECODE */
msdk_ts_DECLARE_BLOCK(b_MFXVideoDECODE_Query);
msdk_ts_DECLARE_BLOCK(b_MFXVideoDECODE_DecodeHeader);
msdk_ts_DECLARE_BLOCK(b_MFXVideoDECODE_QueryIOSurf);
msdk_ts_DECLARE_BLOCK(b_MFXVideoDECODE_Init);
msdk_ts_DECLARE_BLOCK(b_MFXVideoDECODE_Reset);
msdk_ts_DECLARE_BLOCK(b_MFXVideoDECODE_Close);

msdk_ts_DECLARE_BLOCK(b_MFXVideoDECODE_GetVideoParam);
msdk_ts_DECLARE_BLOCK(b_MFXVideoDECODE_GetDecodeStat);
msdk_ts_DECLARE_BLOCK(b_MFXVideoDECODE_SetSkipMode);
msdk_ts_DECLARE_BLOCK(b_MFXVideoDECODE_GetPayload);
msdk_ts_DECLARE_BLOCK(b_MFXVideoDECODE_DecodeFrameAsync);

/* VideoVPP */
msdk_ts_DECLARE_BLOCK(b_MFXVideoVPP_Query);
msdk_ts_DECLARE_BLOCK(b_MFXVideoVPP_QueryIOSurf);
msdk_ts_DECLARE_BLOCK(b_MFXVideoVPP_Init);
msdk_ts_DECLARE_BLOCK(b_MFXVideoVPP_Reset);
msdk_ts_DECLARE_BLOCK(b_MFXVideoVPP_Close);

msdk_ts_DECLARE_BLOCK(b_MFXVideoVPP_GetVideoParam);
msdk_ts_DECLARE_BLOCK(b_MFXVideoVPP_GetVPPStat);
msdk_ts_DECLARE_BLOCK(b_MFXVideoVPP_RunFrameVPPAsync);

/* VideoUser */
msdk_ts_DECLARE_BLOCK(b_MFXVideoUSER_Register);
msdk_ts_DECLARE_BLOCK(b_MFXVideoUSER_Unregister);
msdk_ts_DECLARE_BLOCK(b_MFXVideoUSER_ProcessFrameAsync);
msdk_ts_DECLARE_BLOCK(b_MFXVideoUSER_Load);
msdk_ts_DECLARE_BLOCK(b_MFXVideoUSER_UnLoad);

#if !(defined(LINUX32) || defined(LINUX64))
// audio core
msdk_ts_DECLARE_BLOCK(b_MFXInitAudio);
msdk_ts_DECLARE_BLOCK(b_MFXCloseAudio);

msdk_ts_DECLARE_BLOCK(b_MFXAudioQueryIMPL);
msdk_ts_DECLARE_BLOCK(b_MFXAudioQueryVersion);

//AudioENCODE
msdk_ts_DECLARE_BLOCK(b_MFXAudioENCODE_Query);
msdk_ts_DECLARE_BLOCK(b_MFXAudioENCODE_QueryIOSize);
msdk_ts_DECLARE_BLOCK(b_MFXAudioENCODE_Init);
msdk_ts_DECLARE_BLOCK(b_MFXAudioENCODE_Reset);
msdk_ts_DECLARE_BLOCK(b_MFXAudioENCODE_Close);
msdk_ts_DECLARE_BLOCK(b_MFXAudioENCODE_GetAudioParam);
msdk_ts_DECLARE_BLOCK(b_MFXAudioENCODE_EncodeFrameAsync);


//AudioDecode
msdk_ts_DECLARE_BLOCK(b_MFXAudioDECODE_Query);
msdk_ts_DECLARE_BLOCK(b_MFXAudioDECODE_QueryIOSize);
msdk_ts_DECLARE_BLOCK(b_MFXAudioDECODE_DecodeHeader);
msdk_ts_DECLARE_BLOCK(b_MFXAudioDECODE_Init);
msdk_ts_DECLARE_BLOCK(b_MFXAudioDECODE_Reset);
msdk_ts_DECLARE_BLOCK(b_MFXAudioDECODE_Close);
msdk_ts_DECLARE_BLOCK(b_MFXAudioDECODE_GetAudioParam);
msdk_ts_DECLARE_BLOCK(b_MFXAudioDECODE_DecodeFrameAsync);
#endif // #if !(defined(LINUX32) || defined(LINUX64))
/*tools*/
//IN : request
//OUT: surf_pool
msdk_ts_DECLARE_BLOCK(t_AllocSurfPool);
msdk_ts_DECLARE_BLOCK(t_AllocOpaqueSurfPool);

//IN : vpp_request
//OUT: vpp_surf_pool_in, surf_pool = vpp_surf_pool_out
msdk_ts_DECLARE_BLOCK(t_AllocVPPSurfPools);
#if (defined(LINUX32) || defined(LINUX64))
//OUT: va_display
msdk_ts_DECLARE_BLOCK(t_GetVA);
#endif

msdk_ts_DECLARE_BLOCK(t_DefineAllocator);
msdk_ts_DECLARE_BLOCK(t_AllocGetDevHdl);

//IN : surf_pool
//OUT: p_free_surf = &free_surf
msdk_ts_DECLARE_BLOCK(t_GetFreeSurf);

//IN : vpp_surf_pool_in
//OUT: p_free_surf = &free_surf
msdk_ts_DECLARE_BLOCK(t_GetFreeVPPInSurf);

//IN : vpp_surf_pool_out
//OUT: p_vpp_out_surf = &vpp_out_surf
msdk_ts_DECLARE_BLOCK(t_GetFreeVPPOutSurf);

//IN : p_free_surf, file_name
//OUT: file_reader, eof
msdk_ts_DECLARE_BLOCK(t_ReadRawFrame);

msdk_ts_DECLARE_BLOCK(t_WriteRawFrame);
msdk_ts_DECLARE_BLOCK(t_SetAVCEncDefaultParam); //deprecated !!!
msdk_ts_DECLARE_BLOCK(t_SetVPPDefaultParam); //deprecated !!!
msdk_ts_DECLARE_BLOCK(t_Alloc_mfxBitstream);
msdk_ts_DECLARE_BLOCK(t_Read_mfxBitstream);
msdk_ts_DECLARE_BLOCK(t_ResetBitstream);
msdk_ts_DECLARE_BLOCK(t_ProcessEncodeFrameAsyncStatus);
msdk_ts_DECLARE_BLOCK(t_ProcessDecodeFrameAsyncStatus);

msdk_ts_DECLARE_BLOCK(t_LoadDLLPlugin);

//IN : mfxRes, [eof, expectedRes, break_on_expected, async]
msdk_ts_DECLARE_BLOCK(t_ProcessRunFrameVPPAsyncStatus); // loop control block

msdk_ts_DECLARE_BLOCK(t_inc);

//IN : x, y, [size]
msdk_ts_DECLARE_BLOCK(t_sum);
msdk_ts_DECLARE_BLOCK(t_sub);
msdk_ts_DECLARE_BLOCK(t_mul);
msdk_ts_DECLARE_BLOCK(t_div);
msdk_ts_DECLARE_BLOCK(t_mod);
msdk_ts_DECLARE_BLOCK(t_and);
msdk_ts_DECLARE_BLOCK(t_or );
msdk_ts_DECLARE_BLOCK(t_xor);
msdk_ts_DECLARE_BLOCK(t_shl);
msdk_ts_DECLARE_BLOCK(t_shr);
msdk_ts_DECLARE_BLOCK(t_set);

msdk_ts_DECLARE_BLOCK(t_Dump_mfxBitstream);
msdk_ts_DECLARE_BLOCK(t_Wipe_mfxBitstream);
msdk_ts_DECLARE_BLOCK(t_AttachBuffers);
msdk_ts_DECLARE_BLOCK(t_ParseH264AU);
msdk_ts_DECLARE_BLOCK(t_ParseHEVCAU);
msdk_ts_DECLARE_BLOCK(t_ParseMPEG2);
msdk_ts_DECLARE_BLOCK(t_PackHEVCNALU);

msdk_ts_DECLARE_BLOCK(t_NewBsParser);
msdk_ts_DECLARE_BLOCK(t_ParseNextUnit);
msdk_ts_DECLARE_BLOCK(t_BsLock);
msdk_ts_DECLARE_BLOCK(t_BsUnlock);

//IN : global_allocator_instance
//OUT: surf_cnt
msdk_ts_DECLARE_BLOCK(t_CntAllocatedSurf);

//IN : global_allocator_instance, alloc_mode, lock_mode, opaque_alloc_mode
msdk_ts_DECLARE_BLOCK(t_ResetAllocator);

//OUT: heap_state, heap_state_idx, heap_state_size
msdk_ts_DECLARE_BLOCK(t_GetHeapState);

//OUT: p_buf_allocator, buf_allocator, global_buffer_allocator_instance
msdk_ts_DECLARE_BLOCK(t_DefineBufferAllocator);

//IN : global_buffer_allocator_instance
//OUT: buffers, num_buffers
msdk_ts_DECLARE_BLOCK(t_GetAllocatedBuffers);

/*verification*/
msdk_ts_DECLARE_BLOCK(v_CheckField);
msdk_ts_DECLARE_BLOCK(v_CheckField_NotEqual);
msdk_ts_DECLARE_BLOCK(v_CheckMfxRes);
msdk_ts_DECLARE_BLOCK(v_CheckAVCBPyramid);
msdk_ts_DECLARE_BLOCK(v_CheckMfxExtVPPDoUse);
