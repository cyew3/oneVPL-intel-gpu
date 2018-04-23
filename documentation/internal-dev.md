# **Media SDK Internal Development Document.**

The intention of this document is to document decisions that were made regarding internal implementation of Media SDK. From time to time parts of this document are moved to "official" manual. 

# Core.

## SetAllocator.

Spec allows several consecutive calls of `SetBufferAllocator()` and `SetFrameAllocator()`. But behavior is not defined yet so `MFX_ERR_UNDEFINED_BEHAVIOR` is returned in this case.

## Frame/Buffer Allocator Lock()

Allocator should support multiple calls of its `Lock()` function. Several consecutive locks for the same surface/buffer are allowed. Number of locks should be equal to number of unlocks. Any unlock should clear data pointers in `mfxFrameData` or data pointer for buffer. Typical example is decoding. Decoder locks frame then decode to it, then signal to application about frame data readiness by setting sync point. After this decoder still use data in surface as reference frame, doesn't call `allocator::Unlock()`. Application simultaneously with decoder access data by calling `Lock()` second time.

## SetHandle

If this function is called after internal handle of the same type has been created function fails with `MFX_ERR_UNDEFINED_BEHAVIOR` code.
If this function is used to set device manager (`MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9`) this manager has to contain DirectX device. If manager is empty this function has to return `MFX_ERR_UNDEFINED_BEHAVIOR`.

## MFXInit & mfxIMPL

SW library has to ignore such flags as `MFX_IMPL_VIA_D3D9` and `MFX_IMPL_VIA_ANY`.

# Common for Dec, VPP, Enc

## MemId and pointers handling

This chapter describes how SDK should access content for different types of surfaces. 
 - SDK doesn't try to get handle for surfaces in system memory.
 - SDK doesn't try to lock surfaces or access them by pointers in video memory. Because of DXVA limitations, video memory surfaces have to be allocated by chunks and referenced by handle.
 - SDK component can't directly process data from arbitrary surface. It has to copy data from it to internal surface and to do so it has to know at init time how many surfaces to allocate. Such behavior is implemented only for system memory type.
Formal description of algorithm:

```C
if(IOPattern == IN_SYSTEM_MEMORY){
    if (mfxFrameData::MemId == 0){
        if (mfxVideoParam::YUV != NULL) use YUV pointer;
        else return error;
    } else {
        if(mfxFrameAllocator::Lock(mfxFrameData::MemId)) use locked pointer;
        else return error;
    }
}else if (IOPattern == IN_VIDEO_MEMORY){ //DX9 or DX11
    if(mfxFrameAllocator::GetHDL(mfxFrameData::MemId)) use handle;
    else return error;
}else{ //OPAQUE_MEMORY
    use internal surface;
}
```

## Surface sharing

Component has to calculate `NumFrameMin` taking into account `AsyncDepth`. For optimal memory usage `AsyncDepth` should be set to 1 by application. As result in all feasible cases `NumFrameMin` is equal to `NumFrameSuggested`. Two different values (min and suggested) were introduced to API before `AsyncDepth` and intention was to use them to show number of surfaces for memory constrained and performance optimum cases. One variable is enough starting from API 1.1 (`AsyncDepth` was added in API 1.1 but behavior was not aligned till 1.3).

There is no suitable solution for backward compatibility issue. Simple one was chosen.
1. For old API ver. 1.0, 1.1 and 1.2 use next equation to calculate number of surfaces in shared pool, set AsyncDepth to 0.
	1. if optimal memory usage is more important than performance <br>`N = MinA + MinB - 1`, <br>there `MinA` is `mfxFrameAllocRequest::NumFrameMin`
	2. if performance is more important<br>`N = MinA + MinB - 1 + AsyncLevel`,<br>`AsyncLevel` is called so to distinguish it from `mfxVideoParam::AsyncDepth` below, it has the same meaning but is not part of MSDK API
2. For new API ver. 1.3 and higher use
	1. optimal memory<br>`N = MinA + MinB - 1`
	2. optimal performance<br>`N = NumSuggestedA + NumSuggestedB - AsyncDepth`<br>Meaning of `AsyncDepth` equal to zero is the same, some default async depth. 

For old applications this solution leads in the worst case to allocation of excessive number of surfaces, never to dead lock during execution.

## Handling of extension buffers in Query

|buffer name | decoder | VPP | encoder | comment
| --- | --- | --- | --- | --- |
`mfxExtAVCRefListCtrl` | error | error | error | 
`mfxExtCodingOption` | error | error | check | 
`mfxExtCodingOptionSPSPPS` | error | error | check | Decoder and encoder GetVideoParam fills it in if this buffer is attached.
`mfxExtMVCSeqDesc` | check/ error | check | check/ error | MVC Decoder and VPP check (and copy in mode 2) only `NumView`. The rest is ignored.<br>MVC Encoder checks (and copy in mode 2) only `NumView, NumViewId, NumOP, NumRefsTotal`. The rest is ignored.<br>MVC query function (decoder and encoder) is used if mfxVideoParam:: mfxInfoMFX:: `CodecId` is set to `MFX_CODEC_AVC` and `CodecProfile` to `MFX_PROFILE_AVC_MULTIVIEW_HIGH` or `MFX_PROFILE_AVC_STEREO_HIGH`. In other cases AVC Query is used. AVC query always return error for this buffer.
`mfxExtMVCTargetViews` | check/ error | error | error | decoder checks it only if mfxExtMVCSeqDesc is attached, error otherwise. Also error if AVC profile is set in `mfxVideoParam` 
`mfxExtOpaqueSurfaceAlloc` | ignore | ignore | ignore | `IOPattern` doesn't affect this check.
`mfxExtVideoSignalInfo` | error | error | check | Decoder and encoder GetVideoParam fills it in if this buffer is attached.
`mfxExtVppAuxData` | error | error | error | 
`mfxExtVPPDenoise` | error | check | error | 
`mfxExtVPPDetail` | error | check | error | 
`mfxExtVPPDoNotUse` | error | check | error | 
`mfxExtVPPDoUse` | error | check | error | 
`mfxExtVPPFrameRateConversion` | error | check | error | 
`mfxExtVPPProcAmp` | error | check | error | 

check – component checks content of the buffer for correctness

ignore – component does nothing with this buffer, doesn't copy it from input to output, doesn't return error if there is input buffer but there is not output and so on

error – component return error `MFX_ERR_UNSUPPORTED` if such buffer is attached

## Init

If `Init` function has changed some value in the `mfxVideoParam` structure it has to return `MFX_WRN_INCOMPATIBLE_VIDEO_PARAM`.

## Query

