/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2012 Intel Corporation. All Rights Reserved.
//
*/
#ifndef __PAVP_VIDEO_LIB__
#define __PAVP_VIDEO_LIB__

#include "pavpsdk_sigma.h"
#include "intel_pavp_epid_api.h"
#include "intel_pavp_core_api.h" //PAVP_ENCRYPTION_MODE

/**
@defgroup avUsageCase Protecting video & audio processing pipeline.

Sample for AV playback protection through SIGMA session.
Demonstrate OS independent SIGMA session usage for video playback:

* Determine API supported

* Query capabilities and choose best PAVP mode, algorithm and counter type

* Open SIGMA session for playback

*/

/**
@ingroup avUsageCase
@{
*/

/**Sample base class for stream encryptors can prepare data for PAVP decoding.
@ingroup avUsageCase
*/
class CStreamEncryptor
{
public:
    virtual ~CStreamEncryptor() {};
    
    /** Set stream encryption key.
    
    @param[in] key Pointer to the buffer contain encryption key. Buffer size is 
        implementation dependent.

    @return PAVP_STATUS_SUCCESS No error.
    @return PAVP_STATUS_BAD_POINTER Null pointer.
    @return PAVP_STATUS_MEMORY_ALLOCATION_ERROR Out of memory.
    */
    virtual PavpEpidStatus SetKey(const uint8* key) = 0;

    /** Encrypt data buffer.
    
    @param[in] src Pointer to the buffer to encrypt.
    @param[out] dst Pointer to the buffer recieving encrypted data.
    @param[in] size Size in bytes of src and dst buffers. Must be aligned to key size.
    @param[out] encryptorConfig Encryptor initialization values was used for 
        current encrypt operation. Buffer size is implementation dependent and must 
        be equal to key size.

    @return PAVP_STATUS_SUCCESS No error.
    @return PAVP_STATUS_BAD_POINTER Null pointer.
    @return PAVP_STATUS_LENGTH_ERROR Buffer size is wrong.
    @return PAVP_STATUS_REFRESH_REQUIRED_ERROR Key refresh required. Call SetKey 
        function with refreshed key to reset error.
    */
    virtual PavpEpidStatus Encrypt(
        const uint8* src, 
        uint8* dst, 
        uint32 size, 
        uint8 *encryptorConfig) = 0;
};

/** Sample stream encryptor PAVP_ENCRYPTION_AES128_CTR enryption type.
@ingroup avUsageCase
*/
class CStreamEncryptor_AES128_CTR: public CStreamEncryptor
{
public:
    /**
    Creates sample stream encryptor for AES algorithm with 128bit key operating 
    in counter mode.
    @param[in] counterType Type of the counter.
    @param[in] encryptorConfig Encryptor initialization value.
    */
    CStreamEncryptor_AES128_CTR(
        PAVP_COUNTER_TYPE counterType,
        const uint8 encryptorConfig[16]);
    virtual ~CStreamEncryptor_AES128_CTR();
    virtual PavpEpidStatus SetKey(const uint8* key);
    virtual PavpEpidStatus Encrypt(
        const uint8* src,   
        uint8* dst, 
        uint32 size, 
        uint8* encryptorConfig);
private:
    void* m_rij;
    uint64 m_initialCounter;
    uint64 m_iv_ctr[2];
    uint8 m_counterBitSize;

    PAVPSDK_DISALLOW_COPY_AND_ASSIGN(CStreamEncryptor_AES128_CTR);
};

/** Sample stream encryptor D3DCRYPTOTYPE_INTEL_AES128_CBC enryption type.
@ingroup avUsageCase
*/
class CStreamEncryptor_AES128_CBC: public CStreamEncryptor
{
public:
    /**
    Creates sample stream encryptor for AES algorithm with 128bit key operating 
    in CBC mode.
    @param[in] encryptorConfig Encryptor initialization value.
    */
    CStreamEncryptor_AES128_CBC(const uint8 encryptorConfig[16]);
    virtual ~CStreamEncryptor_AES128_CBC();
    virtual PavpEpidStatus SetKey(const uint8* key);
    virtual PavpEpidStatus Encrypt(
        const uint8* src, 
        uint8* dst, 
        uint32 size, 
        uint8* encryptorConfig);
private:
    void* m_rij;
    uint8 m_iv[16];

