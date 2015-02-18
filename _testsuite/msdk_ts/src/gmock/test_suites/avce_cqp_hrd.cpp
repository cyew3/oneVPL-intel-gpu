#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

namespace avce_cqp_hrd
{
//array of per-frame qp for each test streams
mfxU32 ref_qp[][300] = {
    //foreman_352x288_300.yuv: 1 - CBR, ref_dist 1; 2 - CBR, ref_dist 3; 3 - VBR, ref_dist 1; 4 - VBR, ref_dist 3
    {23,25,26,26,26,26,26,26,26,26,26,26,26,27,27,27,27,27,27,27,27,27,27,26,27,26,27,26,26,26,26,27,28,28,27,27,27,27,27,27,27,27,26,27,26,26,26,26,26,27,27,26,26,26,26,27,27,27,27,27,26,28,29,28,28,28,28,28,28,28,28,27,28,27,28,27,27,27,27,27,27,27,27,27,27,28,28,27,28,27,28,29,29,29,29,29,29,29,29,28,28,28,28,27,27,27,27,27,26,26,26,26,26,26,26,26,26,25,25,25,25,27,28,28,27,27,27,27,27,27,27,28,27,27,27,27,27,27,27,28,28,28,28,28,28,28,28,28,28,28,28,29,29,29,29,30,30,30,30,30,30,29,29,29,28,28,28,28,28,28,27,28,28,28,29,29,29,29,29,29,29,30,30,30,30,30,30,30,30,30,30,29,30,30,30,30,30,30,30,29,29,29,29,29,29,29,29,29,28,28,28,29,29,29,29,29,29,29,29,29,30,30,30,29,29,29,29,29,28,28,29,29,29,29,29,29,28,29,28,29,29,31,32,31,31,31,30,30,30,30,30,30,29,29,29,29,29,29,29,29,29,29,29,29,29,29,28,29,28,29,29,31,32,32,31,31,30,30,29,29,29,29,29,29,29,29,29,29,29,29,28,29,29,28,28,29,28,29,29,29},
    {23,25,27,29,26,29,29,26,29,29,25,29,29,25,28,29,26,29,29,26,29,29,25,29,29,25,28,28,24,27,24,26,28,30,27,30,30,26,30,29,25,29,29,25,28,28,25,28,28,25,28,28,25,28,28,25,28,29,25,28,25,27,29,31,27,30,31,27,30,30,26,30,30,26,29,29,26,29,29,25,29,29,26,29,29,26,29,29,26,29,26,27,30,31,29,31,32,28,32,31,27,31,30,26,30,29,25,29,29,25,28,28,24,27,27,23,26,26,23,26,23,25,27,29,26,29,29,26,29,29,26,29,29,26,29,29,25,29,29,26,29,29,26,29,30,26,29,30,26,29,26,27,30,31,29,31,32,29,32,32,28,32,32,28,31,31,27,31,30,26,30,30,26,29,30,28,30,31,28,31,28,29,32,33,30,33,34,30,33,34,30,33,34,30,33,34,31,34,35,30,34,34,30,33,33,30,33,33,29,33,28,29,32,33,31,33,35,31,35,35,31,35,35,30,34,33,29,33,32,28,32,32,29,32,32,28,32,31,27,31,27,30,32,34,31,34,35,30,34,33,28,32,32,27,31,31,27,30,30,26,30,30,26,29,30,26,29,29,25,29,26,29,31,33,30,33,33,28,32,31,27,31,30,26,30,30,26,29,29,25,29,29,25,28,29,25,28,29,25,28},
    {23,24,24,24,24,25,25,25,25,25,26,26,26,26,26,26,26,27,27,27,27,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,26,27,27,27,27,27,27,27,27,27,27,27,27,27,27,28,28,28,28,28,28,28,28,28,27,27,27,27,27,27,26,26,26,26,26,26,26,26,26,25,26,25,25,25,25,26,26,26,26,26,26,26,26,26,27,27,26,27,27,27,27,27,27,27,27,27,28,28,28,28,28,28,28,28,28,28,28,28,28,29,29,29,29,29,29,29,29,28,28,28,28,28,27,27,28,28,28,28,28,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,30,30,30,30,30,30,30,29,29,30,29,29,29,29,29,29,28,28,29,29,29,29,28,29,29,29,29,29,29,29,29,29,29,29,28,28,28,29,29,29,29,29,29,29,28,29,28,29,30,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,28,29,29,29,29,29,28,29,28,29,29,30,29,29,29,29,28,29,28,28,28,28,28,28,28,29,28,29,29,29,28,29,29,28,28,29,28,29,29,29},
    {23,24,26,27,24,27,27,24,27,27,25,27,28,25,28,29,25,28,29,25,28,29,25,28,28,25,28,28,24,27,24,25,27,28,24,27,27,25,27,28,24,27,27,24,27,27,24,27,27,25,27,28,25,28,28,25,28,28,25,28,25,26,29,29,25,29,29,25,28,29,25,28,29,25,28,29,25,28,29,25,28,29,26,29,29,26,29,29,26,29,26,26,29,30,27,30,30,27,30,30,26,30,29,25,29,29,25,28,28,24,27,27,24,27,27,23,26,26,23,26,23,24,26,27,23,26,26,24,26,27,25,27,29,25,28,29,25,28,29,26,29,29,26,29,30,26,29,30,26,29,26,27,30,31,27,30,31,28,31,32,28,31,31,27,31,30,26,30,30,26,29,30,26,29,30,28,30,31,28,31,28,29,32,33,29,32,33,30,33,33,30,33,34,30,33,34,31,34,35,31,35,35,31,35,35,31,35,35,30,34,30,30,33,34,31,34,35,31,35,35,31,35,35,30,34,33,30,33,33,29,33,33,29,32,32,28,32,32,27,31,28,29,32,32,28,32,31,28,31,31,27,31,31,27,30,30,26,30,30,26,29,29,25,29,29,26,29,29,25,29,26,27,30,30,26,30,30,26,29,29,25,29,29,25,28,29,25,28,29,25,28,29,25,28,29,25,28,28,25,28},
    //bbc_704x576_374.yuv: 1 - CBR, ref_dist 1; 2 - CBR, ref_dist 3; 3 - VBR, ref_dist 1; 4 - VBR, ref_dist 3
    {29,30,30,30,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,30,31,31,31,31,31,31,31,31,31,33,34,34,34,33,33,33,33,33,32,32,32,32,32,32,32,32,31,32,31,32,32,32,31,32,32,32,32,32,32,34,34,34,34,33,33,33,33,33,33,33,32,32,32,32,32,32,32,32,31,32,31,32,31,31,31,32,31,31,32,33,34,33,33,33,33,32,32,32,32,32,32,32,32,32,32,31,32,31,31,31,31,31,32,31,31,32,31,31,31,33,34,34,33,33,33,33,33,32,32,32,32,32,32,32,32,31,32,31,32,31,31,32,31,31,31,31,31,31,31,33,34,34,33,33,33,32,32,32,32,32,31,32,31,31,31,31,31,31,31,31,31,30,31,30,31,30,30,30,30,32,33,33,32,32,32,31,31,31,31,31,31,31,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,32,33,33,33,32,32,32,32,32,32,32,31,32,31,32,31,31,32,31,32,31,32,31,32,32,32,31,32,32,32,34,35,35,35,35,34,34,34,34,33,33,33,33,33,33,33,32,32,32,32,32,32,32,32,31,32,31,31,31,31,33,34,33,33,33,33,33,32,33,32,32,32,32,32,32,32,32,32,32,31,32,31,32,31,32,31,31,32,31},
    {29,30,33,34,31,34,35,32,35,36,32,36,36,31,35,35,31,35,35,31,35,35,31,35,35,30,34,34,31,34,31,33,36,38,34,37,38,34,38,38,33,37,37,32,36,36,32,36,36,31,35,35,31,35,35,31,35,35,31,35,31,33,36,38,36,38,41,36,40,40,35,40,40,34,38,38,34,38,38,33,37,37,33,37,37,32,36,36,32,36,32,33,36,37,34,37,38,33,37,37,33,37,37,32,36,36,32,36,36,31,35,35,31,35,35,31,35,35,30,34,30,32,35,37,34,37,38,33,37,37,32,36,36,32,36,36,31,35,35,31,35,35,30,34,34,31,34,35,30,34,30,32,35,36,33,36,37,33,37,37,32,36,36,31,35,35,31,35,35,30,34,34,30,33,33,30,33,33,29,33,29,31,33,35,32,35,36,31,35,35,31,35,35,30,34,33,30,33,33,29,33,33,29,32,33,29,32,33,29,32,29,31,33,36,33,36,37,32,36,36,32,36,36,32,36,36,31,35,35,31,35,35,31,35,35,31,35,35,31,35,31,34,36,39,36,39,41,36,40,40,35,40,40,34,38,38,34,38,38,33,37,37,33,37,37,32,36,36,31,35,31,33,36,37,33,37,37,33,37,37,33,37,37,32,36,36,32,36,36,31,35,35,31,35,35,31,35,35,30,34},
    {28,29,30,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,30,31,31,31,31,31,31,31,31,31,32,32,32,31,32,31,32,31,32,31,31,32,31,31,31,32,31,32,31,32,31,32,32,31,32,32,32,32,32,31,32,32,32,32,32,32,32,32,32,32,32,32,31,32,31,32,31,31,31,31,31,31,31,32,31,31,31,31,32,31,32,32,32,32,32,31,32,31,32,32,31,32,31,32,31,31,31,32,31,31,31,31,31,32,31,31,32,31,31,31,32,32,32,32,32,31,32,31,32,31,32,31,32,31,31,32,31,31,32,31,31,32,31,31,32,31,31,31,31,31,32,32,31,32,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,30,31,30,31,30,31,30,31,31,31,31,31,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,32,31,32,31,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,31,32,31,31,31,31,32,32,32,32,31,32,32,32,32,32,31,32,32,31,32,32,31,32,32,31,32,31,32,31,32,31,32,32,32},
    {28,29,32,33,31,33,35,31,35,35,31,35,35,31,35,35,31,35,35,30,34,34,30,33,34,30,33,34,31,34,31,31,35,35,32,35,36,32,36,36,31,35,35,31,35,35,31,35,35,31,35,35,31,35,35,31,35,35,31,35,31,31,35,35,33,36,37,33,37,37,33,37,37,33,37,37,33,37,37,32,36,36,32,36,36,31,35,35,31,35,31,32,35,36,32,36,36,32,36,36,31,35,35,31,35,35,31,35,35,31,35,35,31,35,35,31,35,35,30,34,30,31,34,35,31,35,35,32,35,36,31,35,35,31,35,35,31,35,35,31,35,35,31,35,35,30,34,34,30,33,30,31,34,35,32,35,36,31,35,35,31,35,35,31,35,35,31,35,35,30,34,34,30,33,33,30,33,33,29,33,29,30,33,34,30,33,34,30,33,33,30,33,33,29,33,33,29,32,33,29,32,33,29,32,33,29,32,33,29,32,29,30,33,34,31,34,35,31,35,35,31,35,35,31,35,35,30,34,34,31,34,35,31,35,35,31,35,35,31,35,31,32,35,36,33,36,37,33,37,37,33,37,37,33,37,37,32,36,36,32,36,36,32,36,36,31,35,35,31,35,31,32,35,36,31,35,35,32,35,36,31,35,35,31,35,35,31,35,35,31,35,35,31,35,35,31,35,35,31,35},
    //stockholm_1280x720_604.yuv: 1 - CBR, ref_dist 1; 2 - CBR, ref_dist 3; 3 - VBR, ref_dist 1; 4 - VBR, ref_dist 3
    {28,27,32,34,34,33,33,32,32,31,31,31,30,30,30,30,29,29,29,29,29,29,29,29,29,28,29,28,29,28,29,31,32,32,31,31,30,30,30,30,29,29,29,29,29,29,29,29,28,29,28,29,28,29,28,29,28,29,28,28,29,31,32,32,31,31,30,30,30,30,29,29,29,29,29,29,29,28,29,29,28,28,29,28,28,29,28,29,28,28,30,31,32,31,31,31,30,30,30,30,29,29,29,29,29,29,29,29,28,29,28,28,29,28,28,28,29,28,28,28,31,32,31,31,31,30,30,30,30,29,29,29,29,29,29,29,29,29,28,29,29,28,29,28,29,28,29,28,29,28,31,32,32,31,31,30,30,30,30,29,29,29,29,29,29,29,29,29,29,28,29,29,28,29,28,29,28,29,28,29,31,32,31,31,31,30,30,30,29,29,29,29,29,29,29,29,29,28,29,29,28,29,28,29,28,29,29,28,29,28,31,32,32,31,31,31,30,30,30,30,29,29,29,29,29,29,29,29,29,28,29,29,28,29,28,29,28,29,28,29,31,32,31,31,31,30,30,30,30,29,29,29,29,29,29,29,29,29,29,28,29,29,28,29,29,28,29,28,29,28,31,32,32,31,31,31,30,30,30,30,29,29,29,29,29,29,29,29,29,28,29,29,28,29,28,29,28,29,28,29},
    {28,32,36,36,34,37,38,33,37,36,31,36,35,29,33,33,29,32,32,28,32,31,27,31,31,27,30,31,27,30,29,31,33,35,31,35,34,29,33,32,28,32,31,27,31,31,27,30,31,27,30,31,26,30,30,26,29,30,27,30,29,31,33,35,30,34,33,29,33,32,28,32,31,27,31,31,27,30,31,26,30,30,27,30,30,26,30,30,26,29,27,30,32,34,31,34,34,29,33,32,28,32,31,27,31,31,27,30,31,26,30,30,27,30,30,26,30,30,26,29,28,30,32,34,30,33,33,28,32,32,28,31,31,27,31,31,27,30,30,26,30,30,27,30,30,26,30,30,26,29,28,30,32,34,30,33,33,28,32,32,28,31,31,27,31,31,27,30,30,26,30,30,26,29,30,27,30,30,26,30,28,30,32,34,30,33,33,28,32,32,28,31,31,27,31,31,27,30,30,26,30,30,27,30,30,26,30,30,26,29,29,31,33,35,30,34,33,29,33,32,28,32,31,27,31,31,27,30,31,27,30,30,26,30,30,26,29,30,26,29,27,30,32,35,31,34,35,29,33,33,28,32,32,27,31,31,27,30,31,27,30,31,26,30,30,27,30,30,26,30,28,30,32,34,30,33,33,28,32,32,28,31,31,27,31,31,27,30,31,26,30,30,27,30,30,26,30,30,26,29},
    {28,28,27,30,31,32,31,31,30,30,30,29,29,29,29,29,29,29,29,28,29,28,29,28,29,28,29,28,29,28,28,29,29,29,29,29,28,29,29,28,29,28,29,28,29,28,28,29,28,29,28,29,28,28,29,28,29,28,28,29,28,29,29,29,29,29,28,29,28,29,29,28,29,28,29,28,29,28,28,29,28,28,29,28,28,28,29,28,29,28,28,29,29,29,29,29,29,29,29,29,28,29,29,28,29,28,29,28,29,28,29,28,28,28,29,28,28,28,29,28,28,29,29,29,29,29,29,29,28,29,29,28,29,29,28,29,28,29,28,29,28,29,29,28,29,28,29,28,29,28,29,30,29,29,29,29,29,29,29,29,29,29,28,29,29,28,29,29,28,29,29,28,29,28,29,28,29,28,29,29,28,29,29,29,29,29,29,29,29,28,29,29,28,29,28,29,29,28,29,28,29,29,28,29,28,29,29,28,29,29,28,29,30,29,29,29,29,29,29,29,29,28,29,29,29,28,29,29,28,29,29,28,29,29,28,29,28,29,29,28,29,30,30,29,29,29,29,29,29,29,29,29,29,29,29,29,28,29,29,28,29,29,29,28,29,29,28,29,29,28,29,30,30,30,29,29,29,29,29,29,29,29,29,29,29,29,28,29,29,28,29,29,28,29,29,28,29,29,28,29},
    {28,28,31,33,31,33,35,30,34,33,28,32,32,28,31,31,27,31,31,27,30,31,27,30,31,27,30,30,26,30,27,28,31,31,27,31,31,27,30,31,26,30,30,26,29,30,27,30,30,26,30,30,26,29,30,26,29,30,26,29,27,28,31,31,27,31,31,27,30,31,27,30,30,26,30,30,26,29,30,26,29,30,26,29,30,26,29,30,27,30,28,31,33,36,32,35,36,31,35,34,29,33,32,28,32,31,27,31,31,27,30,31,27,30,31,26,30,30,27,30,28,31,33,35,32,35,35,30,35,33,29,33,32,28,32,31,27,31,31,27,30,31,27,30,30,26,30,30,26,29,27,28,31,31,27,31,31,27,30,31,27,30,30,26,30,30,27,30,30,26,30,30,26,29,30,26,29,30,26,29,27,28,31,31,27,31,31,27,30,31,27,30,30,26,30,30,27,30,30,26,30,30,26,29,30,27,30,30,26,30,27,28,31,32,27,31,31,27,30,31,27,30,31,27,30,30,26,30,30,27,30,30,26,30,30,26,29,30,26,29,27,28,31,31,27,31,31,27,30,31,27,30,31,27,30,31,26,30,30,27,30,30,26,30,30,26,29,30,26,29,27,28,31,32,28,31,31,27,31,31,27,30,31,27,30,30,26,30,30,27,30,30,26,30,30,26,29,30,27,30},
    //BQTerrace_1920x1080_60.yuv: 1 - CBR, ref_dist 1; 2 - CBR, ref_dist 3; 3 - VBR, ref_dist 1; 4 - VBR, ref_dist 3
    {35,36,35,35,34,34,34,34,34,34,34,34,34,34,34,33,34,34,33,34,33,34,33,34,33,33,33,33,34,33,36,37,37,36,36,35,35,35,34,35,34,34,34,34,34,34,34,34,33,34,34,33,34,33,33,33,33,33,33,33,35,36,37,36,35,35,35,34,34,34,34,33,33,33,33,33,33,33,33,33,33,33,33,33,32,33,33,33,33,33,34,36,36,35,35,35,34,34,34,34,34,33,34,33,34,33,34,33,33,33,33,33,33,33,32,32,32,32,32,32,32,34,35,35,35,34,34,34,33,33,33,33,33,33,33,32,32,32,32,32,32,31,31,32,31,31,32,31,31,31,31,34,35,34,34,33,33,33,32,32,32,32,32,31,32,31,31,31,31,31,31,31,30,30,30,30,30,30,30,30,30,32,33,33,33,32,32,31,31,31,31,31,31,31,30,31,30,30,30,30,30,30,30,30,29,30,29,29,30,29,30,32,32,32,32,31,31,31,31,31,31,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,32,33,32,32,31,31,31,31,31,31,30,31,30,30,30,30,30,30,30,31,30,30,31,30,30,30,30,31,30,31,33,34,33,33,33,32,32,32,32,31,32,31,32,31,32,31,31,31,31,31,31,31,31,31,31,31,31,31,31},
    {35,36,40,40,33,38,37,33,37,37,31,36,35,32,35,36,31,35,35,30,34,34,30,33,34,30,33,34,30,33,34,37,40,42,36,41,40,34,39,38,33,37,37,32,36,36,31,35,35,31,35,35,31,35,35,30,34,34,30,33,32,34,37,38,35,38,38,33,38,37,32,36,36,31,35,35,31,35,35,30,34,34,30,33,34,30,33,34,30,33,30,33,35,38,35,38,38,33,38,37,32,36,36,31,35,35,31,35,35,31,35,35,30,34,34,30,33,34,29,33,30,33,35,37,34,37,37,32,37,36,31,35,35,31,35,35,30,34,33,29,33,33,29,32,33,29,32,33,29,32,29,32,34,36,32,36,36,31,35,35,30,34,33,29,33,33,29,32,32,28,32,32,28,31,32,28,31,32,28,31,28,31,33,35,32,35,36,31,35,35,29,33,33,29,32,32,28,32,32,28,31,32,28,31,32,28,31,31,27,31,28,31,33,35,31,35,35,30,34,33,29,33,32,28,32,32,28,31,32,28,31,32,28,31,32,28,31,31,28,31,28,31,33,35,31,35,35,30,34,33,29,33,33,29,32,32,28,32,32,29,32,32,28,32,32,28,31,32,28,31,29,31,33,36,32,35,36,31,35,35,30,34,33,29,33,33,29,32,33,29,32,32,28,32,32,29,32,32,28,32},
    {31,32,32,32,32,32,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,34,34,34,34,34,33,34,33,33,33,33,33,33,33,33,33,34,33,33,33,34,33,34,33,33,33,33,33,33,33,33,34,33,33,33,33,33,33,33,32,33,32,33,32,33,32,32,33,32,32,33,32,33,32,33,32,33,33,33,32,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,32,32,32,32,32,32,32,33,33,33,32,32,32,32,32,32,32,33,32,32,32,32,32,32,32,32,31,32,31,31,32,31,31,32,31,31,31,32,32,32,31,32,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,30,31,30,30,31,30,30,30,30,31,31,31,31,31,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,29,30,29,30,29,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,31,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,31,30,31,30,31,30,31,30,31,30,31,31,31,31,31,31,31,31,31,31,31,31,31,32,31,32,31,31,31,32,31,31,31,32,31,31,31,31,31,31},
    {31,32,35,36,31,35,35,30,34,34,30,33,34,30,33,34,30,33,34,30,33,34,30,33,33,29,33,33,30,33,32,35,37,40,36,40,40,34,39,38,33,37,37,32,36,36,31,35,35,31,35,35,30,34,34,30,33,34,30,33,30,31,34,35,31,35,35,30,34,34,30,33,34,30,33,34,30,33,34,30,33,33,29,33,33,30,33,34,30,33,30,31,34,35,30,34,34,30,33,34,30,33,34,30,33,34,31,34,35,30,34,34,30,33,34,30,33,33,30,33,30,30,33,34,30,33,34,30,33,34,30,33,34,30,33,33,29,33,33,29,32,33,29,32,33,29,32,33,29,32,29,30,33,33,29,33,33,29,32,32,28,32,32,29,32,32,28,32,32,28,31,32,28,31,32,28,31,32,28,31,29,30,33,33,29,33,32,28,32,32,29,32,32,28,32,32,28,31,32,28,31,31,28,31,31,27,31,31,28,31,28,29,32,32,28,32,32,29,32,32,28,32,32,28,31,32,28,31,31,28,31,31,28,31,31,28,31,31,28,31,28,29,32,32,29,32,32,28,32,32,28,31,32,28,31,32,29,32,32,28,32,32,29,32,32,28,32,32,29,32,28,29,32,33,29,32,33,29,32,33,29,32,33,29,32,32,29,32,32,28,32,32,29,32,32,28,32,32,29,32},
};

class TestSuite : public tsVideoEncoder
{
public:
    TestSuite() : tsVideoEncoder(MFX_CODEC_AVC) {}
    ~TestSuite() {}

