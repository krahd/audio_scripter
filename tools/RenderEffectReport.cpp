#include "ScriptEngine.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
constexpr double kSampleRate = 44100.0;
constexpr int kProgramActiveSamples = 44100 * 4;
constexpr int kProgramTailSamples = 44100 * 2;
constexpr int kImpulseSamples = 44100 * 3;
constexpr float kPi = 3.14159265358979323846f;

struct Metrics
{
    float rms { 0.0f };
    float peak { 0.0f };
    float diffRms { 0.0f };
    float tailRms { 0.0f };
    float roughness { 0.0f };
    float highBandRatio { 0.0f };
    float zeroCrossingRate { 0.0f };
    float dcOffset { 0.0f };
};

struct RenderedExample
{
    std::string fileName;
    Metrics program;
    Metrics impulse;
};

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
        const auto value = std::stof (match[2].str());
        macros[(size_t) idx] = std::clamp (value, 0.0f, 1.0f);
    }

    return macros;
}

float rms (const std::vector<float>& values, int start, int end)
{
    start = std::clamp (start, 0, (int) values.size());
    end = std::clamp (end, start, (int) values.size());

    if (end <= start)
        return 0.0f;

    double energy = 0.0;
    for (int i = start; i < end; ++i)
        energy += (double) values[(size_t) i] * (double) values[(size_t) i];

    return (float) std::sqrt (energy / (double) (end - start));
}

Metrics analyse (const std::vector<float>& wet, const std::vector<float>& dry, int activeSamples)
{
    Metrics m;
    const auto n = std::min (wet.size(), dry.size());
    if (n == 0)
        return m;

    double energy = 0.0;
    double diffEnergy = 0.0;
    double roughEnergy = 0.0;
    double dc = 0.0;
    double highEnergy = 0.0;
    double totalEnergy = 0.0;
    float low = 0.0f;
    int zeroCrossings = 0;

    for (size_t i = 0; i < n; ++i)
    {
        const auto y = wet[i];
        const auto d = y - dry[i];
        m.peak = std::max (m.peak, std::abs (y));
        energy += (double) y * (double) y;
        diffEnergy += (double) d * (double) d;
        dc += y;

        if (i > 0)
        {
            const auto dy = y - wet[i - 1];
            roughEnergy += (double) dy * (double) dy;
            if ((y >= 0.0f) != (wet[i - 1] >= 0.0f))
                ++zeroCrossings;
        }

        low += 0.08f * (y - low);
        const auto high = y - low;
        highEnergy += (double) high * (double) high;
        totalEnergy += (double) y * (double) y;
    }

    m.rms = (float) std::sqrt (energy / (double) n);
    m.diffRms = (float) std::sqrt (diffEnergy / (double) n);
    m.tailRms = rms (wet, activeSamples, (int) n);
    m.roughness = m.rms > 1.0e-9f ? (float) std::sqrt (roughEnergy / (double) n) / m.rms : 0.0f;
    m.highBandRatio = totalEnergy > 1.0e-12 ? (float) std::sqrt (highEnergy / totalEnergy) : 0.0f;
    m.zeroCrossingRate = (float) zeroCrossings / (float) n;
    m.dcOffset = (float) (dc / (double) n);
    return m;
}

juce::AudioBuffer<float> makeProgramInput()
{
    juce::AudioBuffer<float> buffer (2, kProgramActiveSamples + kProgramTailSamples);
    buffer.clear();

    for (int s = 0; s < kProgramActiveSamples; ++s)
    {
        const auto t = (float) s / (float) kSampleRate;
        const auto beat = std::fmod (t * 2.0f, 1.0f);
        const auto transient = beat < 0.035f ? std::exp (-beat * 95.0f) : 0.0f;
        const auto gate = (std::fmod (t * 0.5f, 1.0f) < 0.58f) ? 1.0f : 0.58f;

        const auto l = gate * (0.18f * std::sin (2.0f * kPi * 110.0f * t)
                             + 0.12f * std::sin (2.0f * kPi * 330.0f * t)
                             + 0.07f * std::sin (2.0f * kPi * 990.0f * t))
                     + 0.20f * transient;
        const auto r = gate * (0.17f * std::sin (2.0f * kPi * 123.0f * t)
                             + 0.11f * std::sin (2.0f * kPi * 369.0f * t)
                             + 0.06f * std::sin (2.0f * kPi * 861.0f * t))
                     + 0.16f * transient;

        buffer.setSample (0, s, l);
        buffer.setSample (1, s, r);
    }

    return buffer;
}

