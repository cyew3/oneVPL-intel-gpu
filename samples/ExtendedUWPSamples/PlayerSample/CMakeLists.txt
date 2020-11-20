if(NOT CMAKE_SYSTEM_NAME MATCHES WindowsStore)
  return()
endif()

add_executable(PlayerSample)

set(ASSETS
    Assets/LockScreenLogo.scale-200.png
    Assets/SplashScreen.scale-200.png
    Assets/Square150x150Logo.scale-200.png
    Assets/Square44x44Logo.scale-200.png
    Assets/Square44x44Logo.targetsize-24_altform-unplated.png
    Assets/StoreLogo.png
    Assets/Wide310x150Logo.scale-200.png
)
target_include_directories(PlayerSample
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set_property(SOURCE Package.appxmanifest PROPERTY VS_DEPLOYMENT_CONTENT 1)
set_property(SOURCE "${ASSETS}" PROPERTY VS_DEPLOYMENT_CONTENT 1)
set_property(SOURCE "${ASSETS}" PROPERTY VS_DEPLOYMENT_LOCATION "Assets")

set_property(SOURCE App.xaml PROPERTY VS_XAML_TYPE ApplicationDefinition)

target_sources(PlayerSample
  PRIVATE
    App.xaml
    MainPage.xaml

    App.xaml.h
    MainPage.xaml.h
    pch.h

    App.xaml.cpp
    MainPage.xaml.cpp
    pch.cpp

    "${ASSETS}"

    PlayerSample_TemporaryKey.pfx
)

set_source_files_properties(pch.cpp PROPERTIES COMPILE_FLAGS /Ycpch.h)

set_source_files_properties(App.xaml.h App.xaml.cpp PROPERTIES OBJECT_DEPENDS App.xaml)
set_source_files_properties(MainPage.xaml.h  MainPage.xaml.cpp PROPERTIES OBJECT_DEPENDS MainPage.xaml)

target_link_libraries(PlayerSample PRIVATE libmfx_uwp WindowsApp MSDKInterop)

target_compile_options(PlayerSample PRIVATE /Yupch.h /ZW /wd4453 /wd28204 /EHsc)
target_link_options(PlayerSample PRIVATE /WINMD /SUBSYSTEM:WINDOWS)

set_target_properties(PlayerSample PROPERTIES VS_WINDOWS_TARGET_PLATFORM_MIN_VERSION "${VS_WINDOWS_TARGET_PLATFORM_MIN_VERSION}")

# set_target_properties(PlayerSample PROPERTIES VS_PACKAGE_REFERENCES MSDKInterop)
