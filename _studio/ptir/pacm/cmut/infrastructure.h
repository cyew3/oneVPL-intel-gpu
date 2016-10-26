//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 1985-2014 Intel Corporation. All Rights Reserved.
//

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