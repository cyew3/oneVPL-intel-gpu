#pragma once

#include "ts_common.h"

namespace tsStruct
{

class Field
{
public:
    Field(){}
    Field(mfxU32 _offset, mfxU32 _size, std::string _name);

    std::string name;
    mfxU32      offset;
    mfxU32      size;
};

template <class T, mfxU32 N> class Array : public T
{
private:
    T m_fields[N];
public:
    Array(){}
    Array(mfxU32 _offset, mfxU32 _size, std::string _name)
        : T(_offset, _size * N, _name)
    {
        for(mfxU32 i = 0; i < N; i ++)
        {
            m_fields[i] = T(_offset + _size * i, _size, _name + "[" + std::to_string(i) + "]");
        }
    }

    inline const T& operator[] (mfxU32 i) const { assert(i < N); return m_fields[i]; }
};

void check_eq(void* base, Field field, mfxU64 expected);
void check_ne(void* base, Field field, mfxU64 expected);

#define STRUCT(name, fields)                        \
    class Wrap_##name : public Field                \
    {                                               \
    private:                                        \
        typedef ::name base_type;                   \
    public:                                         \
        Wrap_##name() {}                            \
        Wrap_##name(mfxU32 base, mfxU32 size, std::string _name);\
        fields                                      \
    };                                              \
    const Wrap_##name name(0, sizeof(::name), #name);
#define FIELD_T(type, name) Array<Field, (sizeof(((base_type*)0)->name)/sizeof(::type))> name;
#define FIELD_S(type, name) Array<Wrap_##type, (sizeof(((base_type*)0)->name)/sizeof(::type))> name;

#include "ts_struct_decl.h"

#undef STRUCT
#undef FIELD_T
#undef FIELD_S

};