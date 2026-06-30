cmake_minimum_required(VERSION 3.22)

project(SmartSampler VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Point this to your JUCE checkout or vendor JUCE with FetchContent instead.
# add_subdirectory(JUCE)

juce_add_plugin(SmartSampler
    COMPANY_NAME "Golden Digital Services"
    BUNDLE_ID com.goldendigitalservices.smartsampler
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE GdSv
    PLUGIN_CODE SmSp
    FORMATS VST3 Standalone
    PRODUCT_NAME "SmartSampler")

juce_generate_juce_header(SmartSampler)

target_sources(SmartSampler
    PRIVATE
        PluginProcessor-7.cpp
        PluginEditor-5.cpp
        GDSEngine-3.cpp
        AnalyzerEngine.cpp
        SamplerEngine-9.cpp)

target_include_directories(SmartSampler
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(SmartSampler
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1)

target_link_libraries(SmartSampler
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
