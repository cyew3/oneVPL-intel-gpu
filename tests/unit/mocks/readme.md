## Introduction

Stubs & mocks are essential part of unit testing that help to isolate unit from its external dependencies and setup our behavior expectations for the unit.

## Tutorial
```C
//mock a [ID3D11Device] that supports a certain video description
auto device = mocks::dx11::make_device(
    D3D11_VIDEO_DECODER_DESC{ DXVA_ModeHEVC_VLD_Main, width, height, DXGI_FORMAT_NV12 }
);
//use the instance directly
HRESULT hr = device->CreateBuffer();

//mock a [ID3D11Device] that is avalable at 'default' adapter and supports 9.3/11.1 levels
auto device = mocks::dx11::make_device(
    std::make_tuple(
        static_cast<IDXGIAdapter*>(nullptr),
        std::make_tuple(D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_11_1)
    ),
);

//inderectly obtain the instance through [D3D11CreateDevice]
CComPtr<ID3D11Device> device;
D3D_FEATURE_LEVEL levels[] = {
    D3D_FEATURE_LEVEL_9_3,
    D3D_FEATURE_LEVEL_11_1
};
D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                  levels, std::extent<decltype(levels)>::value,
                  D3D11_SDK_VERSION, &device, nullptr, nullptr)
```
## Motivation/Rationale

