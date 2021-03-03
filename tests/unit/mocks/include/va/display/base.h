// Copyright (c) 2020-2021 Intel Corporation
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

#include "mocks/include/common.h"

#include "mocks/include/va/configuration/base.h"
#include "mocks/include/va/context/base.h"
#include "mocks/include/va/resource/base.h"
#include "mocks/include/va/vaapi/display.hxx"

#include <map>

namespace mocks { namespace va
{
    struct display
        : vaapi::display
    {
        MOCK_METHOD2(SetErrorCallback, VAMessageCallback(VAMessageCallback, void*));
        //MOCK_METHOD2(SetInfoCallback, VAMessageCallback(VAMessageCallback, void*));
        MOCK_METHOD0(DisplayIsValid, int());

        MOCK_METHOD2(Initialize, VAStatus(int*, int*));
        MOCK_METHOD0(Terminate, VAStatus());
        MOCK_METHOD0(QueryVendorString, char const*());

        MOCK_METHOD0(MaxNumProfiles, int());
        MOCK_METHOD0(MaxNumEntrypoints, int());
        MOCK_METHOD0(MaxNumConfigAttributes, int());

        MOCK_METHOD2(QueryConfigProfiles, VAStatus(VAProfile*, int*));
        MOCK_METHOD3(QueryConfigEntrypoints, VAStatus(VAProfile, VAEntrypoint*, int*));
        MOCK_METHOD4(GetConfigAttributes, VAStatus(VAProfile, VAEntrypoint, VAConfigAttrib*, int));

        //configuration
        MOCK_METHOD5(CreateConfig, VAStatus(VAProfile, VAEntrypoint, VAConfigAttrib*, int, VAConfigID*));
        MOCK_METHOD1(DestroyConfig, VAStatus(VAConfigID));
        MOCK_METHOD5(QueryConfigAttributes, VAStatus(VAConfigID, VAProfile*, VAEntrypoint*, VAConfigAttrib*, int*));
        MOCK_METHOD3(QuerySurfaceAttributes, VAStatus(VAConfigID, VASurfaceAttrib*, unsigned int*));

        //context
        MOCK_METHOD7(CreateContext, VAStatus(VAConfigID, int, int, int, VASurfaceID*, int, VAContextID*));
        MOCK_METHOD1(DestroyContext, VAStatus(VAContextID));

        //resource
        MOCK_METHOD6(CreateBuffer, VAStatus(VAContextID, VABufferType, unsigned int, unsigned int, void*, VABufferID*));
        MOCK_METHOD1(DestroyBuffer, VAStatus(VABufferID));
        MOCK_METHOD2(MapBuffer, VAStatus(VABufferID, void**));
        MOCK_METHOD1(UnmapBuffer, VAStatus(VABufferID));
        MOCK_METHOD4(CreateImage, VAStatus(VAImageFormat*, int, int, VAImage*));
        MOCK_METHOD1(DestroyImage, VAStatus(VAImageID));
        MOCK_METHOD7(CreateSurfaces, VAStatus(unsigned int, unsigned int, unsigned int, VASurfaceID*, unsigned int, VASurfaceAttrib*, unsigned int));
        MOCK_METHOD2(DestroySurfaces, VAStatus(VASurfaceID*, int));
        MOCK_METHOD2(DeriveImage, VAStatus(VASurfaceID, VAImage*));

        // settings
        std::vector<VAProfile>                                                     profiles;
        std::map<VAProfile, std::vector<VAEntrypoint> >                            entrypoints;
        std::map<std::pair<VAProfile, VAEntrypoint>, std::vector<VAConfigAttrib> > attributes;

        // produced objects
        std::vector<std::shared_ptr<configuration> >      configs;
        std::vector<std::shared_ptr<context> >            contexts;
        std::vector<std::shared_ptr<buffer> >             buffers; // only for VAImageBufferType
        std::vector<std::shared_ptr<image> >              images;
        std::vector<std::shared_ptr<surface> >            surfaces;
    };

    namespace detail
    {
        //Need for ADL
        struct display_tag {};

        template <typename T>
        inline
        void mock_display(display_tag, display&, T)
        {
            /* If you trap here it means that you passed an argument to [make_display]
                which is not supported by library, you need to overload [make_display] for that argument's type */
            static_assert(
                std::conjunction<std::false_type, std::bool_constant<sizeof(T) != 0> >::value,
                "Unknown argument was passed to [make_display]"
            );
        }

        template <typename ...Args>
        inline
        void dispatch(display& d, Args&&... args)
        {
            for_each(
                [&d](auto&& x) { mock_display(display_tag{}, d, std::forward<decltype(x)>(x)); },
                std::forward<Args>(args)...
            );
        }
    }

