// Real-time-safety harness for audio_scripter.
//
// Overrides global new/delete with an armable counter, then measures heap
// activity during steady-state ScriptEngine::processBlock calls (after warm-up).
// The central design claim is that the audio path is allocation-free once a
// program is installed and warmed up; this test makes that claim measurable.

#include "ScriptEngine.h"
#include "AudioScripterExampleData.h"

#include <atomic>
#include <cstdlib>
#include <cstdio>
#include <new>
#include <string>
#include <vector>

namespace
{
std::atomic<long> g_allocCount { 0 };
std::atomic<bool> g_armed { false };

inline void countIfArmed()
{
    if (g_armed.load (std::memory_order_relaxed))
        g_allocCount.fetch_add (1, std::memory_order_relaxed);
}
} // namespace

void* operator new (std::size_t n)
{
    countIfArmed();
    if (void* p = std::malloc (n != 0 ? n : 1))
        return p;
    throw std::bad_alloc();
}

void* operator new[] (std::size_t n)
{
    countIfArmed();
    if (void* p = std::malloc (n != 0 ? n : 1))
        return p;
    throw std::bad_alloc();
}

void operator delete (void* p) noexcept { std::free (p); }
void operator delete[] (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept { std::free (p); }

namespace
{
constexpr int kBlockSize = 256;
constexpr int kWarmupBlocks = 16;
constexpr int kMeasuredBlocks = 8;
constexpr double kSampleRate = 44100.0;

void fillSine (juce::AudioBuffer<float>& buf)
{
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        for (int s = 0; s < buf.getNumSamples(); ++s)
            buf.setSample (ch, s, 0.25f * std::sin (2.0f * 3.14159265f * 440.0f * (float) s / (float) kSampleRate));
}

// Returns the number of heap allocations observed across kMeasuredBlocks
// steady-state blocks (after kWarmupBlocks warm-up blocks).
long measureSteadyStateAllocs (const juce::String& source, bool& compiledOk)
{
    scripting::ScriptEngine engine;
    engine.reset (kSampleRate);
    const auto r = engine.compileAndInstall (source);
    compiledOk = r.ok;
    if (! r.ok)
        return -1;

    juce::AudioBuffer<float> buf (2, kBlockSize);
    std::array<float, 8> macros {};
    macros[0] = 0.5f; macros[1] = 0.5f; macros[2] = 0.5f; macros[3] = 0.5f;

    for (int b = 0; b < kWarmupBlocks; ++b)
    {
        fillSine (buf);
        engine.processBlock (buf, macros);
    }

    g_allocCount.store (0, std::memory_order_relaxed);
    g_armed.store (true, std::memory_order_relaxed);
    for (int b = 0; b < kMeasuredBlocks; ++b)
    {
        fillSine (buf);
        engine.processBlock (buf, macros);
    }
    g_armed.store (false, std::memory_order_relaxed);

    return g_allocCount.load (std::memory_order_relaxed);
}

struct EmbeddedScript { std::string name; juce::String source; };

std::vector<EmbeddedScript> embeddedScripts()
{
    std::vector<EmbeddedScript> out;
    const auto names = scripting::exampleNames();
    for (int i = 0; i < names.size(); ++i)
        out.push_back ({ names[i].toStdString(), scripting::exampleScript (i) });
    return out;
}
} // namespace

int main()
{
    int failures = 0;
    long worst = 0;

    for (const auto& s : embeddedScripts())
    {
        bool ok = false;
        const auto allocs = measureSteadyStateAllocs (s.source, ok);
        if (! ok)
        {
            std::printf ("  [compile-fail] %s\n", s.name.c_str());
            ++failures;
            continue;
        }

        const char* tag = allocs == 0 ? "ok " : "ALLOC";
        std::printf ("  [%s] %-28s %ld allocs / %d blocks\n", tag, s.name.c_str(), allocs, kMeasuredBlocks);
        if (allocs > 0)
        {
            ++failures;
            worst = std::max (worst, allocs);
        }
    }

    if (failures != 0)
    {
        std::printf ("\nRT-safety: %d example(s) allocate on the audio thread (worst %ld). FAIL\n", failures, worst);
        return 1;
    }

    std::printf ("\nRT-safety: all examples are allocation-free in steady state. PASS\n");
    return 0;
}
