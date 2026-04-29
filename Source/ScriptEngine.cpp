#include "ScriptEngine.h"

#include <algorithm>
#include <cmath>
#include <memory>

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

juce::String stateKeyWithSuffix (juce::String functionName, int lane, juce::String suffix)
{
    return "__" + functionName + "_" + juce::String (lane) + "_" + suffix;
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

float readLaneStateSuffix (EvalContext& ctx, juce::String functionName, int lane, juce::String suffix)
{
    if (ctx.persistentState == nullptr)
        return 0.0f;

    const auto key = stateKeyWithSuffix (std::move (functionName), lane, std::move (suffix));
    if (const auto it = ctx.persistentState->find (key); it != ctx.persistentState->end())
        return it->second;

    return 0.0f;
}

void writeLaneStateSuffix (EvalContext& ctx, juce::String functionName, int lane, juce::String suffix, float value)
{
    if (ctx.persistentState == nullptr)
        return;

    (*ctx.persistentState)[stateKeyWithSuffix (std::move (functionName), lane, std::move (suffix))] = value;
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

    registry.builtins["hp1"] = [] (EvalContext& ctx, const std::vector<float>& a)
    {
        const auto x = getArg (a, 0);
        const auto coeff = clampf (getArg (a, 1), 0.0f, 1.0f);
        const auto lane = laneFromArgs (a, 2);

        auto y = readLaneState (ctx, "hp1_lpf", lane);
        y += coeff * (x - y);
        writeLaneState (ctx, "hp1_lpf", lane, y);
        return x - y;
    };

    registry.builtins["bp1"] = [] (EvalContext& ctx, const std::vector<float>& a)
    {
        const auto x = getArg (a, 0);
        const auto hpCoeff = clampf (getArg (a, 1), 0.0f, 1.0f);
        const auto lpCoeff = clampf (getArg (a, 2), 0.0f, 1.0f);
        const auto lane = laneFromArgs (a, 3);

        auto hpLp = readLaneState (ctx, "bp1_hplp", lane);
        hpLp += hpCoeff * (x - hpLp);
        writeLaneState (ctx, "bp1_hplp", lane, hpLp);
        const auto hp = x - hpLp;

        auto bp = readLaneState (ctx, "bp1_bp", lane);
        bp += lpCoeff * (hp - bp);
        writeLaneState (ctx, "bp1_bp", lane, bp);
        return bp;
    };

    registry.builtins["svf"] = [] (EvalContext& ctx, const std::vector<float>& a)
    {
        const auto x = getArg (a, 0);
        const auto cutoff = clampf (getArg (a, 1), 0.001f, 0.99f);
        const auto q = std::max (0.05f, getArg (a, 2, 0.7f));
        const auto mode = (int) std::lrint (getArg (a, 3, 0.0f));
        const auto lane = laneFromArgs (a, 4);

        auto low = readLaneStateSuffix (ctx, "svf", lane, "low");
        auto band = readLaneStateSuffix (ctx, "svf", lane, "band");
        const auto high = x - low - q * band;
        band += cutoff * high;
        low += cutoff * band;
        writeLaneStateSuffix (ctx, "svf", lane, "low", low);
        writeLaneStateSuffix (ctx, "svf", lane, "band", band);

        switch (mode)
        {
            case 1: return band;
            case 2: return high;
            default:return low;
        }
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

    registry.builtins["delay"] = [] (EvalContext& ctx, const std::vector<float>& a)
    {
        constexpr int kMaxDelaySamples = 96000;
        const auto x = getArg (a, 0);
        const int samples = std::clamp ((int) std::lrint (getArg (a, 1, 1.0f)), 1, kMaxDelaySamples - 1);
        const int lane = laneFromArgs (a, 2);

        if (ctx.delayBuffers == nullptr)
            return 0.0f;

        auto& buf = (*ctx.delayBuffers)[lane];
        if ((int) buf.size() < kMaxDelaySamples)
            buf.assign ((size_t) kMaxDelaySamples, 0.0f);

        auto& wp = (*ctx.delayWritePositions)[lane];

        int rp = wp - samples;
        if (rp < 0) rp += kMaxDelaySamples;

        const float y = buf[(size_t) rp];
        buf[(size_t) wp] = x;
        wp = (wp + 1) % kMaxDelaySamples;
        return y;
    };

    registry.builtins["sat"] = [] (EvalContext&, const std::vector<float>& a)
    {
        const auto x = getArg (a, 0);
        const auto drive = std::max (0.0f, getArg (a, 1, 1.0f));
        const auto mode = (int) std::lrint (getArg (a, 2, 0.0f));
        const auto d = x * (1.0f + drive);

        switch (mode)
        {
            case 1: return (2.0f / juce::MathConstants<float>::pi) * std::atan (d);
            case 2:
            {
                const auto y = d - (d * d * d) / 3.0f;
                return clampf (y, -1.0f, 1.0f);
            }
            default: return std::tanh (d);
        }
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

    // Use the atomic shared_ptr helpers for portability with libc++.
    std::atomic_store(&activeProgram, std::shared_ptr<const CompiledProgram> (compiled));
    stateResetRequested.store (true);
    lastError.clear();
    return { true, {} };
}

void ScriptEngine::reset (double sampleRate)
{
    currentSampleRate = sampleRate;
    sampleCounter = 0;
    persistentState.clear();
    delayBuffers.clear();
    delayWritePositions.clear();
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
    return std::atomic_load(&activeProgram);
}

constexpr size_t kMaxPersistentStateEntries = 128;
constexpr int kMaxInstructionsPerSample = 4096;

void ScriptEngine::enforcePersistentStateLimit()
{
    while (persistentState.size() > kMaxPersistentStateEntries)
        persistentState.erase (persistentState.begin());
}

void ScriptEngine::processBlock (juce::AudioBuffer<float>& buffer, std::array<float, 8>& macros)
{
    if (stateResetRequested.exchange (false))
    {
        persistentState.clear();
        delayBuffers.clear();
        delayWritePositions.clear();
        sampleCounter = 0;
    }

    const auto program = getProgramSnapshot();
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    for (size_t i = 0; i < 8; ++i)
    {
        float v = macros[i];
        if (! std::isfinite (v)) v = 0.0f;
        macros[i] = clampf (v, 0.0f, 1.0f);
    }

    if (program == nullptr)
    {
        sampleCounter += (uint64_t) numSamples;
        return;
    }

    EvalContext ctx;
    ctx.sr = (float) currentSampleRate;
    ctx.macros = &macros;
    ctx.persistentState = &persistentState;
    ctx.delayBuffers = &delayBuffers;
    ctx.delayWritePositions = &delayWritePositions;
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
    return juce::String::fromUTF8(R"(
        
audio_scripter 1.1.0  —  https://krahd.github.io/audio_scripter/
----------------------------------------------------------------

OVERVIEW

  The script runs once per audio sample, top to bottom.
  Assign outL and outR to produce output (they default to inL, inR).

  x = expression;        assignment — must end with ;
  # comment              everything after # is ignored


SPECIAL VARIABLES  (read-only except outL / outR)

  inL, inR    current input sample (-1.0 to 1.0)
  outL, outR  output sample (pre-set to inL, inR each sample)
  sr          sample rate in Hz  (e.g. 44100.0)
  t           elapsed time in seconds, grows continuously
  p1 .. p8    macro knobs, always in range 0.0 – 1.0, automatable


STATE VARIABLES

  Any variable whose name starts with state_ persists between samples.
  Use them to build oscillators, filters, envelope followers, etc.

    state_phase = wrap(state_phase + 440.0 / sr, 0.0, 1.0);
    tone = sin(6.2831853 * state_phase);


OPERATORS

  +  -  *  /           arithmetic (/ returns 0 when divisor is 0)
  -x                   unary minus
  true  false          boolean literals (1.0 / 0.0)


CONTROL FLOW

  if (condition) { ... } else { ... }
  while (condition) { ... }
  for (i = 0; 8) { ... }          i iterates 0, 1 … 7 (legacy step-by-1 form)
  for (i = 0; lt(i,8); 1) { ... } extended form: condition and step are exprs
  break;                           exit the innermost loop immediately
  continue;                        skip to the next iteration of the innermost loop
  fn name(a, b) { return a + b; }

  Note: use gt/lt/ge/le/select for comparisons — < > == are not operators.


MATH FUNCTIONS

  sin(x)          sine; x in radians  (2*pi = 6.2831853)
  cos(x)          cosine
  tan(x)          tangent — clips very sharply near ±pi/2
  abs(x)          absolute value
  sqrt(x)         square root  (x clamped to 0 internally)
  exp(x)          e raised to x
  log(x)          natural logarithm  (x clamped to 1e-12 internally)
  tanh(x)         hyperbolic tangent — smooth clipper, output in (-1, 1)
  pow(a, b)       a raised to power b
  min(a, b)       smaller of two values
  max(a, b)       larger of two values


SHAPING FUNCTIONS

  clamp(x, lo, hi)        hard-limit x to [lo, hi]
  clip(x, lo, hi)         alias for clamp
  mix(a, b, t)            linear interpolate: a + (b - a) * t
  wrap(x, lo, hi)         wrap x cyclically inside [lo, hi]
  fold(x, lo, hi)         fold x back at boundaries (wave-folder)
  crush(x, steps)         quantise to N amplitude levels (bit-crusher)
  smoothstep(e0, e1, x)   smooth 0->1 S-curve between e0 and e1


DSP / CREATIVE FUNCTIONS

  noise(seed)             deterministic hash noise, output in (-1, 1)
                          use noise(t * sr) for per-sample white noise

  pulse(hz, duty)         square wave: 1.0 while phase < duty, else 0.0
                          phase is based on absolute time t

  env(x, atk, rel, id)   envelope follower on signal x
                          atk, rel are per-sample smoothing coefficients
                          id (optional int) keeps separate state per lane

  lpf1(x, coeff, id)     one-pole low-pass filter
                          coeff ~0.002 = very dark, ~0.5 = bright
                          id (optional) separates state per lane

  slew(target, speed, id) slew-rate limiter — moves toward target
                          at most speed units per sample

  hp1(x, coeff, id)      one-pole high-pass filter (x minus its own lpf1)
                          coeff ~0.002 = very dark HP, ~0.5 = bright
                          id (optional) separates state per lane

  bp1(x, hpC, lpC, id)   band-pass: hp1 then lpf1 in series
                          hpC sets low-cut, lpC sets high-cut
                          id (optional) separates state per lane

  svf(x, cut, q, mode, id)  state-variable filter
                          cut: normalised cutoff 0.001–0.99
                          q: resonance (min 0.05, default 0.7)
                          mode: 0=low-pass  1=band-pass  2=high-pass
                          id (optional) separates state per lane

  delay(x, samples, id)  delay line — delays x by N samples
                          samples clamped 1–96000
                          id (optional) separates state per lane

  sat(x, drive, mode)    saturation / soft-clipping
                          drive >= 0 (0 = unity, higher = more clip)
                          mode: 0=tanh (default)  1=atan  2=cubic


COMPARISON / LOGIC  (return 1.0 for true, 0.0 for false)

  gt(a, b)        1.0 if a > b
  lt(a, b)        1.0 if a < b
  ge(a, b)        1.0 if a >= b
  le(a, b)        1.0 if a <= b
  select(c, a, b) returns a if c != 0, else b


QUICK RECIPES

  # Sine oscillator via phase accumulator
  state_ph = wrap(state_ph + 440.0 / sr, 0.0, 1.0);
  outL = sin(6.2831853 * state_ph);  outR = outL;

  # Envelope-controlled gate
  amp = env(abs(inL), 0.001, 0.05, 0);
  outL = inL * smoothstep(0.01, 0.1, amp);

  # Ring modulator at 200 Hz
  outL = inL * sin(6.2831853 * 200.0 * t);

  # User-defined function
  fn softSat(x, drive) { return tanh(x * drive) / drive; }
    outL = softSat(inL, 3.0);  outR = softSat(inR, 3.0);
)");
}
} // namespace scripting
