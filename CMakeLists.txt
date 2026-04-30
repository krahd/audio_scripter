cmake_minimum_required(VERSION 3.22)
project(audio_scripter VERSION 0.0.9)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
option(AUDIO_SCRIPTER_BUILD_TESTS "Build lightweight parser tests" ON)
option(AUDIO_SCRIPTER_BUILD_PLUGIN "Build the JUCE plugin targets" ON)
option(AUDIO_SCRIPTER_BUILD_TOOLS "Configure offline analysis/render tools" ON)

set(AUDIO_SCRIPTER_JUCE_PATH "" CACHE PATH "Path to a local JUCE checkout (optional)")

if (AUDIO_SCRIPTER_JUCE_PATH)
  message(STATUS "Using local JUCE from: ${AUDIO_SCRIPTER_JUCE_PATH}")
  add_subdirectory(${AUDIO_SCRIPTER_JUCE_PATH} juce)
else()
  include(FetchContent)
  FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.8
  )
  FetchContent_MakeAvailable(juce)
endif()

if (AUDIO_SCRIPTER_BUILD_TOOLS)
  if (NOT TARGET audio_scripter_examples)
    file(GLOB AUDIO_SCRIPTER_EXAMPLE_SOURCES CONFIGURE_DEPENDS
      "${CMAKE_SOURCE_DIR}/examples/*.ascr")

    juce_add_binary_data(audio_scripter_examples
      NAMESPACE AudioScripterExamples
      HEADER_NAME AudioScripterExampleData.h
      SOURCES ${AUDIO_SCRIPTER_EXAMPLE_SOURCES})
  endif()

  add_executable(audio_scripter_render_report EXCLUDE_FROM_ALL
    tools/RenderEffectReport.cpp
    Source/ScriptParser.cpp
    Source/ScriptTokenizer.cpp
    Source/ScriptEngine.cpp)

  target_include_directories(audio_scripter_render_report PRIVATE Source)

  target_compile_definitions(audio_scripter_render_report PRIVATE
    AUDIO_SCRIPTER_VERSION_STRING="${PROJECT_VERSION}"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    EXAMPLES_DIR="${CMAKE_SOURCE_DIR}/examples")

  target_link_libraries(audio_scripter_render_report PRIVATE
    audio_scripter_examples
    juce::juce_audio_formats
    juce::juce_audio_basics
    juce::juce_core
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
endif()

if (AUDIO_SCRIPTER_BUILD_PLUGIN)
  if (NOT TARGET audio_scripter_examples)
    file(GLOB AUDIO_SCRIPTER_EXAMPLE_SOURCES CONFIGURE_DEPENDS
      "${CMAKE_SOURCE_DIR}/examples/*.ascr")

    juce_add_binary_data(audio_scripter_examples
      NAMESPACE AudioScripterExamples
      HEADER_NAME AudioScripterExampleData.h
      SOURCES ${AUDIO_SCRIPTER_EXAMPLE_SOURCES})
  endif()

  juce_add_plugin(audio_scripter
    COMPANY_NAME "Tomas Laurenzo"
    BUNDLE_ID com.audioscripter.plugin
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE AdSc
    PLUGIN_CODE Ascp
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "audio_scripter")

  juce_generate_juce_header(audio_scripter)

  target_sources(audio_scripter PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ScriptEngine.cpp
    Source/ScriptParser.cpp
    Source/ScriptTokenizer.cpp
    Source/ScriptCodeTokeniser.cpp)

  target_include_directories(audio_scripter PRIVATE Source)

  target_compile_definitions(audio_scripter PRIVATE
    AUDIO_SCRIPTER_VERSION_STRING="${PROJECT_VERSION}"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

  # Suppress JUCE VST3/VST2 parameter ID mismatch error for local builds
  target_compile_definitions(audio_scripter PRIVATE
    JUCE_IGNORE_VST3_MISMATCHED_PARAMETER_ID_WARNING=1)

  target_link_libraries(audio_scripter PRIVATE
    audio_scripter_examples
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
endif()

if (AUDIO_SCRIPTER_BUILD_TESTS)
  enable_testing()

  if (NOT TARGET audio_scripter_examples)
    file(GLOB AUDIO_SCRIPTER_EXAMPLE_SOURCES CONFIGURE_DEPENDS
      "${CMAKE_SOURCE_DIR}/examples/*.ascr")

    juce_add_binary_data(audio_scripter_examples
      NAMESPACE AudioScripterExamples
      HEADER_NAME AudioScripterExampleData.h
      SOURCES ${AUDIO_SCRIPTER_EXAMPLE_SOURCES})
  endif()

  add_executable(audio_scripter_parser_tests
    tests/ScriptParserTests.cpp
    Source/ScriptParser.cpp
    Source/ScriptTokenizer.cpp
    Source/ScriptEngine.cpp)

  target_include_directories(audio_scripter_parser_tests PRIVATE Source)

  target_compile_definitions(audio_scripter_parser_tests PRIVATE
    AUDIO_SCRIPTER_VERSION_STRING="${PROJECT_VERSION}"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

  target_link_libraries(audio_scripter_parser_tests PRIVATE
    audio_scripter_examples
    juce::juce_audio_basics
    juce::juce_core
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
  # Provide the absolute examples directory to the test so it can locate example scripts
  target_compile_definitions(audio_scripter_parser_tests PRIVATE EXAMPLES_DIR="${CMAKE_SOURCE_DIR}/examples")

  # Run the test with the project source directory as the working directory to keep behavior consistent
  add_test(NAME audio_scripter_parser_tests COMMAND audio_scripter_parser_tests)
  set_tests_properties(audio_scripter_parser_tests PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()
