#include "PluginEditor.h"
#include <algorithm>
#include <regex>
#include <fstream>
#include <sstream>

// Try to detect and fix common Windows-1252 -> UTF-8 mojibake when loading files.
static std::string cp1252_to_utf8 (const std::string& in)
{
    std::string out;
    out.reserve (in.size() * 2);

    for (unsigned char c : in)
    {
        if (c < 0x80)
        {
            out.push_back ((char) c);
        }
        else if (c >= 0xA0)
        {
            unsigned int cp = c; // 0xA0..0xFF -> U+00A0..U+00FF
            if (cp <= 0x7FF)
            {
                out.push_back ((char) (0xC0 | (cp >> 6)));
                out.push_back ((char) (0x80 | (cp & 0x3F)));
            }
            else
            {
                // not expected here
            }
        }
        else
        {
            unsigned int cp = 0;
            switch (c)
            {
                case 0x80: cp = 0x20AC; break; case 0x81: cp = 0x0081; break; case 0x82: cp = 0x201A; break;
                case 0x83: cp = 0x0192; break; case 0x84: cp = 0x201E; break; case 0x85: cp = 0x2026; break;
                case 0x86: cp = 0x2020; break; case 0x87: cp = 0x2021; break; case 0x88: cp = 0x02C6; break;
                case 0x89: cp = 0x2030; break; case 0x8A: cp = 0x0160; break; case 0x8B: cp = 0x2039; break;
                case 0x8C: cp = 0x0152; break; case 0x8D: cp = 0x008D; break; case 0x8E: cp = 0x017D; break;
                case 0x8F: cp = 0x008F; break; case 0x90: cp = 0x0090; break; case 0x91: cp = 0x2018; break;
                case 0x92: cp = 0x2019; break; case 0x93: cp = 0x201C; break; case 0x94: cp = 0x201D; break;
                case 0x95: cp = 0x2022; break; case 0x96: cp = 0x2013; break; case 0x97: cp = 0x2014; break;
                case 0x98: cp = 0x02DC; break; case 0x99: cp = 0x2122; break; case 0x9A: cp = 0x0161; break;
                case 0x9B: cp = 0x203A; break; case 0x9C: cp = 0x0153; break; case 0x9D: cp = 0x009D; break;
                case 0x9E: cp = 0x017E; break; case 0x9F: cp = 0x0178; break;
            }

            if (cp <= 0x7F)
                out.push_back ((char) cp);
            else if (cp <= 0x7FF)
            {
                out.push_back ((char) (0xC0 | (cp >> 6)));
                out.push_back ((char) (0x80 | (cp & 0x3F)));
            }
            else
            {
                out.push_back ((char) (0xE0 | (cp >> 12)));
                out.push_back ((char) (0x80 | ((cp >> 6) & 0x3F)));
                out.push_back ((char) (0x80 | (cp & 0x3F)));
            }
        }
    }

    return out;
}

static juce::String loadTextFileFixEncoding (const juce::File& file)
{
    std::ifstream in (file.getFullPathName().toStdString(), std::ios::binary);
    if (! in)
        return {};

    std::ostringstream ss;
    ss << in.rdbuf();
    const std::string raw = ss.str();

    // If the raw bytes contain any CP1252-only control codes (0x80..0x9F), try CP1252 -> UTF-8 conversion.
    for (unsigned char c : raw)
    {
        if (c >= 0x80 && c <= 0x9F)
            return juce::String (cp1252_to_utf8 (raw));
    }

    // Otherwise assume UTF-8
    return juce::String (raw);
}

