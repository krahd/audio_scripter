#include <optional>
// --- AST Node Implementations for New Language ---

// BlockStatement
void BlockStatement::execute(EvalContext& ctx) const {
    for (const auto& stmt : statements) {
        stmt->execute(ctx);
    }
}

// IfStatement
void IfStatement::execute(EvalContext& ctx) const {
    if (condition->evaluate(ctx)) {
        if (thenBranch) thenBranch->execute(ctx);
    } else if (elseBranch) {
        elseBranch->execute(ctx);
    }
}

// WhileStatement
void WhileStatement::execute(EvalContext& ctx) const {
    ctx.loopDepth++;
    if (ctx.loopDepth > ctx.maxLoopDepth) { ctx.executionAborted = true; ctx.loopDepth--; return; }
    while (condition->evaluate(ctx) && !ctx.executionAborted) {
        if (body) body->execute(ctx);
    }
    ctx.loopDepth--;
}

// ForStatement
void ForStatement::execute(EvalContext& ctx) const {
    ctx.loopDepth++;
    if (ctx.loopDepth > ctx.maxLoopDepth) { ctx.executionAborted = true; ctx.loopDepth--; return; }
    float start = startExpr->evaluate(ctx);
    float end = endExpr->evaluate(ctx);
    for (float i = start; i < end && !ctx.executionAborted; ++i) {
        ctx.locals[varName] = i;
        if (body) body->execute(ctx);
    }
    ctx.loopDepth--;
}

// FunctionDefStatement (no-op for now, registry needed)
void FunctionDefStatement::execute(EvalContext&) const {}

// ReturnStatement (no-op for now, needs call stack)
void ReturnStatement::execute(EvalContext&) const {}

// AssignmentStatement
void AssignmentStatement::execute(EvalContext& ctx) const {
    float val = expression->evaluate(ctx);
    ctx.setValue(variableName, val);
}

// ExpressionStatement
void ExpressionStatement::execute(EvalContext& ctx) const {
    if (expression) expression->evaluate(ctx);
}
#include "ScriptParser.h"
#include <cmath>

