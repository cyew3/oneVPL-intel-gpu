//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014 Intel Corporation. All Rights Reserved.
//

#ifndef BLOCKY_IO_GENX_H
#define BLOCKY_IO_GENX_H
template <uint R, uint C, typename T>
inline _GENX_ bool blocky_write(SurfaceIndex &s, uint x, uint y, matrix<T, R, C> &m) {
    return blocky_write(s, x, y, m.select_all());
}

template <uint R, uint C, typename T>
inline _GENX_ void blocky_read(SurfaceIndex &s, uint x, uint y, matrix<T, R, C> &m) {
    blocky_read(s, x, y, m.select_all());
}

template <uint R, uint C, typename T>
inline _GENX_ typename std::enable_if<C*sizeof(T)<=32 && R*C*sizeof(T)<=256, void>::type blocky_read(SurfaceIndex &s, uint x, uint y, matrix_ref<T, R, C> m) {
    read(s, x, y, m);
}

template <uint R, uint C, typename T>
inline _GENX_ typename std::enable_if<(C*sizeof(T)<=32) && (R*C*sizeof(T)>256), void>::type blocky_read(SurfaceIndex &s, uint x, uint y, matrix_ref<T, R, C> m) {
    blocky_read(s, x, y,                     m.template select<    256/(C*sizeof(T)), 1, C, 1>(0                , 0));
    blocky_read(s, x, y+(256/(C*sizeof(T))), m.template select<R - 256/(C*sizeof(T)), 1, C, 1>(256/(C*sizeof(T)), 0));
}

template <uint R, uint C, typename T>
inline _GENX_ typename std::enable_if<(C*sizeof(T)>32), void>::type blocky_read(SurfaceIndex &s, uint x, uint y, matrix_ref<T, R, C> m) {
    blocky_read(s, x,    y, m.template select<R, 1,   32/sizeof(T), 1>(0,            0));
    blocky_read(s, x+32, y, m.template select<R, 1, C-32/sizeof(T), 1>(0, 32/sizeof(T)));
}

template <uint R, uint C, typename T>
inline _GENX_ void blocky_read_plane(SurfaceIndex &s, CmSurfacePlaneIndex PID, uint x, uint y, matrix<T, R, C> &m) {
    blocky_read_plane(s, PID, x, y, m.select_all());
}

template <uint R, uint C, typename T>
inline _GENX_ typename std::enable_if<C*sizeof(T)<=32 && R*C*sizeof(T)<=256, void>::type blocky_read_plane(SurfaceIndex &s, CmSurfacePlaneIndex PID, uint x, uint y, matrix_ref<T, R, C> m) {
    read_plane(s, PID, x, y, m);
}

template <uint R, uint C, typename T>
inline _GENX_ typename std::enable_if<(C*sizeof(T)<=32) && (R*C*sizeof(T)>256), void>::type blocky_read_plane(SurfaceIndex &s, CmSurfacePlaneIndex PID, uint x, uint y, matrix_ref<T, R, C> m) {
    blocky_read_plane(s, PID, x, y,                     m.template select<    256/(C*sizeof(T)), 1, C, 1>(0                , 0));
    blocky_read_plane(s, PID, x, y+(256/(C*sizeof(T))), m.template select<R - 256/(C*sizeof(T)), 1, C, 1>(256/(C*sizeof(T)), 0));
}

template <uint R, uint C, typename T>
inline _GENX_ typename std::enable_if<(C*sizeof(T)>32), void>::type blocky_read_plane(SurfaceIndex &s, CmSurfacePlaneIndex PID, uint x, uint y, matrix_ref<T, R, C> m) {
    blocky_read_plane(s, PID, x,    y, m.template select<R, 1,   32/sizeof(T), 1>(0,            0));
    blocky_read_plane(s, PID, x+32, y, m.template select<R, 1, C-32/sizeof(T), 1>(0, 32/sizeof(T)));
}

template <uint R, uint C, typename T>
inline _GENX_ typename std::enable_if<C*sizeof(T)<=32 && R*C*sizeof(T)<=256, bool>::type blocky_write(SurfaceIndex &s, uint x, uint y, matrix_ref<T, R, C> m) {
    return write(s, x, y, m);
}

