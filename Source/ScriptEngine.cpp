#include "ScriptEngine.h"

#include <algorithm>
#include <cmath>

namespace scripting
{
namespace
{
float clampf (float x, float lo, float hi)
{
    return std::min (hi, std::max (lo, x));
}

float mixf (float a, float b, float amount)
{
    return a + (b - a) * amount;
}

float wrapf (float x, float lo, float hi)
{
    const auto width = hi - lo;
    if (width <= 0.0f)
        return lo;

    auto y = std::fmod (x - lo, width);
    if (y < 0.0f)
        y += width;
    return lo + y;
}

float foldf (float x, float lo, float hi)
{
    if (hi <= lo)
        return lo;

    auto y = x;
    while (y < lo || y > hi)
    {
        if (y > hi)
            y = hi - (y - hi);
        else if (y < lo)
            y = lo + (lo - y);
    }

    return y;
}

float smoothstepf (float edge0, float edge1, float x)
{
    if (std::abs (edge1 - edge0) < 1.0e-9f)
        return x >= edge1 ? 1.0f : 0.0f;

    const auto t = clampf ((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float noisef (float seed)
{
    const auto v = std::sin (seed * 12.9898f) * 43758.5453f;
    return 2.0f * (v - std::floor (v)) - 1.0f;
}

float getArg (const std::vector<float>& args, size_t index, float fallback = 0.0f)
{
    return index < args.size() ? args[index] : fallback;
}

juce::String stateKey (juce::String functionName, int lane)
{
    return "__" + functionName + "_" + juce::String (lane);
}

float readLaneState (EvalContext& ctx, juce::String functionName, int lane)
{
    if (ctx.persistentState == nullptr)
        return 0.0f;

    const auto key = stateKey (std::move (functionName), lane);
    if (const auto it = ctx.persistentState->find (key); it != ctx.persistentState->end())
        return it->second;

    return 0.0f;
}

void writeLaneState (EvalContext& ctx, juce::String functionName, int lane, float value)
{
    if (ctx.persistentState == nullptr)
        return;

    (*ctx.persistentState)[stateKey (std::move (functionName), lane)] = value;
}

int laneFromArgs (const std::vector<float>& args, size_t index)
{
    return (int) std::lrint (getArg (args, index, 0.0f));
}

void registerBuiltins (FunctionRegistry& registry)
{
    registry.builtins.clear();

    registry.builtins["sin"] = [] (EvalContext&, const std::vector<float>& a) { return std::sin (getArg (a, 0)); };
    registry.builtins["cos"] = [] (EvalContext&, const std::vector<float>& a) { return std::cos (getArg (a, 0)); };
    registry.builtins["tan"] = [] (EvalContext&, const std::vector<float>& a) { return std::tan (getArg (a, 0)); };
    registry.builtins["abs"] = [] (EvalContext&, const std::vector<float>& a) { return std::abs (getArg (a, 0)); };
    registry.builtins["sqrt"] = [] (EvalContext&, const std::vector<float>& a) { return std::sqrt (std::max (0.0f, getArg (a, 0))); };
    registry.builtins["exp"] = [] (EvalContext&, const std::vector<float>& a) { return std::exp (getArg (a, 0)); };
    registry.builtins["log"] = [] (EvalContext&, const std::vector<float>& a) { return std::log (std::max (1.0e-12f, getArg (a, 0))); };
    registry.builtins["tanh"] = [] (EvalContext&, const std::vector<float>& a) { return std::tanh (getArg (a, 0)); };

    registry.builtins["pow"] = [] (EvalContext&, const std::vector<float>& a) { return std::pow (getArg (a, 0), getArg (a, 1)); };
    registry.builtins["min"] = [] (EvalContext&, const std::vector<float>& a) { return std::min (getArg (a, 0), getArg (a, 1)); };
    registry.builtins["max"] = [] (EvalContext&, const std::vector<float>& a) { return std::max (getArg (a, 0), getArg (a, 1)); };

    registry.builtins["clamp"] = [] (EvalContext&, const std::vector<float>& a) { return clampf (getArg (a, 0), getArg (a, 1), getArg (a, 2)); };
    registry.builtins["clip"] = registry.builtins["clamp"];
    registry.builtins["mix"] = [] (EvalContext&, const std::vector<float>& a) { return mixf (getArg (a, 0), getArg (a, 1), getArg (a, 2)); };
    registry.builtins["wrap"] = [] (EvalContext&, const std::vector<float>& a) { return wrapf (getArg (a, 0), getArg (a, 1), getArg (a, 2)); };
    registry.builtins["fold"] = [] (EvalContext&, const std::vector<float>& a) { return foldf (getArg (a, 0), getArg (a, 1), getArg (a, 2)); };
    registry.builtins["crush"] = [] (EvalContext&, const std::vector<float>& a)
    {
        const auto x = getArg (a, 0);
        const auto steps = std::max (1.0f, getArg (a, 1));
        return std::round (x * steps) / steps;
    };

    registry.builtins["smoothstep"] = [] (EvalContext&, const std::vector<float>& a)
    {
        return smoothstepf (getArg (a, 0), getArg (a, 1), getArg (a, 2));
    };

    registry.builtins["noise"] = [] (EvalContext&, const std::vector<float>& a)
    {
        return noisef (getArg (a, 0));
    };

    registry.builtins["gt"] = [] (EvalContext&, const std::vector<float>& a) { return getArg (a, 0) >  getArg (a, 1) ? 1.0f : 0.0f; };
    registry.builtins["lt"] = [] (EvalContext&, const std::vector<float>& a) { return getArg (a, 0) <  getArg (a, 1) ? 1.0f : 0.0f; };
    registry.builtins["ge"] = [] (EvalContext&, const std::vector<float>& a) { return getArg (a, 0) >= getArg (a, 1) ? 1.0f : 0.0f; };
    registry.builtins["le"] = [] (EvalContext&, const std::vector<float>& a) { return getArg (a, 0) <= getArg (a, 1) ? 1.0f : 0.0f; };
    registry.builtins["select"] = [] (EvalContext&, const std::vector<float>& a) { return getArg (a, 0) != 0.0f ? getArg (a, 1) : getArg (a, 2); };

    registry.builtins["pulse"] = [] (EvalContext& ctx, const std::vector<float>& a)
    {
        const auto freq = std::max (0.0f, getArg (a, 0));
        const auto duty = clampf (getArg (a, 1), 0.0f, 1.0f);
        const auto phase = std::fmod (ctx.t * freq, 1.0f);
        return phase < duty ? 1.0f : 0.0f;
    };

    registry.builtins["lpf1"] = [] (EvalContext& ctx, const std::vector<float>& a)
    {
        const auto x = getArg (a, 0);
        const auto coeff = clampf (getArg (a, 1), 0.0f, 1.0f);
        const auto lane = laneFromArgs (a, 2);

        auto y = readLaneState (ctx, "lpf1", lane);
        y += coeff * (x - y);
        writeLaneState (ctx, "lpf1", lane, y);
        return y;
    };

    registry.builtins["slew"] = [] (EvalContext& ctx, const std::vector<float>& a)
    {
        const auto target = getArg (a, 0);
        const auto speed = std::max (0.0f, getArg (a, 1));
        const auto lane = laneFromArgs (a, 2);

        auto current = readLaneState (ctx, "slew", lane);
        const auto delta = clampf (target - current, -speed, speed);
        current += delta;
        writeLaneState (ctx, "slew", lane, current);
        return current;
    };

    registry.builtins["env"] = [] (EvalContext& ctx, const std::vector<float>& a)
    {
        const auto x = std::abs (getArg (a, 0));
        const auto attack = clampf (getArg (a, 1), 0.0f, 1.0f);
        const auto release = clampf (getArg (a, 2), 0.0f, 1.0f);
        const auto lane = laneFromArgs (a, 3);

        auto y = readLaneState (ctx, "env", lane);
        const auto coeff = x > y ? attack : release;
        y += coeff * (x - y);
        writeLaneState (ctx, "env", lane, y);
        return y;
    };
}
} // namespace

ScriptEngine::CompileResult ScriptEngine::compileAndInstall (const juce::String& source)
{
    ScriptParser parser;
    auto result = parser.parse (source);

    registerBuiltins (result.program.functionRegistry);
    result.program.functionRegistry.user.clear();

    for (const auto& stmt : result.program.statements)
        if (auto* fn = dynamic_cast<FunctionDefStatement*> (stmt.get()))
            result.program.functionRegistry.user[fn->name] = fn;

    if (! result.errors.isEmpty())
    {
        lastError = result.errors.joinIntoString ("\n");
        return { false, result.errors };
    }

    auto compiled = std::make_shared<CompiledProgram>();
    compiled->program = std::move (result.program);
    compiled->source = source;

    activeProgram.store (std::shared_ptr<const CompiledProgram> (compiled));
    stateResetRequested.store (true);
    lastError.clear();
    return { true, {} };
}

void ScriptEngine::reset (double sampleRate)
{
    currentSampleRate = sampleRate;
    sampleCounter = 0;
    persistentState.clear();
    stateResetRequested.store (false);
}

juce::String ScriptEngine::getCurrentSource() const
{
    if (const auto program = getProgramSnapshot())
        return program->source;

    return {};
}

juce::String ScriptEngine::getLastError() const
{
    return lastError;
}

std::shared_ptr<const CompiledProgram> ScriptEngine::getProgramSnapshot() const
{
    return activeProgram.load();
}

constexpr size_t kMaxPersistentStateEntries = 128;
constexpr int kMaxInstructionsPerSample = 4096;

void ScriptEngine::enforcePersistentStateLimit()
{
    while (persistentState.size() > kMaxPersistentStateEntries)
        persistentState.erase (persistentState.begin());
}

void ScriptEngine::processBlock (juce::AudioBuffer<float>& buffer, const std::array<float, 8>& macros)
{
    if (stateResetRequested.exchange (false))
    {
        persistentState.clear();
        sampleCounter = 0;
    }

    const auto program = getProgramSnapshot();
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    std::array<float, 8> safeMacros;
    for (size_t i = 0; i < 8; ++i)
    {
        float v = macros[i];
        if (! std::isfinite (v)) v = 0.0f;
        safeMacros[i] = clampf (v, 0.0f, 1.0f);
    }

    if (program == nullptr)
    {
        sampleCounter += (uint64_t) numSamples;
        return;
    }

    EvalContext ctx;
    ctx.sr = (float) currentSampleRate;
    ctx.macros = &safeMacros;
    ctx.persistentState = &persistentState;
    ctx.functionRegistry = &program->program.functionRegistry;

    for (int s = 0; s < numSamples; ++s)
    {
        ctx.t = (float) sampleCounter / (float) currentSampleRate;
        ctx.inL = numChannels > 0 ? buffer.getSample (0, s) : 0.0f;
        ctx.inR = numChannels > 1 ? buffer.getSample (1, s) : ctx.inL;
        ctx.outL = ctx.inL;
        ctx.outR = ctx.inR;
        ctx.locals.clear();
        ctx.executionAborted = false;
        ctx.returnTriggered = false;
        ctx.returnValue = 0.0f;
        ctx.loopDepth = 0;
        ctx.recursionDepth = 0;
        ctx.instructionCount = 0;
        ctx.maxInstructions = kMaxInstructionsPerSample;

        for (const auto& stmt : program->program.statements)
        {
            if (stmt != nullptr)
                stmt->execute (ctx);
            if (ctx.executionAborted || ctx.returnTriggered)
                break;
        }

        enforcePersistentStateLimit();

        if (numChannels > 0)
            buffer.setSample (0, s, ctx.outL);
        if (numChannels > 1)
            buffer.setSample (1, s, ctx.outR);
        for (int ch = 2; ch < numChannels; ++ch)
            buffer.setSample (ch, s, 0.5f * (ctx.outL + ctx.outR));

        ++sampleCounter;
    }
}

juce::String defaultScript()
{
    return R"(# p1 controls input drive
drive = 0.5 + p1 * 6.0;
outL = tanh(inL * drive);
outR = tanh(inR * drive);
)";
}

juce::StringArray exampleNames()
{
    return {
        "Transparent soft clip",
        "Cross-feedback distortion",
        "Time-ramp ring modulation",
        "Low-pass morph",
        "Wavefold shimmer",
        "Stereo bit crush drift",
        "Noisy transient gate",
        "Rhythmic pulse gate",
        "Envelope duck tremor"
    };
}

juce::String exampleScript (int index)
{
    switch (index)
    {
        case 0:
            return R"(# clean until peaks, then smooth saturation
outL = tanh(inL * 2.7) * 0.7;
outR = tanh(inR * 2.7) * 0.7;
)";
        case 1:
            return R"(# channels influence each other for unstable stereo color
xL = tanh(inL * 3.0 + inR * 1.7);
xR = tanh(inR * 3.0 + inL * 1.7);
outL = xL * 0.8;
outR = xR * 0.8;
)";
        case 2:
            return R"(# ring-mod whose modulator frequency keeps evolving with time
phase = wrap(t * 0.31, 0.0, 1.0);
mod = sin(6.2831853 * (80.0 + phase * 700.0) * t);
outL = inL * mod;
outR = inR * mod;
)";
        case 3:
            return R"(# morph between dry and one-pole filtered signal
amount = 0.5 + 0.5 * sin(t * 0.2);
fL = lpf1(inL, 0.08, 0);
fR = lpf1(inR, 0.08, 1);
outL = mix(inL, fL, amount);
outR = mix(inR, fR, amount);
)";
        case 4:
            return R"(# fold + saturation for airy harmonic layers
preL = inL * 5.0;
preR = inR * 5.0;
outL = tanh(fold(preL, -0.7, 0.7)) * 0.8;
outR = tanh(fold(preR, -0.7, 0.7)) * 0.8;
)";
        case 5:
            return R"(# moving lo-fi texture with independent stereo smoothers