About mode 2.
1. It is application responsibility to allocate `in` and `out` structures including extended buffers. in and out pointers may point to the same structure.
2. If unknown for component extended buffer is attached (unsupported `mfxExtBuffer::BufferId`) then function returns `MFX_ERR_UNSUPPORTED`.
3. If `mfxVideoParam::Protected` is not equal to zero or `mfxVideoParam::mfxInfoMFX::mfxFrameInfo::FourCC` is not equal to MFX_FOURCC_NV12 then function returns `MFX_ERR_UNSUPPORTED`. List of conditions may be extended by codec owner but these two have to be supported.
4. Function doesn't expand default values, if some field in `in` is zero and it is allowed, then field remains zero in output structure. To expand default values initialize component then call `GetVideoParam`.
5. Function checks parameters and corrects them if such correction is possible. For example, if provided bitrate is too low for this resolution function increases it to minimum acceptable value and return `MFX_WRN_INCOMPATIBLE_VIDEO_PARAM`. If correction is impossible, for example provided frame size is 1x1, then function returns error `MFX_ERR_UNSUPPORTED` and zeros corresponded field in output structure to let application know what exactly is wrong.
6. If parameters is completely wrong, for example number of extended buffers attached to in structure is not equal to number of buffers attached to out structure, function returns `MFX_ERR_UNDEFINED_BEHAVIOR`
7. Query should expect partial configuration and do not force an error when certain parameters are zero. For example, Query can be used to enquire about single field in `mfxVideoParam` structure, all other fields may be zero. Complete `mfxVideoParam` validation as the whole is `Init` function responsibility.
8. Though Query should support partial configuration there are several "pair" parameters that can't be verified separately. If one of the parameter in pair is undefined the other should also be undefined. Otherwise Query will return error. This is the complete list of pairs:
- FourCC & ChromaFormat, if FourCC is unknown then ChromaFormat should be equal to monochrome, if FourCC is valid value then ChromaFormat should also be valid
- Width & Height
- CropX & CropY & CropW & CropH
- FrameRateExtN & FrameRateExtD
- AspectRatioW & AspectRatioH

## Common notes about parameters

1. All parameters which are somehow **used** by component should be explicitly marked as 'configurable' and thus reported =1 in Query mode 1. E.g. `PicStruct/AspectRatio` should be reported =1
2. All parameters which are marked as 'configurable' should be checked in `Query` (mode 2) \ `Init`
3. All parameters which are described as 'Ignored' in documentation should be treated in the following or equivalent manner:
    1. Copy all app. specified parameters excepting 'ignored' into internal parameters buffer
    2. Check parameters in this internal\intermediate buffer

# Encoder

## HRD Conformance

Encoder should always satisfy HRD requirement. To do so two steps procedure should be used.
1. On initialization stage application suggests bitrate. Encoder changes it, increases, if bitrate is too small and returns warning `MFX_WRN_INCOMPATIBLE_VIDEO_PARAM`. This minimal bitrate depends upon encoder configuration, number of I, P and B frames in GOP and frame size. It may be calculated as number of bits required to encode frame of particular type with all bitrate reduction technique used multiplied by number of frame of this type in GOP. To assure HRD conformance this calculated value may be doubled or tripled.   
2. During processing encoder uses all implemented technique (such as frame skipping, AC coefficient truncation, re-encoding and so on) to assure HRD conformance.

Application can turn HRD conformance ON or OFF by using `mfxExtCodingOption::NalHrdConformance`. It affects dynamic bitrate change as described in next table:

| NalHrdConformance | VuiNalHrdParameters or VuiVclHrdParameters | key frame and SPS | allowed BRC modes |
| --- | --- | --- | --- |
ON | ON | inserted | VBR only
ON | OFF | not inserted | VBR only
OFF | any | not inserted | CBR, VBR, AVBR

If HRD conformance is turned OFF encoder should ignore both `VuiNalHrdParameters` and `VuiVclHrdParameters` flags, i.e. doesn't write HRD parameters in stream.

## AVC Cropping

It is followed from standard that for 420 subsampling horizontal crop should be dividable by 2, vertical should be dividable by 2 for frame picture and by 4 for field picture. Encoder narrows crop region to satisfy this condition. CropX may be increased by 1. `CropY` may be increased by 1, 2, or 3. `CropW` may be decreased by 1 or 2. `CropH` may be decreased by 1, 2, 3 or 4. This behavior is implemented in `Query`, `Init` and `Reset`. These functions correct invalid crops and return warning `MFX_WRN_INCOMPATIBLE_VIDEO_PARAM`.

## SEI message handling

Encoder generates only buffering_period and pic_timing messages if application doesn't turn them off explicitly.

Encoder doesn't generate any more user_data_unregistered with copyright and encoding options.

In table below "NO" in column "Accept from application?" means that:
- Application can't have enough information to generate a message
- Ill-formed message of such type can make bitstream invalid and it is hard for encoder to validate it.

| SEI type | Accept from application? | Can write itself? | Write itself by default?
| --- | --- | --- | --- |
buffering_period | No, app can't know BRC state (buffer fullness) to generate valid message.  | controlled by `VuiNalHrdParameters` | yes, mostly to be consistent with SW gold, HW will be aligned
pic_timing | No. App needs to know encoding order and be sure about few flags from SPS and VUI. It is possible under very-very specific conditions. | controlled by `PicTimingSEI` | yes, same reason
dec_ref_pic_marking_repetition | No, it is external control of ref-lists. Interesting feature, but time-consuming and difficult to test. Also app must know encoding order. | No | no
spare_pic | No, needs slice_group which is unsupported. | No | no
sub_seq_info | No, restricts sequence_header and slice_header | No | no
sub_seq_layer_characteristics | No, same | No | no
sub_seq_characteristics | No, same | No | no
motion_constrained_slice_group_set | No, same | No | no
recovery_point | Yes, will slightly affect logic of ENCODE. | No | no
user_data_unregistered | Yes | Not any more, before SW AVC ENCODE generated informative message with copyright  and encoding options. Better leave it to application. | Not any more
others | Yes | no | no

## Frame Rate and Aspect Ratio in MPEG2

Due to standard limitations not all values of frame rate and aspect ratio may be encoded. For unsupported values encoder should return `MFX_ERR_INVALID_VIDEO_PARAM` error from `Init` function and `MFX_WRN_INCOMPATIBLE_VIDEO_PARAM` warning from `Query` function. `Query` function should change unsupported value to closest valid one. Value is unsupported if provided by application aspect ratio differs from one of the standard values for more than 0.1%.

## Query

Due to backward compatibility requirement encoder should ignore `mfxExtCodingOption::MESearchType` and `mfxExtCodingOption::MECostType`. 
See also Decode Query.

## GetVideoParam

See Decode GetVideoParam

## PicStruct handling at Init and at Runtime