To get a simple stub for [```ID3D11DeviceContext```](https://docs.microsoft.com/en-us/windows/win32/api/d3d11/nn-d3d11-id3d11devicecontext) interface you have to write a class w/ ~100 methods
just to be able to create an object. Next, you have to stub/mock a certain method(s) you're interested in to specify its behaviour & your expectations.
Than, sometimes, its either not trivial to set up these expectations (even w/ help of frameworks like *GMock*) or it is quite burden syntactically and so could be an error prone.
The third thing is you need repeat to these efforts every time you need the instance of the interface.
And the last (but not least), sometimes you have no way to pass the instance to the unit you're testing because the code creates it itself
(through system API calls like [```D3D11CreateDevice```](https://docs.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-d3d11createdevice)).

In order to facilitate and support these features above the *mocks* library was created. In particular, the library addresses the following goals
 * to simplify a creation of stubs/mocks, that are used as external dependencies
 * to have a higher level of abstraction while setting expectations for system APIs like DX9/11/12 and DXVA rather than it is offered by *GMock*
 * to have a reusable code to avoid code dublication and copy/paste
 * to have a capability of mocking dependencies that are created internally at the unit being tested
 * to support both a simple usage and allow fine-tuning and customization for the power user, have a fallback down to the native *GMock*
 * to be easy to integrate, avoiding any special build steps
 * to be extensible, have a well defined mechanism to add new mocks and expectations

## Structure and Concepts

*mock* libary is organized as a header only and therefore it doesn't require any specific build steps beyound just including the header file that exposes a certain functionality.
The library supports the following API subsets
 - dx9
 - dxgi
 - dx11
 - dx12

### Creation and management

To create an instance of mock class you need to call ```mocks::<subset>::make_<name>```, where ```<subset>``` is supported API (dx9/dx11 etc.) and ```<name>``` is mock object name (device/adapter etc.).
With help of the ```mocks::<subset>::make_<name>``` function a [```std::unique_ptr```](https://en.cppreference.com/w/cpp/memory/unique_ptr) to the object with the expectations depending on provided arguments is obtained. For e.g.
```C
auto device = mocks::dxgi::make_device(
    std::make_tuple(2 /* max surfaces count */, DXGI_SURFACE_DESC{ 640, 480, DXGI_FORMAT_NV12 })
);
```
will return an instance of [```IDXGIDevice```](https://docs.microsoft.com/en-us/windows/win32/api/dxgi/nn-dxgi-idxgidevice) that allows user to create (through [```IDXGIDevice::CreateSurface```](https://docs.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgidevice-createsurface)) at least two instances of *IDXGISurface*.

Creation of mocked objects and theirs expectations at *mocks* library is parameter's driven. It means that mocked methods and expectations you get are depend on parameters that are passed to ```mocks::<subset>::make_<name>```.
In example above user gets the object that has mocked [```IDXGIDevice::CreateSurface```](https://docs.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgidevice-createsurface),
so if user also wants to have mocked [```IDXGIDevice::GetAdapter```](https://docs.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgidevice-getadapter) method, the parameter with type of [```IDXGIAdapter*```](https://docs.microsoft.com/en-us/windows/win32/api/dxgi/nn-dxgi-idxgiadapter) has to be passed:
```C
//adapter is an instance of 'IDXGIAdapter'
auto device = mocks::dxgi::make_device(adapter)
```

All functions ```mocks::<subset>::make_<name>```
accept variadic number of arguments, so if user wants to have a mocked class that supports both different surfaces' types and adapter, all he needs is just call
```C
auto device = mocks::dxgi::make_device(
    adapter,
    std::make_tuple(1, DXGI_SURFACE_DESC{ 640, 480, DXGI_FORMAT_P010 }), //one p010 surface
    std::make_tuple(2, DXGI_SURFACE_DESC{ 320, 200, DXGI_FORMAT_NV12 }), //two nv12 surfaces
    std::make_tuple(4, DXGI_SURFACE_DESC{ 720, 576, DXGI_FORMAT_AYUV })  //four ayuv surfaces
    //...
);
```

Another way to amend the instance of mock class is ```mocks::dxgi::mock_device(*device, /* parameters */)``` function, so it's also possible to write
```C
auto device = mocks::dxgi::make_device(
    std::make_tuple(1, DXGI_SURFACE_DESC{ 640, 480, DXGI_FORMAT_P010 })
);
```
and then append other surfaces' formats -
```C
mocks::dxgi::mock_device(*device,
    std::make_tuple(2, DXGI_SURFACE_DESC{ 320, 200, DXGI_FORMAT_NV12 }),
    std::make_tuple(4, DXGI_SURFACE_DESC{ 720, 576, DXGI_FORMAT_AYUV })
);
```
and then, finally
```C
//adapter is an instance of 'IDXGIAdapter'
mocks::dxgi::mock_device(*device,
    adapter
);
```

Note here, that every argument of ```mocks::<subset>::make_<name>``` is handled independently, so
```C
auto device = mocks::dxgi::make_device();
mocks::dxgi::mock_device(*device, DXGI_SURFACE_DESC{ 320, 200, DXGI_FORMAT_NV12 });
mocks::dxgi::mock_device(*device, DXGI_SURFACE_DESC{ 320, 200, DXGI_FORMAT_NV12 });
```
means that the same [```IDXGIDevice::CreateSurface```](https://docs.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgidevice-createsurface) is mocked twice (overwrite), but not is mocked
to support creation of two surfaces' instances. If it is needed to handle several arguments as one entity (surface description **and** surface number), user has to use [```std::tuple```](https://en.cppreference.com/w/cpp/utility/tuple) to combine them
into one argument
```C
auto device = mocks::dxgi::make_device(
    std::make_tuple(number, surface_description)
);
```

If user passes an unknown (unsupported) argument to ```mocks::<subset>::make_<name>``` function, he gets a compilation error (```static_assert```)
at the function ```mocks::<subset>::detail::mock_<name>``` that is dedicated trap to catch such issues. So, trying to pass unsupported argument with type of ```foo```
```C
struct foo {};
auto device = mocks::dxgi::make_device(foo{});
```
will lead to the compilation error like
```
mocks/include/dxgi/device/device.h(104,1): error C2338: Unknown argument was passed to [make_device]
mocks/include/dxgi/device/device.h(117): message : see reference to function template instantiation 'void mocks::dxgi::detail::mock_device<_Ty>(mocks::dxgi::device &,T)' being compiled
mocks/include/dxgi/device/device.h(117): message :         with
mocks/include/dxgi/device/device.h(117): message :         [
mocks/include/dxgi/device/device.h(117): message :             _Ty=foo,
mocks/include/dxgi/device/device.h(117): message :             T=foo
mocks/include/dxgi/device/device.h(117): message :         ]
```

Any object obtained from ```mocks::<subset>::make_<name>``` is *GMock*'able object, so user can setup expectation in framework's native manner -
```C
auto device = mocks::dxgi::make_device();
EXPECT_CALL(*device, GetAdapter(testing::NotNull())
    .WillRepeatedly(testing::Return(S_OK));
```

### Lifetime and ownership

As it was mentioned above, all ```mocks::<subset>::make_<name>``` returns [```std::unique_ptr```](https://en.cppreference.com/w/cpp/memory/unique_ptr), so user doesn't need to destoy the its instance manually. Despite this, since all supported
objects are COM objects (and thus are reference counted), sometimes it is needed to care about theirs lifetime manually. Consider this -
```C
DXGI_SURFACE_DESC sd{ 320, 200, DXGI_FORMAT_NV12 };
auto device = mocks::dxgi::make_device(sd);
IDXGISurface* surface;
device->CreateSurface(&sd, 1, DXGI_USAGE_READ_ONLY, nullptr, &surface);
//here you have outgoing reference at 'IDXGISurface' object and you have to call 'Release' for it
```
In that case, the best way is to use ```CComPtr<IDXGISurface>```  to avoid manuall resource management.

More complex example - we have a mocked instance of [```IDXGIAdapter```](https://docs.microsoft.com/en-us/windows/win32/api/dxgi/nn-dxgi-idxgiadapter), which we got from ```mocks::dxgi::make_adapter```
and we want to create the mocked [```IDXGIDevice```](https://docs.microsoft.com/en-us/windows/win32/api/dxgi/nn-dxgi-idxgidevice) that will return our mocked adapter from its [```IDXGIDevice::GetAdapter```](https://docs.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgidevice-getadapter):
```C
auto adapter = mocks::dxgi::make_adapter();
auto device = mocks::dxgi::make_device(adapter.get()); //mock device to support our adapter
```
So far, so good. But what if then we do the following:
```C
CComPtr<IDXGIAdapter> xa;
auto adapter = mocks::dxgi::make_adapter(); //reference count == 1
auto device = mocks::dxgi::make_device(adapter.get());
device->GetAdapter(&xa)); //reference count == 2
```
? Here we have the [```std::unique_ptr```](https://en.cppreference.com/w/cpp/memory/unique_ptr) (*adapter*) that owns the object and will destroy it when *adapter* leaves the scope and [```CComPtr```](https://docs.microsoft.com/en-us/cpp/atl/reference/ccomptr-class) (*xa*) that also calls ```Release```, which
in turn invokes ```delete``` on the last reference (counter is equal to zero). The thing is [```CComPtr```](https://docs.microsoft.com/en-us/cpp/atl/reference/ccomptr-class) is declared **before** [```std::unique_ptr```](https://en.cppreference.com/w/cpp/memory/unique_ptr) and therefore object will be unconditionally
destroyed by [```std::unique_ptr```](https://en.cppreference.com/w/cpp/memory/unique_ptr) (that doesn't share ownership) before destructor of [```CComPtr```](https://docs.microsoft.com/en-us/cpp/atl/reference/ccomptr-class) is invoked, so the latter will have a dead object during reference management. To avoid such issue, user either has to
manually call ```CComPtr::Release``` or just declare [```std::unique_ptr```](https://en.cppreference.com/w/cpp/memory/unique_ptr) before [```CComPtr```](https://docs.microsoft.com/en-us/cpp/atl/reference/ccomptr-class). In some situation, the user can also do the following
```C
CComPtr<IDXGIAdapter> xa; //declaration is somewhere

//creation at another place
CComPtr<IDXGIAdapter> adapter;
adapter.Attach(mocks::dxgi::make_adapter().release()); //trick - transfer ownership from 'std::unique_ptr' to `CComPtr`
auto device = mocks::dxgi::make_device(adapter.p);

//... and usage at third
device->GetAdapter(&xa)); //'xa' will correctly share ownership with 'adapter'
```

### Extension and Improvement

Let's amend ```mocks::dxgi::device``` with support of [IDXGIDevice::SetGPUThreadPriority](https://docs.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgidevice-setgputhreadpriority) and [IDXGIDevice::GetGPUThreadPriority](https://docs.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgidevice-getgputhreadpriority).
First of all, you have to declare these methods as mockable at [```struct mocks::dxgi::device```](./include/dxgi/device/device.h) definition:
```C
MOCK_METHOD1(SetGPUThreadPriority, HRESULT(INT));
MOCK_METHOD1(GetGPUThreadPriority, HRESULT(INT*));
```
Than, the way you proceed depends of you requrements:
* If you just want to have a stub (so method will work and return hardcoded status/value), all what you need is to add
```C
EXPECT_CALL(*d, SetGPUThreadPriority(testing::_))
    .WillRepeatedly(testing::Return(S_OK)); //priority value doesn't matter, we will always return OK
EXPECT_CALL(*d, GetGPUThreadPriority(testing::NotNull()))
    .WillRepeatedly(testing::DoAll(
        testing::SetArgPointee<0>(0), //we will report zero as priority's value
        testing::Return(S_OK)
    ));
```
to main [```mocks::dxgi::make_device```](./include/dxgi/device/device.h) function.
* If you want to set certain expectation, you have to do more steps. As it said earlier, mocking parameter's driven. So, to extend library to support mocking of the new methods
you have to 
 * add default expectations to [```mocks::dxgi::make_device```](./include/dxgi/device/device.h) function, similar like it is for stub above (returning a failure for *GMock*'s any matcher [```testing::_```](https://github.com/google/googletest/blob/master/googlemock/docs/for_dummies.md#matchers-what-arguments-do-we-expect))
 * then to define a new overload of ```mocks::<subset>::detail::mock_<name>``` for the ```INT``` parameter that sets expectations for our methods
```C
//namespace detail
inline
void mock_device(device& d, int priority)
{
    EXPECT_CALL(d, SetGPUThreadPriority(testing::Eq(priority)))
        .WillRepeatedly(
            testing::Return(S_OK)
        );

    EXPECT_CALL(d, GetGPUThreadPriority(testing::NotNull()))
        .WillRepeatedly(testing::DoAll(
            testing::SetArgPointee<0>(priority),
            testing::Return(S_OK)
        ));
}
```

## Reference

### Utility

#### [```IUnknown```](https://docs.microsoft.com/en-us/windows/win32/api/unknwn/nn-unknwn-iunknown) implementation - [```unknown_impl```](./include/unknown.h)

#### Win32 API regestry - [registry](./include/registry.h)

#### Direct3D 9
TODO

#### [DirectX Graphics Infrastructure (DXGI)](https://docs.microsoft.com/en-us/windows/win32/direct3ddxgi/dx-graphics-dxgi)
- [```IDXGIFactory```](https://docs.microsoft.com/en-us/windows/win32/api/dxgi/nn-dxgi-idxgifactory) & [```IDXGIFactory1```](https://docs.microsoft.com/en-us/windows/win32/api/dxgi/nn-dxgi-idxgifactory1) - [```mocks::dxgi::factory```](./include/dxgi/device/factory.h)
- [```IDXGIDevice```](https://docs.microsoft.com/en-us/windows/win32/api/dxgi/nn-dxgi-idxgidevice) - [```mocks::dxgi::device```](./include/dxgi/device/device.h)
- [```IDXGIAdapter```](https://docs.microsoft.com/en-us/windows/win32/api/dxgi/nn-dxgi-idxgiadapter) - [```mocks::dxgi::adapter```](./include/dxgi/device/adapter.h)
- [```IDXGIResource```](https://docs.microsoft.com/en-us/windows/win32/api/dxgi/nn-dxgi-idxgiresource) & [```IDXGIResource1```](https://docs.microsoft.com/en-us/windows/win32/api/dxgi1_2/nn-dxgi1_2-idxgiresource1) - [```mocks::dxgi::resource```](./include/dxgi/resource/resource.h)
- [```IDXGISurface```](https://docs.microsoft.com/en-us/windows/win32/api/dxgi/nn-dxgi-idxgisurface) - [```mocks::dxgi::surface```](./include/dxgi/resource/surface.h)
- Conversion from FOURCC to [```DXGI_FORMAT```](https://docs.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format) - [```mocks::dxgi::to_native```](./include/dxgi/format.h)

#### [Direct3D 11](https://docs.microsoft.com/en-us/windows/win32/direct3d11/atoc-dx-graphics-direct3d-11)
TODO

#### [Direct3D 12](https://docs.microsoft.com/en-us/windows/win32/direct3d12/direct3d-12-graphics)
TODO