    PAVPSDK_DISALLOW_COPY_AND_ASSIGN(CStreamEncryptor_AES128_CBC);
};

class CVideoProtection
{
public:
    /** Create video processing pipeline base for CCPDevice.     
    @param[in] cpDevice CP Device to create video processing pipeline protection.
    */
    CVideoProtection() {};
    virtual ~CVideoProtection() {};
    /** Encrypt content for decoder.
    @param[in] src Pointer to the buffer to encrypt.
    @param[out] dst Pointer to the buffer recieving encrypted data.
    @param[in] size Size in bytes of src and dst buffers. Must be aligned to key size.
    @param[out] encryptorConfig Encryptor initialization values was used for 
        current encrypt operation.
    @param[in] encryptorConfigSize Size in bytes of encryptorConfig.
    @return PAVP_STATUS_NOT_SUPPORTED Encryption stream key is not applicable.
    */
    virtual PavpEpidStatus Encrypt(
        const uint8* src, 
        uint8* dst, 
        uint32 size, 
        void *encryptorConfig, 
        uint32 encryptorConfigSize) = 0;
    
    /** Refresh encryption key.
    */
    virtual PavpEpidStatus EncryptionKeyRefresh() = 0;
    
    /// Close protected video processing session, destroy secret data.
    virtual PavpEpidStatus Close() = 0;
};


/** Sample base class to manage video processing pipeline protection.
It provides stream keys management (set, refresh) and encrypt/decrypt 
operations for the stream.
@ingroup avUsageCase
*/
class CPAVPVideo: public CVideoProtection
{
public:
    /** Create video processing pipeline base for CCPDevice.     
    @param[in] cpDevice CP Device to create video processing pipeline protection.
    */
    CPAVPVideo(CCPDevice* cpDevice);
    virtual ~CPAVPVideo();
    
    /** Create protected video processing session.
    It will request stream keys as required by session type CP Device was 
    created for and usage model encryptionType define. 
    
    Decryption stream key will always be requested is session is 
    transoding session. Encryption stream key will be requested if session is 
    decode or transcode and usage model assume re-encryption. In particular 
    encryption stream key will not be requested for DECE encrypted content.

    @param[in] streamId Stream slot to occupy for video preocessing session.
        Slot numbers start from 0.
    @param[in] encryptorConfig Initialization value for encryptor.
    @param[in] encryptorConfigSize Size in bytes of encryptorConfig.
    */
    virtual PavpEpidStatus Init(
        uint32 streamId, 
        const void *encryptorConfig, 
        uint32 encryptorConfigSize) = 0;
    
    /// Get CP commands execution interface in use. 
    CCPDevice *GetCPDevice() { return m_cpDevice; };
    
    /** Get encryptor mode of operation.
    Encryptor is used to supply data for decoder.
    @param[out] mode Mode of operation.
    */
    PavpEpidStatus GetEncryptorMode(PAVP_ENCRYPTION_MODE *mode) 
    {
        if (NULL == mode)
            return PAVP_STATUS_BAD_POINTER;
        *mode = m_encryptorMode;
        return PAVP_STATUS_SUCCESS;
    };

    /** Get decryptor mode of operation.
    Decryptor is typically used to decrypt data from encoder.
    @param[out] mode Mode of operation.
    */
    PavpEpidStatus GetDecryptorMode(PAVP_ENCRYPTION_MODE *mode) 
    {
        if (NULL == mode)
            return PAVP_STATUS_BAD_POINTER;
        *mode = m_decryptorMode;
        return PAVP_STATUS_SUCCESS;
    };