This chapter describes `PicStruct` handling in encoder. If encoder encounters combination of parameters that is not specified in "Major PicStruct table" it uses "Default PicStruct table". To simplify table "MFX_PICSTRUCT_" is omitted, so FIELD_TFF has to be read as `MFX_PICSTRUCT_FIELD_TFF` and so on.

If the encoder has been initialized in field output mode it encodes all frames as field pair. The encoder ignores runtime picture structure for progressive frames and uses initialization picture structure instead.

### Major PicStruct table

| PicStruct (init) | FramePicture (init) | MFX PicStruct value (runtime) | displayed as | coded as | comment | status from EncodeFrameAsync |
| --- | --- | --- | --- | --- | --- | --- |
PROGRESSIVE | any | UNKNOWN | one progressive frame | progressive | any->prog | MFX_ERR_NONE
PROGRESSIVE | any | PROGRESSIVE | one progressive frame | progressive | prog->prog | MFX_ERR_NONE
PROGRESSIVE | any | `PROGRESSIVE|FRAME_DOUBLING` | two identical progressive frames | progressive | prog->prog (repeat) | MFX_ERR_NONE
PROGRESSIVE | any | `PROGRESSIVE|FRAME_TRIPLING` | three identical progressive frames | progressive | prog->prog (repeat) | MFX_ERR_NONE
FIELD_BFF | OFF or UNKNOWN | UNKNOWN | two fields: bottom, top | fields | any->interl BFF | MFX_ERR_NONE
FIELD_BFF or UNKNOWN | OFF or UNKNOWN | FIELD_BFF | two fields: bottom, top | fields | interl->interl BFF | MFX_ERR_NONE
FIELD_TFF | OFF or UNKNOWN | UNKNOWN | two fields: top, bottom | fields | any->interl TFF | MFX_ERR_NONE
FIELD_TFF or UNKNOWN | OFF or UNKNOWN | FIELD_TFF | two fields: top, bottom | fields | interl->interl TFF | MFX_ERR_NONE
FIELD_BFF | ON | UNKNOWN | two fields: bottom, top | interlace frame | any->interl BFF, MBAFF | MFX_ERR_NONE
FIELD_BFF or UNKNOWN | ON | FIELD_BFF | two fields: bottom, top | interlace frame | interl->interl BFF, MBAFF | MFX_ERR_NONE
FIELD_TFF | ON | UNKNOWN | two fields: top, bottom | interlace frame | any->interl TFF, MBAFF | MFX_ERR_NONE
FIELD_TFF or UNKNOWN | ON | FIELD_TFF | two fields: top, bottom | interlace frame | interl->interl TFF, MBAFF | MFX_ERR_NONE
UNKNOWN | any | `PROGRESSIVE|FIELD_BFF` | two fields: bottom, top | progressive | prog->interlace (telecine) | MFX_ERR_NONE
UNKNOWN | any | `PROGRESSIVE|FIELD_TFF` | two fields: top, bottom | progressive | prog->interlace (telecine) | MFX_ERR_NONE
UNKNOWN | any | `PROGRESSIVE|FIELD_BFF|FIELD_REPEATED` | three fields: bottom, top, bottom | progressive | prog->interlace (telecine) | MFX_ERR_NONE
UNKNOWN | any | `PROGRESSIVE|FIELD_TFF|FIELD_REPEATED` | three fields: top, bottom, top | progressive | prog->interlace (telecine) | MFX_ERR_NONE
FIELD_TFF or UNKNOWN | any | PROGRESSIVE | two fields: top, bottom | progressive | prog->intelace (telecine) | MFX_ERR_NONE
FIELD_BFF | any | PROGRESSIVE | two fields: bottom, top | progressive | prog->intelace (telecine) | MFX_ERR_NONE
UNKNOWN | any | UNKNOWN | n/a | n/a | n/a | MFX_ERR_UNDEFINED_BEHAVIOR

### Default PicStruct table

| PicStruct (init) | FramePicture (init) | MFX picStruct value (runtime) | displayed as | coded as | comment | status from EncodeFrameAsync |
| --- | --- | --- | --- | --- | --- | --- |
PROGRESSIVE | any | any | one progressive frame | progressive | any->prog | MFX_WRN_INCOMPATIBLE_VIDEO_PARAM
FIELD_TFF | OFF or UNKNOWN | any | two fields: top, bottom | fields | any->interl TFF | MFX_WRN_INCOMPATIBLE_VIDEO_PARAM
FIELD_TFF | ON | any | two fields: top, bottom | interlace frame | any->interl TFF, MBAFF | MFX_WRN_INCOMPATIBLE_VIDEO_PARAM
FIELD_BFF | OFF or UNKNOWN | any | two fiields: bottom, top | fields | any->interl BFF | MFX_WRN_INCOMPATIBLE_VIDEO_PARAM
FIELD_BFF | ON | any | two fiields: bottom, top | interlace frame | any->interl BFF, MBAFF | MFX_WRN_INCOMPATIBLE_VIDEO_PARAM
UNKNOWN | OFF | any | two fields: top, bottom | fields | any->interl TFF | MFX_WRN_INCOMPATIBLE_VIDEO_PARAM
UNKNOWN | ON or UNKNOWN | any | one progressive frame | progressive | any->prog | MFX_WRN_INCOMPATIBLE_VIDEO_PARAM

## Wrong alignment of interlaced content

If surface holds interlaced content (major picture structure is not equal to `MFX_PICSTRUCT_PROGRESSIVE`) and surface height is not properly aligned (aligned to 16 but not aligned to 32) then encoder should return error `MFX_ERR_INVALID_VIDEO_PARAM` during initialization and runtime. (It is impossible to encode interlaced content if surface height is not aligned properly).

## Multiple-Segment Encoding

This chapter strictly defines encoder behavior for "Multiple-segment encoding" feature. See mediasdk-man for general description.
1. Encoder should create stream with equivalent SPS/PPS. That means that imported SPS/PPS may be different from encoded ones (not bit exact), but replacing of SPS/PPS in encoded stream by imported ones should produce compliance stream, that can be correctly decoded. For example, vui_parameters_present_flag may be set to 1 in imported SPS and to 0 in encoded SPS. 
2. If encoder can't produce equivalent SPS/PPS it has to return `MFX_ERR_INCOMPATIBLE_VIDEO_PARAM` from `Init`, `QueryIOSurf` and `Reset` functions and `MFX_ERR_UNSUPPORTED` from `Query`.
3. Encoder has to support `mfxExtCodingOptionSPSPPS` in `Init`, `Query`, `QueryIOSurf`, `GetVideoParam` and `Reset` functions.
4. Encoder should produce HRD conformance stream for encoded segment. Encoder can't assure HRD conformance for stream created by joining separately encoded segments.
5. Encoder puts encoded SPS/PPS in output stream. It is application responsibility to cut them off before joining segments.
6. Parameters from SPS/PPS buffer overwrite parameters in `mfxVideoParam` or any other extended buffers without returning any warning.

