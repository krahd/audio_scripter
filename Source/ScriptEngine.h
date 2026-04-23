#pragma once

#include "ScriptParser.h"
#include <atomic>

namespace scripting
{
struct CompiledProgram
{
    Program program;
    juce::String source;
};

class ScriptEngine
{
public:
    struct CompileResult
    {
        bool ok { false };
        juce::StringArray errors;
    };

    CompileResult compileAndInstall (const juce::String& source);

    void reset (double sampleRate);
    void processBlock (juce::AudioBuffer<float>& buffer, const std::array<float, 8>& macros);

    juce::String getCurrentSource() const;

private:
    std::shared_ptr<const CompiledProgram> getProgramSnapshot() const;

    std::atomic<std::shared_ptr<const CompiledProgram>> activeProgram;
    std::map<juce::String, float> persistentState;
    std::atomic<bool> stateResetRequested { false };
    double currentSampleRate { 44100.0 };
    uint64_t sampleCounter { 0 };
};

juce::String defaultScript();
juce::StringArray exampleNames();
juce::String exampleScript (int index);
juce::String helpText();
} // namespace scripting