juce::AudioBuffer<float> makeImpulseInput()
{
    juce::AudioBuffer<float> buffer (2, kImpulseSamples);
    buffer.clear();
    buffer.setSample (0, 0, 0.95f);
    buffer.setSample (1, 0, 0.75f);
    return buffer;
}

std::vector<float> channelToVector (const juce::AudioBuffer<float>& buffer, int channel)
{
    std::vector<float> values ((size_t) buffer.getNumSamples());
    for (int s = 0; s < buffer.getNumSamples(); ++s)
        values[(size_t) s] = buffer.getSample (channel, s);
    return values;
}

juce::AudioBuffer<float> render (const std::string& source,
                                 std::array<float, 8> macros,
                                 const juce::AudioBuffer<float>& input)
{
    scripting::ScriptEngine engine;
    engine.reset (kSampleRate);

    const auto result = engine.compileAndInstall (juce::String (source));
    if (! result.ok)
        throw std::runtime_error (result.errors.joinIntoString ("\n").toStdString());

    juce::AudioBuffer<float> output;
    output.makeCopyOf (input);
    engine.processBlock (output, macros);
    return output;
}

void writeWav (const std::filesystem::path& path, const juce::AudioBuffer<float>& buffer)
{
    juce::WavAudioFormat format;
    juce::File file (path.string());
    file.deleteFile();

    auto stream = std::make_unique<juce::FileOutputStream> (file);
    if (! stream->openedOk())
        throw std::runtime_error ("Could not open " + path.string());

    std::unique_ptr<juce::AudioFormatWriter> writer (
        format.createWriterFor (stream.get(), kSampleRate, (unsigned int) buffer.getNumChannels(), 24, {}, 0));

    if (writer == nullptr)
        throw std::runtime_error ("Could not create WAV writer for " + path.string());

    stream.release();
    writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
}

std::string fixed (float value, int precision = 4)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision (precision) << value;
    return out.str();
}

std::string waveformSvg (const std::vector<float>& values)
{
    constexpr int width = 520;
    constexpr int height = 86;
    constexpr int points = 260;

    std::ostringstream svg;
    svg << "<svg viewBox=\"0 0 " << width << " " << height << "\" width=\"" << width
        << "\" height=\"" << height << "\" role=\"img\">";
    svg << "<rect width=\"100%\" height=\"100%\" fill=\"#10151b\"/>";
    svg << "<line x1=\"0\" y1=\"" << (height / 2) << "\" x2=\"" << width << "\" y2=\""
        << (height / 2) << "\" stroke=\"#2d4858\"/>";
    svg << "<polyline fill=\"none\" stroke=\"#4ec9b0\" stroke-width=\"1.3\" points=\"";

    for (int i = 0; i < points; ++i)
    {
        const auto index = (size_t) ((double) i / (double) (points - 1)
                                    * (double) std::max<size_t> (1, values.size() - 1));
        const auto x = (double) i / (double) (points - 1) * width;
        const auto y = (height * 0.5) - std::clamp (values[index], -1.0f, 1.0f) * (height * 0.42f);
        svg << fixed ((float) x, 2) << "," << fixed ((float) y, 2) << " ";
    }

    svg << "\"/></svg>";
    return svg.str();
}