    int RunTest(unsigned int id);
    mfxStatus CheckParams();

    enum
    {
        MFXPAR = 1,
    };

    enum
    {
        CBR = 1,
        VBR = 1 << 2
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
        mfxU32* qp;
    };
    static const tc_struct test_case[];
    static const unsigned int n_cases;
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*01*/ MFX_ERR_NONE, CBR, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV420},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_NV12}}, ref_qp[0]},
    {/*02*/ MFX_ERR_NONE, CBR, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 3},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV420},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_NV12}}, ref_qp[1]},
    {/*03*/ MFX_ERR_NONE, VBR, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV420},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_NV12}}, ref_qp[2]},
    {/*04*/ MFX_ERR_NONE, VBR, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 3},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV420},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_NV12}}, ref_qp[3]},
    {/*05*/ MFX_ERR_NONE, CBR, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV420},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_NV12}}, ref_qp[4]},
    {/*06*/ MFX_ERR_NONE, CBR, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 3},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV420},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_NV12}}, ref_qp[5]},
    {/*07*/ MFX_ERR_NONE, VBR, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV420},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_NV12}}, ref_qp[6]},
    {/*08*/ MFX_ERR_NONE, VBR, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 3},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV420},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_NV12}}, ref_qp[7]},
    {/*09*/ MFX_ERR_NONE, CBR, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV420},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_NV12}}, ref_qp[8]},
    {/*10*/ MFX_ERR_NONE, CBR, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 3},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV420},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_NV12}}, ref_qp[9]},
    {/*11*/ MFX_ERR_NONE, VBR, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV420},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_NV12}}, ref_qp[10]},
    {/*12*/ MFX_ERR_NONE, VBR, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 3},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV420},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_NV12}}, ref_qp[11]},
    {/*13*/ MFX_ERR_NONE, CBR, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV420},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_NV12}}, ref_qp[12]},
    {/*14*/ MFX_ERR_NONE, CBR, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 3},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV420},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_NV12}}, ref_qp[13]},
    {/*15*/ MFX_ERR_NONE, VBR, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV420},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_NV12}}, ref_qp[14]},
    {/*16*/ MFX_ERR_NONE, VBR, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 3},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV420},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_NV12}}, ref_qp[15]},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

