add_executable(vf_calibrate vf_calibrate.cpp)
target_link_libraries(vf_calibrate PRIVATE vf::core)

add_executable(vf_fx_catalog vf_fx_catalog.cpp)
target_link_libraries(vf_fx_catalog PRIVATE vf::core)

add_executable(vf_cab_catalog vf_cab_catalog.cpp)
target_link_libraries(vf_cab_catalog PRIVATE vf::core)

add_executable(vf_dynamics vf_dynamics.cpp)
target_link_libraries(vf_dynamics PRIVATE vf::core)
if(VF_WITH_NAM)
    target_link_libraries(vf_dynamics PRIVATE vf::nam)
    target_compile_definitions(vf_dynamics PRIVATE VF_WITH_NAM=1)
endif()
