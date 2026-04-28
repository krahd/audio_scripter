#pragma once

#include <JuceHeader.h>
#include "Constants.h"
#include "ScriptParser.h"
#include <atomic>
#include <memory>

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
    void processBlock (juce::AudioBuffer<float>& buffer, std::array<float, 8>& macros);

    juce::String getCurrentSource() const;
    juce::String getLastError() const;

private:
    std::shared_ptr<const CompiledProgram> getProgramSnapshot() const;
    void enforcePersistentStateLimit();

    // Use a plain shared_ptr and the atomic helper functions
    // (std::atomic_store / std::atomic_load) for portability with libc++.
    std::shared_ptr<const CompiledProgram> activeProgram;
    std::map<juce::String, float> persistentState;
    std::atomic<bool> stateResetRequested { false };
    double currentSampleRate { 44100.0 };
    uint64_t sampleCounter { 0 };
    juce::String lastError;
};

juce::String defaultScript();
juce::StringArray exampleNames();
juce::String exampleScript (int index);
juce::String helpText();
} // namespace scripting