    template <typename ...Args>
    inline
    display& mock_display(display& d, Args&&... args)
    {
        detail::dispatch(d, std::forward<Args>(args)...);

        return d;
    }

    template <typename ...Args>
    inline
    std::shared_ptr<display>
    make_display(Args&&... args)
    {
        auto d = std::shared_ptr<testing::NiceMock<display> >{
            new testing::NiceMock<display>{}
        };

        using testing::_;

        EXPECT_CALL(*d, SetErrorCallback(_, _))
            .WillRepeatedly(testing::Return(nullptr));

        EXPECT_CALL(*d, QueryVendorString())
            .WillRepeatedly(testing::Return(nullptr));

        EXPECT_CALL(*d, Initialize(_, _))
            .WillRepeatedly(testing::Return(VA_STATUS_ERROR_INVALID_PARAMETER));

        EXPECT_CALL(*d, Terminate())
            .WillRepeatedly(testing::Return(VA_STATUS_SUCCESS));

        EXPECT_CALL(*d, MaxNumProfiles())
            .WillRepeatedly(testing::Invoke(
                [d = d.get()]() { return int(d->profiles.size()); }
            ));

        //VAStatus vaQueryConfigProfiles(VADisplay, VAProfile*, int*));
        EXPECT_CALL(*d, QueryConfigProfiles(_, _))
            .WillRepeatedly(testing::Return(VA_STATUS_ERROR_INVALID_PARAMETER));

        //VAStatus vaQueryConfigEntrypoints(VADisplay, VAProfile, VAEntrypoint*, int*);
        EXPECT_CALL(*d, QueryConfigEntrypoints(_, _, _))
            .WillRepeatedly(testing::Return(VA_STATUS_ERROR_INVALID_PARAMETER));

        //VAStatus GetConfigAttributes(VADisplay, VAProfile, VAEntrypoint, VAConfigAttrib*, int)
        EXPECT_CALL(*d, GetConfigAttributes(_, _, _, _))
            .WillRepeatedly(testing::Return(VA_STATUS_ERROR_INVALID_PARAMETER));

        //VAStatus vaCreateConfig(VADisplay, VAProfile, VAEntrypoint, VAConfigAttrib*, int, VAConfigID*)
        EXPECT_CALL(*d, CreateConfig(_, _, _, _, _))
            .WillRepeatedly(testing::Return(VA_STATUS_ERROR_INVALID_PARAMETER));

        //VAStatus vaDestroyConfig(VADisplay, VAConfigID);
        EXPECT_CALL(*d, DestroyConfig(_))
            .WillRepeatedly(testing::WithArgs<0>(testing::Invoke(
                [d = d.get()](VAConfigID id)
                {
                    auto c = std::find_if(std::begin(d->configs), std::end(d->configs),
                        [id](auto&& c) { return c->id == id; }
                    );

                    return c != std::end(d->configs) ?
                        d->configs.erase(c), VA_STATUS_SUCCESS : VA_STATUS_ERROR_INVALID_CONFIG;
                }))
            );

        //VAStatus vaQueryConfigAttributes(VADisplay, VAConfigID, VAProfile*, VAEntrypoint, VAConfigAttrib*, int*)
        EXPECT_CALL(*d, QueryConfigAttributes(_, _, _, _, _))
            .WillRepeatedly(testing::WithArgs<0, 1, 2, 3, 4>(testing::Invoke(
                [d = d.get()](VAConfigID id, VAProfile* profile, VAEntrypoint* entrypoint, VAConfigAttrib* attributes, int* count)
                {
                    return invoke_if(d->configs,
                        [&](auto&& c) { return c->QueryConfigAttributes(profile, entrypoint, attributes, count); },
                        [&](auto&& c) { return c->id == id; }
                    );
                }))
            );

        //VAStatus vaQuerySurfaceAttributes(VADisplay, VAConfigID, VASurfaceAttrib*, unsigned int*);
        EXPECT_CALL(*d, QuerySurfaceAttributes(_, _, _))
            .WillRepeatedly(testing::WithArgs<0, 1, 2>(testing::Invoke(
                [d = d.get()](VAConfigID id, VASurfaceAttrib* attributes, unsigned int* count)
                {
                    return invoke_if(d->configs,
                        [&](auto&& c) { return c->QuerySurfaceAttributes(attributes, count); },
                        [&](auto&& c) { return c->id == id; }
                    );
                }))
            );

        //VAStatus vaCreateContext(VADisplay, VAConfigID, int width, int height, int flags, VASurfaceID*, int count, VAContextID*)
        EXPECT_CALL(*d, CreateContext(_, _, _, _, _, _, _))
            .WillRepeatedly(testing::Return(VA_STATUS_ERROR_INVALID_PARAMETER));

        //VAStatus vaDestroyContext(VADisplay, VAContextID)
        EXPECT_CALL(*d, DestroyContext(_))
            .WillRepeatedly(testing::WithArgs<0>(testing::Invoke(
                [d = d.get()](VAContextID id)
                {
                    auto c = std::find_if(std::begin(d->contexts), std::end(d->contexts),
                        [id](auto&& c) { return c->id == id; }
                    );

                    return c != std::end(d->contexts) ?
                        d->contexts.erase(c), VA_STATUS_SUCCESS : VA_STATUS_ERROR_INVALID_CONTEXT;
                }))
            );

        //VAStatus vaCreateBuffer(VADisplay, VAContextID, VABufferType, unsigned int, unsigned int, void*, VABufferID*);
        EXPECT_CALL(*d, CreateBuffer(_, _, _, _, _, _))
            .WillRepeatedly(testing::Invoke(
                [d = d.get()](VAContextID c_id, VABufferType type, unsigned int size, unsigned int count, void* data, VABufferID* id)
                {
                    return invoke_if(d->contexts,
                        [&](auto&& c) { return c->CreateBuffer(type, size, count, data, id); },
                        [&](auto&& c) { return c->id == c_id; }
                    );
                })
            );

        //VAStatus vaDestroyBuffer(VADisplay d, VABufferID id)
        EXPECT_CALL(*d, DestroyBuffer(_))
            .WillRepeatedly(testing::Invoke(
                [d = d.get()](VABufferID id)
                {
                    auto parent_id = detail::get_parent_id<VABufferID>(id);
                    if (parent_id == detail::DISPLAY_PARENT_ID)
                    {
                        auto b = std::find_if(std::begin(d->buffers), std::end(d->buffers),
                            [id](auto&& b) { return b->id == id; }
                        );

                        return b != std::end(d->buffers) ?
                            d->buffers.erase(b), VA_STATUS_SUCCESS : VA_STATUS_ERROR_INVALID_BUFFER;
                    }
                    else
                    {
                        return invoke_if(d->contexts,
                            [&](auto&& c) { return c->DestroyBuffer(id); },
                            [&](auto&& c) { return c->id == parent_id; }
                        );
                    }
                })
            );

        //VAStatus vaMapBuffer(VADisplay, VABufferID, void**)
        EXPECT_CALL(*d, MapBuffer(_, _))
            .WillRepeatedly(testing::Invoke(
                [d = d.get()](VABufferID id, void** data)
                {
                    auto parent_id = detail::get_parent_id<VABufferID>(id);
                    if (parent_id == detail::DISPLAY_PARENT_ID)
                    {
                        return invoke_if(d->buffers,
                            [&](auto&& b) { return b->MapBuffer(id, data); },
                            [&](auto&& b) { return b->id == id; }
                        );
                    }
                    else
                    {
                        return invoke_if(d->contexts,
                            [&](auto&& c) { return c->MapBuffer(id, data); },
                            [&](auto&& c) { return c->id == parent_id; }
                        );
                    }
                })
            );

        //VAStatus vaUnmapBuffer(VADisplay, VABufferID)
        EXPECT_CALL(*d, UnmapBuffer(_))
            .WillRepeatedly(testing::Invoke(
                [d = d.get()](VABufferID id)
                {
                    auto parent_id = detail::get_parent_id<VABufferID>(id);
                    if (parent_id == detail::DISPLAY_PARENT_ID)
                    {
                        return invoke_if(d->buffers,
                            [&](auto&& b) { return b->UnmapBuffer(id); },
                            [&](auto&& b) { return b->id == id; }
                        );
                    }
                    else
                    {
                        return invoke_if(d->contexts,
                            [&](auto&& c) { return c->UnmapBuffer(id); },
                            [&](auto&& c) { return c->id == parent_id; }
                        );
                    }
                })
            );

        //VAStatus vaCreateImage(VADisplay, VAImageFormat*, int, int, VAImage*)
        EXPECT_CALL(*d, CreateImage(_, _, _, _))
            .WillRepeatedly(testing::Return(VA_STATUS_ERROR_INVALID_PARAMETER));

        //VAStatus vaDestroyImage(VADisplay, VAImageID)
        EXPECT_CALL(*d, DestroyImage(_))
            .WillRepeatedly(testing::WithArgs<0>(testing::Invoke(
                [d = d.get()](VAImageID id)
                {
                    auto i = std::find_if(std::begin(d->images), std::end(d->images),
                        [id](auto&& i) { return i->id == id; }
                    );
                    if (i != std::end(d->images))
                    {
                        auto b = std::find_if(std::begin(d->buffers), std::end(d->buffers),
                            [&](auto&& b) { return b->id == (*i)->buf_id; }
                        );
                        if (b != std::end(d->buffers))
                            d->buffers.erase(b);

                        return d->images.erase(i), VA_STATUS_SUCCESS;
                    }

                    return VA_STATUS_ERROR_INVALID_IMAGE;
                }))
            );

        //VAStatus vaCreateSurfaces(VADisplay, unsigned int, unsigned int, unsigned int, VASurfaceID*, unsigned int, VASurfaceAttrib*, unsigned int)
        EXPECT_CALL(*d, CreateSurfaces(_, _, _, _, _, _, _))
            .WillRepeatedly(testing::Return(VA_STATUS_ERROR_INVALID_PARAMETER));

        //VAStatus vaDestroySurfaces(VADisplay, VASurfaceID*, int)
        EXPECT_CALL(*d, DestroySurfaces(_, _))
            .WillRepeatedly(testing::WithArgs<0, 1>(testing::Invoke(
                [d = d.get()](VASurfaceID* surfaces, int count)
                {
                    for (auto k = 0; k != count; ++k)
                    {
                        auto s = std::find_if(std::begin(d->surfaces), std::end(d->surfaces),
                            [&](auto&& s) { return s->id == surfaces[k]; }
                        );

                        if (s != std::end(d->surfaces))
                            d->surfaces.erase(s);
                        else
                            return VA_STATUS_ERROR_INVALID_SURFACE;
                    }
                    return VA_STATUS_SUCCESS;
                }
            )));

        //VAStatus vaDeriveImage(VADisplay, VASurfaceID, VAImage*)
        EXPECT_CALL(*d, DeriveImage(testing::_, testing::_))
            .WillRepeatedly(testing::Invoke(
                [d = d.get()](VASurfaceID id, VAImage* image)
                {
                    return invoke_if(d->surfaces,
                        [&](auto&& s) { return s->DeriveImage(id, image); },
                        [&](auto&& s) { return s->id == id; }
                    );
                })
            );

        mock_display(*d, std::forward<Args>(args)...);

        return d;
    }
}
    template <>
    struct default_result<std::shared_ptr<va::display> >
    {
        using type = int;
        static constexpr type value = VA_STATUS_ERROR_INVALID_DISPLAY;
        constexpr type operator()() const noexcept
         { return value; }
    };
}