class SurfProc : public tsSurfaceProcessor
{
    std::string m_file_name;
    mfxU32 m_nframes;
    tsRawReader m_raw_reader;
    mfxEncodeCtrl* pCtrl;
    mfxFrameInfo* pFrmInfo;
    mfxU32 m_curr_frame;
    const mfxU32* m_ref_qp;
public:
    SurfProc(const char* fname, mfxFrameInfo& fi, mfxU32 n_frames)
        : pCtrl(0)
        , m_curr_frame(0)
        , m_raw_reader(fname, fi, n_frames)
        , pFrmInfo(&fi)
        , m_file_name(fname)
        , m_nframes(n_frames)
    {}

    mfxStatus Init(mfxEncodeCtrl& ctrl, const mfxU32* ref_qp)
    {
        pCtrl = &ctrl;
        m_ref_qp = ref_qp;
        return MFX_ERR_NONE;
    }
    ~SurfProc() {} ;

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        if (m_curr_frame >= m_nframes)
        {
            s.Data.Locked++;
            m_eos = true;
            return MFX_ERR_NONE;
        }
        pCtrl->QP = (mfxU16)m_ref_qp[m_curr_frame];

        mfxStatus sts = m_raw_reader.ProcessSurface(s);

        m_curr_frame++;
        return sts;
    }
};

