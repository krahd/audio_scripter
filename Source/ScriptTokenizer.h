#pragma once

#include <juce_core/juce_core.h>

namespace scripting
{
enum class TokenType
{
    identifier,
    number,
    leftParen,
    rightParen,
    comma,
    equal,
    semicolon,
    plus,
    minus,
    star,
    slash,
    end,
    invalid
};

struct Token
{
    TokenType type { TokenType::invalid };
    juce::String text;
    double numberValue { 0.0 };
    int line { 1 };
};

class ScriptTokenizer
{
public:
    explicit ScriptTokenizer (juce::String sourceText);

    Token next();
    Token peek();

private:
    void skipWhitespace();
    Token makeIdentifier();
    Token makeNumber();

    juce::String source;
    int position { 0 };
    int line { 1 };
    Token cachedToken;
    bool hasCachedToken { false };
};
} // namespace scripting
