cmake_minimum_required(VERSION 3.22)

project(NiveladorVMix VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# No Windows: runtime estático (não precisa instalar o Visual C++ Redistributable no PC do vMix)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(NIVELADOR_BUILD_PLUGIN "Compila o plugin (precisa do JUCE)" ON)
option(NIVELADOR_VST3_MANIFEST "Gera moduleinfo.json (desligar em compilação cruzada)" ON)

# Testes do processamento de áudio (não dependem do JUCE)
enable_testing()
add_executable(test_dsp tests/test_dsp.cpp)
target_include_directories(test_dsp PRIVATE Source)
add_test(NAME test_dsp COMMAND test_dsp)
if(MSVC)
    target_compile_options(test_dsp PRIVATE /utf-8)
endif()

if(NIVELADOR_BUILD_PLUGIN)
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)

    juce_add_plugin(NiveladorVMix
        COMPANY_NAME "JCO"
        PLUGIN_MANUFACTURER_CODE Jcoa
        PLUGIN_CODE Nvlr
        PRODUCT_NAME "Nivelador vMix"
        FORMATS VST3
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES Fx Dynamics
        VST3_AUTO_MANIFEST ${NIVELADOR_VST3_MANIFEST})

    target_sources(NiveladorVMix PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

    target_compile_definitions(NiveladorVMix PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

    if(MSVC)
        target_compile_options(NiveladorVMix PRIVATE /utf-8)
    endif()

    target_link_libraries(NiveladorVMix
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endif()