class BsDump : public tsBitstreamProcessor, tsParserH264AU
{
    tsBitstreamWriter m_writer;
    bool m_mode;
    mfxU32 m_flag;
public:
    BsDump(const char* fname)
        : m_writer(fname)
        , tsParserH264AU(BS_H264_INIT_MODE_DEFAULT)
        , m_mode(false)
    {
        set_trace_level(BS_H264_TRACE_LEVEL_SPS);
    }
    mfxStatus SetMode(mfxU32 flag)
    {
        m_mode = true;
        m_flag = flag;
        return MFX_ERR_NONE;
    }
    ~BsDump() {}

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        if (bs.Data)
            set_buffer(bs.Data + bs.DataOffset, bs.DataLength+1);
        if (m_mode)
        {
            UnitType& au = ParseOrDie();

            for (Bs32u i = 0; i < au.NumUnits; i++)
            {
                if (!(au.NALU[i].nal_unit_type == 0x07))
                {
                    continue;
                }
                if (au.NALU[i].SPS == NULL || au.NALU[i].SPS->vui == NULL || au.NALU[i].SPS->vui->hrd == NULL)
                {
                    g_tsLog << "ERROR: Can't parse cbr_flag";
                    return MFX_ERR_NULL_PTR;
                }
                byte cbr_flag = *(au.NALU[i].SPS->vui->hrd->cbr_flag);
                if (m_flag == TestSuite::CBR && cbr_flag == 0)
                {
                    g_tsLog << "ERROR: cbr_flag is not equal 1 as expected ";
                    return MFX_ERR_INVALID_VIDEO_PARAM;
                }
                else if (m_flag == TestSuite::VBR && cbr_flag == 1)
                {
                    g_tsLog << "ERROR: cbr_flag is not equal 0 as expected ";
                    return MFX_ERR_INVALID_VIDEO_PARAM;
                }
                break;
            }
        }

