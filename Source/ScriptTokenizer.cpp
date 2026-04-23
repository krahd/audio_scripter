#include "ScriptTokenizer.h"

namespace scripting
{
ScriptTokenizer::ScriptTokenizer (juce::String sourceText)
    : source (std::move (sourceText))
{
}

Token ScriptTokenizer::peek()
{
    if (! hasCachedToken)
    {
        cachedToken = next();
        hasCachedToken = true;
    }

    return cachedToken;
}

Token ScriptTokenizer::next()
{
    if (hasCachedToken)
    {
        hasCachedToken = false;
        return cachedToken;
    }

    skipWhitespace();

    if (position >= source.length())
        return { TokenType::end, {}, 0.0, line };

    const auto c = source[position];

    if (juce::CharacterFunctions::isLetter (c) || c == '_')
        return makeIdentifier();

    if (juce::CharacterFunctions::isDigit (c) || c == '.')
        return makeNumber();

    ++position;

    switch (c)
    {
        case '(' : return { TokenType::leftParen, "(", 0.0, line };
        case ')' : return { TokenType::rightParen, ")", 0.0, line };
        case ',' : return { TokenType::comma, ",", 0.0, line };
        case '=' : return { TokenType::equal, "=", 0.0, line };
        case ';' : return { TokenType::semicolon, ";", 0.0, line };
        case '+' : return { TokenType::plus, "+", 0.0, line };
        case '-' : return { TokenType::minus, "-", 0.0, line };
        case '*' : return { TokenType::star, "*", 0.0, line };
        case '/' : return { TokenType::slash, "/", 0.0, line };
        default: break;
    }

    return { TokenType::invalid, juce::String::charToString (c), 0.0, line };
}

void ScriptTokenizer::skipWhitespace()
{
    while (position < source.length())
    {
        const auto c = source[position];

        if (c == '\n')
        {
            ++line;
            ++position;
            continue;
        }

        if (juce::CharacterFunctions::isWhitespace (c))
        {
            ++position;
            continue;
        }

        if (c == '#')
        {
            while (position < source.length() && source[position] != '\n')
                ++position;
            continue;
        }

        break;
    }
}

Token ScriptTokenizer::makeIdentifier()
{
    const int start = position;

    while (position < source.length())
    {
        const auto c = source[position];
        if (! juce::CharacterFunctions::isLetterOrDigit (c) && c != '_')
            break;
        ++position;
    }

    return { TokenType::identifier, source.substring (start, position), 0.0, line };
}

Token ScriptTokenizer::makeNumber()
{
    const int start = position;
    bool sawDot = false;

    while (position < source.length())
    {
        const auto c = source[position];
        if (c == '.')
        {
            if (sawDot)
                break;
            sawDot = true;
            ++position;
            continue;
        }

        if (! juce::CharacterFunctions::isDigit (c))
            break;

        ++position;
    }

    const auto text = source.substring (start, position);
    return { TokenType::number, text, text.getDoubleValue(), line };
}
} // namespace scripting