### Test case A

1. Several streams with different parameters are encoded, preferably by third party encoders, the more encoders the better. Test was performed for all streams. Let’s call it stream A.
2. Test application extracts SPS and PPS from stream A and initializes MSDK encoder with it.
3. Test application encodes additional raw YUV stream (stream B) and adds it to the end of the original stream without SPS and PPS. So output stream will look like this:`<original SPS><original PPS><original stream A, encoded by third party encoder><encoded by MSDK stream B>`
4. Test application decodes the whole output stream, extracts last frames (stream B) and compares them with original. Test passes if PSNR is good enough.

### Test case B
1. Prepare a set of encoding configurations that the HW/SW encoders can take.
2. Run the SDK encoders and extract the headers.
3. Start from scratch and encode with the imported headers above.
4. Compare the results from step (2) and (3). They should be exact.
5. Try importing configurations that SDK cannot take. 

## Direct3D9 surface allocation during initialization

All input or output surfaces of MSDK component have to belong to one surface chain and this chain has to belong to the same D3D device that has been used for component creation. Different components may use different chains. Input and output chains of the same component may be different. Decoder requests his chain during initialization from external allocator, other components (encoders and VPP) don’t request any external surfaces during initialization.

## Configuration Parameter Constraints

| Parameters | During SDK initialization | During SDK operation |
| --- | --- | --- |
`Width`<br>`Height` | H.264: Encoded frame size<br>MPEG-2: ignored | H.264: The values must be the same or bigger than the initialization values<br>MPEG-2: The values must be the same or bigger than aligned frame size

## MVC encoder

MVC encoder has the same behavior as AVC one except next cases:
1. Frame rate in `mfxVideoParam::mfxInfoMFX::mfxFrameInfo::FrameRateExtN/D` is frame rate of single view. It is not number of all views in stream per second.  
2. Bit rate in `mfxVideoParam::mfxInfoMFX::TargetKbps/MaxKbps` and so on is whole bitstream bitrate, that includes all views.
3. In both modes of Query function MVC encoder ignores all fields in `mfxExtMVCSeqDesc` buffer except `NumView`, `NumViewId`, `NumOP`, `NumRefsTotal`. MVC query function is used if `mfxVideoParam::mfxInfoMFX::CodecId` is set to `MFX_CODEC_AVC` and `CodecProfile` to `MFX_PROFILE_AVC_MULTIVIEW_HIGH` or `MFX_PROFILE_AVC_STEREO_HIGH`. In other cases AVC Query is used.
4. If different views have different levels then `GetVideoParam` returns highest level among all views in `mfxVideoParam::mfxInfoMFX::CodecLevel`.
5. `mfxBitstream::FrameType` is type of base view frame. No information about other views is available. There is no way to show it in current API.

## 4Kx4K resolution

IVB HW supports up to 4096x4096 resolution but according to AVC standard maximum supported level is 5.1 and it allows maximum frame size up to 36 864 MBs, for example 4096x2304. If resolution in VideoParam exceeds this limit encoder still has to encode stream but return `MFX_WRN_INCOMPATIBLE_VIDEO_PARAM` warning from `Init` or `Reset`. This requirement is applied for both HW and SW encoders.

## mfxExtVideoSignalInfo

Encoder's Query function checks variables in this structure for compatibility with standard and returns `MFX_ERR_INVALID_VIDEO_PARAM` if incompatibility is found. Wrong value is zeroed.

## Frame type in mfxEncodeCtrl

Next combinations of frame type flags are supported.

Display order:

| | |
| --- | --- |
`MFX_FRAMETYPE_I | REF | IDR`   | reference IDR frame
`MFX_FRAMETYPE_I | REF`         | I frame, (current implementation inserts IDR, not supported before API 1.3)
`MFX_FRAMETYPE_I`               | non-reference I frame, (current implementation inserts IDR, not supported before API 1.3)
`… | xI | xREF`                 | IDR/Iref field pair
`… | xI`                        | IDR/Iref field pair
`… | xP | xREF`                 | IDR/Pref field pair
`… | xP`                        | IDR/Pref field pair

Encoded order:

| | |
| --- | --- |
`MFX_FRAMETYPE_I`			    | non-reference I frame
`MFX_FRAMETYPE_I | REF`         | reference I frame, non IDR
`MFX_FRAMETYPE_I | REF | IDR`   | reference IDR frame
`MFX_FRAMETYPE_P`               | non-reference P frame
`MFX_FRAMETYPE_P | REF`         | reference P frame
`MFX_FRAMETYPE_B`               | non-reference B frame
`MFX_FRAMETYPE_B | REF`         | reference B frame (non ref for SNB, HW limitation)
`… | xI | xREF`                 | IDR/Iref field pair
`… | xI`                        | IDR/Iref field pair
`… | xP | xREF`                 | IDR/Pref field pair
`… | xP`                        | IDR/Pref field pair

All other combinations lead to error.

## Profile and Level handling

It is recommended in newly developed code to preserve profiler and level if they conflict with other initialization parameters. Recommended behavior is to return error `MFX_ERR_IVALID_VIDEO_PARAM` if such conflict can't be resolved. In existent code it is allowed to change profile and level.

## Picture Timing SEI

Encoder inserts PT SEI message in encoded bitstream if HRD information in stream is enabled (`mfxExtCodingOption::VuiNal/VclHrdParameters` is ON) OR PT SEI is enabled (`mfxExtCodingOption::PicTimingSEI` is ON). If both conditions are not satisfied PT SEI is not inserted and `mfxExtPictureTimingSEI` is ignored.
If PT SEI is enabled and the application attaches the mfxExtPictureTimingSEI structure to the mfxVideoParam structure during initialization, the H.264 encoder inserts the picture timing SEI message based on provided template in every access unit of coded bitstream. 

If PT SEI is enabled and the application attaches the mfxExtPictureTimingSEI structure to the `mfxEncodeCtrl` structure at runtime, the H.264 encoder inserts the picture timing SEI message based on provided template in access unit that represents current frame. If frame is encoded as two separate access units (two fields), encoder uses `mfxExtPictureTimingSEI->TimeStamp[0]` for first AU and `mfxExtPictureTimingSEI->TimeStamp[1]` for second. 

`VuiNalHrdParameters`/`VuiVclHrdParameters` equal to ON means that next elements are inserted in bitstream:
1. hrd_parameters
2. buffering_period SEI with `if( NalHrdBpPresentFlag )` section. 
3. pic_timing SEI with “`if( CpbDpbDelaysPresentFlag )` section. 

