#include "ScriptParser.h"

#include <cmath>

namespace scripting
{
namespace
{
constexpr int kMaxStatements = 256;

bool consumeInstruction (EvalContext& ctx)
{
    ++ctx.instructionCount;
    if (ctx.instructionCount > ctx.maxInstructions)
    {
        ctx.executionAborted = true;
        return false;
    }

    return true;
}

struct NumberExpr final : Expr
{
    explicit NumberExpr (float v) : value (v) {}
    float evaluate (EvalContext&) const override { return value; }
    float value;
};

struct VariableExpr final : Expr
{
    explicit VariableExpr (juce::String n) : name (std::move (n)) {}
    float evaluate (EvalContext& ctx) const override { return ctx.getValue (name); }

    juce::String name;
};

struct BinaryExpr final : Expr
{
    enum class Op { add, sub, mul, div };

    BinaryExpr (Op o, std::unique_ptr<Expr> l, std::unique_ptr<Expr> r)
        : op (o), left (std::move (l)), right (std::move (r)) {}

    float evaluate (EvalContext& ctx) const override
    {
        const auto l = left->evaluate (ctx);
        const auto r = right->evaluate (ctx);

        switch (op)
        {
            case Op::add: return l + r;
            case Op::sub: return l - r;
            case Op::mul: return l * r;
            case Op::div: return std::abs (r) < 1.0e-9f ? 0.0f : l / r;
        }

        return 0.0f;
    }

    Op op;
    std::unique_ptr<Expr> left;
    std::unique_ptr<Expr> right;
};

struct UnaryExpr final : Expr
{
    explicit UnaryExpr (std::unique_ptr<Expr> e) : expr (std::move (e)) {}
    float evaluate (EvalContext& ctx) const override { return -expr->evaluate (ctx); }