template <uint R, uint C, typename T>
inline _GENX_ typename std::enable_if<(C*sizeof(T)<=32 && R*C*sizeof(T)>256 && R%((256/(C*sizeof(T))))==0), bool>::type blocky_write(SurfaceIndex &s, uint x, uint y, matrix_ref<T, R, C> m) {
    bool r=true;
#pragma unroll
    for (int i = 0; i<R; i+=(256/(C*sizeof(T)))) {
        r = r & write(s, x, y+i, m.template select<(256/(C*sizeof(T))), 1, C, 1>(i, 0));
    }
    return r;
}

template <uint R, uint C, typename T>
inline _GENX_ typename std::enable_if<(C*sizeof(T)<=32 && R*C*sizeof(T)>256 && R%(256/(C*sizeof(T)))!=0), bool>::type blocky_write(SurfaceIndex &s, uint x, uint y, matrix_ref<T, R, C> m) {
    int i = 0;
    bool r=true;
#pragma unroll
    for (; i<R-R%(256/(C*sizeof(T))); i+=(256/(C*sizeof(T)))) {
        static_assert(256/(C*sizeof(T))*C<=256, "Wrong");
        r = r & write(s, x, y+i, m.template select<(256/(C*sizeof(T))), 1, C, 1>(i, 0));
    }
    r = r & write(s, x, y+i, m.template select<R%(256/(C*sizeof(T))), 1, C, 1>(i, 0));
    return r;
}

template <uint R, uint C, typename T>
inline _GENX_ typename std::enable_if<(C*sizeof(T)>32), bool>::type blocky_write(SurfaceIndex &s, uint x, uint y, matrix_ref<T, R, C> m) {
    bool r = blocky_write(s, x, y, m.template select<R, 1, 32 / sizeof(T), 1>(0, 0));
    r = r & blocky_write(s, x + 32, y, m.template select<R, 1, C - 32 / sizeof(T), 1>(0, 32 / sizeof(T)));
    return r;
}

template <uint R, uint C, typename T>
inline _GENX_ typename std::enable_if<C*sizeof(T) <= 32 && R*C*sizeof(T) <= 256, bool>::type blocky_write_plane(SurfaceIndex &s, CmSurfacePlaneIndex PID, uint x, uint y, matrix_ref<T, R, C> m) {
    return write_plane(s, PID, x, y, m);
}

template <uint R, uint C, typename T>
inline _GENX_ typename std::enable_if<(C*sizeof(T) <= 32 && R*C*sizeof(T)>256 && R % ((256 / (C*sizeof(T)))) == 0), bool>::type blocky_write_plane(SurfaceIndex &s, CmSurfacePlaneIndex PID, uint x, uint y, matrix_ref<T, R, C> m) {
    bool r = true;
#pragma unroll
    for (int i = 0; i<R; i += (256 / (C*sizeof(T)))) {
        r = r & write_plane(s, PID, x, y + i, m.template select<(256 / (C*sizeof(T))), 1, C, 1>(i, 0));
    }
    return r;
}

template <uint R, uint C, typename T>
inline _GENX_ typename std::enable_if<(C*sizeof(T) <= 32 && R*C*sizeof(T)>256 && R % (256 / (C*sizeof(T))) != 0), bool>::type blocky_write_plane(SurfaceIndex &s, CmSurfacePlaneIndex PID, uint x, uint y, matrix_ref<T, R, C> m) {
    int i = 0;
    bool r = true;
#pragma unroll
    for (; i<R - R % (256 / (C*sizeof(T))); i += (256 / (C*sizeof(T)))) {
        static_assert(256 / (C*sizeof(T))*C <= 256, "Wrong");
        r = r & write_plane(s, PID, x, y + i, m.template select<(256 / (C*sizeof(T))), 1, C, 1>(i, 0));
    }
    r = r & write_plane(s, x, PID, y + i, m.template select<R % (256 / (C*sizeof(T))), 1, C, 1>(i, 0));
    return r;
}

template <uint R, uint C, typename T>
inline _GENX_ typename std::enable_if<(C*sizeof(T)>32), bool>::type blocky_write_plane(SurfaceIndex &s, CmSurfacePlaneIndex PID, uint x, uint y, matrix_ref<T, R, C> m) {
    bool r = blocky_write_plane(s, PID, x, y, m.template select<R, 1, 32 / sizeof(T), 1>(0, 0));
    r = r & blocky_write_plane(s, PID, x + 32, y, m.template select<R, 1, C - 32 / sizeof(T), 1>(0, 32 / sizeof(T)));
    return r;
}

#endif