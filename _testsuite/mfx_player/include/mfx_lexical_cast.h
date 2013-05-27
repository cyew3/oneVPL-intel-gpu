/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once
#include <limits>
#include <string>
#include <typeinfo>

template <class T>
class StringCvt_t
{
public:
    StringCvt_t(const std::basic_string<T> & /*input*/)
    {
    }
};

template <>
class StringCvt_t<char>
{
    tstring m_str;
public:
    StringCvt_t(const std::basic_string<char> & input)
        : m_str(input.begin(), input.end())
    {
    }
    tstring str()
    {
        return m_str;
    }
};

template <>
class StringCvt_t<wchar_t>
{
    tstring m_str;
public:
    StringCvt_t(const std::basic_string<wchar_t> & input)
        : m_str(input.begin(), input.end())
    {
    }
    tstring str ()
    {
        return m_str;
    }
};

template<class T>
StringCvt_t<T> StringCvt(const std::basic_string<T> & input)
{
    StringCvt_t<T> item(input);
    return item;
}

template<class T>
StringCvt_t<T> StringCvt(const T * input)
{
    StringCvt_t<T> item(input);
    return item;
}


template <class T>
class ParseableType
{
public:
    typedef T type;
    static double cast_to_double(const  T &){return 0.;}
};

template <>
class ParseableType<char>
{
public:
    typedef short type;
    static const double cast_to_double(const short & val){return static_cast<double>(static_cast<char>(val));}
};

template <>
class ParseableType<unsigned char>
{
public:
    typedef short type;
    static const double cast_to_double(const short & val){return static_cast<double>(static_cast<unsigned char>(val));}
};

template <>
class ParseableType<short>
{
public:
    typedef short type;
    static const double cast_to_double(const short & val){return static_cast<double>(val);}
};

template <>
class ParseableType<unsigned short>
{
public:
    typedef unsigned short type;
    static const double cast_to_double(const unsigned short & val){return static_cast<double>(val);}
};

template <>
class ParseableType<int>
{
public:
    typedef int type;
    static const double cast_to_double(const int & val){return static_cast<double>(val);}
};

template <>
class ParseableType<unsigned int>
{
public:
    typedef unsigned int type;
    static const double cast_to_double(const unsigned int & val){return static_cast<double>(val);}
};

template <>
class ParseableType<__INT64>
{
public:
    typedef __INT64 type;
    static const double cast_to_double(const __INT64 & val){return static_cast<double>(val);}
};

template <>
class ParseableType<unsigned __INT64>
{
public:
    typedef unsigned __INT64 type;
    static const double cast_to_double(const unsigned __INT64 & val){return static_cast<double>(val);}
};

template <>
class ParseableType<float>
{
public:
    typedef float type;
    static const double cast_to_double(const float & val){return static_cast<double>(val);}
};

template <>
class ParseableType<double>
{
public:
    typedef double type;
    static const double cast_to_double(const double & val){return val;}
};


#pragma warning( push )
#pragma warning( disable : 4127 ) //condition expression is constant

//convert from string to specific type, like double, int, etc
template <class T> 
void lexical_cast(const tstring & from, T & value)
{
    tstringstream sstream; 
    sstream.str(from); 
    typename ParseableType<T>::type parsed_value;
    
    //type is not a number, but custom type with custom parser, exception should be thrown by that class
    if (!std::numeric_limits<T>::is_specialized)
    {
        if ((sstream >> parsed_value).fail() || !sstream.eof())
        {
            throw tstring(VM_STRING("Cannot parse ")) + from + VM_STRING(" as type ") + StringCvt(typeid(value).name()).str();
        }
    }
    else
    {
        //try to parse as native type first, this will save from parsing E or e for integer types
        if ((sstream >> parsed_value).fail() || !sstream.eof())
        {
            throw tstring(VM_STRING("Cannot parse ")) + from + VM_STRING(" as type ") + StringCvt(typeid(value).name()).str();
        }
        
        //try to parse as double if it not a float type
        if (std::numeric_limits<T>::is_integer)
        {
            double d;
            sstream.str(from);
            sstream.clear();
            sstream >> d;

            // could be range overflow
            if (ParseableType<T>::cast_to_double(parsed_value) != d)
            {
                throw tstring(VM_STRING("Cannot parse ")) + from + VM_STRING(" as type ") + StringCvt(typeid(value).name()).str();
            }
        }
        else
        {
            //check for floating overflow
            if (std::numeric_limits<T>::infinity() == parsed_value )
            {
                throw tstring(VM_STRING("Cannot parse ")) + from + VM_STRING(" as type ") + StringCvt(typeid(value).name()).str();
            }
        }
    }
    
    value = static_cast<T>(parsed_value);
}

#pragma warning( pop )
