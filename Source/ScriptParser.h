#pragma once

#include "Constants.h"
#include "ScriptTokenizer.h"
#include <array>
#include <map>

namespace scripting
{
struct EvalContext {
    int loopDepth = 0;
    int maxLoopDepth = 1024;
    int recursionDepth = 0;
    int maxRecursionDepth = 64;
    bool executionAborted = false;
    // ...existing fields (macros, locals, etc.)
};


// Forward declarations for new AST nodes
struct Expr;
struct Statement;
struct BlockStatement;
struct IfStatement;
struct WhileStatement;
struct ForStatement;
struct FunctionDefStatement;
struct ReturnStatement;


enum class ValueType {
    Float,
    Int,
    Bool
};

struct Expr {
    virtual ~Expr() = default;
    virtual float evaluate(EvalContext&) const = 0;
    virtual ValueType getType() const { return ValueType::Float; }
};

struct TypedLiteralExpr : Expr {
    float value;
    ValueType type;
    TypedLiteralExpr(float v, ValueType t) : value(v), type(t) {}
    float evaluate(EvalContext&) const override { return value; }
    ValueType getType() const override { return type; }
};

struct BoolLiteralExpr : Expr {
    bool value;
    BoolLiteralExpr(bool v) : value(v) {}
    float evaluate(EvalContext&) const override { return value ? 1.0f : 0.0f; }
    ValueType getType() const override { return ValueType::Bool; }
};

// Function call expression (can call built-in or user-defined)
struct FunctionCallExpr : Expr {
    juce::String functionName;
    std::vector<std::unique_ptr<Expr>> arguments;
    float evaluate(EvalContext&) const override;
};

// Statement base
struct Statement {
    virtual ~Statement() = default;
    virtual void execute(EvalContext&) const = 0;
    int line { 1 };
};

// Block of statements
struct BlockStatement : Statement {
    std::vector<std::unique_ptr<Statement>> statements;
    void execute(EvalContext&) const override;
};

// If statement
struct IfStatement : Statement {
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Statement> thenBranch;
    std::unique_ptr<Statement> elseBranch;
    void execute(EvalContext&) const override;
};

// While statement
struct WhileStatement : Statement {
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Statement> body;
    void execute(EvalContext& ctx) const override;
};

// For statement
struct ForStatement : Statement {
    juce::String varName;
    std::unique_ptr<Expr> startExpr;
    std::unique_ptr<Expr> endExpr;
    std::unique_ptr<Statement> body;
    void execute(EvalContext& ctx) const override;
};

// Function definition
struct FunctionDefStatement : Statement {
    juce::String name;
    std::vector<juce::String> parameters;
    std::unique_ptr<BlockStatement> body;
    void execute(EvalContext&) const override;
};

// Return statement
struct ReturnStatement : Statement {
    std::unique_ptr<Expr> value;
    void execute(EvalContext&) const override;
};

// Assignment statement
struct AssignmentStatement : Statement {
    juce::String variableName;
    std::unique_ptr<Expr> expression;
    void execute(EvalContext&) const override;
};

// Expression statement
struct ExpressionStatement : Statement {
    std::unique_ptr<Expr> expression;
    void execute(EvalContext&) const override;
};

// Extensible function registry
using BuiltinFunction = std::function<float(EvalContext&, const std::vector<float>&)>;
struct FunctionRegistry {
    std::map<juce::String, BuiltinFunction> builtins;
    std::map<juce::String, FunctionDefStatement*> user;
};

// Program root
struct Program {
    std::vector<std::unique_ptr<Statement>> statements;
    FunctionRegistry functionRegistry;
};

struct ParseResult
{
    Program program;
    juce::StringArray errors;
};

class ScriptParser {
public:
    ParseResult parse(const juce::String& source);

private:
    std::unique_ptr<Statement> parseStatement();
    std::unique_ptr<BlockStatement> parseBlock();
    std::unique_ptr<Statement> parseIf();
    std::unique_ptr<Statement> parseWhile();
    std::unique_ptr<Statement> parseFor();
    std::unique_ptr<Statement> parseFunctionDef();
    std::unique_ptr<Statement> parseReturn();
    std::unique_ptr<Statement> parseAssignmentOrExpr();
    std::unique_ptr<Expr> parseExpression();
    std::unique_ptr<Expr> parseAddSub();
    std::unique_ptr<Expr> parseMulDiv();
    std::unique_ptr<Expr> parseUnary();
    std::unique_ptr<Expr> parsePrimary();

    bool expect(TokenType type, const juce::String& message);
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

    const std::array<float, kNumMacros>* macros { nullptr };
    std::map<juce::String, float> locals;
    std::map<juce::String, float>* persistentState { nullptr };

    float getValue (const juce::String&) const;
    void setValue (const juce::String&, float);
};
} // namespace scripting
