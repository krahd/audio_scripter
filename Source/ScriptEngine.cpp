#include "ScriptEngine.h"

namespace scripting
{
ScriptEngine::CompileResult ScriptEngine::compileAndInstall (const juce::String& source)
{
    ScriptParser parser;
    auto result = parser.parse (source);

    // Build extensible function registry
    result.program.functionRegistry.builtins.clear();
    result.program.functionRegistry.user.clear();
    // Register built-in functions (example: sin)
    result.program.functionRegistry.builtins["sin"] = [](EvalContext&, const std::vector<float>& args) { return std::sin(args[0]); };
    // ...register other built-ins here...
    for (const auto& stmt : result.program.statements) {
        if (auto* fn = dynamic_cast<FunctionDefStatement*>(stmt.get())) {
            result.program.functionRegistry.user[fn->name] = fn;
        }
    }

    if (! result.errors.isEmpty()) {
        lastError = result.errors.joinIntoString("\n");
        return { false, result.errors };
    }
juce::String ScriptEngine::getLastError() const {
    return lastError;
}

    auto compiled = std::make_shared<CompiledProgram>();
    compiled->program = std::move (result.program);
    compiled->source = source;

    activeProgram.store (std::shared_ptr<const CompiledProgram> (compiled));
    stateResetRequested.store (true);
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

std::shared_ptr<const CompiledProgram> ScriptEngine::getProgramSnapshot() const
{
    return activeProgram.load();
}

constexpr size_t kMaxPersistentStateEntries = 128;

void ScriptEngine::enforcePersistentStateLimit()
{
    while (persistentState.size() > kMaxPersistentStateEntries) {
        // Remove oldest entry
        persistentState.erase(persistentState.begin());
    }
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

    // Macro parameter validation: clamp to [0, 1] and ensure finite
    std::array<float, 8> safeMacros;
    for (size_t i = 0; i < 8; ++i) {
        float v = macros[i];
        if (!std::isfinite(v)) v = 0.0f;
        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;
        safeMacros[i] = v;
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
    if (program) ctx.functionRegistry = &program->program.functionRegistry;

    for (int s = 0; s < numSamples; ++s)
    {
        ctx.t    = (float) sampleCounter / (float) currentSampleRate;
        ctx.inL  = numChannels > 0 ? buffer.getSample (0, s) : 0.0f;
        ctx.inR  = numChannels > 1 ? buffer.getSample (1, s) : ctx.inL;
        ctx.outL = 0.0f;
        ctx.outR = 0.0f;
        ctx.locals.clear();

        for (const auto& stmt : program->program.statements)
        {
            if (stmt) stmt->execute(ctx);
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
