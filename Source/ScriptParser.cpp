#include "ScriptParser.h"
#include <cmath>

namespace scripting
{
namespace
{
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

struct FunctionCallExpr final : Expr
{
    explicit FunctionCallExpr (juce::String fn) : functionName (std::move (fn)) {}

    float evaluate (EvalContext& ctx) const override
    {
        std::vector<float> values;
        values.reserve (arguments.size());
        for (const auto& arg : arguments)
            values.push_back (arg->evaluate (ctx));

        const auto fn = functionName.toLowerCase();

        if (fn == "sin") return std::sin (values[0]);
        if (fn == "cos") return std::cos (values[0]);
        if (fn == "tan") return std::tan (values[0]);
        if (fn == "abs") return std::abs (values[0]);
        if (fn == "sqrt") return std::sqrt (std::max (0.0f, values[0]));
        if (fn == "exp") return std::exp (values[0]);
        if (fn == "log") return std::log (std::max (1.0e-9f, values[0]));
        if (fn == "pow") return std::pow (values[0], values[1]);
        if (fn == "min") return std::min (values[0], values[1]);
        if (fn == "max") return std::max (values[0], values[1]);
        if (fn == "clamp") return juce::jlimit (values[1], values[2], values[0]);
        if (fn == "mix") return values[0] * (1.0f - values[2]) + values[1] * values[2];
        if (fn == "wrap")
        {
            const auto lo = values[1];
            const auto hi = values[2];
            const auto width = hi - lo;
            if (std::abs (width) < 1.0e-9f) return lo;
            auto x = std::fmod (values[0] - lo, width);
            if (x < 0.0f) x += width;
            return lo + x;
        }

        if (fn == "lpf1")
        {
            const auto x = values[0];
            const auto coeff = juce::jlimit (0.0f, 1.0f, values[1]);
            const int id = (int) (values.size() > 2 ? values[2] : 0.0f);
            const auto key = "state_lpf1_" + juce::String (id);
            const auto y = coeff * x + (1.0f - coeff) * ctx.getValue (key);
            ctx.setValue (key, y);
            return y;
        }

        if (fn == "slew")
        {
            const auto target = values[0];
            const auto speed = std::max (0.0f, values[1]);
            const int id = (int) (values.size() > 2 ? values[2] : 0.0f);
            const auto key = "state_slew_" + juce::String (id);
            const auto current = ctx.getValue (key);
            const auto step = juce::jlimit (-speed, speed, target - current);
            const auto y = current + step;
            ctx.setValue (key, y);
            return y;
        }

        if (fn == "env")
        {
            const auto x = std::abs (values[0]);
            const auto attack = juce::jlimit (0.0f, 1.0f, values[1]);
            const auto release = juce::jlimit (0.0f, 1.0f, values[2]);
            const int id = (int) (values.size() > 3 ? values[3] : 0.0f);
            const auto key = "state_env_" + juce::String (id);
            const auto current = ctx.getValue (key);
            const auto coeff = x > current ? attack : release;
            const auto y = coeff * x + (1.0f - coeff) * current;
            ctx.setValue (key, y);
            return y;
        }
        if (fn == "clip") return juce::jlimit (values[1], values[2], values[0]);
        if (fn == "fold")
        {
            const auto lo = values[1];
            const auto hi = values[2];
            auto x = values[0];
            if (hi <= lo || ! std::isfinite (x)) return lo;

            int guard = 64;
            while ((x < lo || x > hi) && --guard > 0)
            {
                if (x > hi) x = hi - (x - hi);
                if (x < lo) x = lo + (lo - x);
            }

            return juce::jlimit (lo, hi, x);
        }
        if (fn == "crush")
        {
            const auto steps = std::max (1.0f, values[1]);
            return std::round (values[0] * steps) / steps;
        }

        if (fn == "gt") return values[0] > values[1] ? 1.0f : 0.0f;
        if (fn == "lt") return values[0] < values[1] ? 1.0f : 0.0f;
        if (fn == "ge") return values[0] >= values[1] ? 1.0f : 0.0f;
        if (fn == "le") return values[0] <= values[1] ? 1.0f : 0.0f;
        if (fn == "select") return values[0] >= 0.5f ? values[1] : values[2];

        if (fn == "pulse")
        {
            const auto freq = std::max (0.0f, values[0]);
            const auto duty = juce::jlimit (0.0f, 1.0f, values[1]);
            const auto phase = std::fmod (ctx.t * freq, 1.0f);
            return phase < duty ? 1.0f : -1.0f;
        }
        if (fn == "smoothstep")
        {
            const auto edge0 = values[0];
            const auto edge1 = values[1];
            if (edge1 <= edge0) return 0.0f;
            auto x = (values[2] - edge0) / (edge1 - edge0);
            x = juce::jlimit (0.0f, 1.0f, x);
            return x * x * (3.0f - 2.0f * x);
        }

        if (fn == "noise")
        {
            const auto seed = values[0];
            const auto x = std::sin ((ctx.t + seed) * 43758.5453f) * 12345.6789f;
            return (std::fmod (x, 2.0f) - 1.0f);
        }

        if (fn == "tanh") return std::tanh (values[0]);
        return 0.0f;
    }