`PicTimingSEI` equal to ON means that next elements are inserted in bitstream:
1. pic_struct_present_flag = 1
2. buffering_period SEI with `seq_parameter_set_id` but possibly without `if( NalHrdBpPresentFlag )` and `if(VclHrdBpPresentFlag)` section
3. pic_timing SEI with `if( pic_struct_present_flag )` but possibly without `if( CpbDpbDelaysPresentFlag )` section

For both flags `MFX_CODINGOPTION_UNKNOWN` is equal to `MFX_CODINGOPTION_ON`.

## mfxExtAVCRefListCtrl

RejectedRefList - should permanently remove frame from DPB regardless of reference type, short or long term. LongTermRefList - marks frame as LT (second appearance of frame in this list does nothing). PreferredRefList is a suggestion to encoder how to reorder reference list. Encoder should try to reorder reference list as close to specified as possible, but still has flexibility to use different list from specified by application. For example, if reference frame specified by application is not present in DPB. 

In case of one list contradicts to another encoder has to return error to application. For example if frame included in both RejectedRefList and LongTermRefList.

## mfxExtAvcTemporalLayers

AVC encoder supports up to 4 dyadic temporal layers. Number of reference frames may be calculated from number of temporal layers as follow:

`NumRefFrames = 1 << (NumTempLayers - 2)`

Encoder should not use long-term reference for temporal scalability. `Query` corrects number of temporal layers and return warning. `Init` fails if number is wrong.

## Support of 422V format by MJPEG encoder

MJPEG encoder can accept input data in 422V subsampling format through the surfaces in `MFX_FOURCC_YUY2` format. Due to difference in the chroma component, the data cannot be packed in the same order as 422H. In this case, each 4x2 block of the original image, in planar format looks like this:

| | |
| --- | --- |
`Y2m;n` | `Y2m;n+1` | `Y2m;n+2` | `Y2m;n+3`
`Y2m+1;n` | `Y2m+1;n+1` | `Y2m+1;n+2` | `Y2m+1;n+3`

| | |
| --- | --- |
`Um;n` | `Um;n+1` | `Um;n+2` | `Um;n+3`

| | |
| --- | --- |
`Vm;n` | `Vm;n+1` | `Vm;n+2` | `Vm;n+3`

Should be converted to the following packed block of YUY2 surface:

| | |
| --- | --- |
`Y2m;n    Um;n     Y2m;n+1    Vm;n`   | `Y2m;n+2    Um;n+2  Y2m;n+3     Vm;n+2`
`Y2m+1;n  Um;n+1   Y2m+1;n+1  Vm;n+1` | `Y2m+1;n+2  Um;n+3  Y2m+1;n+3   Vm;n+3`

# Decode

## Frame size handling

1. During Init stage aligned frame size from stream (16 for height progressive, 32 for height interlaced, 16 for width) should be provided. Use size returned from DecodeHeader to avoid complications. For example, for progressive 15x15 stream correct initialization values are 16x16 and for interlaced 16(w)x32(h). If wrong combination is provided decoder will fail on first DecodeFrame then it will parse first SPS.
2. If decoder has encountered different frame size in stream, it will return error `MFX_ERR_INCOMPATIBLE_VIDEO_PARAM`.  For example, decoder was initialized for progressive (256,256) frame size. It will work for (250,250) stream, but fail for (240,240) stream.
3. Decoder ignores cropping on `Init`. 
4. Decoder set cropping in output surface.
5. Decoder may receive from allocator and may decode to surfaces which size is aligned (general MSDK rule) and is big enough to hold decoded frame. I.e. frame size in `DecodeFrame` may be bigger than specified in VideoParam at `Init`.

## New SPS warning

1. First SPS is not considered as new. Decoder returns no warning for it. For all other SPSs in stream decoder returns this warning (`MFX_WRN_VIDEO_PARAM_CHANGED`) regarding changes in SPS fields.
2. If several SPSs are encountered in stream side by side (no frame data between these SPSs), then for each one warning is returned. If such bunch of SPSs is encountered at the beginning of stream then only first header is processed without warning. For all following SPSs warning will be returned. So if three SPSs are located at the beginning if the stream two warnings will be returned.  

## Decoded Order

`mfxFrameSurface1::mfxFrameData::FrameOrder` should be set in sync part of DecodeFrameAsync to simplify skip frame processing. If surface contains skip frame it will have several consecutive frame numbers and application should show or encode this surface several times.   

## Payloads in Decoded Order

It is not always possible to calculate PTS of payloads in decoded order. Decoder should set it to -1 in this case. To find out what payload corresponds to this particular frame call GetPayload immediately after DecodeFrame. If returned by DecodeFrame frame has corresponding payload it should be returned by subsequent call of GetPayload. One frame may have several payloads, so several calls of GetPayload may be necessary.

## DecodeHeader

`DecodeHeader` function fills in fields in mfxVideoParam structure according to next table:

| Parameters | DecodeHeader |
| --- | --- |
FourCC | MFX_FOURCC_NV12
Width<br>Height | values from stream, aligned according to PicStruct: 16 for progressive, 32 for interlaced, 16 or 32 for unknown (depending on standard)
CropX, CropY<br>CropW, CropH | values from stream
AspectRatioW<br>AspectRatioH | values from stream or 0
FrameRateExtN<br>FrameRateExtD | values from stream or 0
PicStruct | MFX_PICSTRUCT_PROGRESSIVE or MFX_PICSTRUCT_UNKNOWN   
ChromaFormat | values from stream
Profile/level | values from stream

There are two different modes of operation. First one is AVC mode, when function is looking for SPS only  and second one is MVC mode when function parses complete AU looking for SPS and extensions. Function distinguishes these modes by mfxExtMVCSeqDesc buffer. If it is not attached, it is AVC mode, otherwise this is MVC mode. Note, that much bigger bitstream buffer is necessary in second mode, to hold complete AU.

## GetVideoParam

This function expects that par points to allocated by application structure. If application needs to retrieve contents of extended buffers it should provide correctly allocated buffers. Their pointers in mfxVideoParam::ExtParam and number of them in mfxVideoParam::NumExtParam. If no extended buffers are provided NumExtParam should be zero and ExtParam be NULL. If unknown for component extended buffer is provided component should return MFX_ERR_UNSUPPORTED.

## SetSkipMode

To disable deblocking in decoders call this function with mode = 0x22. There is no way to enable it back except reset or close/init procedure. This special mode is completely independent from general SetSkipMode operation. This workaround is used to simplify ELK testing, is implemented only in HW library and will be removed in future releases.

## DecodeFrameAsync

It is stated in mediasdk-man that "If the decoder returns MFX_ERR_NONE, the pointer surface_out points to the output frame in decoded or display order." It was decided to strengthen this statement like this: “The pointer surface_out points to the output frame in decoded or display order when and only when decoder returns MFX_ERR_NONE.” In other words if any error code or warning is returned then surface_out is NULL.

