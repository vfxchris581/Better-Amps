# DSP core: header-only, no JUCE, no allocation on the audio path.
add_library(vf_core INTERFACE)
add_library(vf::core ALIAS vf_core)
target_include_directories(vf_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(vf_core INTERFACE cxx_std_17)

if(MSVC)
    target_compile_definitions(vf_core INTERFACE _USE_MATH_DEFINES NOMINMAX)
endif()
