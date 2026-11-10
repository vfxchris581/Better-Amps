# Developer tool: renders the editor for several scenarios and smoke-tests audio.
juce_add_console_app(vf_snapshot PRODUCT_NAME "vf_snapshot")
target_sources(vf_snapshot PRIVATE Snapshot.cpp)
target_include_directories(vf_snapshot PRIVATE ${CMAKE_SOURCE_DIR}/app/source ${CMAKE_SOURCE_DIR}/nam/include)
target_compile_definitions(vf_snapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 VF_WITH_NAM=$<BOOL:${VF_WITH_NAM}> JUCE_MODAL_LOOPS_PERMITTED=1)
# ViridianForge (the plugin's shared code) already contains the NAM objects; only headers are needed here.
target_link_libraries(vf_snapshot PRIVATE ViridianForge ViridianForgeAssets vf::core juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
juce_generate_juce_header(vf_snapshot)