namespace scripting
{
namespace
{
constexpr int kMaxStatements = 256;
constexpr int kFoldGuard     = 64;

struct NumberExpr final : Expr
{
    explicit NumberExpr (float v) : value (v) {}
    float evaluate (EvalContext&) const override { return value; }
    float value;
};

struct VariableExpr final : Expr {
    explicit VariableExpr(juce::String n) : name(std::move(n)) {}
    float evaluate(EvalContext& ctx) const override { return ctx.getValue(name); }
    ValueType getType() const override { return ValueType::Float; } // Could be improved with symbol table
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

enum class FunctionId
{
    sin_, cos_, tan_, abs_, sqrt_, exp_, log_, tanh_,
    pow_, min_, max_,
    clamp_, clip_, mix_, wrap_,
    fold_, crush_, smoothstep_, noise_,
    gt_, lt_, ge_, le_, select_, pulse_,
    lpf1_, slew_, env_,
    unknown
};

FunctionId resolveFunctionId (const juce::String& lowercaseName)
{
    if (lowercaseName == "sin")        return FunctionId::sin_;
    if (lowercaseName == "cos")        return FunctionId::cos_;
    if (lowercaseName == "tan")        return FunctionId::tan_;
    if (lowercaseName == "abs")        return FunctionId::abs_;
    if (lowercaseName == "sqrt")       return FunctionId::sqrt_;
    if (lowercaseName == "exp")        return FunctionId::exp_;
    if (lowercaseName == "log")        return FunctionId::log_;
    if (lowercaseName == "tanh")       return FunctionId::tanh_;
    if (lowercaseName == "pow")        return FunctionId::pow_;
    if (lowercaseName == "min")        return FunctionId::min_;
    if (lowercaseName == "max")        return FunctionId::max_;
    if (lowercaseName == "clamp")      return FunctionId::clamp_;
    if (lowercaseName == "clip")       return FunctionId::clip_;
    if (lowercaseName == "mix")        return FunctionId::mix_;
    if (lowercaseName == "wrap")       return FunctionId::wrap_;
    if (lowercaseName == "fold")       return FunctionId::fold_;
    if (lowercaseName == "crush")      return FunctionId::crush_;
    if (lowercaseName == "smoothstep") return FunctionId::smoothstep_;
    if (lowercaseName == "noise")      return FunctionId::noise_;
    if (lowercaseName == "gt")         return FunctionId::gt_;
    if (lowercaseName == "lt")         return FunctionId::lt_;
    if (lowercaseName == "ge")         return FunctionId::ge_;
    if (lowercaseName == "le")         return FunctionId::le_;
    if (lowercaseName == "select")     return FunctionId::select_;
    if (lowercaseName == "pulse")      return FunctionId::pulse_;
    if (lowercaseName == "lpf1")       return FunctionId::lpf1_;
    if (lowercaseName == "slew")       return FunctionId::slew_;
    if (lowercaseName == "env")        return FunctionId::env_;
    return FunctionId::unknown;
}


// --- FunctionCallExpr Implementation ---
float FunctionCallExpr::evaluate(EvalContext& ctx) const {
    std::vector<float> values;
    values.reserve(arguments.size());
    for (const auto& arg : arguments)
        values.push_back(arg->evaluate(ctx));

    // Extensible function registry lookup
    if (ctx.functionRegistry) {
        // Built-in
        auto it = ctx.functionRegistry->builtins.find(functionName);
        if (it != ctx.functionRegistry->builtins.end())
            return it->second(ctx, values);
        // User-defined
        auto uit = ctx.functionRegistry->user.find(functionName);
        if (uit != ctx.functionRegistry->user.end()) {
            // TODO: User-defined function call logic (already implemented)
            // For now, just return 0
            return 0.0f;
        }
    }
    return 0.0f;
}

// To add a new DSP function:
// 1. Implement a BuiltinFunction lambda or function.
// 2. Register it in ScriptEngine::compileAndInstall via functionRegistry.builtins["name"] = ...;
// 3. Document its usage in LANGUAGE_SPEC.md.

bool checkArity (const juce::String& fn, int arity)
{
    switch (resolveFunctionId (fn.toLowerCase()))
    {
        case FunctionId::sin_: case FunctionId::cos_: case FunctionId::tan_:
        case FunctionId::abs_: case FunctionId::sqrt_: case FunctionId::exp_:
        case FunctionId::log_: case FunctionId::tanh_: case FunctionId::noise_:
            return arity == 1;

        case FunctionId::pow_: case FunctionId::min_: case FunctionId::max_:
        case FunctionId::crush_: case FunctionId::gt_: case FunctionId::lt_:
        case FunctionId::ge_: case FunctionId::le_: case FunctionId::pulse_:
            return arity == 2;

        case FunctionId::clamp_: case FunctionId::clip_: case FunctionId::mix_:
        case FunctionId::wrap_: case FunctionId::fold_: case FunctionId::smoothstep_:
        case FunctionId::select_:
            return arity == 3;

        // Optional trailing id argument.
        case FunctionId::lpf1_: case FunctionId::slew_:
            return arity == 2 || arity == 3;

        case FunctionId::env_:
            return arity == 3 || arity == 4;

        case FunctionId::unknown:
        default:
            return false;
    }
}
} // namespace

float EvalContext::getValue (const juce::String& name) const
{
    if (name == "inL") return inL;
    if (name == "inR") return inR;
    if (name == "outL") return outL;
    if (name == "outR") return outR;
    if (name == "sr") return sr;
    if (name == "t") return t;

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

ParseResult ScriptParser::parse(const juce::String& source) {
    ParseResult result;
    tokenizer = std::make_unique<ScriptTokenizer>(source);
    errors = &result.errors;

    while (peek().type != TokenType::end) {
        auto stmt = parseStatement();
        if (!stmt) break;
        result.program.statements.push_back(std::move(stmt));
        if ((int)result.program.statements.size() > kMaxStatements) {
            errors->add("Program too large: maximum " + juce::String(kMaxStatements) + " statements.");
            break;
        }
    }
    if (result.program.statements.empty())
        errors->add("Script is empty. Add at least one statement.");
    return result;
}

// --- New Statement Parsing ---

std::unique_ptr<Statement> ScriptParser::parseStatement() {
    const auto token = peek();
    switch (token.type) {
        case TokenType::kw_if:    return parseIf();
        case TokenType::kw_while: return parseWhile();
        case TokenType::kw_for:   return parseFor();
        case TokenType::kw_fn:    return parseFunctionDef();
        case TokenType::kw_return:return parseReturn();
        case TokenType::leftBrace:return parseBlock();
        case TokenType::identifier:
            return parseAssignmentOrExpr();
        default:
            errors->add("Line " + juce::String(token.line) + ": unexpected token at statement start: '" + token.text + "'.");
            consume();
            return nullptr;
    }
}

std::unique_ptr<BlockStatement> ScriptParser::parseBlock() {
    if (!expect(TokenType::leftBrace, "expected '{' to start block")) return nullptr;
    auto block = std::make_unique<BlockStatement>();
    while (peek().type != TokenType::rightBrace && peek().type != TokenType::end) {
        auto stmt = parseStatement();
        if (!stmt) return nullptr;
        block->statements.push_back(std::move(stmt));
    }
    if (!expect(TokenType::rightBrace, "expected '}' to end block")) return nullptr;
    return block;
}

std::unique_ptr<Statement> ScriptParser::parseIf() {
    consume(); // 'if'
    if (!expect(TokenType::leftParen, "expected '(' after 'if'")) return nullptr;
    auto cond = parseExpression();
    if (!cond) return nullptr;
    if (!expect(TokenType::rightParen, "expected ')' after condition")) return nullptr;
    auto thenBranch = parseStatement();
    std::unique_ptr<Statement> elseBranch;
    if (peek().type == TokenType::kw_else) {
        consume();
        elseBranch = parseStatement();
    }
    auto node = std::make_unique<IfStatement>();
    node->condition = std::move(cond);
    node->thenBranch = std::move(thenBranch);
    node->elseBranch = std::move(elseBranch);
    return node;
}

std::unique_ptr<Statement> ScriptParser::parseWhile() {
    consume(); // 'while'
    if (!expect(TokenType::leftParen, "expected '(' after 'while'")) return nullptr;
    auto cond = parseExpression();
    if (!cond) return nullptr;
    if (!expect(TokenType::rightParen, "expected ')' after condition")) return nullptr;
    auto body = parseStatement();
    auto node = std::make_unique<WhileStatement>();
    node->condition = std::move(cond);
    node->body = std::move(body);
    return node;
}

std::unique_ptr<Statement> ScriptParser::parseFor() {
    consume(); // 'for'
    if (!expect(TokenType::leftParen, "expected '(' after 'for'")) return nullptr;
    auto id = consume();
    if (id.type != TokenType::identifier) {
        errors->add("Line " + juce::String(id.line) + ": expected identifier in for loop.");
        return nullptr;
    }
    if (!expect(TokenType::equal, "expected '=' after for variable")) return nullptr;
    auto start = parseExpression();
    if (!start) return nullptr;
    if (!expect(TokenType::semicolon, "expected ';' after for start expr")) return nullptr;
    auto end = parseExpression();
    if (!end) return nullptr;
    if (!expect(TokenType::rightParen, "expected ')' after for end expr")) return nullptr;
    auto body = parseStatement();
    auto node = std::make_unique<ForStatement>();
    node->varName = id.text;
    node->startExpr = std::move(start);
    node->endExpr = std::move(end);
    node->body = std::move(body);
    return node;
}

std::unique_ptr<Statement> ScriptParser::parseFunctionDef() {
    consume(); // 'fn'
    auto id = consume();
    if (id.type != TokenType::identifier) {
        errors->add("Line " + juce::String(id.line) + ": expected function name after 'fn'.");
        return nullptr;
    }
    if (!expect(TokenType::leftParen, "expected '(' after function name")) return nullptr;
    std::vector<juce::String> params;
    if (peek().type != TokenType::rightParen) {
        while (true) {
            auto param = consume();
            if (param.type != TokenType::identifier) {
                errors->add("Line " + juce::String(param.line) + ": expected parameter name.");
                return nullptr;
            }
            params.push_back(param.text);
            if (peek().type == TokenType::comma) { consume(); continue; }
            break;
        }
    }
    if (!expect(TokenType::rightParen, "expected ')' after parameter list")) return nullptr;
    auto body = parseBlock();
    auto node = std::make_unique<FunctionDefStatement>();
    node->name = id.text;
    node->parameters = std::move(params);
    node->body = std::move(body);
    return node;
}

std::unique_ptr<Statement> ScriptParser::parseReturn() {
    consume(); // 'return'
    auto expr = parseExpression();
    if (!expect(TokenType::semicolon, "expected ';' after return value")) return nullptr;
    auto node = std::make_unique<ReturnStatement>();
    node->value = std::move(expr);
    return node;
}

std::unique_ptr<Statement> ScriptParser::parseAssignmentOrExpr() {
    // Lookahead for assignment
    auto id = consume();
    if (peek().type == TokenType::equal) {
        consume();
        auto expr = parseExpression();
        if (!expect(TokenType::semicolon, "expected ';' after assignment")) return nullptr;
        auto node = std::make_unique<AssignmentStatement>();
        node->variableName = id.text;
        node->expression = std::move(expr);
        return node;
    } else {
        // Expression statement
        auto expr = parseExpression();
        if (!expect(TokenType::semicolon, "expected ';' after expression")) return nullptr;
        auto node = std::make_unique<ExpressionStatement>();
        node->expression = std::move(expr);
        return node;
    }
}

std::unique_ptr<Expr> ScriptParser::parseExpression() { return parseAddSub(); }

std::unique_ptr<Expr> ScriptParser::parseAddSub()
{
    auto left = parseMulDiv();
    if (! left)
        return {};

    while (true)
    {
        const auto token = peek();
        if (token.type != TokenType::plus && token.type != TokenType::minus)
            break;

        consume();
        auto right = parseMulDiv();
        if (! right)
            return {};

        left = std::make_unique<BinaryExpr> (token.type == TokenType::plus ? BinaryExpr::Op::add : BinaryExpr::Op::sub,
                                             std::move (left), std::move (right));
    }

    return left;
}

std::unique_ptr<Expr> ScriptParser::parseMulDiv()
{
    auto left = parseUnary();
    if (! left)
        return {};

    while (true)
    {
        const auto token = peek();
        if (token.type != TokenType::star && token.type != TokenType::slash)
            break;

        consume();
        auto right = parseUnary();
        if (! right)
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
        if (! expr)
            return {};
        return std::make_unique<UnaryExpr> (std::move (expr));
    }

    return parsePrimary();
}

std::unique_ptr<Expr> ScriptParser::parsePrimary()
{
    const auto token = consume();


    if (token.type == TokenType::number) {
        // Distinguish int vs float
        if (token.text.containsChar('.'))
            return std::make_unique<TypedLiteralExpr>((float)token.numberValue, ValueType::Float);
        else
            return std::make_unique<TypedLiteralExpr>((float)token.numberValue, ValueType::Int);
    }

    if (token.type == TokenType::kw_true)
        return std::make_unique<BoolLiteralExpr>(true);
    if (token.type == TokenType::kw_false)
        return std::make_unique<BoolLiteralExpr>(false);

    if (token.type == TokenType::identifier)
    {
        if (token.type == TokenType::identifier)
        {
            if (peek().type != TokenType::leftParen)
                return std::make_unique<VariableExpr>(token.text);

            consume(); // '('
            std::vector<std::unique_ptr<Expr>> args;
            if (peek().type != TokenType::rightParen)
            {
                while (true)
                {
                    auto arg = parseExpression();
                    if (!arg)
                        return {};
                    args.push_back(std::move(arg));
                    if (peek().type == TokenType::comma)
                    {
                        consume();
                        continue;
                    }
                    break;
                }
            }
            if (!expect(TokenType::rightParen, "expected ')' after function arguments."))
                return {};
            // No arity check here: user-defined functions may have any arity
            auto call = std::make_unique<FunctionCallExpr>();
            call->functionName = token.text;
            call->arguments = std::move(args);
            return call;
        }
    if (token.type == TokenType::leftParen)
    {
        auto expr = parseExpression();
        if (! expr)
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

Token ScriptParser::consume() { return tokenizer->next(); }
Token ScriptParser::peek() { return tokenizer->peek(); }
} // namespace scripting
