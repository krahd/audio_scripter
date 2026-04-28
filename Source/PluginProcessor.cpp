#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

AudioScripterAudioProcessor::AudioScripterAudioProcessor()
    : AudioProcessor (BusesProperties()
                      .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
    parameters (*this, nullptr, juce::Identifier ("PARAMETERS"), createParameterLayout())
{
    for (auto& pending : pendingMacroValues)
        pending.store (0.0f);

    for (int i = 0; i < 8; ++i)
        macroParamAtoms[(size_t) i] = parameters.getRawParameterValue (macroParamId (i));

    setScript (scripting::defaultScript());
}

juce::AudioProcessorValueTreeState::ParameterLayout AudioScripterAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> layout;

    for (int i = 0; i < 8; ++i)
    {
        layout.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (macroParamId (i), 1),
            "Macro " + juce::String (i + 1),
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f),
            i == 0 ? 0.25f : 0.0f));
    }

    return { layout.begin(), layout.end() };
}

std::array<float, 8> AudioScripterAudioProcessor::getMacroValues() const
{
    std::array<float, 8> values {};

    for (int i = 0; i < 8; ++i)
        values[(size_t) i] = macroParamAtoms[(size_t) i] != nullptr ? macroParamAtoms[(size_t) i]->load() : 0.0f;

    return values;
}

void AudioScripterAudioProcessor::prepareToPlay (double sampleRate, int)
{
    engine.reset (sampleRate);
}

void AudioScripterAudioProcessor::releaseResources() {}

bool AudioScripterAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo())
        && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
        && in == out;
}

void AudioScripterAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused (midi);
    juce::ScopedNoDenormals noDenormals;

    auto macroValues = getMacroValues();
    engine.processBlock (buffer, macroValues);
    queueScriptMacroUpdate (macroValues);
}

void AudioScripterAudioProcessor::queueScriptMacroUpdate (const std::array<float, 8>& values)
{
    bool changed = false;

    for (int i = 0; i < 8; ++i)
    {
        const auto current = macroParamAtoms[(size_t) i] != nullptr ? macroParamAtoms[(size_t) i]->load() : 0.0f;
        const auto target = juce::jlimit (0.0f, 1.0f, values[(size_t) i]);

        if (std::abs (target - current) > 1.0e-4f)
        {
            pendingMacroValues[(size_t) i].store (target);
            changed = true;
        }
    }

    if (changed)
    {
        pendingMacroUpdate.store (true);
        triggerAsyncUpdate();
    }
}

void AudioScripterAudioProcessor::handleAsyncUpdate()
{
    if (! pendingMacroUpdate.exchange (false))
        return;

    for (int i = 0; i < 8; ++i)
    {
        auto* parameter = parameters.getParameter (macroParamId (i));
        if (parameter == nullptr)
            continue;

        const auto target = pendingMacroValues[(size_t) i].load();
        const auto current = macroParamAtoms[(size_t) i] != nullptr ? macroParamAtoms[(size_t) i]->load() : 0.0f;
        if (std::abs (target - current) <= 1.0e-4f)
            continue;

        parameter->setValueNotifyingHost (target);
    }
}

juce::AudioProcessorEditor* AudioScripterAudioProcessor::createEditor()
{
    return new AudioScripterAudioProcessorEditor (*this);
}

scripting::ScriptEngine::CompileResult AudioScripterAudioProcessor::setScript (const juce::String& source)
{
    return engine.compileAndInstall (source);
}

juce::String AudioScripterAudioProcessor::getScript() const
{
    return engine.getCurrentSource();
}

void AudioScripterAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    state.setProperty ("script", getScript(), nullptr);

    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void AudioScripterAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState == nullptr)
        return;

    const auto state = juce::ValueTree::fromXml (*xmlState);
    if (! state.isValid())
        return;

    if (state.hasType (parameters.state.getType()))
        parameters.replaceState (state);

    setScript (state.getProperty ("script", scripting::defaultScript()).toString());
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioScripterAudioProcessor();
}