extern "C"
{
    inline
    VAMessageCallback vaSetErrorCallback(VADisplay d, VAMessageCallback callback, void* ctx)
    {
        return
            static_cast<mocks::va::display*>(d)->SetErrorCallback(callback, ctx);
    }

    inline
    VAStatus vaInitialize(VADisplay d, int* major, int* minor)
    {
        return
            static_cast<mocks::va::display*>(d)->Initialize(major, minor);
    }

    inline
    VAStatus vaTerminate(VADisplay d)
    {
        return
            static_cast<mocks::va::display*>(d)->Terminate();
    }

    inline
    char const* vaQueryVendorString(VADisplay d)
    {
        return
            static_cast<mocks::va::display*>(d)->QueryVendorString();
    }

    inline
    int vaMaxNumProfiles(VADisplay d)
    {
        return
            static_cast<mocks::va::display*>(d)->MaxNumProfiles();
    }

    inline
    int vaMaxNumEntrypoints(VADisplay d)
    {
        return
            static_cast<mocks::va::display*>(d)->MaxNumEntrypoints();
    }

    inline
    int vaMaxNumConfigAttributes(VADisplay d)
    {
        return
            static_cast<mocks::va::display*>(d)->MaxNumConfigAttributes();
    }

    inline
    VAStatus vaQueryConfigProfiles(VADisplay d, VAProfile* profiles, int* count)
    {
        return
            static_cast<mocks::va::display*>(d)->QueryConfigProfiles(profiles, count);
    }

    inline
    VAStatus vaQueryConfigEntrypoints(VADisplay d, VAProfile profile, VAEntrypoint* entrypoints, int* count)
    {
        return
            static_cast<mocks::va::display*>(d)->QueryConfigEntrypoints(profile, entrypoints, count);
    }

    inline
    VAStatus vaCreateConfig(VADisplay d, VAProfile profile, VAEntrypoint entrypoint, VAConfigAttrib* attributes, int count, VAConfigID* id)
    {
        return
            static_cast<mocks::va::display*>(d)->CreateConfig(profile, entrypoint, attributes, count, id);
    }

    inline
    VAStatus vaDestroyConfig(VADisplay d, VAConfigID id)
    {
        return
            static_cast<mocks::va::display*>(d)->DestroyConfig(id);
    }

    inline
    VAStatus vaQueryConfigAttributes(VADisplay d, VAConfigID id, VAProfile* profile, VAEntrypoint* entrypoint, VAConfigAttrib* attributes, int* count)
    {
        return
            static_cast<mocks::va::display*>(d)->QueryConfigAttributes(id, profile, entrypoint, attributes, count);
    }
}
