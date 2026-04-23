#pragma once

#include "ScriptTokenizer.h"
#include <array>
#include <map>

namespace scripting
{
struct EvalContext;

struct Expr
{
    virtual ~Expr() = default;
    virtual float evaluate (EvalContext&) const = 0;
};

struct Statement
{
    juce::String variableName;
    std::unique_ptr<Expr> expression;
    int line { 1 };
};

struct Program
{
    std::vector<Statement> statements;
};

struct ParseResult
{
    Program program;
    juce::StringArray errors;
};

class ScriptParser
{
public:
    ParseResult parse (const juce::String& source);

private:
    std::unique_ptr<Expr> parseExpression();
    std::unique_ptr<Expr> parseAddSub();
    std::unique_ptr<Expr> parseMulDiv();
    std::unique_ptr<Expr> parseUnary();
    std::unique_ptr<Expr> parsePrimary();

    bool expect (TokenType type, const juce::String& message);
    Token consume();
    Token peek();

    std::unique_ptr<ScriptTokenizer> tokenizer;
    juce::StringArray* errors { nullptr };
};

struct EvalContext
{
    float inL { 0.0f };
    float inR { 0.0f };
    float outL { 0.0f };
    float outR { 0.0f };
    float sr { 44100.0f };
    float t { 0.0f };

    const std::array<float, 8>* macros { nullptr };
    std::map<juce::String, float> locals;
    std::map<juce::String, float>* persistentState { nullptr };

    float getValue (const juce::String&) const;
    void setValue (const juce::String&, float);
};
} // namespace scripting