## PicStruct handling at Init and at Runtime

Decoder uses next table to specify PicStruct of decoded frame. See also Encode “PicStruct handling at Init and at Runtime” for more information.

| coded as (AVC PicStruct value) | displayed as | MFX picStruct value (output) |
| --- | --- | --- |
progressive (0) | one progressive frame | `PROGRESSIVE`
progressive (7) | two identical progressive frames | `PROGRESSIVE | FRAME_DOUBLING`
progressive (8) | three identical progressive frames | `PROGRESSIVE | FRAME_TRIPLING`
progressive (4, telecine) | two fields: bottom, top | `PROGRESSIVE | FIELD_BFF`
progressive (3, telecine) | two fields: top, bottom | `PROGRESSIVE | FIELD_TFF`
progressive (6, telecine) | three fields: bottom, top, bottom | `PROGRESSIVE | FIELD_BFF | FIELD_REPEATED`
progressive (5, telecine) | three fields: top, bottom, top | `PROGRESSIVE | FIELD_TFF | FIELD_REPEATED`
fields (2, including MBAFF) | two fields: bottom, top | `FIELD_BFF`
fields (1, including MBAFF) | two fields: top, bottom | `FIELD_TFF`

## Wrong alignment of interlaced content

If surface holds interlaced content (PicStruct is equal to TFF or BFF) and surface height is not properly aligned (aligned to 16 but not aligned to 32) then decoder should return error MFX_ERR_INVALID_VIDEO_PARAM during initialization and runtime. (It is impossible to decode interlaced content in improperly aligned surface)

## mfxFrameInfo parameters in Async pipeline

Decoder has to set variables specified in table below in synchronous part of DecodeFrameAsync and doesn’t touch them (modify or use) in asynchronous part (during task execution). Application may change these parameters directly after DecodeFrameAsync calls before sending surface to downstream component (encoder or VPP). For example, application wants to crop part of the decoded stream and enlarge it to whole frame by VPP. In this case it has to change crop variables directly after DecodeFrameAsync call but before calling RunFrameVPPAsync.

| | |
| --- | --- |
FourCC | n/a
Width | n/a
Height | n/a
CropX | set
CropY | set
CropW | set
CropH | set
FrameRateExtN | set
FrameRateExtD | set
AspectRatioW | set
AspectRatioH | set
PicStruct | set
ChromaFormat | n/a

Legend:<br>n/a - decoder doesn’t change this value<br>set - decoder set this value in sync part

## Priority of Init parameters

If application explicitly sets mfxVideoParam::mfxFrameInfo::FrameRateExtN / FrameRateExtD or AspectRatioW / AspectRatioH during initialization then decoder has to use these values during decoding regardless of values from stream and should not update them on any new SPS. If application sets them to 0, then decoder has to use values from stream and update them on each SPS.

Because GetVideoParam returns "current working parameters" it has to return values that application provided at Init. To get value from stream application has to use DecodeHeader.

## Direct3D9 surface allocation during initialization

See the same chapter for encoder.

## Configuration Parameter Constraints

| Parameters | During SDK initialization | During SDK operation |
| --- | --- | --- |
Width<br>Height | Aligned frame size from stream. | The values must be the same or bigger than the initialization values.

## mfxExtCodingOptionSPSPPS support

MVC decoder doesn’t support it. AVC supports both SPS and PPS. MPEG2 only SPS, PPS pointer has to be NULL. VC1 supports only SPS (sequence header), PPS has to be NULL, no support for entry point header.
 
# VPP
## Init

If list of algorithms in mfxExtVPPDoNotUse buffer contains more than one entry for the same algorithm, then function should fail with MFX_ERR_UNDEFINED_BEHAVIOR.   

## Query

See Decode Query.

## GetVideoParam

See Decode GetVideoParam

## PicStruct handling at Init and at Runtime

VPP may be initialized in three different modes in regard to picture structure. First one is Pass Trough (PT), when VPP preserves original picture structure (progressive input produces progressive output , interlaced input produces interlaced output with the same field order, TFF or BFF). Second one is Dynamic Deinterlacing (DD), when VPP performs deinterlacing if input picture is interlaced and uses PT mode if input picture is progressive. Third one is inverse telecine (ITC). It is switched on if at init stage input picture is interlaced, output is progressive and input frame rate is 30 frames per second and output is 24.  ITC is static (not dynamic) because ITC algorithm needs several frames to find cadence and can’t be arbitrary switched on or off without severe quality degradation. 

### Initialization table

| Input/Output | Progressive (O) | Interlaced (O) | UNKNOWN (O) |
| --- | --- | --- | --- |
Progressive (I) | PT | PT | PT
Interlaced (I) | DD or ITC (30->24fps) | PT | PT
UNKNOWN (I) | DD | PT | PT

### Runtime table for Pass Trough (PT) mode

| Input/Output | Progressive (O) | Interlaced (O) | UNKNOWN (O) |
| --- | --- | --- | --- |
Progressive (I) | PT, OK  | PT, warning | PT, OK
Interlaced (I) | PT, warning  | PT, OK or warning (if TFF->BFF or BFF->TFF) | PT, OK 
UNKNOWN (I) | runtime error  | runtime error  | runtime error 

warning - means that VPP returns MFX_WRN_INCOMPATIBLE_VIDEO_PARAM from RunFrameVPPAsync,  SyncOperation returns MFX_ERR_NONE.
runtime error - means that RunFrameVPPAsync returns MFX_ERR_UNDEFINED_BEHAVIOR.

### Runtime table for Dynamic Deinterlacing (DD) mode

| Input/Output | Progressive (O) | Interlaced (O) | UNKNOWN (O) |
| --- | --- | --- | --- |
Progressive (I) | PT, OK  | PT, warning | PT, OK
Interlaced (I) | DD, OK  | DD, warning  | DD, OK
UNKNOWN (I) | runtime error  | runtime error  | runtime error 

### Runtime table for Inverse Telecine (ITC) mode

| Input/Output | Progressive (O) | Interlaced (O) | UNKNOWN (O) |
| --- | --- | --- | --- |
Progressive (I) | ITC, warning | ITC, warning | ITC, warning
Interlaced (I) | ITC, OK  | ITC, warning  | ITC, OK
UNKNOWN (I) | runtime error  | runtime error  | runtime error 

### Decorative flags 

Picture structure may also hold so called decorative flags. They are used to specify how picture should be displayed. See also Decode and Encode “PicStruct handling at Init and at Runtime” chapters.

```C
PROGRESSIVE
PROGRESSIVE | FRAME_DOUBLING
PROGRESSIVE | FRAME_TRIPLING
PROGRESSIVE | FIELD_BFF
PROGRESSIVE | FIELD_TFF
PROGRESSIVE | FIELD_BFF | FIELD_REPEATED
PROGRESSIVE | FIELD_TFF | FIELD_REPEATED
FIELD_BFF
FIELD_TFF
```

