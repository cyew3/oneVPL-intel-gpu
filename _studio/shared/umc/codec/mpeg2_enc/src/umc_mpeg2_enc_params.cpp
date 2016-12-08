//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2002-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_MPEG2_VIDEO_ENCODER)

#include "umc_mpeg2_enc_defs.h"
#include "vm_strings.h"
#include "umc_par_file_util.h"
#include <ipps.h>

using namespace UMC;

///* identifies valid profile/level combinations */
//static Ipp8u Profile_Level_Defined[5][4] =
//{
///* HL   H-14 ML   LL  */
//  {1,   1,   1,   0},  /* HP   */
//  {0,   1,   0,   0},  /* Spat */
//  {0,   0,   1,   1},  /* SNR  */
//  {1,   1,   1,   1},  /* MP   */
//  {0,   0,   1,   0}   /* SP   */
//};

static struct LevelLimits
{
  Ipp32s f_code[2];
  Ipp32s hor_size;
  Ipp32s vert_size;
  Ipp32s sample_rate;
  Ipp32s bit_rate;         /* Mbit/s */
  Ipp32s vbv_buffer_size;  /* 16384 bit steps */
} MaxValTbl[5] =
{
  {{9, 5}, 1920, 1152, 62668800, 80, 597}, /* HL */
  {{9, 5}, 1440, 1152, 47001600, 60, 448}, /* H-14 */
  {{8, 5},  720,  576, 10368000, 15, 112}, /* ML */
  {{7, 4},  352,  288,  3041280,  4,  29}, /* LL */
  {{9, 9}, 8192, 8192, 0x7fffffff, 1000,  10000}  /* some limitations for unlimited */
};

#define SP   5
#define MP   4
#define SNR  3
#define SPAT 2
#define HP   1

#define LL  10
#define ML   8
#define H14  6
#define HL   4

/* chroma_format for MPEG2EncoderParams::chroma_format*/
#define CHROMA420 1
#define CHROMA422 2
#define CHROMA444 3

#define error(Message) \
{ \
  vm_debug_trace(VM_DEBUG_ERROR, VM_STRING(Message)); \
  return UMC_ERR_FAILED; \
}

/* ISO/IEC 13818-2, 6.3.11 */
/*  this pretty table was designed to
    avoid coincidences with any GPL code */
static VM_ALIGN16_DECL(Ipp16s) DefaultIntraQuantMatrix[64] =
{
     8,
     16,

     19,        22, 26, 27,             29, 34, 16,
     16,        22,         24,         27,         29,
     34,        37,             19,     22,             26,
     27,        29,             34,     34,             38,
     22,        22,             26,     27,             29,
     34,        37,             40,     22,             26,
     27,        29,         32,         35,         40,
     48,        26, 27, 29,             32, 35, 40,
     48,        58,                     26,
     27,        29,                     34,
     38,        46,                     56,
     69,        27,                     29,
     35,        38,                     46,
     56,        69,                     83
};

static const Ipp64f ratetab[8]=
    {24000.0/1001.0,24.0,25.0,30000.0/1001.0,30.0,50.0,60000.0/1001.0,60.0};
static const Ipp32s aspecttab[3][2] =
    {{4,3},{16,9},{221,100}};

