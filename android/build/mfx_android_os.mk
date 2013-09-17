ifeq ($(MFX_ANDROID_NDK_BUILD), false)

    LOCAL_C_INCLUDES += \
        system/core/include \
        hardware/libhardware/include \
        frameworks/base/include \
        frameworks/base/include/media/stagefright/openmax \
        frameworks/native/include/media/openmax \
        frameworks/native/include/media/hardware \
        $(TARGET_OUT_HEADERS)/libva

ifeq ($(MFX_ANDROID_VERSION), MFX_OTC_JB)
    LOCAL_C_INCLUDES += \
        $(TARGET_OUT_HEADERS)/libmixcodec
endif

ifeq ($(MFX_ANDROID_VERSION), MFX_MCG_JB)
    LOCAL_C_INCLUDES += \
        $(TARGET_OUT_HEADERS)/libmix_videoencoder
endif

else # ifeq ($(MFX_ANDROID_NDK_BUILD), true)

    ifneq ($(MFX_ANDROID_HOME), )

    LOCAL_CFLAGS += -include $(MFX_ANDROID_HOME)/build/core/combo/include/arch/linux-x86/AndroidConfig.h

    ## For libVA support
    LOCAL_C_INCLUDES += \
        $(MFX_ANDROID_HOME)/frameworks/native/include \
        $(MFX_ANDROID_HOME)/frameworks/native/include/media/openmax \
        $(MFX_ANDROID_HOME)/frameworks/native/include/media/hardware \
        $(MFX_ANDROID_HOME)/system/core/include

    ## For Rtest build
    LOCAL_C_INCLUDES += \
        $(MFX_ANDROID_HOME)/frameworks/av/include \
        $(MFX_ANDROID_HOME)/hardware/libhardware/include/


    ifneq ($(MFX_ANDROID_HOME_OUT), )
        LOCAL_LDFLAGS += \
            -Wl,-rpath-link=$(MFX_ANDROID_HOME_OUT)/obj/lib \
            -L$(MFX_ANDROID_HOME_OUT)/obj/lib

        ifeq ($(MFX_IMPL), hw)
            LOCAL_C_INCLUDES += \
                $(MFX_ANDROID_HOME_OUT)/obj/include/libva \
                $(MFX_ANDROID_HOME_OUT)/obj/include/libmixcodec
        endif
    endif

    endif # ifneq ($(MFX_ANDROID_HOME), )

endif # ifeq ($(MFX_ANDROID_NDK_BUILD), true)
