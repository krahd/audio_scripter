#include "PluginEditor.h"

namespace
{
juce::String macroParamId (int index)
{
    return "macro" + juce::String (index + 1);
}
} // namespace

AudioScripterAudioProcessorEditor::AudioScripterAudioProcessorEditor (AudioScripterAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setSize (1020, 760);

    titleLabel.setText ("audio_scripter 1.0.5 — script your own impossible effects", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    titleLabel.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    addAndMakeVisible (titleLabel);

    scriptEditor.setFont (juce::FontOptions (15.0f));
    scriptEditor.setMultiLine (true);
    scriptEditor.setReturnKeyStartsNewLine (true);
    scriptEditor.setTabKeyUsedAsCharacter (true);
    scriptEditor.setText (processor.getScript(), false);
    addAndMakeVisible (scriptEditor);

    outputPanel.setMultiLine (true);
    outputPanel.setReadOnly (true);
    outputPanel.setColour (juce::TextEditor::backgroundColourId, juce::Colours::black.withAlpha (0.45f));
    outputPanel.setColour (juce::TextEditor::textColourId, juce::Colours::lightgreen);
    outputPanel.setText ("Ready. p1..p8 macros are available in scripts and can be automated from your DAW.");
    addAndMakeVisible (outputPanel);

    applyButton.addListener (this);
    saveButton.addListener (this);
    loadButton.addListener (this);
    addAndMakeVisible (applyButton);
    addAndMakeVisible (saveButton);
    addAndMakeVisible (loadButton);

    examplesBox.addListener (this);
    examplesBox.addItem ("Select example...", 1);
    const auto names = scripting::exampleNames();
    for (int i = 0; i < names.size(); ++i)
        examplesBox.addItem (names[i], i + 2);
    addAndMakeVisible (examplesBox);

    for (int i = 0; i < 8; ++i)
    {
        auto& slider = macroSliders[(size_t) i];
        slider.setSliderStyle (juce::Slider::LinearVertical);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, true, 52, 18);
        slider.setRange (0.0, 1.0, 0.0001);
        addAndMakeVisible (slider);

        auto& label = macroLabels[(size_t) i];
        label.setText ("p" + juce::String (i + 1), juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (label);

        macroAttachments.push_back (
            std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                processor.getValueTreeState(), macroParamId (i), slider));
    }

    helpPanel.setMultiLine (true);
    helpPanel.setReadOnly (true);
    helpPanel.setColour (juce::TextEditor::backgroundColourId, juce::Colours::darkslategrey.withAlpha (0.5f));
    helpPanel.setText (scripting::helpText());
    addAndMakeVisible (helpPanel);
}

AudioScripterAudioProcessorEditor::~AudioScripterAudioProcessorEditor()
{
    applyButton.removeListener (this);
    saveButton.removeListener (this);
    loadButton.removeListener (this);
    examplesBox.removeListener (this);
}

void AudioScripterAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (22, 26, 32));

    auto bounds = getLocalBounds().toFloat();
    juce::ColourGradient grad (juce::Colour::fromRGB (46, 58, 68), bounds.getTopLeft(),
                               juce::Colour::fromRGB (22, 26, 32), bounds.getBottomRight(), false);
    g.setGradientFill (grad);
    g.fillRect (bounds);
}

void AudioScripterAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    titleLabel.setBounds (area.removeFromTop (30));

    auto controls = area.removeFromTop (30);
    applyButton.setBounds (controls.removeFromLeft (80));
    saveButton.setBounds (controls.removeFromLeft (80));
    loadButton.setBounds (controls.removeFromLeft (80));
    controls.removeFromLeft (10);
    examplesBox.setBounds (controls.removeFromLeft (240));

    area.removeFromTop (8);

    auto macroArea = area.removeFromTop (155);
    const int spacing = 8;
    const int width = (macroArea.getWidth() - spacing * 7) / 8;

    for (int i = 0; i < 8; ++i)
    {
        auto b = macroArea.removeFromLeft (width);
        macroLabels[(size_t) i].setBounds (b.removeFromTop (20));
        macroSliders[(size_t) i].setBounds (b);
        macroArea.removeFromLeft (spacing);
    }

    area.removeFromTop (8);

    auto top = area.removeFromTop (area.getHeight() * 3 / 5);

    scriptEditor.setBounds (top.removeFromLeft (top.getWidth() * 2 / 3));
    top.removeFromLeft (8);
    outputPanel.setBounds (top);

    area.removeFromTop (8);
    helpPanel.setBounds (area);
}

void AudioScripterAudioProcessorEditor::buttonClicked (juce::Button* b)
{
    if (b == &applyButton)
        applyScript();
    else if (b == &saveButton)
        saveScriptToFile();
    else if (b == &loadButton)
        loadScriptFromFile();
}

void AudioScripterAudioProcessorEditor::comboBoxChanged (juce::ComboBox* box)
{
    if (box != &examplesBox)
        return;

    const auto idx = examplesBox.getSelectedId() - 2;
    if (idx >= 0)
        scriptEditor.setText (scripting::exampleScript (idx));
}

void AudioScripterAudioProcessorEditor::applyScript()
{
    const auto result = processor.setScript (scriptEditor.getText());

    if (result.ok)
    {
        outputPanel.setColour (juce::TextEditor::textColourId, juce::Colours::lightgreen);
        outputPanel.setText ("Compiled successfully.");
        return;
    }

    outputPanel.setColour (juce::TextEditor::textColourId, juce::Colours::orange);
    outputPanel.setText (result.errors.joinIntoString ("\n"));
}

void AudioScripterAudioProcessorEditor::saveScriptToFile()
{
    auto chooser = std::make_shared<juce::FileChooser> ("Save script", lastScriptDirectory, "*.ascr");

    chooser->launchAsync (
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
        [this, chooser] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file.getFullPathName().isEmpty())
                return;

            lastScriptDirectory = file.getParentDirectory();
            const bool ok = file.replaceWithText (scriptEditor.getText());

            juce::MessageManager::callAsync ([this, ok, file]
            {
                if (ok)
                {
                    outputPanel.setColour (juce::TextEditor::textColourId, juce::Colours::lightgreen);
                    outputPanel.setText ("Saved script to: " + file.getFullPathName());
                }
                else
                {
                    outputPanel.setColour (juce::TextEditor::textColourId, juce::Colours::red);
                    outputPanel.setText ("Could not save script.");
                }
            });
        });
}

void AudioScripterAudioProcessorEditor::loadScriptFromFile()
{
    auto chooser = std::make_shared<juce::FileChooser> ("Load script", lastScriptDirectory, "*.ascr");

    chooser->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file.getFullPathName().isEmpty())
                return;

            lastScriptDirectory = file.getParentDirectory();
            const auto text = file.loadFileAsString();

            juce::MessageManager::callAsync ([this, text, file]
            {
                scriptEditor.setText (text);
                outputPanel.setText ("Loaded: " + file.getFullPathName());
            });
        });
}
