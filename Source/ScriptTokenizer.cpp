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
        return makeKeywordOrIdentifier();

    if (juce::CharacterFunctions::isDigit (c) || c == '.')
        return makeNumber();

    ++position;

    switch (c)
    {
        case '(' : return { TokenType::leftParen,  "(", 0.0, line };
        case ')' : return { TokenType::rightParen, ")", 0.0, line };
        case '{' : return { TokenType::leftBrace,  "{", 0.0, line };
        case '}' : return { TokenType::rightBrace, "}", 0.0, line };
        case ',' : return { TokenType::comma,      ",", 0.0, line };
        case ';' : return { TokenType::semicolon,  ";", 0.0, line };
        case '+' : return { TokenType::plus,       "+", 0.0, line };
        case '-' : return { TokenType::minus,      "-", 0.0, line };
        case '*' : return { TokenType::star,       "*", 0.0, line };
        case '/' : return { TokenType::slash,      "/", 0.0, line };
        case '^' : return { TokenType::caret,      "^", 0.0, line };
        case '=':
            if (position < source.length() && source[position] == '=')
                { ++position; return { TokenType::equalEqual, "==", 0.0, line }; }
            return { TokenType::equal, "=", 0.0, line };
        case '<':
            if (position < source.length() && source[position] == '<')
                { ++position; return { TokenType::shiftLeft, "<<", 0.0, line }; }
            if (position < source.length() && source[position] == '=')
                { ++position; return { TokenType::lessEqual, "<=", 0.0, line }; }
            return { TokenType::less, "<", 0.0, line };
        case '>':
            if (position < source.length() && source[position] == '>')
                { ++position; return { TokenType::shiftRight, ">>", 0.0, line }; }
            if (position < source.length() && source[position] == '=')
                { ++position; return { TokenType::greaterEqual, ">=", 0.0, line }; }
            return { TokenType::greater, ">", 0.0, line };
        case '!':
            if (position < source.length() && source[position] == '=')
                { ++position; return { TokenType::notEqual, "!=", 0.0, line }; }
            return { TokenType::notOp, "!", 0.0, line };
        case '&':
            if (position < source.length() && source[position] == '&')
                { ++position; return { TokenType::andAnd, "&&", 0.0, line }; }
            return { TokenType::ampersand, "&", 0.0, line };
        case '|':
            if (position < source.length() && source[position] == '|')
                { ++position; return { TokenType::orOr, "||", 0.0, line }; }
            return { TokenType::pipe, "|", 0.0, line };
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


Token ScriptTokenizer::makeKeywordOrIdentifier()
{
    const int start = position;
    while (position < source.length())
    {
        const auto c = source[position];
        if (!juce::CharacterFunctions::isLetterOrDigit(c) && c != '_')
            break;
        ++position;
    }
    const auto text = source.substring(start, position);
    // Keywords
    if (text == "if")    return { TokenType::kw_if, text, 0.0, line };
    if (text == "else")  return { TokenType::kw_else, text, 0.0, line };
    if (text == "while") return { TokenType::kw_while, text, 0.0, line };
    if (text == "for")   return { TokenType::kw_for, text, 0.0, line };
    if (text == "fn")    return { TokenType::kw_fn, text, 0.0, line };
    if (text == "return") return { TokenType::kw_return, text, 0.0, line };
    if (text == "break") return { TokenType::kw_break, text, 0.0, line };
    if (text == "continue") return { TokenType::kw_continue, text, 0.0, line };
    if (text == "true")  return { TokenType::kw_true, text, 1.0, line };
    if (text == "false") return { TokenType::kw_false, text, 0.0, line };
    return { TokenType::identifier, text, 0.0, line };
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