MPEG2EncoderParams::MPEG2EncoderParams()
{
  Ipp32s i;

  *IntraQMatrix = 0;
  *NonIntraQMatrix = 0;

  lFlags = FLAG_VENC_REORDER;
  info.bitrate = 5000000;
  info.framerate = 30;
  numEncodedFrames = 0;
  qualityMeasure = -1;

  info.interlace_type = PROGRESSIVE;          // progressive sequence
  //progressive_frame = 1; // progressive frame
  CustomIntraQMatrix = 0;
  CustomNonIntraQMatrix = 0;
  for(i=0; i<64; i++) {   // for reconstruction
    IntraQMatrix[i] = DefaultIntraQuantMatrix[i];
    NonIntraQMatrix[i] = 16;
  }
  IPDistance = 3;         // distance between key-frames
  gopSize = 15;           // size of GOP
  strictGOP = 0;

  info.framerate = 30;
  info.aspect_ratio_width = 4;
  info.aspect_ratio_height = 3;
  profile = MP;
  level = ML;
  info.color_format = YUV420;
  //repeat_first_field = 0;
  //top_field_first = 0;    // display top field first
  //intra_dc_precision = 0; // 8 bit
  FieldPicture = 0;       // field or frame picture (if progframe=> frame)
  VBV_BufferSize = 112;
  low_delay = 0;
  frame_pred_frame_dct[0] = 1;
  frame_pred_frame_dct[1] = 1;
  frame_pred_frame_dct[2] = 1;
  intraVLCFormat[0] = 1;
  intraVLCFormat[1] = 1;
  intraVLCFormat[2] = 1;
  altscan_tab[0] = 0;
  altscan_tab[1] = 0;
  altscan_tab[2] = 0;
  mpeg1 = 0;               // 1 - mpeg1 (unsupported), 0 - mpeg2
  idStr[0] = 0;            // user data to put to each sequence
  UserData = (Ipp8u*)idStr;
  UserDataLen = 0;
  numThreads = 1;
  performance = 0;
  encode_time = 0;
  motion_estimation_perf = 0;
  inputtype = 0;   // unused
  constrparms = 0; // unused
  me_alg_num = 3;
  me_auto_range = 1;
  allow_prediction16x8 = 0;
  rc_mode = RC_CBR;
  quant_vbr[0] = 0;
  quant_vbr[1] = 0;
  quant_vbr[2] = 0;

  rangeP[0] = 8*IPDistance;
  rangeP[1] = 4*IPDistance;
  rangeB[0][0] = rangeB[1][0] = rangeP[0] >> 1;
  rangeB[0][1] = rangeB[1][1] = rangeP[1] >> 1;

  color_description = 0;
  video_format = 0;
  color_primaries = 0;
  transfer_characteristics = 0;
  matrix_coefficients = 0;
  display_horizontal_size = 0;
  display_vertical_size = 0;
  conceal_tab[0] = 0;
  conceal_tab[1] = 0;
  conceal_tab[2] = 0;

#ifdef MPEG2_USE_DUAL_PRIME
  enable_Dual_prime=0;
#endif//MPEG2_USE_DUAL_PRIME
}

MPEG2EncoderParams::~MPEG2EncoderParams()
{
}

Status MPEG2EncoderParams::ReadQMatrices(vm_char* IntraQMatrixFName, vm_char* NonIntraQMatrixFName)
{
  IntraQMatrixFName;
  NonIntraQMatrixFName;
#if 0
  Ipp32s i, temp;
  vm_file *InputFile;

  if( IntraQMatrixFName==0 || IntraQMatrixFName[0] == 0 || IntraQMatrixFName[0] == '-' )
  {
    // use default intra matrix
    CustomIntraQMatrix = 0;
    for(i=0; i<64; i++)
      IntraQMatrix[i] = DefaultIntraQuantMatrix[i];
  }
  else
  {
    // load custom intra matrix
    CustomIntraQMatrix = 1;
    if( 0 == (InputFile = vm_file_open(IntraQMatrixFName,VM_STRING("rt"))) )
    {
      vm_debug_trace1(VM_DEBUG_ERROR, VM_STRING("Can't open quant matrix file %s\n"), IntraQMatrixFName);
      return UMC_ERR_OPEN_FAILED;
    }

    for(i=0; i<64; i++)
    {
      vm_file_fscanf( InputFile, VM_STRING("%d"), &temp );
      if( temp < 1 || temp > 255 )
      {
        vm_file_fclose( InputFile );
        error("invalid value in quant matrix\n");
      }
      IntraQMatrix[i] = (Ipp16s)temp;
    }

    vm_file_fclose( InputFile );
  }

  if (NonIntraQMatrixFName == 0 || NonIntraQMatrixFName[0] == 0 || NonIntraQMatrixFName[0] == '-')
  {
    // use default non-intra matrix
    CustomNonIntraQMatrix = 0;
    for(i=0; i<64; i++)
      NonIntraQMatrix[i] = 16;
  }
  else
  {
    // load custom non-intra matrix
    CustomNonIntraQMatrix = 1;
    if( 0 == (InputFile = vm_file_open(NonIntraQMatrixFName,VM_STRING("rt"))) )
    {
      vm_debug_trace1(VM_DEBUG_ERROR, VM_STRING("Couldn't open quant matrix file %s\n"), NonIntraQMatrixFName);
      return UMC_ERR_OPEN_FAILED;
    }

    for(i=0; i<64; i++)
    {
      vm_file_fscanf( InputFile, VM_STRING("%d"), &temp );
      if( temp < 1 || temp > 255 )
      {
        vm_file_fclose( InputFile );
        error("invalid value in quant matrix\n");
      }
      NonIntraQMatrix[i] = (Ipp16s)temp;
    }

    vm_file_fclose( InputFile );
  }

  return UMC_OK;
#else
  return UMC_ERR_FAILED;
#endif
}

