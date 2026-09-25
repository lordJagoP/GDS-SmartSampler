cmake_minimum_required(VERSION 3.22)

project(SmartSampler VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Prefer a local JUCE checkout when present. Otherwise fetch a pinned release so
# Windows and Linux builds use the same framework version.
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 9.0.2
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(SmartSampler
    COMPANY_NAME "Golden Digital Services"
    BUNDLE_ID com.goldendigitalservices.smartsampler
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE GdSv
    PLUGIN_CODE SmSp
    FORMATS VST3 Standalone
    PRODUCT_NAME "SmartSampler"
)

juce_generate_juce_header(SmartSampler)

target_sources(SmartSampler
    PRIVATE
        PluginProcessor.cpp
        PluginEditor.cpp
        AnalyzerEngine.cpp
        SamplerEngine.cpp
)

target_compile_definitions(SmartSampler
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(SmartSampler
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
