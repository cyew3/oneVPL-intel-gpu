ifeq ($(MFX_ANDROID_NDK_BUILD), false)

    LOCAL_C_INCLUDES += \
        system/core/include \
        hardware/libhardware/include \
        frameworks/base/include \
        frameworks/base/include/media/stagefright/openmax \
        $(TARGET_OUT_HEADERS)/libva

else # ifeq ($(MFX_ANDROID_NDK_BUILD), true)

    ifneq ($(MFX_ANDROID_HOME), )

    LOCAL_CFLAGS += -include $(MFX_ANDROID_HOME)/system/core/include/arch/linux-x86/AndroidConfig.h

    LOCAL_C_INCLUDES += \
        $(MFX_ANDROID_HOME)/system/core/include \
        $(MFX_ANDROID_HOME)/hardware/libhardware/include \
        $(MFX_ANDROID_HOME)/frameworks/base/include \
        $(MFX_ANDROID_HOME)/frameworks/base/include/media/stagefright/openmax

#    LOCAL_C_INCLUDES += \
        $(MFX_ANDROID_HOME)/frameworks/base/include/media/stagefright/openmax

    ifneq ($(MFX_ANDROID_HOME_OUT), )
        LOCAL_LDFLAGS += \
            -Wl,-rpath-link=$(MFX_ANDROID_HOME_OUT)/obj/lib \
            -L$(MFX_ANDROID_HOME_OUT)/obj/lib

        ifeq ($(MFX_IMPL), hw)
            LOCAL_C_INCLUDES += $(MFX_ANDROID_HOME_OUT)/obj/include/libva
        endif
    endif

    endif # ifneq ($(MFX_ANDROID_HOME), )

endif # ifeq ($(MFX_ANDROID_NDK_BUILD), true)
