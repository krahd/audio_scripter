#include "ScriptEngine.h"

namespace scripting
{
ScriptEngine::CompileResult ScriptEngine::compileAndInstall (const juce::String& source)
{
    ScriptParser parser;
    auto result = parser.parse (source);

    if (! result.errors.isEmpty())
        return { false, result.errors };

    auto compiled = std::make_shared<CompiledProgram>();
    compiled->program = std::move (result.program);
    compiled->source = source;

    std::atomic_store (&activeProgram, std::shared_ptr<const CompiledProgram> (compiled));
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
    return std::atomic_load (&activeProgram);
}

void ScriptEngine::processBlock (juce::AudioBuffer<float>& buffer, const std::array<float, 8>& macros)
{
    if (stateResetRequested.exchange (false))
    {
        persistentState.clear();
        sampleCounter = 0;
    }

    const auto program = getProgramSnapshot();
    if (program == nullptr)
        return;

    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();

    for (int s = 0; s < numSamples; ++s)
    {
        EvalContext ctx;
        ctx.inL = numChannels > 0 ? buffer.getSample (0, s) : 0.0f;
        ctx.inR = numChannels > 1 ? buffer.getSample (1, s) : ctx.inL;
        ctx.outL = ctx.inL;
        ctx.outR = ctx.inR;
        ctx.sr = (float) currentSampleRate;
        ctx.t = (float) (sampleCounter / currentSampleRate);
        ctx.persistentState = &persistentState;
        ctx.macros = &macros;

        for (const auto& st : program->program.statements)
        {
            const auto value = st.expression->evaluate (ctx);
            ctx.setValue (st.variableName, value);
        }

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
        "Noisy transient gate"
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
- lpf1(x, coeff [, id])      # one-pole low-pass (stateful)
- slew(target, speed [, id]) # slew limiter (stateful)

State variables
- Any variable starting with state_ is persistent across samples.
- Optional function id lets you keep separate states per channel.
)";
}
} // namespace scripting
