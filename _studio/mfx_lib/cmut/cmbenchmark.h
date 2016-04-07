/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */
#pragma once

#if defined MDFUT_PERF && !defined(__GNUC__)
#include "windows.h"
#include "winbase.h"

#include "hash_map"
#include "string"
#include "iostream"
#include "iomanip"

#include "assert.h"
#include "cminfrastructure.h"

namespace mdfut {
  namespace benchmark {
    class Benchmark
    {
    public:
      Benchmark(std::string name) : name(name) {}
      Benchmark() : name("Benchmark") {}
      ~Benchmark()
      {
        Statistics();
        for (auto pair : clocks) {
          delete pair.second;
        }
        clocks.clear();
      }

      LONGLONG Duration(std::string name) const
      {
        auto iter = clocks.find(name);
        assert(iter != clocks.end());

        return iter->second->Duration();
      }

      void Begin(std::string name)
      {
        auto iter = clocks.find(name);
        if (iter == clocks.end()) {
          auto pair = clocks.insert(std::make_pair(name, new Clock()));
          iter = pair.first;
          //if (!pair.second) {
          //  throw std::exception ();
          //}
        }

        iter->second->Begin();
      }

      void End(std::string name)
      {
        auto iter = clocks.find(name);
        assert(iter != clocks.end());

        if (iter != clocks.end()) {
          iter->second->End();
        }
      }

      void Statistics()
      {
        for (auto pair : clocks) {
          cout << name << " " << pair.first << " # "
               << pair.second->Count() << " calls, "
               << pair.second->Duration() << " microseconds, "
               << pair.second->Duration() / pair.second->Count() << " microseconds/call"
               << endl;
        }
      }

    protected:
      stdext::hash_map<std::string, mdfut::Clock *> clocks;
      std::string name;
    };

    class BenchmarkAspect
    {
    public:
      static Benchmark & benchmark()
      {
        static Benchmark benchmark;
        return benchmark;
      }

      BenchmarkAspect(const char * pFunctionName) : functionName(pFunctionName)
      {
        benchmark().Begin(functionName.c_str());
      }

      ~BenchmarkAspect()
      {
        benchmark().End(functionName.c_str());
      }

    protected:
      std::string functionName;
    };
  };
};

template<typename R, typename C, typename ArgType>
R DEDUCE_RETURN_TYPE_1(R (C::*)(ArgType));

template<typename C, typename ArgType>
void DEDUCE_RETURN_TYPE_1(void (C::*)(ArgType));

#define MDFUT_ASPECT_CUT_FUNCTION_1(FunctionName, ArgType)  \
  auto FunctionName(ArgType v) -> decltype(DEDUCE_RETURN_TYPE_1(&CuttedCls::##FunctionName))  \
  { \
    AspectCls aspect(__FUNCTION__); \
    return CuttedCls::##FunctionName(v); \
  }

#define MDFUT_ASPECT_CUT_START(cls, aspect) \
  class cls : public mdfut::##cls \
  { \
  public: \
    typedef mdfut::##cls CuttedCls; \
    typedef aspect AspectCls;

#define MDFUT_ASPECT_CUT_END  \
  };

#endif