// Detect and fix common UTF-8 -> CP1252 mojibake that appears as sequences like
// "â" when a UTF-8 file was interpreted as Latin-1/CP1252. This tries a
// conservative recovery only when the pattern is present and all characters
// are in the single-byte range.
static juce::String fixCp1252Mojibake (const juce::String& in)
{
    // Quick heuristic: look for the E2 80 .. sequence turned into three
    // single-byte characters such as U+00E2 U+0080 U+00XX.
    bool suspicious = false;
    const int len = in.length();
    for (int i = 0; i + 2 < len; ++i)
    {
        const auto a = (juce_wchar) in[i];
        const auto b = (juce_wchar) in[i + 1];
        const auto c = (juce_wchar) in[i + 2];
        if (a == 0x00E2 && (b >= 0x0080 && b <= 0x00BF) && (c >= 0x0080 && c <= 0x00BF))
        {
            suspicious = true;
            break;
        }
    }

    if (! suspicious)
        return in;

    // Ensure all characters are in the single-byte range so we can reconstruct
    // the original raw bytes and pass them through the CP1252 -> UTF-8 fixer.
    std::string raw;
    raw.reserve ((size_t) len);
    for (int i = 0; i < len; ++i)
    {
        const juce_wchar w = in[i];
        if (w > 0xFF)
            return in; // abort if we encounter non-single-byte chars
        raw.push_back ((char) (w & 0xFF));
    }

    return juce::String (cp1252_to_utf8 (raw));
}

static void applyMacroInitialValuesFromText (AudioScripterAudioProcessor& processorRef, const juce::String& text)
{
    try
    {
        const std::string s = text.toStdString();
        std::regex re ("\\bp([1-8])\\s*=\\s*([-+]?(?:\\d+\\.?\\d*|\\.\\d+))");
        std::smatch m;
        auto it = s.cbegin();
        while (std::regex_search (it, s.cend(), m, re))
        {
            const int idx = m[1].str()[0] - '1';
            double val = 0.0;
            try { val = std::stod (m[2].str()); } catch (...) { val = 0.0; }
            if (val < 0.0) val = 0.0; if (val > 1.0) val = 1.0;
            if (idx >= 0 && idx < 8)
            {
                if (auto* p = processorRef.getValueTreeState().getParameter (macroParamId (idx)))
                    p->setValueNotifyingHost ((float) val);
            }
            it = m.suffix().first;
        }
    }
    catch (...) {}
}

static std::array<juce::String, 8> parseMacroLabelsFromText (const juce::String& text)
{
    std::array<juce::String, 8> labels;
    for (int i = 0; i < 8; ++i)
        labels[(size_t) i] = "p" + juce::String (i + 1);

    try
    {
        const std::string source = text.toStdString();
        std::regex re ("^\\s*#\\s*@p([1-8])\\s*[:=]\\s*(.*?)\\s*$", std::regex::icase);
        std::smatch m;
        std::stringstream lines (source);
        std::string line;

        while (std::getline (lines, line))
        {
            if (! std::regex_match (line, m, re))
                continue;

            const int idx = m[1].str()[0] - '1';
            auto label = juce::String (m[2].str()).trim();
            if (idx < 0 || idx >= 8 || label.isEmpty())
                continue;
            labels[(size_t) idx] = label;
        }
    }
    catch (...) {}

    return labels;
}


