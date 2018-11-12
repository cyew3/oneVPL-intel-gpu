// Copyright (c) 2014-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

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