    /** Encrypt content for decoder.
    @param[in] src Pointer to the buffer to encrypt.
    @param[out] dst Pointer to the buffer recieving encrypted data.
    @param[in] size Size in bytes of src and dst buffers. Must be aligned to key size.
    @param[out] encryptorConfig Encryptor initialization values was used for 
        current encrypt operation.
    @param[in] encryptorConfigSize Size in bytes of encryptorConfig.
    @return PAVP_STATUS_NOT_SUPPORTED Encryption stream key is not applicable.
    */
    virtual PavpEpidStatus Encrypt(
        const uint8* src, 
        uint8* dst, 
        uint32 size, 
        void *encryptorConfig, 
        uint32 encryptorConfigSize);
    
    /** Decrypt content received from encoder.
    @param[in] src Pointer to the buffer to decrypt.
    @param[out] dst Pointer to the buffer recieving decrypted data.
    @param[in] size Size in bytes of src and dst buffers. Must be aligned to key size.
    @param[in] config Decryptor initialization values to be used for 
        current decrypt operation.
    @param[in] configSize Size in bytes of config.

    @return PAVP_STATUS_NOT_SUPPORTED Decryption stream key is not applicable.
    */
    virtual PavpEpidStatus Decrypt(
        const uint8* src, 
        uint8* dst, 
        uint32 size, 
        const void *config, 
        uint32 configSize);

    /** Refresh encryption key.
    */
    virtual PavpEpidStatus EncryptionKeyRefresh();

    /** Refresh decryption key. 
    */
    virtual PavpEpidStatus DecryptionKeyRefresh();

    /** Set key/keys. 
    CP device will be swithced to new key/keys immediatelly.
    */
    virtual PavpEpidStatus SetKey(const uint8 *encryptionKey, const uint8 *decryptionKey);
    
    /// Close protected video processing session, destroy secret data.
    virtual PavpEpidStatus Close() = 0;
protected:
    /** Create encryptor
    Base class own encryption and encryption key stoprage. Child calsses should 
    call this method to initilize encryption.
    @param[in] encryptorMode Encryptor mode of operation.
    @param[in] key Encryption key.
    @param[in] encryptorConfig Encryptor configuration data or NULL.
    @param[in] encryptorConfigSize Size in bytes of encryptorConfig.
    */
    PavpEpidStatus CreateEncryptor(
        PAVP_ENCRYPTION_MODE encryptorMode,
        const uint8 *key,
        const uint8 *encryptorConfig, 
        uint32 encryptorConfigSize);

    /** Reset encryptor with new key.
    Set new encryption for encryptor object managed. Do not communicate new key to HW.
    @param[in] buf Buffer contains new encyption key if isFreshnes=false or 
        freshness value if isFreshnes=true. Freshness value will be XORed with 
        key to get new key value.
    @param[in] isFreshness Defines how buf buffer value to be applied.
    */
    PavpEpidStatus ResetEncryptor(const uint8 *buf, bool isFreshness);

    /// Desctroy encryptor and clean up memory.
    void DestroyEncryptor();

    bool m_isDecryptorModeExists;           ///< true if m_decryptorMode is set.
    PAVP_ENCRYPTION_MODE m_decryptorMode;   ///< Decryptor mode of operation.
    bool m_isEncryptorModeExists;           ///< true if m_encryptorMode is set.
    PAVP_ENCRYPTION_MODE m_encryptorMode;   ///< Encryptor mode of operation.

    uint8 m_decryptionKey[16];              ///< Decryption key. @todo: wrap into class
    uint8 m_decryptionKeyBitSize;           ///< Decryption key bit size. @todo: wrap into class


private:
    CCPDevice* m_cpDevice;

    CStreamEncryptor *m_encryptor;          ///< Encryptor object.
    uint8 m_encryptorBlockSize;             ///< Encryption alogithm block size.
    uint8 m_encryptionKey[16];              ///< Encryption key. @todo: wrap into class
    uint8 m_encryptionKeyBitSize;           ///< Encryption key bit size. @todo: wrap into class
};

/** @} */ //@ingroup avUsageCase

#endif//__PAVP_VIDEO_LIB__