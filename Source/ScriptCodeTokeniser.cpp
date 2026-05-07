#include "ScriptCodeTokeniser.h"

namespace
{
    enum TokenType
    {
        error = 0,
        comment,
        keyword,
        builtIn,
        identifier,
        number,
        stringLiteral,
        operatorToken,
        bracket,
        punctuation,
        preprocessor
    };

    bool startsWithMacroName (const juce::String& ident)
    {
        if (ident.length() != 2 || ident[0] != 'p')
            return false;

        return ident[1] >= '1' && ident[1] <= '8';
    }

    bool isReadOnlyInput (const juce::String& ident)
    {
        static const juce::StringArray readOnlyNames { "inL", "inR", "sr", "t" };
        return readOnlyNames.contains (ident);
    }
}

bool ScriptCodeTokeniser::isIdentifierStart (juce_wchar c)
{
    return juce::CharacterFunctions::isLetter (c) || c == '_';
}

bool ScriptCodeTokeniser::isIdentifierBody (juce_wchar c)
{
    return isIdentifierStart (c) || isDigit (c);
}

bool ScriptCodeTokeniser::isDigit (juce_wchar c)
{
    return c >= '0' && c <= '9';
}

bool ScriptCodeTokeniser::isSpecialVariable (const juce::String& ident)
{
    return startsWithMacroName (ident) || isReadOnlyInput (ident)
        || ident == "outL" || ident == "outR"
        || ident.startsWith ("state_");
}

bool ScriptCodeTokeniser::isBuiltInFunction (const juce::String& ident)
{
    static const juce::StringArray names {
        "sin", "cos", "tan", "abs", "sqrt", "exp", "log", "tanh",
        "pow", "min", "max", "clamp", "clip", "mix", "wrap", "fold",
        "crush", "smoothstep", "noise", "gt", "lt", "ge", "le", "select",
        "pulse", "lpf1", "hp1", "bp1", "svf", "slew", "env", "delay", "sat"
    };

    return names.contains (ident);
}

int ScriptCodeTokeniser::readNextToken (juce::CodeDocument::Iterator& source)
{
    source.skipWhitespace();

    const juce_wchar c = source.peekNextChar();
    if (c == 0)
        return error;

    if (c == '#')
    {
        source.skip();
        const bool metadataDirective = source.peekNextChar() == '@';

        while (source.peekNextChar() != 0 && source.peekNextChar() != '\n' && source.peekNextChar() != '\r')
            source.skip();

        return metadataDirective ? preprocessor : comment;
    }

    if (c == '/')
    {
        auto lookahead = source;
        lookahead.skip();

        if (lookahead.peekNextChar() == '/')
        {
            source.skip();
            source.skip();
            while (source.peekNextChar() != 0 && source.peekNextChar() != '\n' && source.peekNextChar() != '\r')
                source.skip();
            return comment;
        }

        source.skip();
        return operatorToken;
    }

    if (c == '"' || c == '\'')
    {
        const juce_wchar quote = c;
        source.skip();

        while (source.peekNextChar() != 0)
        {
            const auto next = source.peekNextChar();
            source.skip();

            if (next == quote)
                return stringLiteral;

            if (next == '\\' && source.peekNextChar() != 0)
                source.skip();

            if (next == '\n' || next == '\r')
                return error;
        }

        return error;
    }

    auto dotLookahead = source;
    dotLookahead.skip();
    if (isDigit (c) || (c == '.' && isDigit (dotLookahead.peekNextChar())))
    {
        bool hasDot = false;

        if (c == '.')
        {
            hasDot = true;
            source.skip();
        }

        while (isDigit (source.peekNextChar()))
            source.skip();

        if (! hasDot && source.peekNextChar() == '.')
        {
            hasDot = true;
            source.skip();
            while (isDigit (source.peekNextChar()))
                source.skip();
        }

        if (source.peekNextChar() == 'e' || source.peekNextChar() == 'E')
        {
            auto expLookahead = source;
            expLookahead.skip();
            if (expLookahead.peekNextChar() == '+' || expLookahead.peekNextChar() == '-')
                expLookahead.skip();

            if (isDigit (expLookahead.peekNextChar()))
            {
                source.skip();
                if (source.peekNextChar() == '+' || source.peekNextChar() == '-')
                    source.skip();

                while (isDigit (source.peekNextChar()))
                    source.skip();
            }
        }

        return number;
    }

    if (isIdentifierStart (c))
    {
        juce::String ident;
        while (isIdentifierBody (source.peekNextChar()))
            ident << source.nextChar();

        static const juce::StringArray keywords { "if", "else", "while", "for", "fn", "return", "break", "continue", "true", "false" };
        if (keywords.contains (ident))
            return keyword;

        if (isSpecialVariable (ident) || isBuiltInFunction (ident))
            return builtIn;

        return identifier;
    }

    if (c == '+' || c == '-' || c == '*' || c == '^')
    {
        source.skip();
        return operatorToken;
    }

    if (c == '=' || c == '!')
    {
        source.skip();
        if (source.peekNextChar() == '=')
            source.skip();
        return operatorToken;
    }

    if (c == '<')
    {
        source.skip();
        const auto n = source.peekNextChar();
        if (n == '<' || n == '=')
            source.skip();
        return operatorToken;
    }

    if (c == '>')
    {
        source.skip();
        const auto n = source.peekNextChar();
        if (n == '>' || n == '=')
            source.skip();
        return operatorToken;
    }

    if (c == '&')
    {
        source.skip();
        if (source.peekNextChar() == '&')
            source.skip();
        return operatorToken;
    }

    if (c == '|')
    {
        source.skip();
        if (source.peekNextChar() == '|')
            source.skip();
        return operatorToken;
    }

    if (c == '(' || c == ')' || c == '{' || c == '}')
    {
        source.skip();
        return bracket;
    }

    if (c == ',' || c == ';')
    {
        source.skip();
        return punctuation;
    }

    source.skip();
    return error;
}

juce::CodeEditorComponent::ColourScheme ScriptCodeTokeniser::getDefaultColourScheme()
{
    juce::CodeEditorComponent::ColourScheme scheme;
    scheme.set ("Error", juce::Colour (0xffff6b6b));
    scheme.set ("Comment", juce::Colour (0xff6a9955));
    scheme.set ("Keyword", juce::Colour (0xff569cd6));
    scheme.set ("Builtin", juce::Colour (0xff4ec9b0));
    scheme.set ("Identifier", juce::Colour (0xffd4d4d4));
    scheme.set ("Number", juce::Colour (0xffb5cea8));
    scheme.set ("String", juce::Colour (0xffce9178));
    scheme.set ("Operator", juce::Colour (0xffd4d4d4));
    scheme.set ("Bracket", juce::Colour (0xffffd700));
    scheme.set ("Punctuation", juce::Colour (0xffd4d4d4));
    scheme.set ("Preprocessor", juce::Colour (0xff9cdcfe));
    return scheme;
}