Status MPEG2EncoderParams::Profile_and_Level_Checks()
{
  Ipp32s i,j,k;
  struct LevelLimits *MaxVal;
  Ipp32s newLevel = level;
  Ipp32s newProfile = profile;
  Status ret = UMC_OK;

  if( profile == SNR || profile == SPAT )
    error("The encoder doesn't support scalable profiles\n");

  // check level, select appropriate when it is wrong
  if( newLevel < HL ||
    newLevel > LL ||
    newLevel & 1 )
    newLevel = LL;
  if(info.color_format != YUV420 && newLevel == LL )
    newLevel = ML;
  if( info.framerate > ratetab[5 - 1] && newLevel >= ML )
    newLevel = H14;

  MaxVal = &MaxValTbl[(newLevel - 4) >> 1];
  while( info.clip_info.width > MaxVal->hor_size ||
         info.clip_info.height > MaxVal->vert_size ||
         info.clip_info.width * info.clip_info.height * info.framerate > MaxVal->sample_rate ||
         info.bitrate > 1000000 * (Ipp32u)MaxVal->bit_rate )
  {
    if(newLevel > HL) {
      newLevel -= 2;
      MaxVal --;
    } else {
      vm_debug_trace(VM_DEBUG_WARNING, VM_STRING("encoding parameters exceed highest level limitations\n"));
      MaxVal = &MaxValTbl[4];
      break;
    }
  }

  // check profile values
  if( newProfile < HP || newProfile > SP ) {
    newProfile = MP;
  }
  if(info.color_format != YUV420 && info.color_format != NV12) {
    newProfile = HP;
    if(info.color_format == YUV444) {
      vm_debug_trace(VM_DEBUG_WARNING, VM_STRING("CHROMA444 has no appropriate prifile\n"));
    }
  }

  // check profile constraints
  if( newProfile == SP && IPDistance != 1 ) {
    newProfile = MP;
  }

  // check profile - level combination
  if(newProfile == SP && newLevel != ML)
    newProfile = MP;
  if(newProfile == HP && newLevel == LL) {
    newLevel = ML;
    MaxVal = &MaxValTbl[(newLevel - 4) >> 1];
  }

  //if( newProfile != HP && intra_dc_precision == 3 )
  //  newProfile = HP;

  //// SP, MP: constrained repeat_first_field
  //if( newProfile >= MP && repeat_first_field &&
  //    ( info.framerate <= ratetab[2 - 1] ||
  //      info.framerate <= ratetab[6 - 1] && info.interlace_type == PROGRESSIVE ) ) {
  //  vm_debug_trace(VM_DEBUG_WARNING, VM_STRING("repeat_first_field set to zero\n"));
  //  repeat_first_field = 0;
  //}

  if(IPDistance < 1) IPDistance = 1;
  if(gopSize < 1)    gopSize = 1;
  if(gopSize > 132)  gopSize = 132;
  if(IPDistance > gopSize) IPDistance = gopSize;

  // compute f_codes from ranges, check limits, extend to the f_code limit
  for(i = 0; i < IPP_MIN(IPDistance,2); i++) // P, B
    for(k=0; k<2; k++) {                     // FW, BW
      Ipp32s* prange;
      if (i == 0) {
        if( k==1)
          continue; // no backward for P-frame
        prange = rangeP;
      } else {
        prange = rangeB[k];
      }
      for(j=0; j<2; j++) {                   // x, y
        Ipp32s req_f_code;
        RANGE_TO_F_CODE(prange[j], req_f_code);
        if( req_f_code > MaxVal->f_code[j] ) {
          vm_debug_trace(VM_DEBUG_WARNING, VM_STRING("search range is greater than permitted in specified Level\n"));
          req_f_code = MaxVal->f_code[j];
          prange[j] = 4<<req_f_code;
        }
        if (i==0) // extend range only for P
          prange[j] = 4<<req_f_code;
      }
    }

  // Table 8-13
  if( VBV_BufferSize > MaxVal->vbv_buffer_size )
    VBV_BufferSize = MaxVal->vbv_buffer_size;

  if(newLevel != level) {
    vm_debug_trace(VM_DEBUG_WARNING, VM_STRING("Level changed\n"));
    level = newLevel;
  }
  if(newProfile != profile) {
    vm_debug_trace(VM_DEBUG_WARNING, VM_STRING("Profile changed\n"));
    profile = newProfile;
  }

  return ret;
}