        return m_writer.ProcessBitstream(bs, nFrames);
    }
};

mfxStatus TestSuite::CheckParams()
{
    g_tsLog << "Check value of RateControlMethod...\n";
    if (m_par.mfx.RateControlMethod != MFX_RATECONTROL_CQP)
    {
        g_tsLog << "\nERROR: RateControlMethod != MFX_RATECONTROL_CQP\n";
        return MFX_ERR_ABORTED;
    }
    g_tsLog << "OK\n";

    mfxExtCodingOption* co = (mfxExtCodingOption*)m_par.ExtParam[0];
    g_tsLog << "Check value of VuiNalHrdParameters...\n";
    if (co->VuiNalHrdParameters != MFX_CODINGOPTION_ON)
    {
        g_tsLog << "\nERROR: VuiNalHrdParameters != MFX_CODINGOPTION_ON\n";
        return MFX_ERR_ABORTED;
    }
    g_tsLog << "OK\n";
    g_tsLog << "Check value of NalHrdConformance...";
    if (co->NalHrdConformance != MFX_CODINGOPTION_OFF)
    {
        g_tsLog << "\nERROR: NalHrdConformance != MFX_CODINGOPTION_OFF\n";
        return MFX_ERR_ABORTED;
    }
    g_tsLog << "OK\n";
    g_tsLog << "Check other required parameters...";
    if (m_par.mfx.FrameInfo.FrameRateExtD == 0)
    {
        g_tsLog << "\nERROR: FrameRateExtD = 0. CQP HRD mode is OFF\n";
        return MFX_ERR_ABORTED;
    }
    if (m_par.mfx.FrameInfo.FrameRateExtN == 0)
    {
        g_tsLog << "\nERROR: FrameRateExtN = 0. CQP HRD mode is OFF\n";
        return MFX_ERR_ABORTED;
    }
    if (m_par.mfx.BufferSizeInKB == 0)
    {
        g_tsLog << "\nERROR: BufferSizeInKB = 0. CQP HRD mode is OFF\n";
        return MFX_ERR_ABORTED;
    }
    if (m_par.mfx.InitialDelayInKB == 0)
    {
        g_tsLog << "\nERROR: InitialDelayInKB = 0. CQP HRD mode is OFF\n";
        return MFX_ERR_ABORTED;
    }
    if (m_par.mfx.TargetKbps == 0)
    {
        g_tsLog << "\nERROR: TargetKbps = 0. CQP HRD mode is OFF\n";
        return MFX_ERR_ABORTED;
    }
    g_tsLog << "OK\n";
    return MFX_ERR_NONE;
}

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    tc_struct tc = test_case[id];

    mfxU32 nframes = 300;
    m_par.mfx.RateControlMethod = MFX_RATECONTROL_CQP;
    m_par.mfx.GopPicSize = 30;

    MFXInit();

    // setup input stream
    std::string input = ENV("TS_INPUT_YUV", "");
    std::string in_name;
    if (input.length())
    {
        m_par.mfx.FrameInfo.CropH = m_par.mfx.FrameInfo.Height = 0;
        m_par.mfx.FrameInfo.CropW = m_par.mfx.FrameInfo.Width = 0;

        std::stringstream s(input);
        s >> in_name;
        s >> m_par.mfx.FrameInfo.Width;
        s >> m_par.mfx.FrameInfo.Height;
        m_par.mfx.FrameInfo.CropH = m_par.mfx.FrameInfo.Height;
        m_par.mfx.FrameInfo.CropW = m_par.mfx.FrameInfo.Width;
    }
    if (!in_name.length() || !m_par.mfx.FrameInfo.Width || !m_par.mfx.FrameInfo.Height)
    {
        g_tsLog << "ERROR: input stream is not defined\n";
        return 1;
    }

    // save parameters used in both modes: CQP and CQP HRD 
    mfxVideoParam par_default;
    memcpy(&par_default, m_pPar, sizeof(mfxVideoParam));

    // encode stream in CQP mode
    set_brc_params(&m_par);
    SETPARS(m_pPar, MFXPAR);

    SetFrameAllocator();
    SurfProc proc_ref(in_name.c_str(), m_par.mfx.FrameInfo, nframes);
    proc_ref.Init(m_ctrl, tc.qp); 
    m_filler = &proc_ref;

    // setup output stream
    char tmp_out[15] = {0};
