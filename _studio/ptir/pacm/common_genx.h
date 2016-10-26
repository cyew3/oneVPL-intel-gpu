//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014 Intel Corporation. All Rights Reserved.
//

#pragma once
#include <cm/cm.h>

template<typename T> constexpr char* FORMATCHAR () { return ""; }
template<> constexpr char* FORMATCHAR<double>() { return "%lf ";}
template<> constexpr char* FORMATCHAR<float>() { return "%f "; }
template<> constexpr char* FORMATCHAR<int>() { return "%d "; }
template<> constexpr char* FORMATCHAR<uint>() { return "%u "; }
template<> constexpr char* FORMATCHAR<short>() { return "%d "; }
template<> constexpr char* FORMATCHAR<ushort>() { return "%u "; }
template<> constexpr char* FORMATCHAR<char>() { return "%d "; }
template<> constexpr char* FORMATCHAR<uchar>() { return "%u "; }

template<typename T>
void printm(const T m, char* Name = NULL, char* AlterMessage = NULL) {
    static_assert(std::is_arithmetic<T>::value, "printm can only handle numeric scalar");
    if (AlterMessage) {
        printf(AlterMessage);
        printf(":");
    }
    else if (Name) {
        printf(Name);
        printf(":");
    }
    printf(FORMATCHAR<T>(), m);
    printf("\n");
}

template <typename T, int R, int C>
inline _GENX_ void printm(const matrix<T, R, C> &m, char* Name = NULL, char* AlterMessage = NULL) {
    static_assert(std::is_arithmetic<T>::value, "printm can only handle numeric matrix.");
    if (AlterMessage) {
        printf(AlterMessage);
        printf(":");
    }
    else if (Name) {
        printf(Name);
        printf(":");
    }
    printf("M[%d, %d]:\n", R, C);
    for (int i = 0; i<R; i++) {
        for (int j = 0; j < C; j++)
            printf(FORMATCHAR<T>(), m(i, j));
        printf(";\n");
    }
}

template <typename T, int R, int C>
inline _GENX_ void printm(const matrix_ref<T, R, C> m, char* Name = NULL, char* AlterMessage = NULL) {
    static_assert(std::is_arithmetic<T>::value, "printm can only handle numeric matrix.");
    if (AlterMessage) {
        printf(AlterMessage);
        printf(":");
    }
    else if (Name) {
        printf(Name);
        printf(":");
    }
    printf("M[%d, %d]:\n", R, C);
    for (int i = 0; i<R; i++) {
        for (int j = 0; j < C; j++)
            printf(FORMATCHAR<T>(), m(i, j));
        printf(";\n");
    }
}
