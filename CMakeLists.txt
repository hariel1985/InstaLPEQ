cmake_minimum_required(VERSION 3.22)
project(InstaLPEQ VERSION 1.2.2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../JUCE ${CMAKE_CURRENT_BINARY_DIR}/JUCE)

juce_add_plugin(InstaLPEQ
    COMPANY_NAME "InstaLPEQ"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_MANUFACTURER_CODE Inst
    PLUGIN_CODE Ilpe
    FORMATS VST3 AU LV2
    LV2URI "https://github.com/hariel1985/InstaLPEQ"
    PRODUCT_NAME "InstaLPEQ"
    COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_generate_juce_header(InstaLPEQ)

juce_add_binary_data(InstaLPEQData SOURCES
    Resources/Rajdhani-Regular.ttf
    Resources/Rajdhani-Medium.ttf
    Resources/Rajdhani-Bold.ttf
)

target_sources(InstaLPEQ
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/LookAndFeel.cpp
        Source/EQCurveDisplay.cpp
        Source/FIREngine.cpp
        Source/NodeParameterPanel.cpp
        Source/SignalChainPanel.cpp
)

target_compile_definitions(InstaLPEQ
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(InstaLPEQ
    PRIVATE
        InstaLPEQData
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_dsp
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