#pragma warning(disable:4996)
    sprintf(tmp_out, "%04d_ref.h264", id+1);
#pragma warning(default:4996)
    std::string out_name = ENV("TS_OUTPUT_REF", tmp_out);
    BsDump bs_ref(out_name.c_str());
    m_bs_processor = &bs_ref;
    Init();

    EncodeFrames(nframes);
    Close();

    memcpy(m_pPar, &par_default, sizeof(mfxVideoParam));
    // remove default values
    m_par.mfx.QPI = m_par.mfx.QPP = m_par.mfx.QPB = 0;
    // set required parameters to turn on CQP HRD
    m_par.mfx.RateControlMethod = MFX_RATECONTROL_CQP;
    m_par.mfx.FrameInfo.FrameRateExtN = 30;
    m_par.mfx.FrameInfo.FrameRateExtD = 1;
    m_par.mfx.TargetKbps = 600;
    if (tc.mode == VBR)
        m_par.mfx.MaxKbps = m_par.mfx.TargetKbps * 1.3;
    mfxU32 fr = mfxU32(m_par.mfx.FrameInfo.FrameRateExtN / m_par.mfx.FrameInfo.FrameRateExtD);
    // buffer = 0.5 sec
    mfxU32 br = m_par.mfx.MaxKbps ? m_par.mfx.MaxKbps : m_par.mfx.TargetKbps;
    m_par.mfx.BufferSizeInKB = mfxU16(br / fr * mfxU16(fr / 2));
    m_par.mfx.InitialDelayInKB = mfxU16(m_par.mfx.BufferSizeInKB / 2);

    mfxExtCodingOption& cod = m_par;
    cod.VuiNalHrdParameters = MFX_CODINGOPTION_ON;
    cod.NalHrdConformance = MFX_CODINGOPTION_OFF;

    // set test case parameters
    SETPARS(m_pPar, MFXPAR);

    SetFrameAllocator();
    SurfProc proc(in_name.c_str(), m_par.mfx.FrameInfo, nframes);
    proc.Init(m_ctrl, tc.qp); 
    m_filler = &proc;

    // setup output stream
#pragma warning(disable:4996)
    sprintf(tmp_out, "%04d.h264", id+1);
#pragma warning(default:4996)
    out_name = ENV("TS_OUTPUT_NAME", tmp_out);
    BsDump bs(out_name.c_str());
    bs.SetMode(tc.mode);
    m_bs_processor = &bs;

    g_tsStatus.expect(tc.sts);
    Init();

    // Check CQP and VuiNalHrdParameters
    GetVideoParam();
    g_tsStatus.check(CheckParams());

    EncodeFrames(nframes);


    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(avce_cqp_hrd);
};
