# Neural Amp Modeler support: the official NeuralAmpModelerCore (MIT) compiled
# as C++20 behind vf/nam/NamEngine.h, so the rest of the project stays C++17.

file(GLOB VF_NAM_CORE_SOURCES CONFIGURE_DEPENDS
    "${VF_NAM_DIR}/NAM/*.cpp"
    "${VF_NAM_DIR}/NAM/wavenet/*.cpp")

# OBJECT library: NAM registers its architectures with static initializers, which a
# static library would let the linker drop.
add_library(vf_nam OBJECT src/NamEngine.cpp ${VF_NAM_CORE_SOURCES})
add_library(vf::nam ALIAS vf_nam)

set_target_properties(vf_nam PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON POSITION_INDEPENDENT_CODE ON)
target_include_directories(vf_nam
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${VF_NAM_DIR} ${VF_NAM_DIR}/NAM ${VF_NAM_DIR}/Dependencies/nlohmann)
target_include_directories(vf_nam SYSTEM PRIVATE ${VF_EIGEN_DIR})
target_compile_definitions(vf_nam PRIVATE NAM_SAMPLE_FLOAT NAM_ENABLE_A2_FAST)
target_link_libraries(vf_nam PUBLIC vf::core)

if(MSVC)
    target_compile_options(vf_nam PRIVATE /bigobj /W3)
    target_compile_definitions(vf_nam PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _USE_MATH_DEFINES)
else()
    # Third-party code: keep our own warnings for NamEngine.cpp only.
    set_source_files_properties(${VF_NAM_CORE_SOURCES} PROPERTIES COMPILE_OPTIONS "-w")
endif()