In this table all possible combinations of major picture structure with decorative flags are given. Decorative flags are gray. All other combinations are invalid and will produce run time error MFX_ERR_UNDEFINED_BEHAVIOR.

### PicStruct update

If input and output picture structures are not equal then VPP may update output picture structure. In general VPP tries to keep output structure equal to input, but there are some exception when it can’t be done. Below are all possible cases:
1. If output picture structure is UNKNOWN, then input structure is copied to output in PT mode and set to `PROGRESSIVE` in DD mode.
2. If major flags are equal then decorative flags are added to output PicStruct. Example, input `PROGRESSIVE | FIELD_TFF` output `PROGRESSIVE`, output is updated to `PROGRESSIVE | FIELD_TFF`.
3. If major flags are different in PT mode, then output flag remain untouched and VPP return warning MFX_WRN_INCOMPATIBLE_VIDEO_PARAM from RunFrameVPPAsync. For example, input `FIELD_BFF` output `FIELD_TFF`.
4. If major flags are different in DD mode, then output structure is either UNKNOWN, case one here or PROGRESSIVE and remains unchanged.

## Wrong alignment of interlaced content

If surface holds interlaced content (PicStruct is equal to TFF or BFF) and surface height is not properly aligned (aligned to 16 but not aligned to 32) then VPP should ignore this during initialization and runtime and process content as interlaced.

## mfxFrameInfo parameters in Async pipeline

VPP has to set variables specified in table below in synchronous part of RunFrameVPPAsync and doesn’t touch them (modify or use) in asynchronous part (during task execution). Application may change these parameters directly after RunFrameVPPAsync calls before sending surface to downstream component (encoder or other VPP). For example, application wants to encode progressive frame as interlaced. VPP doesn’t provide such functionality and application has to change PicStruct variable from MFX_PICSTRUCT_PROGRESSIVE to MFX_PICSTRUCT_FIELD_TFF directly after RunFrameVPPAsync call but before calling EncodeFrameAsync.

| | |
| --- | --- |
FourCC | n/a
Width | n/a
Height | n/a
CropX | n/a
CropY | n/a
CropW | n/a
CropH | n/a
FrameRateExtN | set
FrameRateExtD | set
AspectRatioW | set
AspectRatioH | set
PicStruct | set
ChromaFormat | n/a

Legend:<br>n/a - VPP doesn’t change this value<br>set – VPP set this value in sync part

## Direct3D9 surface allocation during initialization

See the same chapter for encoder.

## FrameOrder and PTS handling

VPP uses mfxExtVPPFrameRateConversion to select TimeStamp and FrameOrder calculation algorithm. See main manual for details. Two algorithms are supported:
1. `MFX_FRCALGM_PRESERVE_TIMESTAMP`. VPP doesn't calculate `mfxFrameData::TimeStamp` or `mfxFrameData::FrameOrder` for added frames (for example due to frame rate conversion) and sets them to `MFX_TIMESTAMP_UNKNOWN` and `MFX_FRAMEORDER_UNKNOWN`.
2. `MFX_FRCALGM_DISTRIBUTED_TIMESTAMP`. This algorithm is suitable for both type of conversions from low to high frame rate and back. It inserts / drops frames based on their time stamps not on input output frame rate ratio. As result gaps in input time stamps are corrected by repeating / dropping of input frames.

Algorithm is based on two simple rules:
1. to output previous input frame if time stamp of current input frame is in the future relative to current output time stamp.
2. to drop previous input frame if time stamp of current input frame is in the past relative to current output time stamp.

Formal description of algorithm:
```C
    delta= 0.5 * min(1./FRI,1/FRO);
    Tout[0]=0.;
    i = j = 0;

    for each input frame {
        if( Tout[j] + delta < Tin[i+1] ){
            //output i-th input frame
            //sets its time stamp to Tout[j]
            Tout[j+1]=Tout[j] + 1./FRO;
            j++;
        } else {
            //go to next input frame
            //drop it if it has not been outputed before
            i++;
        }
    }
```
Usage of delta is not mandatory. It makes algorithm more tolerant to rounding errors and helps to keep input time stamp if it is well aligned to output one.

FrameOrder of output frames for this algorithm is monotonically increasing sequence for both low to high and to high to low frame rate conversions. 

### Example:

`MFX_FRCALGM_PRESERVE_TIMESTAMP`

| in TimeStamp | in FrameOrder | out TimeStamp | out FrameOrder |
| --- | --- | --- | --- |
1 | 1 | 1 | 1
n/a | n/a | -1 | -1
3 | 2 | 3 | 2
n/a | n/a | -1 | -1
5 | 3 | 5 | 3
n/a | n/a | -1 | -1

`MFX_FRCALGM_DISTRIBUTED_TIMESTAMP`

| in TimeStamp | in FrameOrder | out TimeStamp | out FrameOrder |
| --- | --- | --- | --- |
1 | 1 | 1 | 1
n/a | n/a | 2 | 2
3 | 2 | 3 | 3
n/a | n/a | 4 | 4
5 | 3 | 5 | 5
n/a | n/a | 6 | 6

These algorithms are applied for explicit frame rate conversion (when input frame rate is not equal to output during initialization), inverse telecine and  any deinterlacing filters that change frame rate (60i -> 60p for example).

For other processed frames VPP passes through `mfxFrameData::TimeStamp` and `mfxFrameData::FrameOrder`.

## Multi-view in VPP

Application should specify list of view to be processed in mfxExtMVCSeqDesc buffer during VPP initialization. If list of view is incorrect then VPP should return `MFX_ERR_INVALID_VIDEO_PARAM`.

# Opaque memory (feature)

Decoder, VPP and encoder have to check mfxExtOpaqueSurfaceAlloc in Reset. This structure should be exactly the same as structure passed at Init stage. Violation of this requirement leads to error MFX_ERR_INCOMPATIBLE_VIDEO_PARAM. If this structure is present at Init but not at Reset or vice versa then the same error is returned. 

# Opaque memory (manual)

his chapter contains original version of opaque memory manual from developers' point of view. Based on it two separate chapters were created, one in mediasdk-man the other in mediasdkusr-man manuals. 

## Overview

To simplify application development and hide memory management complexity from application SDK introduces opaque memory type. It behaves similar to other memory types. Frame-locking mechanism remains the same (behavior of mfxFrameData::Locked). Most of the mfxFrameSurface1 members have the same meaning - frame order, frame type, width, height, crop and so on. The only difference is “real” frame data. They are not accessible by application and stored in allocated by SDK internal memory. To assure best possible on current system performance SDK selects necessary storage place for frame data automatically.

