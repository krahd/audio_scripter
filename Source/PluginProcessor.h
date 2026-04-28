#pragma once

#include <JuceHeader.h>
#include <array>
#include "ScriptEngine.h"


inline juce::String macroParamId (int index)
{
    return "macro" + juce::String (index + 1);
}

class AudioScripterAudioProcessor final : public juce::AudioProcessor,
                                          private juce::AsyncUpdater
{
public:
    AudioScripterAudioProcessor();
    ~AudioScripterAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    scripting::ScriptEngine::CompileResult setScript (const juce::String& source);
    juce::String getScript() const;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return parameters; }

private:
    void handleAsyncUpdate() override;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    std::array<float, 8> getMacroValues() const;
    void queueScriptMacroUpdate (const std::array<float, 8>& values);

    scripting::ScriptEngine engine;
    juce::AudioProcessorValueTreeState parameters;
    std::array<std::atomic<float>*, 8> macroParamAtoms {};
    std::array<std::atomic<float>, 8> pendingMacroValues {};
    std::atomic<bool> pendingMacroUpdate { false };
};