AudioScripterAudioProcessorEditor::AudioScripterAudioProcessorEditor (AudioScripterAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    // allow host/standalone window to be resizable when supported
    setResizable (true, true);
    setResizeLimits (600, 350, 3840, 2400);

    titleLabel.setText ("audio_scripter 0.0.8", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    titleLabel.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    addAndMakeVisible (titleLabel);

    websiteButton.setFont (juce::FontOptions (12.0f), false, juce::Justification::centredRight);
    websiteButton.setColour (juce::HyperlinkButton::textColourId, juce::Colour (0xff4ec9b0));
    addAndMakeVisible (websiteButton);

    aboutButton.setFont (juce::FontOptions (12.0f), false, juce::Justification::centredRight);
    aboutButton.setColour (juce::HyperlinkButton::textColourId, juce::Colour (0xff4ec9b0));

    // Create tokeniser + code editor with syntax colouring and line numbers
    codeTokeniser = std::make_unique<ScriptCodeTokeniser>();
    scriptEditor = std::make_unique<juce::CodeEditorComponent> (codeDocument, codeTokeniser.get());
    // Use a consistent font with fallback enabled and a 1.25 line-spacing factor.
    const float baseEditorFontHeight = 13.0f;
    const float lineSpacingFactor = 1.25f;
    auto editorFontOptions = juce::FontOptions (baseEditorFontHeight * lineSpacingFactor).withFallbackEnabled (true);
    scriptEditor->setFont (editorFontOptions);
    scriptEditor->setLineNumbersShown (true);

    // Solid colour: darkslategrey@50% pre-blended over the plugin gradient midpoint.
    // CodeEditorComponent is internally opaque so alpha values do not composite correctly.
    scriptEditor->setColour (juce::CodeEditorComponent::backgroundColourId,    juce::Colour::fromRGB (38, 56, 60));
    scriptEditor->setColour (juce::CodeEditorComponent::defaultTextColourId,   juce::Colour (0xffd4d4d4));
    scriptEditor->setColour (juce::CodeEditorComponent::highlightColourId,     juce::Colour (0xff264f78));
    scriptEditor->setColour (juce::CodeEditorComponent::lineNumberTextId,      juce::Colour (0xff858585));
    scriptEditor->setColour (juce::CodeEditorComponent::lineNumberBackgroundId, juce::Colour::fromRGB (30, 46, 50));

    // Script-aware colour scheme with full token coverage from ScriptCodeTokeniser.
    scriptEditor->setColourScheme (codeTokeniser->getDefaultColourScheme());
    // initial content (fix encoding when loading from disk)
    scriptEditor->loadContent (processor.getScript());
    addAndMakeVisible (*scriptEditor);

    outputPanel.setMultiLine (true);
    outputPanel.setReadOnly (true);
    outputPanel.setScrollbarsShown (true);
    outputPanel.setColour (juce::TextEditor::backgroundColourId, juce::Colours::black.withAlpha (0.45f));
    addAndMakeVisible (outputPanel);
    appendToLog ("Ready. p1..p8 macros are available in scripts and can be automated from your DAW.");

    applyButton.addListener (this);
    saveButton.addListener (this);
    loadButton.addListener (this);
    aboutButton.addListener (this);
    defaultsButton.addListener (this);
    addAndMakeVisible (applyButton);
    addAndMakeVisible (saveButton);
    addAndMakeVisible (loadButton);
    addAndMakeVisible (aboutButton);
    addAndMakeVisible (defaultsButton);

    examplesBox.addListener (this);
    examplesBox.addItem ("Select example...", 1);

#if defined(EXAMPLES_DIR)
    {
        juce::File examplesDir (EXAMPLES_DIR);
        if (examplesDir.isDirectory())
        {
            juce::Array<juce::File> files;
            examplesDir.findChildFiles (files, juce::File::findFiles, false, "*.ascr");
            if (files.size() > 1)
            {
                std::sort(files.getRawDataPointer(), files.getRawDataPointer() + files.size(),
                          [] (const juce::File& a, const juce::File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });
            }
            for (int i = 0; i < files.size(); ++i)
            {
                examplesBox.addItem (files[i].getFileNameWithoutExtension(), i + 2);
                exampleFiles.push_back (files[i]);
            }
        }
        else
        {
            const auto names = scripting::exampleNames();
            for (int i = 0; i < names.size(); ++i)
                examplesBox.addItem (names[i], i + 2);
        }
    }
#else
    {
        const auto names = scripting::exampleNames();
        for (int i = 0; i < names.size(); ++i)
            examplesBox.addItem (names[i], i + 2);
    }
#endif

    addAndMakeVisible (examplesBox);
    // make "Select example..." visible by default
    examplesBox.setSelectedId (1, juce::dontSendNotification);

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

    helpPanel = std::make_unique<juce::CodeEditorComponent> (helpDocument, codeTokeniser.get());
    helpPanel->setReadOnly (true);
    helpPanel->setLineNumbersShown (false);
    // Use the same font options as the main editor so glyph fallback and spacing match.
    helpPanel->setFont (editorFontOptions);
    helpPanel->setColour (juce::CodeEditorComponent::backgroundColourId,    juce::Colour::fromRGB (38, 56, 60));
    helpPanel->setColour (juce::CodeEditorComponent::defaultTextColourId,   juce::Colour (0xffd4d4d4));
    helpPanel->setColour (juce::CodeEditorComponent::highlightColourId,     juce::Colour (0xff264f78));
    helpPanel->setColour (juce::CodeEditorComponent::lineNumberBackgroundId, juce::Colour::fromRGB (38, 56, 60));
    helpPanel->setColourScheme (codeTokeniser->getDefaultColourScheme());
    helpDocument.replaceAllContent (fixCp1252Mojibake (scripting::helpText()));
    addAndMakeVisible (*helpPanel);

    applyScriptMetadata();

    // setSize must come after all child components are created so that the
    // first resized() call can lay them out correctly.
    setSize (1020, 760);
}

AudioScripterAudioProcessorEditor::~AudioScripterAudioProcessorEditor()
{
    applyButton.removeListener (this);
    saveButton.removeListener (this);
    loadButton.removeListener (this);
    aboutButton.removeListener (this);
    defaultsButton.removeListener (this);
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
    auto titleRow = area.removeFromTop (30);
    websiteButton.setBounds (titleRow.removeFromRight (240));
    aboutButton.setBounds (titleRow.removeFromRight (46));
    titleLabel.setBounds (titleRow);

    auto controls = area.removeFromTop (30);
    saveButton.setBounds (controls.removeFromLeft (80));
    loadButton.setBounds (controls.removeFromLeft (80));
    controls.removeFromLeft (10);
    examplesBox.setBounds (controls.removeFromLeft (240));
    controls.removeFromLeft (10);
    applyButton.setBounds (controls.removeFromLeft (80));
    controls.removeFromLeft (10);
    defaultsButton.setBounds (controls.removeFromLeft (80));

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

    const auto editorArea = top.removeFromLeft (top.getWidth() * 2 / 3);
    if (scriptEditor)
        scriptEditor->setBounds (editorArea);
    else
        scriptEditor = nullptr;
    top.removeFromLeft (8);
    outputPanel.setBounds (top);

    area.removeFromTop (8);
    if (helpPanel)
        helpPanel->setBounds (area);
}

void AudioScripterAudioProcessorEditor::buttonClicked (juce::Button* b)
{
    if (b == &applyButton)
        applyScript();
    else if (b == &saveButton)
        saveScriptToFile();
    else if (b == &loadButton)
        loadScriptFromFile();
    else if (b == &aboutButton)
        showAboutBox();
    else if (b == &defaultsButton)
        applyScriptMetadata();
}

void AudioScripterAudioProcessorEditor::comboBoxChanged (juce::ComboBox* box)
{
    if (box != &examplesBox)
        return;

    const auto idx = examplesBox.getSelectedId() - 2;
    if (idx < 0)
        return;

    // If we have example files discovered at runtime, load the file contents.
    if (idx < (int) exampleFiles.size())
    {
        const auto file = exampleFiles[(size_t) idx];
        if (file.existsAsFile())
        {
            scriptEditor->loadContent (loadTextFileFixEncoding (file));
            applyScript();
        }
        return;
    }

    // Fallback to built-in indexed examples (if available).
    const auto fallbackIdx = idx - (int) exampleFiles.size();
    if (fallbackIdx >= 0)
    {
        scriptEditor->loadContent (scripting::exampleScript (fallbackIdx));
        applyScript();
    }
}

void AudioScripterAudioProcessorEditor::applyScript()
{
    applyScriptMetadata();
    const auto result = processor.setScript (scriptEditor->getDocument().getAllContent());

    if (result.ok)
    {
        appendToLog ("Compiled successfully.");
        return;
    }

    appendToLog (result.errors.joinIntoString ("\n"), juce::Colours::orange);
}

void AudioScripterAudioProcessorEditor::applyScriptMetadata()
{
    if (scriptEditor == nullptr)
        return;

    const auto text = scriptEditor->getDocument().getAllContent();
    applyMacroInitialValuesFromText (processor, text);
    applyMacroLabelsFromText (text);
}

void AudioScripterAudioProcessorEditor::applyMacroLabelsFromText (const juce::String& text)
{
    const auto labels = parseMacroLabelsFromText (text);
    for (int i = 0; i < 8; ++i)
        macroLabels[(size_t) i].setText (labels[(size_t) i], juce::dontSendNotification);
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
            const bool ok = file.replaceWithText (scriptEditor->getDocument().getAllContent());

            juce::MessageManager::callAsync ([this, ok, file]
            {
                if (ok)
                    appendToLog ("Saved: " + file.getFullPathName());
                else
                    appendToLog ("Could not save script.", juce::Colours::red);
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
            const auto text = loadTextFileFixEncoding (file);

            juce::MessageManager::callAsync ([this, text, file]
            {
                if (scriptEditor)
                    scriptEditor->loadContent (text);
                applyScriptMetadata();
                appendToLog ("Loaded: " + file.getFullPathName());
            });
        });
}

void AudioScripterAudioProcessorEditor::appendToLog (const juce::String& message, juce::Colour colour)
{
    const auto ts = juce::Time::getCurrentTime().formatted ("%H:%M:%S");
    const auto existing = outputPanel.getText();
    outputPanel.setColour (juce::TextEditor::textColourId, colour);
    outputPanel.setText (existing.isEmpty() ? "[" + ts + "] " + message
                                            : existing + "\n[" + ts + "] " + message);
    outputPanel.moveCaretToEnd();
}

void AudioScripterAudioProcessorEditor::showAboutBox()
{
    struct AboutContent final : public juce::Component
    {
        AboutContent()
        {
            title.setText ("audio_scripter 0.0.8", juce::dontSendNotification);
            title.setFont (juce::FontOptions (17.0f, juce::Font::bold));
            title.setJustificationType (juce::Justification::centred);
            title.setColour (juce::Label::textColourId, juce::Colour (0xff4ec9b0));
            addAndMakeVisible (title);

            tagline.setText ("Real-time scriptable audio effects plugin", juce::dontSendNotification);
            tagline.setFont (juce::FontOptions (12.5f));
            tagline.setJustificationType (juce::Justification::centred);
            tagline.setColour (juce::Label::textColourId, juce::Colour (0xff8a9aaa));
            addAndMakeVisible (tagline);

            const auto dot = juce::String::charToString (0x00B7);
            formats.setText ("VST3  " + dot + "  AU  " + dot + "  Standalone", juce::dontSendNotification);
            formats.setFont (juce::FontOptions (11.5f));
            formats.setJustificationType (juce::Justification::centred);
            formats.setColour (juce::Label::textColourId, juce::Colour (0xff8a9aaa));
            addAndMakeVisible (formats);

            link.setColour (juce::HyperlinkButton::textColourId, juce::Colour (0xff4ec9b0));
            link.setFont (juce::FontOptions (12.0f), false, juce::Justification::centred);
            addAndMakeVisible (link);

            const auto copy = juce::String::charToString (0x00A9);
            copyright.setText ("MIT License  " + copy + "  2026 krahd", juce::dontSendNotification);
            copyright.setFont (juce::FontOptions (11.0f));
            copyright.setJustificationType (juce::Justification::centred);
            copyright.setColour (juce::Label::textColourId, juce::Colour (0xff525e68));
            addAndMakeVisible (copyright);

            setSize (320, 148);
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colour::fromRGB (38, 56, 60));
        }

        void resized() override
        {
            auto b = getLocalBounds().reduced (16, 12);
            title.setBounds (b.removeFromTop (26));
            b.removeFromTop (2);
            tagline.setBounds (b.removeFromTop (18));
            formats.setBounds (b.removeFromTop (16));
            b.removeFromTop (6);
            link.setBounds (b.removeFromTop (22));
            b.removeFromTop (4);
            copyright.setBounds (b.removeFromTop (18));
        }

        juce::Label title;
        juce::Label tagline;
        juce::Label formats;
        juce::HyperlinkButton link {
            "krahd.github.io/audio_scripter",
            juce::URL ("https://krahd.github.io/audio_scripter/")
        };
        juce::Label copyright;
    };

    juce::DialogWindow::LaunchOptions opts;
    opts.dialogTitle                = "About audio_scripter";
    opts.content.setOwned           (new AboutContent());
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar          = false;
    opts.resizable                  = false;
    opts.dialogBackgroundColour     = juce::Colour::fromRGB (38, 56, 60);
    opts.launchAsync();
}