    juce::String functionName;
    std::vector<std::unique_ptr<Expr>> arguments;
};

bool checkArity (const juce::String& fn, int arity)
{
    const auto name = fn.toLowerCase();
    static const std::map<juce::String, int> map {
        { "sin", 1 }, { "cos", 1 }, { "tan", 1 }, { "abs", 1 }, { "sqrt", 1 },
        { "exp", 1 }, { "log", 1 }, { "pow", 2 }, { "min", 2 }, { "max", 2 },
        { "clamp", 3 }, { "mix", 3 }, { "wrap", 3 }, { "tanh", 1 }, { "clip", 3 }, { "fold", 3 }, { "crush", 2 }, { "smoothstep", 3 }, { "noise", 1 }, { "gt", 2 }, { "lt", 2 }, { "ge", 2 }, { "le", 2 }, { "select", 3 }, { "pulse", 2 }, { "env", 3 }
    };

    if (name == "lpf1" || name == "slew")
        return arity == 2 || arity == 3;
        if (name == "env")
        return arity == 3 || arity == 4;
    const auto it = map.find (name);
    return it != map.end() && it->second == arity;
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

ParseResult ScriptParser::parse (const juce::String& source)
{
    ParseResult result;
    tokenizer = std::make_unique<ScriptTokenizer> (source);
    errors = &result.errors;

    while (peek().type != TokenType::end)
    {
        auto id = consume();
        if (id.type != TokenType::identifier)
        {
            errors->add ("Line " + juce::String (id.line) + ": expected identifier at statement start.");
            break;
        }

        if (! expect (TokenType::equal, "expected '=' after identifier."))
            break;

        auto expr = parseExpression();
        if (! expr)
            break;

        if (! expect (TokenType::semicolon, "expected ';' at end of statement."))
            break;

        result.program.statements.push_back ({ id.text, std::move (expr), id.line });

        if (result.program.statements.size() > 256)
        {
            errors->add ("Program too large: maximum 256 statements.");
            break;
        }
    }

    if (result.program.statements.empty())
        errors->add ("Script is empty. Add at least one statement ending with ';'.");

    return result;
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

    if (token.type == TokenType::number)
        return std::make_unique<NumberExpr> ((float) token.numberValue);

    if (token.type == TokenType::identifier)
    {
        if (peek().type != TokenType::leftParen)
            return std::make_unique<VariableExpr> (token.text);

        consume(); // '('
        auto call = std::make_unique<FunctionCallExpr> (token.text);

        if (peek().type != TokenType::rightParen)
        {
            while (true)
            {
                auto arg = parseExpression();
                if (! arg)
                    return {};

                call->arguments.push_back (std::move (arg));

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

        if (! checkArity (token.text, (int) call->arguments.size()))
        {
            errors->add ("Line " + juce::String (token.line) + ": invalid function or argument count for '" + token.text + "'.");
            return {};
        }

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

    errors->add ("Line " + juce::String (token.line) + ": " + message);
    return false;
}

Token ScriptParser::consume() { return tokenizer->next(); }
Token ScriptParser::peek() { return tokenizer->peek(); }
} // namespace scripting