void writeReport (const std::filesystem::path& outputDir, const std::vector<RenderedExample>& examples)
{
    {
        std::ofstream csv (outputDir / "metrics.csv");
        csv << "example,render,rms,peak,diff_rms,tail_rms,roughness,high_band_ratio,zero_crossing_rate,dc_offset\n";
        for (const auto& e : examples)
        {
            for (const auto& [kind, m] : { std::pair<const char*, Metrics> { "program", e.program },
                                           std::pair<const char*, Metrics> { "impulse", e.impulse } })
            {
                csv << e.fileName << "," << kind << ","
                    << fixed (m.rms) << "," << fixed (m.peak) << "," << fixed (m.diffRms) << ","
                    << fixed (m.tailRms) << "," << fixed (m.roughness) << ","
                    << fixed (m.highBandRatio) << "," << fixed (m.zeroCrossingRate) << ","
                    << fixed (m.dcOffset) << "\n";
            }
        }
    }

    std::ofstream html (outputDir / "index.html");
    html << "<!doctype html><meta charset=\"utf-8\"><title>audio_scripter effect report</title>";
    html << "<style>body{font-family:system-ui;background:#161a20;color:#d4d4d4;margin:24px}"
            "table{border-collapse:collapse;width:100%;font-size:13px}td,th{border-bottom:1px solid #2d4858;padding:8px;vertical-align:top}"
            "th{text-align:left;color:#8a9aaa}.name{font-weight:700;color:#4ec9b0}audio{width:260px}</style>";
    html << "<h1>audio_scripter effect report</h1>";
    html << "<p>Deterministic offline renders using the same ScriptEngine as the plugin. "
            "Use the WAV players to listen and the waveforms/metrics to spot noise, broken tails, or accidental delay.</p>";
    html << "<table><thead><tr><th>Example</th><th>Program render</th><th>Impulse render</th><th>Metrics</th></tr></thead><tbody>";

    for (const auto& e : examples)
    {
        const auto stem = std::filesystem::path (e.fileName).stem().string();
        const auto programWav = "program/" + stem + ".wav";
        const auto impulseWav = "impulse/" + stem + ".wav";

        html << "<tr><td class=\"name\">" << e.fileName << "</td>";
        html << "<td><audio controls src=\"" << programWav << "\"></audio><br>"
             << "<img src=\"program/" << stem << ".svg\" alt=\"program waveform\"></td>";
        html << "<td><audio controls src=\"" << impulseWav << "\"></audio><br>"
             << "<img src=\"impulse/" << stem << ".svg\" alt=\"impulse waveform\"></td>";
        html << "<td><b>Program</b><br>rms " << fixed (e.program.rms)
             << " peak " << fixed (e.program.peak)
             << " rough " << fixed (e.program.roughness)
             << " high " << fixed (e.program.highBandRatio)
             << " tail " << fixed (e.program.tailRms)
             << "<br><b>Impulse</b><br>rms " << fixed (e.impulse.rms)
             << " peak " << fixed (e.impulse.peak)
             << " rough " << fixed (e.impulse.roughness)
             << " high " << fixed (e.impulse.highBandRatio)
             << " tail " << fixed (e.impulse.tailRms) << "</td></tr>";
    }

    html << "</tbody></table>";
}

void writeSvgFile (const std::filesystem::path& path, const std::vector<float>& values)
{
    std::ofstream out (path);
    out << waveformSvg (values);
}
} // namespace

int main (int argc, char** argv)
{
    try
    {
        const auto outputDir = argc > 1 ? std::filesystem::path (argv[1])
                                        : std::filesystem::path ("build/effect_report");
        const auto examplesDir = argc > 2 ? std::filesystem::path (argv[2])
                                          : examplesDirectory();

        std::filesystem::create_directories (outputDir / "program");
        std::filesystem::create_directories (outputDir / "impulse");

        const auto dryProgram = makeProgramInput();
        const auto dryImpulse = makeImpulseInput();
        writeWav (outputDir / "dry_program.wav", dryProgram);
        writeWav (outputDir / "dry_impulse.wav", dryImpulse);

        std::vector<RenderedExample> rendered;

        for (const auto& entry : std::filesystem::directory_iterator (examplesDir))
        {
            if (entry.path().extension() != ".ascr")
                continue;

            const auto source = readTextFile (entry.path());
            const auto macros = macroDefaultsFromSource (source);
            const auto stem = entry.path().stem().string();

            const auto program = render (source, macros, dryProgram);
            const auto impulse = render (source, macros, dryImpulse);
            const auto dryProgramL = channelToVector (dryProgram, 0);
            const auto dryImpulseL = channelToVector (dryImpulse, 0);
            const auto programL = channelToVector (program, 0);
            const auto impulseL = channelToVector (impulse, 0);

            writeWav (outputDir / "program" / (stem + ".wav"), program);
            writeWav (outputDir / "impulse" / (stem + ".wav"), impulse);
            writeSvgFile (outputDir / "program" / (stem + ".svg"), programL);
            writeSvgFile (outputDir / "impulse" / (stem + ".svg"), impulseL);

            rendered.push_back ({ entry.path().filename().string(),
                                  analyse (programL, dryProgramL, kProgramActiveSamples),
                                  analyse (impulseL, dryImpulseL, 1) });
        }

        std::sort (rendered.begin(), rendered.end(), [] (const auto& a, const auto& b)
        {
            return a.fileName < b.fileName;
        });

        writeReport (outputDir, rendered);
        std::cout << "Rendered " << rendered.size() << " examples to " << outputDir << "\n";
        std::cout << "Open " << (outputDir / "index.html") << " to inspect WAVs and waveforms.\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << "Render report failed: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
