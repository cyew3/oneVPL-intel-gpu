LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/mdp_msdk-lib/android/mfx_env.mk

include $(CLEAR_VARS)
include $(MFX_HOME)/mdp_msdk-lib/android/mfx_defs.mk

YAML_CPP_ROOT := ../../../mdp_msdk-contrib/yaml-cpp

LOCAL_SRC_FILES := \
    $(addprefix src/, $(notdir $(wildcard $(LOCAL_PATH)/src/*.cpp)))

LOCAL_SRC_FILES += \
    $(YAML_CPP_ROOT)/src/aliasmanager.cpp \
    $(YAML_CPP_ROOT)/src/binary.cpp \
    $(YAML_CPP_ROOT)/src/contrib/graphbuilder.cpp \
    $(YAML_CPP_ROOT)/src/contrib/graphbuilderadapter.cpp \
    $(YAML_CPP_ROOT)/src/conversion.cpp \
    $(YAML_CPP_ROOT)/src/directives.cpp \
    $(YAML_CPP_ROOT)/src/emitfromevents.cpp \
    $(YAML_CPP_ROOT)/src/emitter.cpp \
    $(YAML_CPP_ROOT)/src/emitterstate.cpp \
    $(YAML_CPP_ROOT)/src/emitterutils.cpp \
    $(YAML_CPP_ROOT)/src/exp.cpp \
    $(YAML_CPP_ROOT)/src/iterator.cpp \
    $(YAML_CPP_ROOT)/src/node.cpp \
    $(YAML_CPP_ROOT)/src/nodebuilder.cpp \
    $(YAML_CPP_ROOT)/src/nodeownership.cpp \
    $(YAML_CPP_ROOT)/src/null.cpp \
    $(YAML_CPP_ROOT)/src/ostream.cpp \
    $(YAML_CPP_ROOT)/src/parser.cpp \
    $(YAML_CPP_ROOT)/src/regex.cpp \
    $(YAML_CPP_ROOT)/src/scanner.cpp \
    $(YAML_CPP_ROOT)/src/scanscalar.cpp \
    $(YAML_CPP_ROOT)/src/scantag.cpp \
    $(YAML_CPP_ROOT)/src/scantoken.cpp \
    $(YAML_CPP_ROOT)/src/simplekey.cpp \
    $(YAML_CPP_ROOT)/src/singledocparser.cpp \
    $(YAML_CPP_ROOT)/src/stream.cpp \
    $(YAML_CPP_ROOT)/src/tag.cpp

LOCAL_C_INCLUDES := \
    $(MFX_INCLUDES) \
    $(MFX_HOME)/mdp_msdk-contrib/yaml-cpp/include

LOCAL_CFLAGS := $(MFX_CFLAGS)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libmfx_serializer

include $(BUILD_STATIC_LIBRARY)
