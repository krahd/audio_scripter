#pragma once

#include "Constants.h"
#include "ScriptTokenizer.h"
#include <array>
#include <functional>
#include <map>
#include <unordered_map>
#include <vector>

namespace scripting
{
struct JuceStringHash
{
    size_t operator() (const juce::String& s) const noexcept { return (size_t) s.hashCode(); }
};

template <typename V>
using StringMap = std::unordered_map<juce::String, V, JuceStringHash>;

struct Expr;
struct Statement;
struct FunctionDefStatement;
struct FunctionRegistry;

enum class VarKind
{
    Input,
    Output,
    SampleRate,
    Time,
    Macro,
    Local,
    State,
    Unknown
};

struct VarRef
{
    VarKind kind { VarKind::Unknown };
    int slot { -1 };
    juce::String name;
};

struct EvalContext
{
    int loopDepth { 0 };
    int maxLoopDepth { 1024 };
    int recursionDepth { 0 };
    int maxRecursionDepth { 64 };
    bool executionAborted { false };
    bool returnTriggered { false };
    bool breakTriggered { false };
    bool continueTriggered { false };
    float returnValue { 0.0f };
    int instructionCount { 0 };
    int maxInstructions { 4096 };

    float inL { 0.0f };
    float inR { 0.0f };
    float outL { 0.0f };
    float outR { 0.0f };
    float sr { 44100.0f };
    float t { 0.0f };

    std::array<float, kNumMacros>* macros { nullptr };
    std::vector<float> locals;
    std::vector<float>* stateSlots { nullptr };
    StringMap<float>* persistentState { nullptr };
    const FunctionRegistry* functionRegistry { nullptr };
    std::unordered_map<int, std::vector<float>>* delayBuffers { nullptr };
    std::unordered_map<int, int>* delayWritePositions { nullptr };
    std::vector<std::vector<float>> callArgFrames;
    int callArgDepth { 0 };

    // Set by FunctionCallExpr::evaluate before calling a stateful builtin whose
    // lane arg was a literal at parse time; -1 means "use persistentState map".
    int builtinSlotBase { -1 };

    float getValue (const VarRef&) const;
    void setValue (const VarRef&, float);
};

enum class ValueType
{
    Float,
    Int,
    Bool
};

struct Expr
{
    virtual ~Expr() = default;
    virtual float evaluate (EvalContext&) const = 0;
    virtual ValueType getType() const { return ValueType::Float; }
};

struct TypedLiteralExpr : Expr
{
    TypedLiteralExpr (float v, ValueType t) : value (v), type (t) {}
    float evaluate (EvalContext&) const override { return value; }
    ValueType getType() const override { return type; }

    float value;
    ValueType type;
};

struct BoolLiteralExpr : Expr
{
    explicit BoolLiteralExpr (bool v) : value (v) {}
    float evaluate (EvalContext&) const override { return value ? 1.0f : 0.0f; }
    ValueType getType() const override { return ValueType::Bool; }

    bool value;
};

struct FunctionCallExpr : Expr
{
    juce::String functionName;
    juce::String functionNameLower;
    std::vector<std::unique_ptr<Expr>> arguments;
    // >= 0 when the lane argument is a compile-time literal; index into
    // EvalContext::stateSlots for this builtin's private state.
    int preResolvedStateSlotBase { -1 };

    float evaluate (EvalContext&) const override;
};

struct Statement
{
    virtual ~Statement() = default;
    virtual void execute (EvalContext&) const = 0;
    int line { 1 };
};

struct BlockStatement : Statement
{
    std::vector<std::unique_ptr<Statement>> statements;
    void execute (EvalContext&) const override;
};

struct IfStatement : Statement
{
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Statement> thenBranch;
    std::unique_ptr<Statement> elseBranch;
    void execute (EvalContext&) const override;
};

struct WhileStatement : Statement
{
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Statement> body;
    void execute (EvalContext&) const override;
};

struct ForStatement : Statement
{
    VarRef loopVar;
    std::unique_ptr<Expr> startExpr;
    std::unique_ptr<Expr> conditionExpr;
    std::unique_ptr<Expr> stepExpr;
    bool isLegacyRangeLoop { false };
    std::unique_ptr<Statement> body;
    void execute (EvalContext&) const override;
};

struct FunctionDefStatement : Statement
{
    juce::String name;
    std::vector<juce::String> parameters;
    std::vector<int> parameterSlots;
    std::unique_ptr<BlockStatement> body;
    void execute (EvalContext&) const override;
};

struct ReturnStatement : Statement
{
    std::unique_ptr<Expr> value;
    void execute (EvalContext&) const override;
};

struct BreakStatement : Statement
{
    void execute (EvalContext&) const override;
};

struct ContinueStatement : Statement
{
    void execute (EvalContext&) const override;
};

struct AssignmentStatement : Statement
{
    VarRef variable;
    std::unique_ptr<Expr> expression;
    void execute (EvalContext&) const override;
};

struct ExpressionStatement : Statement
{
    std::unique_ptr<Expr> expression;
    void execute (EvalContext&) const override;
};

using BuiltinFunction = std::function<float (EvalContext&, const std::vector<float>&)>;

struct FunctionRegistry
{
    StringMap<BuiltinFunction> builtins;
    StringMap<FunctionDefStatement*> user;
};

struct Program
{
    std::vector<std::unique_ptr<Statement>> statements;
    FunctionRegistry functionRegistry;
    int localSlotCount { 0 };
    std::vector<juce::String> stateSlotNames;
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
    std::unique_ptr<Statement> parseStatement();
    std::unique_ptr<BlockStatement> parseBlock();
    std::unique_ptr<Statement> parseIf();
    std::unique_ptr<Statement> parseWhile();
    std::unique_ptr<Statement> parseFor();
    std::unique_ptr<Statement> parseFunctionDef();
    std::unique_ptr<Statement> parseReturn();
    std::unique_ptr<Statement> parseBreak();
    std::unique_ptr<Statement> parseContinue();
    std::unique_ptr<Statement> parseAssignmentOrExpr();
    std::unique_ptr<Expr> parseExpression();
    std::unique_ptr<Expr> parseLogicalOr();
    std::unique_ptr<Expr> parseLogicalAnd();
    std::unique_ptr<Expr> parseBitwiseOr();
    std::unique_ptr<Expr> parseBitwiseXor();
    std::unique_ptr<Expr> parseBitwiseAnd();
    std::unique_ptr<Expr> parseEquality();
    std::unique_ptr<Expr> parseComparison();
    std::unique_ptr<Expr> parseShift();
    std::unique_ptr<Expr> parseAddSub();
    std::unique_ptr<Expr> parseMulDiv();
    std::unique_ptr<Expr> parseUnary();
    std::unique_ptr<Expr> parsePrimary();

    bool expect (TokenType type, const juce::String& message);
    Token consume();
    Token peek();

    VarRef resolveVariableRef (const juce::String& name);
    int getOrCreateLocalSlot (const juce::String& name);
    int getOrCreateStateSlot (const juce::String& name);
    int getOrCreateBuiltinStateSlotBase (const juce::String& key, int numSlots);
    void tryResolveBuiltinStateSlots (FunctionCallExpr& call);

    std::unique_ptr<ScriptTokenizer> tokenizer;
    juce::StringArray* errors { nullptr };
    StringMap<int> localSlots;
    StringMap<int> stateSlots;
    StringMap<int> builtinStateSlots;
    std::vector<juce::String> stateSlotNames;
};
} // namespace scripting