Note, let’s consider pipeline SW Dec1 + HW Dec2 -> SW Enc, it works fine with SW memory but slow with HW, due to double copy in Dec1 to Enc chain. Thus it is better to allow application to select memory type in such pipelines. In simple pipelines like Dec->Enc, it doesn’t matter what type to use, so app may choose any.

It is recommended to use opaque memory whenever possible to increase performance. For example fastest transcoding pipeline looks like this:

`bitstream -> DEC -> opq mem -> VPP -> opq mem -> ENC -> bitstream`

This pipeline is also supported:

`bitstream -> DEC -> video mem -> VPP -> opq mem -> ENC -> bitstream`

To use opaque memory application has to initialize component with mfxVideoParam::IOPattern field set to MFX_IOPATTERN_IN/OUT_OPAQUE_MEMORY. All combinations of input/output patterns are supported, and application may, for example, use this to retrieve data from opaque memory. For this initialize VPP as opaque memory input and system or video memory output. 

## Application level

Suppose app builds pipeline A -> B. App calls QueryIOSurf for A and for B. 
    In - MFX_IOPATTERN_IN/OUT_OPAQUE_MEMORY
    Out – suggested number of surfaces (Na, Nb) and native surf type (Ta, Tb)

App calculates number of surfaces in shared surface pool N and chooses memory type T.

Note, application doesn’t know surface type  
```C
if( Ta==Tb )
   T=Ta; N=Na + Nb – AsyncDepth;
else 
   T = Ta; N = Na; (also possible T = Tb; N = Nb; )
```

App creates pool of opaque surfaces. This is N empty mfxFrameSurface1 structures Data.Y= Data.U= Data.V= Data.A= Data.MemId=0

Application initializes both components A and B with this opaque surface pool and chosen memory type (use mfxExtOpaqueSurfaceAlloc).

During runtime each surface from opaque pool looks for app exactly like real surface. It has width, height, may be locked and so on. App finds free surface and sends it to MSDK for processing. The only difference is that this surface has no real data and app can’t access its content.

## Library level

### Initialization

Component receives mfxExtOpaqueSurfaceAlloc with two opaque surface pools and memory types for input and output.

Component always requests core to allocate “real” memory for this opaque surface pool. During processing of this request core firstly increases reference counter of the surface pool. Then core checks if “real” memory has been already allocated. To do this check properly sessions should be joined before component initialization. This is first reason for joining.

If memory has been already allocated then core does nothing more. Otherwise core performs allocations and maps opaque surface pool to real memory. Because application may choose any type of surfaces from component A or B, it is possible that component receives unknown for it memory type. This is second reason why sessions have to be joined before initialization, to share allocators, so each component may allocate any type of memory.

### Runtime

Core keeps opaque to real surface mapping. When component receives opaque surface from application it requests core for real surface (DX9 or system memory). Then it uses real surface for processing as usual except component has to lock/unlock both surfaces opaque and “real” one.

### Closing time (life time of real surface)

It is important to properly free allocated “real” memory. As follows from algorithm described above “real” memory is allocated by component that was initialized first. To avoid access violations “real” memory should be freed by last closed component. To do so each component during closing has to notify core that it doesn’t use this opaque pool anymore. Core decreases reference counter for opaque pool and frees real memory when counter became zero.

## Plugin level

Plugin has exactly the same capability as other MSDK components and should support the same behavior. To support opaque memory in plugin four additional functions were added to core interface MapOpaqueSurface, UnmapOpaqueSurface, GetRealSurface, GetOpaqueSurface and also complete internal allocator was exposed to allow access to unknown for plugin types of memory.

### Initialization

Plugin calls MapOpaqueSurface function from the core interface. This function increases reference counter for this opaque pool (thread safe), then checks if real memory was allocated for this pool and if not allocates it. No additional processing from plugin is necessary. 

Note, no real surfaces returned at this stage. If plugin needs them it can call GetRealSurface for each opaque surface. 

### Runtime

Plugin calls GetRealSurface function from the core interface to convert input/output opaque surface to “real” one. Then plugin processes them as usual. If it is system memory then pointers to memory are available in mfxFrameSurface1::mfxFrameData::Y/U/V. For video memory plugin can get handle by calling mfxCoreInterface::FrameAllocator::GetHDL (mfxFrameSurface1::mfxFrameData::MemId). For unknown memory type plugin can get pointers by calling mfxCoreInterface:: FrameAllocator::Lock (mfxFrameSurface1::mfxFrameData::MemId). 

To simplify plugin development opposite to GetRealSurface function was added – GetOpaqueSurface. It may be useful for example if plugin keeps only real surfaces in internal buffer and needs to update some information in opaque surface after processing, set PTS for example. 

### Closing time

Plugin calls UnmapOpaqueSurface function. This function decreases internal opaque pool reference counter and deletes real surface pool if counter is zero. It is expected that plugin has unlocked all opaque and real surfaces before calling this function. No additional processing from plugin is required.

### Memory allocation.

All real surfaces returned from GetRealSurface (mfxFrameSurface1 structures) are allocated by MSDK, plugin should not free them explicitly. These surfaces should not be accessed after plugin called UnmapOpaqueSurface. 

## API extensions

SDK strictly requires that for opaque data next values in mfxFrameData are set to zero: Pitch, Y, U, V, A, MemId.

Next enumerators were extended:
```C
enum {
    ...
    MFX_IOPATTERN_IN_OPAQUE_MEMORY  = 0x04,
    ...
    MFX_IOPATTERN_OUT_OPAQUE_MEMORY  = 0x40,
};
```

Next Ext buffer was added
```C
typedef struct {
    mfxExtBuffer    Header;
    mfxU32      reserved1[2];
    struct {
        mfxFrameSurface1 **Surfaces;
        mfxU32  reserved2[5];
        mfxU16  Type;
        mfxU16  NumSurface;
    } In, Out;  
} mfxExtOpaqueSurfaceAlloc;
```

Four functions and internal allocator were added to core interface:
```C
typedef struct mfxCoreInterface {
    ...
    mfxFrameAllocator FrameAllocator;
    ...
    mfxStatus (*MapOpaqueSurface)(mfxHDL pthis, mfxU32  num, mfxU32  type, mfxFrameSurface1 **op_surf);
    mfxStatus (*UnmapOpaqueSurface)(mfxHDL pthis, mfxU32  num, mfxU32  type, mfxFrameSurface1 **op_surf);

    mfxStatus (*GetRealSurface)(mfxHDL pthis, mfxFrameSurface1 **surf, mfxFrameSurface1 *op_surf);
    mfxStatus (*GetOpaqueSurface)(mfxHDL pthis, mfxFrameSurface1 **op_surf, mfxFrameSurface1 *surf);

    mfxHDL reserved4[4];
} mfxCoreInterface;
```
