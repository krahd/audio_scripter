#include "ScriptParser.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace
{
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

    const std::filesystem::path examplesDir = std::filesystem::path ("examples");
    for (const auto& entry : std::filesystem::directory_iterator (examplesDir))
    {
        if (entry.path().extension() != ".ascr")
            continue;

        std::ifstream in (entry.path());
        std::stringstream buffer;
        buffer << in.rdbuf();

        scripting::ScriptParser parser;
        const auto result = parser.parse (buffer.str());
        if (! result.errors.isEmpty())
        {
            std::cerr << "Expected example script to parse successfully: " << entry.path().string() << "\n";
            std::cerr << result.errors.joinIntoString ("\n") << "\n";
            return 1;
        }
    }

    return 0;
}
}

int main()
{
    return run();
}