    std::unique_ptr<Expr> expr;
};
} // namespace

void BlockStatement::execute (EvalContext& ctx) const
{
    if (! consumeInstruction (ctx))
        return;

    for (const auto& stmt : statements)
    {
        if (ctx.executionAborted || ctx.returnTriggered)
            break;
        if (stmt != nullptr)
            stmt->execute (ctx);
    }
}

void IfStatement::execute (EvalContext& ctx) const
{
    if (! consumeInstruction (ctx))
        return;

    if (condition != nullptr && condition->evaluate (ctx) != 0.0f)
    {
        if (thenBranch != nullptr)
            thenBranch->execute (ctx);
    }
    else if (elseBranch != nullptr)
    {
        elseBranch->execute (ctx);
    }
}

void WhileStatement::execute (EvalContext& ctx) const
{
    if (! consumeInstruction (ctx))
        return;

    ++ctx.loopDepth;
    if (ctx.loopDepth > ctx.maxLoopDepth)
    {
        ctx.executionAborted = true;
        --ctx.loopDepth;
        return;
    }

    while (! ctx.executionAborted && ! ctx.returnTriggered && condition != nullptr && condition->evaluate (ctx) != 0.0f)
        if (body != nullptr)
            body->execute (ctx);

    --ctx.loopDepth;
}

void ForStatement::execute (EvalContext& ctx) const
{
    if (! consumeInstruction (ctx))
        return;

    ++ctx.loopDepth;
    if (ctx.loopDepth > ctx.maxLoopDepth)
    {
        ctx.executionAborted = true;
        --ctx.loopDepth;
        return;
    }

    const auto start = startExpr != nullptr ? startExpr->evaluate (ctx) : 0.0f;
    const auto end   = endExpr   != nullptr ? endExpr->evaluate (ctx)   : 0.0f;

    for (float i = start; i < end && ! ctx.executionAborted && ! ctx.returnTriggered; i += 1.0f)
    {
        ctx.locals[varName] = i;
        if (body != nullptr)
            body->execute (ctx);
    }

    --ctx.loopDepth;
}

void FunctionDefStatement::execute (EvalContext&) const {}
void ReturnStatement::execute (EvalContext& ctx) const
{
    if (! consumeInstruction (ctx))
        return;

    ctx.returnValue = value != nullptr ? value->evaluate (ctx) : 0.0f;
    ctx.returnTriggered = true;
}

void AssignmentStatement::execute (EvalContext& ctx) const
{
    if (! consumeInstruction (ctx))
        return;

    if (expression != nullptr)
        ctx.setValue (variableName, expression->evaluate (ctx));
}

void ExpressionStatement::execute (EvalContext& ctx) const
{
    if (! consumeInstruction (ctx))
        return;

    if (expression != nullptr)
        expression->evaluate (ctx);
}

float FunctionCallExpr::evaluate (EvalContext& ctx) const
{
    std::vector<float> values;
    values.reserve (arguments.size());
    for (const auto& arg : arguments)
        values.push_back (arg != nullptr ? arg->evaluate (ctx) : 0.0f);

    if (ctx.functionRegistry == nullptr)
        return 0.0f;

    const auto fnLower = functionName.toLowerCase();

    if (const auto it = ctx.functionRegistry->builtins.find (fnLower); it != ctx.functionRegistry->builtins.end())
        return it->second (ctx, values);

    if (const auto uit = ctx.functionRegistry->user.find (functionName); uit != ctx.functionRegistry->user.end() && uit->second != nullptr)
    {
        if (ctx.recursionDepth >= ctx.maxRecursionDepth)
        {
            ctx.executionAborted = true;
            return 0.0f;
        }

        auto* fn = uit->second;
        if (fn->body == nullptr)
            return 0.0f;
        if (values.size() != fn->parameters.size())
            return 0.0f;

        auto savedLocals = ctx.locals;
        const auto previousReturnTriggered = ctx.returnTriggered;
        const auto previousReturnValue = ctx.returnValue;

        ctx.returnTriggered = false;
        ctx.returnValue = 0.0f;
        ++ctx.recursionDepth;

        for (size_t i = 0; i < fn->parameters.size(); ++i)
            ctx.locals[fn->parameters[i]] = i < values.size() ? values[i] : 0.0f;

        fn->body->execute (ctx);

        const auto callValue = ctx.returnValue;
        ctx.locals = std::move (savedLocals);
        ctx.returnTriggered = previousReturnTriggered;
        ctx.returnValue = previousReturnValue;
        --ctx.recursionDepth;
        return callValue;
    }

    return 0.0f;
}

float EvalContext::getValue (const juce::String& name) const
{
    if (name == "inL")  return inL;
    if (name == "inR")  return inR;
    if (name == "outL") return outL;
    if (name == "outR") return outR;
    if (name == "sr")   return sr;
    if (name == "t")    return t;

    if (macros != nullptr)
    {
        if (name == "p1") return (*macros)[0];
        if (name == "p2") return (*macros)[1];
        if (name == "p3") return (*macros)[2];
        if (name == "p4") return (*macros)[3];
        if (name == "p5") return (*macros)[4];
        if (name == "p6") return (*macros)[5];
        if (name == "p7") return (*macros)[6];
        if (name == "p8") return (*macros)[7];
    }

    if (const auto it = locals.find (name); it != locals.end())
        return it->second;

    if (persistentState != nullptr)
        if (const auto it = persistentState->find (name); it != persistentState->end())
            return it->second;

    return 0.0f;
}

void EvalContext::setValue (const juce::String& name, float value)
{
    if (name == "outL") { outL = value; return; }
    if (name == "outR") { outR = value; return; }

    if (name.startsWith ("state_"))
    {
        if (persistentState != nullptr)
            (*persistentState)[name] = value;
        return;
    }

    locals[name] = value;
}

ParseResult ScriptParser::parse (const juce::String& source)
{
    ParseResult result;
    tokenizer = std::make_unique<ScriptTokenizer> (source);
    errors = &result.errors;

    while (peek().type != TokenType::end)
    {
        auto stmt = parseStatement();
        if (stmt == nullptr)
            break;

        result.program.statements.push_back (std::move (stmt));

        if ((int) result.program.statements.size() > kMaxStatements)
        {
            errors->add ("Program too large: maximum " + juce::String (kMaxStatements) + " statements.");
            break;
        }
    }

    if (result.program.statements.empty())
        errors->add ("Script is empty. Add at least one statement.");

    return result;
}

std::unique_ptr<Statement> ScriptParser::parseStatement()
{
    const auto token = peek();

    switch (token.type)
    {
        case TokenType::kw_if:     return parseIf();
        case TokenType::kw_while:  return parseWhile();
        case TokenType::kw_for:    return parseFor();
        case TokenType::kw_fn:     return parseFunctionDef();
        case TokenType::kw_return: return parseReturn();
        case TokenType::leftBrace: return parseBlock();
        case TokenType::identifier:return parseAssignmentOrExpr();
        default:
            errors->add ("Line " + juce::String (token.line)
                         + ": unexpected token at statement start: '" + token.text + "'.");
            consume();
            return nullptr;
    }
}

std::unique_ptr<BlockStatement> ScriptParser::parseBlock()
{
    if (! expect (TokenType::leftBrace, "expected '{' to start block"))
        return nullptr;

    auto block = std::make_unique<BlockStatement>();

    while (peek().type != TokenType::rightBrace && peek().type != TokenType::end)
    {
        auto stmt = parseStatement();
        if (stmt == nullptr)
            return nullptr;
        block->statements.push_back (std::move (stmt));
    }

    if (! expect (TokenType::rightBrace, "expected '}' to end block"))
        return nullptr;

    return block;
}

std::unique_ptr<Statement> ScriptParser::parseIf()
{
    consume();

    if (! expect (TokenType::leftParen, "expected '(' after 'if'"))
        return nullptr;

    auto cond = parseExpression();
    if (cond == nullptr)
        return nullptr;

    if (! expect (TokenType::rightParen, "expected ')' after condition"))
        return nullptr;

    auto thenBranch = parseStatement();
    std::unique_ptr<Statement> elseBranch;

    if (peek().type == TokenType::kw_else)
    {
        consume();
        elseBranch = parseStatement();
    }

    auto node = std::make_unique<IfStatement>();
    node->condition = std::move (cond);
    node->thenBranch = std::move (thenBranch);
    node->elseBranch = std::move (elseBranch);
    return node;
}

std::unique_ptr<Statement> ScriptParser::parseWhile()
{
    consume();

    if (! expect (TokenType::leftParen, "expected '(' after 'while'"))
        return nullptr;

    auto cond = parseExpression();
    if (cond == nullptr)
        return nullptr;

    if (! expect (TokenType::rightParen, "expected ')' after condition"))
        return nullptr;

    auto body = parseStatement();

    auto node = std::make_unique<WhileStatement>();
    node->condition = std::move (cond);
    node->body = std::move (body);
    return node;
}

std::unique_ptr<Statement> ScriptParser::parseFor()
{
    consume();

    if (! expect (TokenType::leftParen, "expected '(' after 'for'"))
        return nullptr;

    auto id = consume();
    if (id.type != TokenType::identifier)
    {
        errors->add ("Line " + juce::String (id.line) + ": expected identifier in for loop.");
        return nullptr;
    }

    if (! expect (TokenType::equal, "expected '=' after for variable"))
        return nullptr;

    auto start = parseExpression();
    if (start == nullptr)
        return nullptr;

    if (! expect (TokenType::semicolon, "expected ';' after for start expr"))
        return nullptr;

    auto end = parseExpression();
    if (end == nullptr)
        return nullptr;

    if (! expect (TokenType::rightParen, "expected ')' after for end expr"))
        return nullptr;

    auto body = parseStatement();

    auto node = std::make_unique<ForStatement>();
    node->varName = id.text;
    node->startExpr = std::move (start);
    node->endExpr = std::move (end);
    node->body = std::move (body);
    return node;
}

std::unique_ptr<Statement> ScriptParser::parseFunctionDef()
{
    consume();

    auto id = consume();
    if (id.type != TokenType::identifier)
    {
        errors->add ("Line " + juce::String (id.line) + ": expected function name after 'fn'.");
        return nullptr;
    }

    if (! expect (TokenType::leftParen, "expected '(' after function name"))
        return nullptr;

    std::vector<juce::String> params;

    if (peek().type != TokenType::rightParen)
    {
        while (true)
        {
            auto param = consume();
            if (param.type != TokenType::identifier)
            {
                errors->add ("Line " + juce::String (param.line) + ": expected parameter name.");
                return nullptr;
            }

            params.push_back (param.text);
            if (peek().type == TokenType::comma)
            {
                consume();
                continue;
            }

            break;
        }
    }

    if (! expect (TokenType::rightParen, "expected ')' after parameter list"))
        return nullptr;

    auto body = parseBlock();

    auto node = std::make_unique<FunctionDefStatement>();
    node->name = id.text;
    node->parameters = std::move (params);
    node->body = std::move (body);
    return node;
}

std::unique_ptr<Statement> ScriptParser::parseReturn()
{
    consume();
    auto expr = parseExpression();

    if (! expect (TokenType::semicolon, "expected ';' after return value"))
        return nullptr;

    auto node = std::make_unique<ReturnStatement>();
    node->value = std::move (expr);
    return node;
}

std::unique_ptr<Statement> ScriptParser::parseAssignmentOrExpr()
{
    auto id = consume();

    if (peek().type == TokenType::equal)
    {
        consume();
        auto expr = parseExpression();
        if (! expect (TokenType::semicolon, "expected ';' after assignment"))
            return nullptr;

        auto node = std::make_unique<AssignmentStatement>();
        node->variableName = id.text;
        node->expression = std::move (expr);
        return node;
    }

    if (peek().type == TokenType::leftParen)
    {
        consume();
        std::vector<std::unique_ptr<Expr>> args;

        if (peek().type != TokenType::rightParen)
        {
            while (true)
            {
                auto arg = parseExpression();
                if (arg == nullptr)
                    return nullptr;

                args.push_back (std::move (arg));

                if (peek().type == TokenType::comma)
                {
                    consume();
                    continue;
                }

                break;
            }
        }

        if (! expect (TokenType::rightParen, "expected ')' after function arguments."))
            return nullptr;

        if (! expect (TokenType::semicolon, "expected ';' after expression"))
            return nullptr;

        auto call = std::make_unique<FunctionCallExpr>();
        call->functionName = id.text;
        call->arguments = std::move (args);

        auto exprStmt = std::make_unique<ExpressionStatement>();
        exprStmt->expression = std::move (call);
        return exprStmt;
    }

    if (! expect (TokenType::semicolon, "expected ';' after expression"))
        return nullptr;

    auto exprStmt = std::make_unique<ExpressionStatement>();
    exprStmt->expression = std::make_unique<VariableExpr> (id.text);
    return exprStmt;
}

std::unique_ptr<Expr> ScriptParser::parseExpression()
{
    return parseAddSub();
}

std::unique_ptr<Expr> ScriptParser::parseAddSub()
{
    auto left = parseMulDiv();
    if (left == nullptr)
        return {};

    while (true)
    {
        const auto token = peek();
        if (token.type != TokenType::plus && token.type != TokenType::minus)
            break;

        consume();

        auto right = parseMulDiv();
        if (right == nullptr)
            return {};

        left = std::make_unique<BinaryExpr> (token.type == TokenType::plus ? BinaryExpr::Op::add : BinaryExpr::Op::sub,
                                             std::move (left), std::move (right));
    }

    return left;
}

std::unique_ptr<Expr> ScriptParser::parseMulDiv()
{
    auto left = parseUnary();
    if (left == nullptr)
        return {};

    while (true)
    {
        const auto token = peek();
        if (token.type != TokenType::star && token.type != TokenType::slash)
            break;

        consume();

        auto right = parseUnary();
        if (right == nullptr)
            return {};

        left = std::make_unique<BinaryExpr> (token.type == TokenType::star ? BinaryExpr::Op::mul : BinaryExpr::Op::div,
                                             std::move (left), std::move (right));
    }

    return left;
}

std::unique_ptr<Expr> ScriptParser::parseUnary()
{
    if (peek().type == TokenType::minus)
    {
        consume();
        auto expr = parseUnary();
        if (expr == nullptr)
            return {};

        return std::make_unique<UnaryExpr> (std::move (expr));
    }

    return parsePrimary();
}

std::unique_ptr<Expr> ScriptParser::parsePrimary()
{
    const auto token = consume();

    if (token.type == TokenType::number)
    {
        if (token.text.containsChar ('.'))
            return std::make_unique<TypedLiteralExpr> ((float) token.numberValue, ValueType::Float);

        return std::make_unique<TypedLiteralExpr> ((float) token.numberValue, ValueType::Int);
    }

    if (token.type == TokenType::kw_true)
        return std::make_unique<BoolLiteralExpr> (true);

    if (token.type == TokenType::kw_false)
        return std::make_unique<BoolLiteralExpr> (false);

    if (token.type == TokenType::identifier)
    {
        if (peek().type != TokenType::leftParen)
            return std::make_unique<VariableExpr> (token.text);

        consume();

        std::vector<std::unique_ptr<Expr>> args;
        if (peek().type != TokenType::rightParen)
        {
            while (true)
            {
                auto arg = parseExpression();
                if (arg == nullptr)
                    return {};

                args.push_back (std::move (arg));
                if (peek().type == TokenType::comma)
                {
                    consume();
                    continue;
                }

                break;
            }
        }

        if (! expect (TokenType::rightParen, "expected ')' after function arguments."))
            return {};

        auto call = std::make_unique<FunctionCallExpr>();
        call->functionName = token.text;
        call->arguments = std::move (args);
        return call;
    }

    if (token.type == TokenType::leftParen)
    {
        auto expr = parseExpression();
        if (expr == nullptr)
            return {};

        if (! expect (TokenType::rightParen, "expected ')' after expression."))
            return {};

        return expr;
    }

    errors->add ("Line " + juce::String (token.line) + ": unexpected token '" + token.text + "'.");
    return {};
}

bool ScriptParser::expect (TokenType type, const juce::String& message)
{
    const auto token = consume();
    if (token.type == type)
        return true;

    errors->add ("Line " + juce::String (token.line) + ": " + message + " (got '" + token.text + "')");
    return false;
}

Token ScriptParser::consume()
{
    return tokenizer->next();
}

Token ScriptParser::peek()
{
    return tokenizer->peek();
}
} // namespace scripting
