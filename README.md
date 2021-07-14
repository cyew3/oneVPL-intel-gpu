# Intel® oneVPL GPU Runtime

Intel® oneVPL GPU Runtime is a Runtime implementation of [oneVPL](https://github.com/oneapi-src/oneVPL/)
API for Intel Gen GPUs. Runtime provides access to hardware-accelerated video decode, encode and filtering.

**Supported video encoders**: HEVC, AVC, MPEG-2, JPEG, VP9  
**Supported video decoders**: HEVC, AVC, VP8, VP9, MPEG-2, VC1, JPEG, AV1  
**Supported video pre-processing filters**: Color Conversion, Deinterlace, Denoise, Resize, Rotate, Composition  

Implementation is written in C++ 14 with parts in C-for-Media (CM).

oneVPL GPU Runtime is a part of Intel software stack for graphics:
* [Linux Graphics Drivers](https://intel.com/linux-graphics-drivers) - General Purpose GPU Drivers for Linux Operating Systems
  * Visit [documentation](https://dgpu-docs.intel.com) for instructions on installing, deploying, and updating Intel software to enable general purpose GPU (GPGPU) capabilities for Linux-based operating system distributions.

oneVPL Library and oneVPL GPU Runtime are successors for Intel [Media SDK](https://github.com/Intel-Media-SDK/MediaSDK).

# How to use

This runtime implementation is not self-sufficient. Application should use
one of the available frontend dispatcher libraries:

* [oneVPL](https://github.com/oneapi-src/oneVPL/)
* [Media SDK](https://github.com/Intel-Media-SDK/MediaSDK)

We strongly recommend to use [oneVPL](https://github.com/oneapi-src/oneVPL/)
as a dispatcher frontend (libvpl.so.2). [Media SDK](https://github.com/Intel-Media-SDK/MediaSDK)
dispatcher (libmfx.so.1) can be used as well, but you will get capabilities limited by those which
are exposed via Media SDK API and which are actually implemented by oneVPL
Runtime. Basically, compatibility with Media SDK is provided to support
Media SDK based applications which did not yet migrate to oneVPL on a new HW
platforms.

oneVPL Gen Runtime implementation supports the following hardware platforms:

| GPU | Supported |
| --- | --------- |
| TGL (Tiger Lake) | ✔ |
| DG1 (Xe MAX) | ✔ |
| RKL (Rocket Lake) | ✔ |
| ADL-S (Alder Lake S) | ✔ |
| ADL-P (Alder Lake P) | ✔ |

# Dependencies
Intel Media SDK depends on [LibVA](https://github.com/intel/libva/).
This version of Intel Media SDK is compatible with the open source [Intel Media Driver for VAAPI](https://github.com/intel/media-driver).

# Table of contents

  * [License](#license)
  * [How to contribute](#how-to-contribute)
  * [How to build](#how-to-build)
    * [Build steps](#build-steps)
    * [Enabling Instrumentation and Tracing Technology (ITT)](#enabling-instrumentation-and-tracing-technology-itt)
  * [Documentation](#documentation)
  * [Tutorials](#tutorials)
  * [Products which use Media SDK](#products-which-use-media-sdk)
  * [System requirements](#system-requirements)
  * [See also](#see-also)

# License
Intel Media SDK is licensed under MIT license. See [LICENSE](./LICENSE) for details.

# How to contribute
See [CONTRIBUTING](./CONTRIBUTING.md) for details. Thank you!

# How to build

## Build steps

Visit our wiki pages:
- [oneVPL Build Process](https://wiki.ith.intel.com/display/MediaSDK/oneVPL+Build+Process)
- [CMake-based build system](https://wiki.ith.intel.com/display/MediaSDK/CMake-based+build+system)

## Enabling Instrumentation and Tracing Technology (ITT)

To enable the Instrumentation and Tracing Technology (ITT) API you need to:
* Either install [Intel® VTune™ Amplifier](https://software.intel.com/en-us/intel-vtune-amplifier-xe)
* Or manually build an open source version (see [IntelSEAPI](https://github.com/intel/IntelSEAPI/tree/master/ittnotify) for details)

and configure Media SDK with the -DENABLE_ITT=ON. In case of VTune it will be searched in the default location (/opt/intel/vtune_amplifier). You can adjust ITT search path with  CMAKE_ITT_HOME.

Once Media SDK was built with ITT support, enable it in a runtime creating per-user configuration file ($HOME/.mfx_trace) with the following content:
```sh
Output=0x10
```

# Documentation

To get copy of Media SDK documentation use Git* with [LFS](https://git-lfs.github.com/) support.

Please find full documentation under the [./doc](./doc) folder. Key documents:
* [Media SDK Manual](./doc/mediasdk-man.md)
* Additional Per-Codec Manuals:
  * [Media SDK JPEG Manual](./doc/mediasdkjpeg-man.md)
  * [Media SDK VP8 Manual](./doc/mediasdkvp8-man.md)
* Advanced Topics:
  * [Media SDK User Plugins Manual](./doc/mediasdkusr-man.md)
  * [Media SDK FEI Manual](./doc/mediasdkfei-man.md)
  * [Media SDK HEVC FEI Manual](./doc/mediasdkhevcfei-man.md)
  * [MFE Overview](./doc/MFE-Overview.md)
  * [HEVC FEI Overview](./doc/HEVC_FEI_overview.pdf)
  * [Interlace content support in HEVC encoder](./doc/mediasdk_hevc_interlace_whitepaper.md)

Generic samples information is available in [Media Samples Guide](./doc/samples/Media_Samples_Guide_Linux.md)

Linux Samples Readme Documents:
* [Sample Multi Transcode](./doc/samples/readme-multi-transcode_linux.md)
* [Sample Decode](./doc/samples/readme-decode_linux.md)
* [Sample Encode](./doc/samples/readme-encode_linux.md)
* [Sample VPP](./doc/samples/readme-vpp_linux.md)
* [Metrics Monitor](./doc/samples/readme-metrics_monitor_linux.md)

Visit our [Github Wiki](https://github.com/Intel-Media-SDK/MediaSDK/wiki) for the detailed setting and building instructions, runtime tips and other information.

# Tutorials

* [Tutorials Overview](./doc/tutorials/mediasdk-tutorials-readme.md)
* [Tutorials Command Line Reference](./doc/tutorials/mediasdk-tutorials-cmd-reference.md)

# Products which use Media SDK

Use Media SDK via popular frameworks:
* [FFmpeg](http://ffmpeg.org/) via [ffmpeg-qsv](https://trac.ffmpeg.org/wiki/Hardware/QuickSync) plugins
* [GStreamer](https://gstreamer.freedesktop.org/) via plugins set included
  into [gst-plugins-bad](https://gitlab.freedesktop.org/gstreamer/gst-plugins-bad)

Learn best practises and borrow fragments for final solutions:
* https://github.com/intel/media-delivery
  * This collection of samples demonstrates best practices to achieve optimal video quality and
    performance on Intel GPUs for content delivery networks. Check out the demo, recommended command
    lines and quality and performance measuring tools.

Use Media SDK via other Intel products:
* [OpenVINO Toolkit](https://github.com/openvinotoolkit/openvino)
  * This toolkit allows developers to deploy pre-trained deep learning models through a high-level C++ Inference Engine API integrated with application logic.
* [Open Visual Cloud](https://github.com/OpenVisualCloud)
  * The Open Visual Cloud is a set of open source software stacks (with full end-to-end sample pipelines) for media, analytics, graphics and immersive media, optimized for cloud native deployment on commercial-off-the-shelf x86 CPU architecture.


# System requirements

**Operating System:**
* Linux x86-64 fully supported
* Linux x86 only build
* Windows (not all features are supported in Windows build - see Known Limitations for details)

**Software:**
* [LibVA](https://github.com/intel/libva)
* VAAPI backend driver:
  * [Intel Media Driver for VAAPI](https://github.com/intel/media-driver)
* Some features require CM Runtime library (part of [Intel Media Driver for VAAPI](https://github.com/intel/media-driver) package)

**Hardware:** Intel platforms supported by the [Intel Media Driver for VAAPI](https://github.com/intel/media-driver)

Media SDK test and sample applications may require additional software packages (for example, X Server, Wayland, LibDRM, etc.) to be functional.

**Operating System:** Windows **(experimental)**

Requires Microsoft Visual Studio 2017 for building.

# See also

Intel Media SDK: https://software.intel.com/en-us/media-sdk
