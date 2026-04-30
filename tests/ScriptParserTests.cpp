#include "ScriptParser.h"
#include "ScriptEngine.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace
{
std::filesystem::path examplesDirectory()
{
#if defined(EXAMPLES_DIR)
    return std::filesystem::path (EXAMPLES_DIR);
#else
    return std::filesystem::path ("examples");
#endif
}

std::string readTextFile (const std::filesystem::path& path)
{
    std::ifstream in (path);
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::array<float, 8> macroDefaultsFromSource (const std::string& source)
{
    std::array<float, 8> macros {};
    const std::regex re (R"(^\s*#\s*p([1-8])\s*=\s*([-+]?(?:\d+\.?\d*|\.\d+))\s*$)",
                         std::regex::icase);
    std::stringstream lines (source);
    std::string line;
    std::smatch match;

    while (std::getline (lines, line))
    {
        if (! std::regex_match (line, match, re))
            continue;

        const auto idx = (int) (match[1].str()[0] - '1');
        auto value = std::stof (match[2].str());
        macros[(size_t) idx] = std::clamp (value, 0.0f, 1.0f);
    }

    return macros;
}

struct RenderResult
{
    std::vector<float> dryL;
    std::vector<float> wetL;
    float diffRms { 0.0f };
    float outRms { 0.0f };
    float tailRms { 0.0f };
    float peak { 0.0f };
};

RenderResult renderExample (const std::string& source, std::array<float, 8> macros)
{
    constexpr double sampleRate = 44100.0;
    constexpr int activeSamples = 16384;
    constexpr int tailSamples = 2048;
    constexpr int totalSamples = activeSamples + tailSamples;
    constexpr int warmup = 1024;

    scripting::ScriptEngine engine;
    engine.reset (sampleRate);
    auto r = engine.compileAndInstall (juce::String (source));
    if (! r.ok)
        throw std::runtime_error ("compile failed: " + r.errors.joinIntoString ("\n").toStdString());

    juce::AudioBuffer<float> buf (2, totalSamples);
    RenderResult result;
    result.dryL.resize ((size_t) totalSamples);

    for (int s = 0; s < totalSamples; ++s)
    {
        float l = 0.0f;
        float rch = 0.0f;

        if (s < activeSamples)
        {
            const auto t = (float) s / (float) sampleRate;
            const auto burst = (s % 8192) < 4200 ? 1.0f : 0.42f;
            l = burst * (0.20f * std::sin (2.0f * 3.14159265f * 110.0f * t)
                       + 0.13f * std::sin (2.0f * 3.14159265f * 440.0f * t)
                       + 0.06f * std::sin (2.0f * 3.14159265f * 1760.0f * t));
            rch = burst * (0.18f * std::sin (2.0f * 3.14159265f * 123.0f * t)
                         + 0.12f * std::sin (2.0f * 3.14159265f * 391.0f * t)
                         + 0.05f * std::sin (2.0f * 3.14159265f * 1550.0f * t));
        }

        result.dryL[(size_t) s] = l;
        buf.setSample (0, s, l);
        buf.setSample (1, s, rch);
    }

    engine.processBlock (buf, macros);
    result.wetL.resize ((size_t) totalSamples);

    double diffEnergy = 0.0;
    double outEnergy = 0.0;
    double tailEnergy = 0.0;
    int diffCount = 0;

    for (int s = 0; s < totalSamples; ++s)
    {
        const auto out = buf.getSample (0, s);
        result.wetL[(size_t) s] = out;
        result.peak = std::max (result.peak, std::abs (out));

        if (s >= warmup && s < activeSamples)
        {
            const auto d = out - result.dryL[(size_t) s];
            diffEnergy += (double) d * (double) d;
            outEnergy += (double) out * (double) out;
            ++diffCount;
        }

        if (s >= activeSamples)
            tailEnergy += (double) out * (double) out;
    }

    result.diffRms = diffCount > 0 ? (float) std::sqrt (diffEnergy / (double) diffCount) : 0.0f;
    result.outRms = diffCount > 0 ? (float) std::sqrt (outEnergy / (double) diffCount) : 0.0f;
    result.tailRms = (float) std::sqrt (tailEnergy / (double) tailSamples);
    return result;
}

float rmsDifference (const std::vector<float>& a, const std::vector<float>& b)
{
    const auto n = std::min (a.size(), b.size());
    if (n == 0)
        return 0.0f;

    double energy = 0.0;
    for (size_t i = 0; i < n; ++i)
    {
        const auto d = a[i] - b[i];
        energy += (double) d * (double) d;
    }

    return (float) std::sqrt (energy / (double) n);
}

bool exampleMayHaveTail (const std::string& name)
{
    return name == "clean_doubler.ascr"
        || name == "chorus.ascr"
        || name == "ping_pong_delay.ascr"
        || name == "reverb.ascr"
        || name == "simple_delay.ascr";
}

int run()
{
    {
        scripting::ScriptParser parser;
        const auto result = parser.parse (R"(
outL = inL;
outR = inR;
)");

        if (! result.errors.isEmpty())
        {
            std::cerr << "Expected simple script to parse successfully.\n";
            std::cerr << result.errors.joinIntoString ("\n") << "\n";
            return 1;
        }
    }

    {
        scripting::ScriptParser parser;
        const auto result = parser.parse ("");
        if (result.errors.isEmpty())
        {
            std::cerr << "Expected empty script parse to fail.\n";
            return 1;
        }
    }

    {
        scripting::ScriptParser parser;
        const auto result = parser.parse (R"(
sum = 0.0;
for (i = 0.0; lt(i, 8.0); 1.0) {
    if (lt(i, 2.0)) {
        continue;
    }
    if (gt(i, 5.0)) {
        break;
    }
    sum = sum + i;
}
outL = sum * 0.1;
outR = outL;
)");

        if (! result.errors.isEmpty())
        {
            std::cerr << "Expected extended for-loop script to parse successfully.\n";
            std::cerr << result.errors.joinIntoString ("\n") << "\n";
            return 1;
        }
    }

    {
        scripting::ScriptParser parser;
        const auto result = parser.parse (R"(
fn ident(x) {
    return x;
}
outL = ident(inL);
outR = ident(inR);
)");

        if (! result.errors.isEmpty())
        {
            std::cerr << "Expected function script to parse successfully.\n";
            std::cerr << result.errors.joinIntoString ("\n") << "\n";
            return 1;
        }
    }

    {
        scripting::ScriptParser parser;
        const auto result = parser.parse (R"(
state_gain = 0.2 + p1 * 0.8;
outL = inL * state_gain;
outR = inR * state_gain;
)");

        if (! result.errors.isEmpty())
        {
            std::cerr << "Expected state/macro script to parse successfully.\n";
            std::cerr << result.errors.joinIntoString ("\n") << "\n";
            return 1;
        }
    }

    // Determine examples directory: prefer compile-time EXAMPLES_DIR (absolute),
    // otherwise fall back to a relative "examples" path.
    const auto examplesDir = examplesDirectory();

    if (! std::filesystem::exists (examplesDir))
    {
        std::cerr << "Examples directory not found: " << examplesDir.string() << " — skipping example tests.\n";
        return 0;
    }

    for (const auto& entry : std::filesystem::directory_iterator (examplesDir))
    {
        if (entry.path().extension() != ".ascr")
            continue;

        const auto source = readTextFile (entry.path());

        scripting::ScriptParser parser;
        const auto result = parser.parse (source);
        if (! result.errors.isEmpty())
        {
            std::cerr << "Expected example script to parse successfully: " << entry.path().string() << "\n";
            std::cerr << result.errors.joinIntoString ("\n") << "\n";
            return 1;
        }
    }

    return 0;
}

int runEngineTests()
{
    // --- gain: output must be 2× input ---
    {
        scripting::ScriptEngine engine;
        engine.reset (44100.0);
        auto r = engine.compileAndInstall ("outL = inL * 2.0; outR = inR * 2.0;");
        if (! r.ok)
        {
            std::cerr << "Engine gain test: compile failed.\n";
            return 1;
        }

        juce::AudioBuffer<float> buf (2, 64);
        for (int s = 0; s < 64; ++s)
        {
            buf.setSample (0, s, 0.5f);
            buf.setSample (1, s, -0.25f);
        }
        std::array<float, 8> macros {};
        engine.processBlock (buf, macros);

        for (int s = 0; s < 64; ++s)
        {
            if (std::abs (buf.getSample (0, s) - 1.0f) > 1e-5f ||
                std::abs (buf.getSample (1, s) - (-0.5f)) > 1e-5f)
            {
                std::cerr << "Engine gain test: wrong output at sample " << s << ": "
                          << buf.getSample (0, s) << " " << buf.getSample (1, s) << "\n";
                return 1;
            }
        }
    }

    // --- silence: outL/outR assignment to 0 produces silence ---
    {
        scripting::ScriptEngine engine;
        engine.reset (44100.0);
        auto r = engine.compileAndInstall ("outL = 0.0; outR = 0.0;");
        if (! r.ok)
        {
            std::cerr << "Engine silence test: compile failed.\n";
            return 1;
        }

        juce::AudioBuffer<float> buf (2, 64);
        for (int s = 0; s < 64; ++s)
        {
            buf.setSample (0, s, 0.8f);
            buf.setSample (1, s, -0.6f);
        }
        std::array<float, 8> macros {};
        engine.processBlock (buf, macros);

        for (int s = 0; s < 64; ++s)
        {
            if (std::abs (buf.getSample (0, s)) > 1e-5f || std::abs (buf.getSample (1, s)) > 1e-5f)
            {
                std::cerr << "Engine silence test: expected zero output at sample " << s << "\n";
                return 1;
            }
        }
    }

    // --- tanh: drive=2 → tanh(2)≈0.964, clearly below linear amp and below 1.0 ---
    {
        scripting::ScriptEngine engine;
        engine.reset (44100.0);
        // tanh(inL * 2.0): input 1.0 → tanh(2.0) ≈ 0.9640, not 2.0 and not 1.0
        auto r = engine.compileAndInstall ("outL = tanh(inL * 2.0); outR = tanh(inR * 2.0);");
        if (! r.ok)
        {
            std::cerr << "Engine tanh test: compile failed.\n";
            return 1;
        }

        juce::AudioBuffer<float> buf (2, 2);
        buf.setSample (0, 0,  1.0f); buf.setSample (1, 0,  1.0f);
        buf.setSample (0, 1, -1.0f); buf.setSample (1, 1, -1.0f);

        std::array<float, 8> macros {};
        engine.processBlock (buf, macros);

        // Output must be less than linear (2.0) and strictly less than 1.0
        const float out0 = buf.getSample (0, 0);
        if (out0 >= 1.0f || out0 < 0.9f)
        {
            std::cerr << "Engine tanh test: expected output in [0.9, 1.0), got " << out0 << "\n";
            return 1;
        }
        // Negative half must be symmetric
        if (std::abs (buf.getSample (0, 1) + out0) > 1e-4f)
        {
            std::cerr << "Engine tanh test: tanh not symmetric\n";
            return 1;
        }
    }

    // --- delay: 1-sample delay shifts signal by one sample ---
    {
        scripting::ScriptEngine engine;
        engine.reset (44100.0);
        auto r = engine.compileAndInstall ("outL = delay(inL, 1, 0); outR = delay(inR, 1, 1);");
        if (! r.ok)
        {
            std::cerr << "Engine delay test: compile failed.\n";
            return 1;
        }

        juce::AudioBuffer<float> buf (2, 3);
        buf.setSample (0, 0, 0.5f);  buf.setSample (1, 0, -0.3f);
        buf.setSample (0, 1, 0.0f);  buf.setSample (1, 1, 0.0f);
        buf.setSample (0, 2, 0.0f);  buf.setSample (1, 2, 0.0f);

        std::array<float, 8> macros {};
        engine.processBlock (buf, macros);

        if (std::abs (buf.getSample (0, 0)) > 1e-5f)
        {
            std::cerr << "Engine delay test: first sample should be 0 (buffer cold), got "
                      << buf.getSample (0, 0) << "\n";
            return 1;
        }
        if (std::abs (buf.getSample (0, 1) - 0.5f) > 1e-4f)
        {
            std::cerr << "Engine delay test: second sample should be 0.5 (delayed), got "
                      << buf.getSample (0, 1) << "\n";
            return 1;
        }
    }

    // --- delay: fractional times interpolate instead of zippering to integers ---
    {
        scripting::ScriptEngine engine;
        engine.reset (44100.0);
        auto r = engine.compileAndInstall ("outL = delay(inL, 1.5, 0); outR = outL;");
        if (! r.ok)
        {
            std::cerr << "Engine fractional delay test: compile failed.\n";
            return 1;
        }

        juce::AudioBuffer<float> buf (2, 4);
        buf.clear();
        buf.setSample (0, 0, 1.0f);

        std::array<float, 8> macros {};
        engine.processBlock (buf, macros);

        if (std::abs (buf.getSample (0, 1) - 0.5f) > 1e-4f
            || std::abs (buf.getSample (0, 2) - 0.5f) > 1e-4f)
        {
            std::cerr << "Engine fractional delay test: expected split impulse at samples 1/2, got "
                      << buf.getSample (0, 1) << " " << buf.getSample (0, 2) << "\n";
            return 1;
        }
    }

    // --- lpf1: a one-pole low-pass smooths a step from 0 to 1 ---
    {
        scripting::ScriptEngine engine;
        engine.reset (44100.0);
        // coeff = 0.5 → half-way each sample; 8 samples should be well above 0
        auto r = engine.compileAndInstall ("outL = lpf1(inL, 0.5, 0); outR = outL;");
        if (! r.ok)
        {
            std::cerr << "Engine lpf1 test: compile failed.\n";
            return 1;
        }

        juce::AudioBuffer<float> buf (2, 8);
        for (int s = 0; s < 8; ++s)
            buf.setSample (0, s, 1.0f);
        buf.setSample (1, 0, 1.0f);

        std::array<float, 8> macros {};
        engine.processBlock (buf, macros);

        // After 1 sample: 0.5. After 8 samples: 1-(0.5^8)≈0.996. Must be rising.
        if (buf.getSample (0, 0) >= buf.getSample (0, 7) || buf.getSample (0, 0) <= 0.0f)
        {
            std::cerr << "Engine lpf1 test: expected rising ramp, got "
                      << buf.getSample (0, 0) << " → " << buf.getSample (0, 7) << "\n";
            return 1;
        }
        if (buf.getSample (0, 7) < 0.99f)
        {
            std::cerr << "Engine lpf1 test: after 8 samples value should be near 1, got "
                      << buf.getSample (0, 7) << "\n";
            return 1;
        }
    }

    // --- runaway scripts: aborted samples fall back to dry passthrough ---
    {
        scripting::ScriptEngine engine;
        engine.reset (44100.0);
        auto r = engine.compileAndInstall ("outL = 0.0; outR = 0.0; while (true) { state_spin = state_spin + 1.0; }");
        if (! r.ok)
        {
            std::cerr << "Engine abort fallback test: compile failed.\n";
            return 1;
        }

        juce::AudioBuffer<float> buf (2, 1);
        buf.setSample (0, 0, 0.25f);
        buf.setSample (1, 0, -0.5f);

        std::array<float, 8> macros {};
        engine.processBlock (buf, macros);

        if (std::abs (buf.getSample (0, 0) - 0.25f) > 1e-5f
            || std::abs (buf.getSample (1, 0) + 0.5f) > 1e-5f)
        {
            std::cerr << "Engine abort fallback test: expected dry passthrough, got "
                      << buf.getSample (0, 0) << " " << buf.getSample (1, 0) << "\n";
            return 1;
        }
    }

    // --- user functions: locals are call-scoped, state_ remains shared DSP state ---
    {
        scripting::ScriptEngine engine;
        engine.reset (44100.0);
        auto r = engine.compileAndInstall (R"(
x = 0.75;
fn readOuter() {
    return x;
}
fn bump(v) {
    state_sum = state_sum + v;
    return state_sum;
}
outL = readOuter();
outR = bump(0.25);
)");
        if (! r.ok)
        {
            std::cerr << "Engine function scope test: compile failed.\n";
            return 1;
        }

        juce::AudioBuffer<float> buf (2, 2);
        buf.clear();
        std::array<float, 8> macros {};
        engine.processBlock (buf, macros);

        if (std::abs (buf.getSample (0, 0)) > 1e-5f
            || std::abs (buf.getSample (1, 0) - 0.25f) > 1e-5f
            || std::abs (buf.getSample (1, 1) - 0.5f) > 1e-5f)
        {
            std::cerr << "Engine function scope test: unexpected output "
                      << buf.getSample (0, 0) << " "
                      << buf.getSample (1, 0) << " "
                      << buf.getSample (1, 1) << "\n";
            return 1;
        }
    }

    // --- reset: runtime state and sample clock are swapped/reset on the audio path ---
    {
        scripting::ScriptEngine engine;
        engine.reset (44100.0);
        auto r = engine.compileAndInstall ("state_acc = state_acc + 1.0; outL = state_acc; outR = t;");
        if (! r.ok)
        {
            std::cerr << "Engine reset test: compile failed.\n";
            return 1;
        }

        juce::AudioBuffer<float> buf (2, 3);
        buf.clear();
        std::array<float, 8> macros {};
        engine.processBlock (buf, macros);

        if (std::abs (buf.getSample (0, 2) - 3.0f) > 1e-5f)
        {
            std::cerr << "Engine reset test: expected accumulated state before reset, got "
                      << buf.getSample (0, 2) << "\n";
            return 1;
        }

        engine.reset (48000.0);

        juce::AudioBuffer<float> resetBuf (2, 1);
        resetBuf.clear();
        engine.processBlock (resetBuf, macros);

        if (std::abs (resetBuf.getSample (0, 0) - 1.0f) > 1e-5f)
        {
            std::cerr << "Engine reset test: expected state to restart at 1 after reset, got "
                      << resetBuf.getSample (0, 0) << "\n";
            return 1;
        }

        if (std::abs (resetBuf.getSample (1, 0)) > 1e-5f)
        {
            std::cerr << "Engine reset test: expected sample clock to restart at 0 after reset, got "
                      << resetBuf.getSample (1, 0) << "\n";
            return 1;
        }
    }

    // --- example scripts: compile, execute without crash, produce output ---
    const auto examplesDir = examplesDirectory();

    if (std::filesystem::exists (examplesDir))
    {
        int fileExampleCount = 0;

        for (const auto& entry : std::filesystem::directory_iterator (examplesDir))
        {
            if (entry.path().extension() != ".ascr")
                continue;

            ++fileExampleCount;
            const std::string src = readTextFile (entry.path());

            scripting::ScriptEngine engine;
            engine.reset (44100.0);
            auto r = engine.compileAndInstall (juce::String (src));
            if (! r.ok)
            {
                std::cerr << "Engine example test: compile failed for "
                          << entry.path().filename().string() << "\n"
                          << r.errors.joinIntoString ("\n") << "\n";
                return 1;
            }

            // Feed 512 samples of a 440 Hz sine wave at -12 dBFS
            constexpr int kN = 512;
            constexpr float kAmp = 0.25f;
            juce::AudioBuffer<float> buf (2, kN);
            for (int s = 0; s < kN; ++s)
            {
                const float v = kAmp * std::sin (2.0f * 3.14159265f * 440.0f * (float) s / 44100.0f);
                buf.setSample (0, s, v);
                buf.setSample (1, s, v);
            }

            std::array<float, 8> macros {};
            macros[0] = 0.5f; macros[1] = 0.5f; macros[2] = 0.5f;

            engine.processBlock (buf, macros);

            // Verify no NaN/Inf in output
            for (int s = 0; s < kN; ++s)
            {
                if (! std::isfinite (buf.getSample (0, s)) || ! std::isfinite (buf.getSample (1, s)))
                {
                    std::cerr << "Engine example test: NaN/Inf output in "
                              << entry.path().filename().string() << " at sample " << s << "\n";
                    return 1;
                }
            }
        }

        const auto embeddedNames = scripting::exampleNames();
        if (embeddedNames.size() != fileExampleCount)
        {
            std::cerr << "Engine example behavior test: embedded example count "
                      << embeddedNames.size() << " does not match file count "
                      << fileExampleCount << "\n";
            return 1;
        }

        for (int i = 0; i < embeddedNames.size(); ++i)
        {
            scripting::ScriptEngine engine;
            engine.reset (44100.0);
            const auto r = engine.compileAndInstall (scripting::exampleScript (i));
            if (! r.ok)
            {
                std::cerr << "Engine embedded example test: compile failed for "
                          << embeddedNames[i] << "\n"
                          << r.errors.joinIntoString ("\n") << "\n";
                return 1;
            }
        }

        std::vector<std::pair<std::string, RenderResult>> renders;

        for (const auto& entry : std::filesystem::directory_iterator (examplesDir))
        {
            if (entry.path().extension() != ".ascr")
                continue;

            const auto name = entry.path().filename().string();
            const auto path = examplesDir / name;
            if (! std::filesystem::exists (path))
            {
                std::cerr << "Engine example behavior test: missing " << name << "\n";
                return 1;
            }

            const auto source = readTextFile (path);
            auto rendered = renderExample (source, macroDefaultsFromSource (source));

            if (rendered.outRms < 0.015f)
            {
                std::cerr << "Engine example behavior test: " << name
                          << " output is unexpectedly quiet; rms=" << rendered.outRms << "\n";
                return 1;
            }

            if (rendered.diffRms < 0.006f)
            {
                std::cerr << "Engine example behavior test: " << name
                          << " is too close to dry signal; diff rms=" << rendered.diffRms << "\n";
                return 1;
            }

            if (! exampleMayHaveTail (name) && rendered.tailRms > 0.012f)
            {
                std::cerr << "Engine example behavior test: " << name
                          << " leaves a non-delay tail; tail rms=" << rendered.tailRms << "\n";
                return 1;
            }

            if (rendered.peak > 1.25f)
            {
                std::cerr << "Engine example behavior test: " << name
                          << " peak is too hot; peak=" << rendered.peak << "\n";
                return 1;
            }

            renders.push_back ({ name, std::move (rendered) });
        }

        for (size_t i = 0; i < renders.size(); ++i)
        {
            for (size_t j = i + 1; j < renders.size(); ++j)
            {
                const auto distance = rmsDifference (renders[i].second.wetL, renders[j].second.wetL);
                if (distance < 0.004f)
                {
                    std::cerr << "Engine example behavior test: "
                              << renders[i].first << " and " << renders[j].first
                              << " render too similarly; rms distance=" << distance << "\n";
                    return 1;
                }
            }
        }
    }

    return 0;
}
}

int main()
{
    if (int r = run(); r != 0)
        return r;
    return runEngineTests();
}
