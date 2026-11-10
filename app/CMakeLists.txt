set(VF_FORMATS VST3 Standalone)
if(APPLE AND VF_BUILD_AU)
    list(APPEND VF_FORMATS AU)
endif()

juce_add_plugin(ViridianForge
    COMPANY_NAME                "Viridian Audio Works"
    COMPANY_WEBSITE             "https://github.com/vfxchris581/new-app"
    BUNDLE_ID                   "com.viridianaudioworks.forge"
    PRODUCT_NAME                "Viridian Forge"
    VERSION                     ${PROJECT_VERSION}
    PLUGIN_MANUFACTURER_CODE    Vrdn
    PLUGIN_CODE                 VFg1
    FORMATS                     ${VF_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT  "Viridian Forge needs audio input to process your guitar."
    VST3_CATEGORIES             Fx Distortion
    AU_MAIN_TYPE                kAudioUnitType_Effect
    ICON_BIG                    ${CMAKE_SOURCE_DIR}/resources/branding/icon_1024.png
    ICON_SMALL                  ${CMAKE_SOURCE_DIR}/resources/branding/icon_256.png
    COPY_PLUGIN_AFTER_BUILD     FALSE)

juce_generate_juce_header(ViridianForge)

juce_add_binary_data(ViridianForgeAssets
    NAMESPACE VFAssets
    SOURCES
        ${CMAKE_SOURCE_DIR}/resources/branding/icon_256.png
        ${CMAKE_SOURCE_DIR}/resources/branding/logo_wordmark.png)

target_sources(ViridianForge PRIVATE
    source/PluginProcessor.cpp
    source/PluginEditor.cpp
    source/engine/AmpBlock.cpp
    source/engine/CabBlock.cpp
    source/engine/NamLoaderFx.cpp
    source/ui/ViridianLookAndFeel.cpp
    source/ui/Widgets.cpp
    source/ui/Panels.cpp
    source/ui/Pedalboard.cpp
    source/ui/CabRoom.cpp
    source/ui/CabCube.cpp)

target_include_directories(ViridianForge PRIVATE source)

target_compile_definitions(ViridianForge PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STANDALONE_FILTER_WINDOW_USE_KIOSK_MODE=0)

if(VF_WITH_NAM)
    target_link_libraries(ViridianForge PRIVATE vf::nam)
    target_compile_definitions(ViridianForge PRIVATE VF_WITH_NAM=1)
else()
    target_compile_definitions(ViridianForge PRIVATE VF_WITH_NAM=0)
endif()

target_link_libraries(ViridianForge
    PRIVATE
        vf::core
        ViridianForgeAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

option(VF_BUILD_SNAPSHOT_TOOL "Build the UI snapshot / smoke-test tool" OFF)
if(VF_BUILD_SNAPSHOT_TOOL)
    add_subdirectory(${CMAKE_SOURCE_DIR}/tools/snapshot ${CMAKE_BINARY_DIR}/tools/snapshot)
endif()
