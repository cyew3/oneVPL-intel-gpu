// Copyright (c) 1985-2018 Intel Corporation
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
#include <memory>

template<bool b>
struct bool2type {};

struct null_type;

template<typename ty, unsigned int size>
class data_view
{
protected:
  data_view(const data_view &);
  data_view & operator = (const data_view &);
public:
  template<typename ty2>
  explicit data_view(ty2 * pData) 
  {
    this->pDatas = new ty[size];

    typedef typename ::pointer_traits<ty2>::tail_pointee_type tail_pointee_type;
    for (unsigned int i = 0; i < size; i++) {   
      this->pDatas[i] = (ty)((tail_pointee_type *)pData)[i];
    }
  }

  template<typename ty2>
  explicit data_view(ty2 data) 
  {
    this->pDatas = new ty[size];

    for (unsigned int i = 0; i < size; i++) {   
      this->pDatas[i] = (ty)data;
    }
  }

  ~data_view()
  {
    delete []pDatas;
  }

  ty operator [] (unsigned int index) const { return pDatas[index]; }

  template<typename ty2>
  operator const ty2 * () const { return (const ty2 *)pDatas; }

protected:
  //ty datas[size];
  ty * pDatas;
};