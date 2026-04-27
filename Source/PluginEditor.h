#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class AudioScripterAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                private juce::Button::Listener,
                                                private juce::ComboBox::Listener
{
public:
    explicit AudioScripterAudioProcessorEditor (AudioScripterAudioProcessor&);
    ~AudioScripterAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void buttonClicked (juce::Button*) override;
    void comboBoxChanged (juce::ComboBox*) override;

    void applyScript();
    void saveScriptToFile();
    void loadScriptFromFile();

    AudioScripterAudioProcessor& processor;

    juce::Label titleLabel;
    juce::TextEditor scriptEditor;
    juce::TextEditor outputPanel;
    juce::TextButton applyButton { "Apply" };
    juce::TextButton saveButton { "Save" };
    juce::TextButton loadButton { "Load" };
    juce::ComboBox examplesBox;
    std::vector<juce::File> exampleFiles;
    juce::TextEditor helpPanel;

    std::array<juce::Slider, 8> macroSliders;
    std::array<juce::Label, 8> macroLabels;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> macroAttachments;

    juce::File lastScriptDirectory;
};
