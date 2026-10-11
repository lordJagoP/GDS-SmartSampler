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

function(add_smartsampler target product bundle code synth category)
juce_add_plugin(${target}
    COMPANY_NAME "Golden Digital Services"
    BUNDLE_ID ${bundle}
    IS_SYNTH ${synth}
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE GdSv
    PLUGIN_CODE ${code}
    FORMATS VST3 Standalone
    VST3_CATEGORIES ${category}
    PRODUCT_NAME "${product}"
)

juce_generate_juce_header(${target})

target_sources(${target}
    PRIVATE
        PluginProcessor.cpp
        PluginEditor.cpp
        AnalyzerEngine.cpp
        SamplerEngine.cpp
)

target_compile_definitions(${target}
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(${target}
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
endfunction()

# Separate identities keep existing instrument sessions and presets intact.
add_smartsampler(SmartSampler "SmartSampler"
    com.goldendigitalservices.smartsampler SmSp TRUE Instrument)
add_smartsampler(SmartSamplerFX "SmartSampler FX"
    com.goldendigitalservices.smartsamplerfx SmFx FALSE Fx)

option(SMARTSAMPLER_BUILD_TESTS "Build processor regression tests" OFF)
if (SMARTSAMPLER_BUILD_TESTS)
    enable_testing()
    foreach(role IN ITEMS SmartSampler SmartSamplerFX)
        juce_add_console_app(${role}Tests PRODUCT_NAME "${role} Tests")
        target_include_directories(${role}Tests PRIVATE
            "${CMAKE_CURRENT_BINARY_DIR}/${role}_artefacts/JuceLibraryCode"
            "${CMAKE_CURRENT_SOURCE_DIR}"
            "$<TARGET_PROPERTY:juce::juce_audio_utils,INTERFACE_INCLUDE_DIRECTORIES>")
        target_sources(${role}Tests PRIVATE ../Tests/ProcessorTests.cpp)
        target_link_libraries(${role}Tests PRIVATE ${role})
        add_test(NAME ${role}Tests COMMAND ${role}Tests)
    endforeach()
endif()