steps = 8.0 + 7.0 * sin(t * 0.29);
cL = crush(inL, steps);
cR = crush(inR, steps + 2.0);
outL = slew(cL, 0.025, 0);
outR = slew(cR, 0.020, 1);
)";
        case 6:
            return R"(# emphasize transients with deterministic pseudo-noise texture
envL = abs(inL);
envR = abs(inR);
gL = smoothstep(0.02, 0.25, envL);
gR = smoothstep(0.02, 0.25, envR);
n = noise(3.0 + p2 * 20.0) * (0.02 + p1 * 0.25);
outL = inL + n * gL;
outR = inR + n * gR;
)";
        case 7:
            return R"(# synced pulse gating with blend control
freq = 1.0 + p1 * 16.0;
duty = 0.1 + p2 * 0.8;
g = pulse(freq, duty);
mask = gt(g, 0.0);
outL = mix(inL * 0.15, inL, mask);
outR = mix(inR * 0.15, inR, mask);
)";
        case 8:
            return R"(# envelope follower ducks a moving tremolo
envL = env(inL, 0.25, 0.01, 0);
envR = env(inR, 0.25, 0.01, 1);
mod = 0.5 + 0.5 * pulse(2.0 + p1 * 8.0, 0.5);
depthL = clamp((1.0 - envL * (0.7 + p2)), 0.2, 1.0);
depthR = clamp((1.0 - envR * (0.7 + p2)), 0.2, 1.0);
outL = inL * mix(depthL, 1.0, mod);
outR = inR * mix(depthR, 1.0, mod);
)";
        default:
            break;
    }

    return defaultScript();
}

juce::String helpText()
{
    return R"(audio_scripter language quick guide

Statements
- One statement per line: variable = expression;
- Comments start with #

Built-in inputs
- inL, inR : input samples
- sr       : sample rate
- t        : absolute time in seconds
- p1..p8   : user macro controls (automatable)

Outputs
- outL, outR : assign these to emit audio

Math operators
- + - * /

Functions
- sin(x), cos(x), tan(x), abs(x), sqrt(x), exp(x), log(x), tanh(x)
- min(a,b), max(a,b), pow(a,b)
- clamp(x, lo, hi), clip(x, lo, hi)
- mix(a, b, amount)
- wrap(x, lo, hi)
- fold(x, lo, hi)
- crush(x, steps)
- smoothstep(edge0, edge1, x)
- noise(seed)
- gt(a,b), lt(a,b), ge(a,b), le(a,b)
- select(cond, a, b)
- pulse(freqHz, duty)
- env(x, attack, release [, id])
- lpf1(x, coeff [, id])      # one-pole low-pass (stateful)
- slew(target, speed [, id]) # slew limiter (stateful)

State variables
- Any variable starting with state_ is persistent across samples.
- Optional function id lets you keep separate states per channel.
)";
}
} // namespace scripting
