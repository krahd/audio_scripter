#pragma once

#include <JuceHeader.h>

class ScriptCodeTokeniser final : public juce::CodeTokeniser
{
public:
    ScriptCodeTokeniser() = default;

    int readNextToken (juce::CodeDocument::Iterator& source) override;
    juce::CodeEditorComponent::ColourScheme getDefaultColourScheme() override;

private:
    static bool isIdentifierStart (juce_wchar c);
    static bool isIdentifierBody (juce_wchar c);
    static bool isDigit (juce_wchar c);
    static bool isSpecialVariable (const juce::String& ident);
    static bool isBuiltInFunction (const juce::String& ident);
};
