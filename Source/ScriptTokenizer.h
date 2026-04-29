#pragma once

#include <juce_core/juce_core.h>

namespace scripting
{
enum class TokenType
{
    // Literals and identifiers
    identifier,
    number,
    // Symbols
    leftParen,
    rightParen,
    leftBrace,
    rightBrace,
    comma,
    equal,
    equalEqual,
    semicolon,
    plus,
    minus,
    star,
    slash,
    less,
    lessEqual,
    greater,
    greaterEqual,
    notOp,
    notEqual,
    andAnd,
    orOr,
    ampersand,
    pipe,
    caret,
    shiftLeft,
    shiftRight,
    // Keywords
    kw_if,
    kw_else,
    kw_while,
    kw_for,
    kw_fn,
    kw_return,
    kw_break,
    kw_continue,
    kw_true,
    kw_false,
    // End of input
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
    Token makeKeywordOrIdentifier();

    void skipWhitespace();
    Token makeNumber();

    juce::String source;
    int position { 0 };
    int line { 1 };
    Token cachedToken;
    bool hasCachedToken { false };
};
} // namespace scripting
