cmake_minimum_required(VERSION 3.22)
project(SmartSampler VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(/home/jago/Downloads/JUCE /home/jago/Downloads/SmartSampler/buildV3/JUCE)

juce_add_plugin(SmartSampler
    COMPANY_NAME             "GoldenDigitalServices"
    PLUGIN_MANUFACTURER_CODE GDS1
    PLUGIN_CODE              SmSp
    FORMATS                  VST3
    PRODUCT_NAME             "SmartSampler"
    IS_SYNTH                 TRUE
    NEEDS_MIDI_INPUT         TRUE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES          "Instrument|Sampler"
)

target_sources(SmartSampler PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/AnalyzerEngine.cpp
    Source/SamplerEngine.cpp
    Source/GDSEngine.cpp
)

target_compile_definitions(SmartSampler PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(SmartSampler
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
