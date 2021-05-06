# Copyright (c) 2020 Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

add_library(vc1_common STATIC)
set_property(TARGET vc1_common PROPERTY FOLDER "umc")

target_sources(vc1_common
  PRIVATE
    include/umc_vc1_common.h
    include/umc_vc1_common_acinter.h
    include/umc_vc1_common_acintra.h
    include/umc_vc1_common_blk_order_tbl.h
    include/umc_vc1_common_cbpcy_tbl.h
    include/umc_vc1_common_dc_tbl.h
    include/umc_vc1_common_defs.h
    include/umc_vc1_common_interlace_mb_mode_tables.h
    include/umc_vc1_common_interlace_mv_tables.h
    include/umc_vc1_common_interlaced_cbpcy_tables.h
    include/umc_vc1_common_macros_defs.h
    include/umc_vc1_common_mv_block_pattern_tables.h
    include/umc_vc1_common_mvdiff_tbl.h
    include/umc_vc1_common_tables.h
    include/umc_vc1_common_tables_adv.h
    include/umc_vc1_common_ttmb_tbl.h
    include/umc_vc1_common_zigzag_tbl.h
    include/umc_vc1_spl_frame_constr.h
    include/umc_vc1_spl_tbl.h

    src/umc_vc1_common.cpp
    src/umc_vc1_common_acinter.cpp
    src/umc_vc1_common_acintra.cpp
    src/umc_vc1_common_blk_order_tbl.cpp
    src/umc_vc1_common_cbpcy_tbl.cpp
    src/umc_vc1_common_dc_tbl.cpp
    src/umc_vc1_common_interlace_mb_mode_tables.cpp
    src/umc_vc1_common_interlace_mv_tables.cpp
    src/umc_vc1_common_interlaced_cbpcy_tables.cpp
    src/umc_vc1_common_mv_block_pattern_tables.cpp
    src/umc_vc1_common_mvdiff_tbl.cpp
    src/umc_vc1_common_tables.cpp
    src/umc_vc1_common_tables_adv.cpp
    src/umc_vc1_common_ttmb_tbl.cpp
    src/umc_vc1_common_zigzag_tbl.cpp
    src/umc_vc1_spl_frame_constr.cpp
    src/umc_vc1_spl_tbl.cpp
  )

target_include_directories(vc1_common
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  )

target_compile_options(vc1_common
  PRIVATE
    $<IF:$<PLATFORM_ID:Linux>,-U MFX_DISABLE_SW_FALLBACK,/UMFX_DISABLE_SW_FALLBACK>
)

target_link_libraries(vc1_common PUBLIC vm_plus umc_va_hw)