Status MPEG2EncoderParams::RelationChecks()
{
  Status ret = UMC_OK;

  if( mpeg1 ) {
    if(info.interlace_type != PROGRESSIVE)
    {
      vm_debug_trace(VM_DEBUG_WARNING, VM_STRING("setting progressive_sequence = 1\n"));
      info.interlace_type = PROGRESSIVE;
    }
    vm_debug_trace(VM_DEBUG_WARNING, VM_STRING("MPEG1 is not implemented. Setting to MPEG2\n"));
    mpeg1 = 0;
  }

  //if(aspectRatio < 1 || aspectRatio > 15) {
  //  vm_debug_trace(VM_DEBUG_WARNING, VM_STRING("setting aspect ratio to 1\n"));
  //  aspectRatio = 1;
  //}

  //progressive_frame = (progressive_frame != 0) ? 1 : 0;
  //if( info.interlace_type == PROGRESSIVE && !progressive_frame )
  //{
  //  vm_debug_trace(VM_DEBUG_WARNING, VM_STRING("setting progressive_frame = 1\n"));
  //  progressive_frame = 1;
  //}

  //repeat_first_field = (repeat_first_field != 0) ? 1 : 0;
  ////if( !progressive_frame && repeat_first_field )
  //if( info.interlace_type != PROGRESSIVE && repeat_first_field )
  //{
  //  vm_debug_trace(VM_DEBUG_WARNING, VM_STRING("setting repeat_first_field = 0\n"));
  //  repeat_first_field = 0;
  //}

  frame_pred_frame_dct[0] = (frame_pred_frame_dct[0] != 0) ? 1 : 0;
  frame_pred_frame_dct[1] = (frame_pred_frame_dct[1] != 0) ? 1 : 0;
  frame_pred_frame_dct[2] = (frame_pred_frame_dct[2] != 0) ? 1 : 0;

  //if( progressive_frame )
  if( info.interlace_type == PROGRESSIVE )
  {
    if( FieldPicture )
    {
      vm_debug_trace(VM_DEBUG_WARNING, VM_STRING("setting FieldPicture = 0\n"));
      FieldPicture = 0;
    }
    if( !frame_pred_frame_dct[0] )
    {
      vm_debug_trace(VM_DEBUG_WARNING, VM_STRING("setting frame_pred_frame_dct[I_PICTURE] = 1\n"));
      frame_pred_frame_dct[0] = 1;
    }
    if( !frame_pred_frame_dct[1] )
    {
      vm_debug_trace(VM_DEBUG_WARNING, VM_STRING("setting frame_pred_frame_dct[P_PICTURE] = 1\n"));
      frame_pred_frame_dct[1] = 1;
    }
    if( !frame_pred_frame_dct[2] )
    {
      vm_debug_trace(VM_DEBUG_WARNING, VM_STRING("setting frame_pred_frame_dct[B_PICTURE] = 1\n"));
      frame_pred_frame_dct[2] = 1;
    }
  }

  //top_field_first = (top_field_first != 0) ? 1 : 0;
  //if (info.interlace_type == PROGRESSIVE && !repeat_first_field && top_field_first)
  //{
  //  vm_debug_trace(VM_DEBUG_WARNING, VM_STRING("setting top_field_first = 1\n"));
  //  top_field_first = 0;
  //}

  if(info.color_format != NV12)
  if(info.color_format < YUV420 || info.color_format > YUV444) {
    vm_debug_trace(VM_DEBUG_WARNING, VM_STRING("color_format fixed to YUV420\n"));
    info.color_format = YUV420;
  }

  return ret;
}

#endif // UMC_ENABLE_MPEG2_VIDEO_ENCODER
