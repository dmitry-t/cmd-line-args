// Command line argument parser
//
// Copyright 2020, Dmitry Tarakanov
// SPDX-License-Identifier: MIT
//
#include "over9000/cmd_line_args/parser.h"

#include <iostream>
#include <type_traits>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif // _WIN32

enum class Enum
{
    VALUE0,
    VALUE1,
    VALUE2,
};

std::ostream& operator<<(std::ostream& lhs, Enum rhs)
{
    switch (rhs)
    {
    case Enum::VALUE0:
        lhs << "VALUE0";
        break;

    case Enum::VALUE1:
        lhs << "VALUE1";
        break;

    case Enum::VALUE2:
        lhs << "VALUE2";
        break;

    default:
        lhs << "Unknown(" << static_cast<int>(rhs) << ")";
    }
    return lhs;
}

template<class T>
void dump(const T& value)
{
#ifdef _WIN32
    std::wcout << std::boolalpha << value;
#else
    std::cout << std::boolalpha << value;
#endif // _WIN32
}

#ifdef _WIN32

void dump(const std::string& value)
{
    std::string string(value);
    std::wcout << std::wstring(string.begin(), string.end());
}

void dump(const char* value)
{
    dump(std::string(value));
}

void dump(Enum value)
{
    std::ostringstream stream;
    stream << value;
    dump(stream.str());
}

#endif // _WIN32

template<class T>
void dump(const std::string& title, const T& value)
{
    dump(title);
    dump(": ");
    dump(value);
    dump("\n");
}

template<class T>
void dump(const std::string& title, const std::vector<T>& values)
{
    dump(title);
    dump(": [");
    std::string delimiter = "";
    for (const auto& value : values)
    {
        dump(delimiter);
        delimiter = ", ";
        dump(value);
    }
    dump("]\n");
}

#ifdef _WIN32
int wmain(int argc, const wchar_t* argv[]) try
#else
int main(int argc, const char* argv[]) try
#endif //_WIN32
{
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stderr), _O_U16TEXT);
#endif

    over9000::cmd_line_args::Parser parser("Sample program");

    bool flag = false;
    parser.addFlag(flag, "flag", 'f', "Flag");

    std::basic_string<over9000::cmd_line_args::Char> string;
    parser.addParam(string, "string", "String");

    std::string ascii;
    parser.addParam(ascii, "ascii", "ASCII string");

    int integer = 0;
    parser.addParam(integer, "integer", "Integer");

    Enum enumeration = Enum::VALUE0;
    parser.addParam(enumeration, "enum", "Enumeration",
                    {{"value1", Enum::VALUE1}, {"value2", Enum::VALUE2}});

    std::string optString;
    parser.addParam(optString, "optString", "Optional string", over9000::cmd_line_args::OPTIONAL);

    int optInteger = 0;
    parser.addParam(optInteger, "optInteger", "Optional integer",
                    over9000::cmd_line_args::OPTIONAL);

    Enum optEnumeration = Enum::VALUE0;
    parser.addParam(optEnumeration, "optEnum", "Optional enumeration",
                    {{"value1", Enum::VALUE1}, {"value2", Enum::VALUE2}},
                    over9000::cmd_line_args::OPTIONAL);

    std::vector<std::string> strings;
    parser.addParam(strings, "strings", 's', "Strings");

    std::vector<int> integers;
    parser.addParam(integers, "integers", 'i', "Integers");

    std::vector<Enum> enumerations;
    parser.addParam(enumerations, "enums", 'e', "Enumerations",
                    {{"value1", Enum::VALUE1}, {"value2", Enum::VALUE2}});

    std::vector<std::string> optStrings;
    parser.addParam(optStrings, "optStrings", "Optional string", over9000::cmd_line_args::OPTIONAL);

    std::vector<int> optIntegers;
    parser.addParam(optIntegers, "optIntegers", "Optional integers",
                    over9000::cmd_line_args::OPTIONAL);

    std::vector<Enum> optEnumerations;
    parser.addParam(optEnumerations, "optEnums", "Optional enumerations",
                    {{"value1", Enum::VALUE1}, {"value2", Enum::VALUE2}},
                    over9000::cmd_line_args::OPTIONAL);

    std::string positionalString;
    parser.addPositional(positionalString, "posString", "Positional string");

    int positionalInteger = 0;
    parser.addPositional(positionalInteger, "posInteger", "Positional integer");

    std::vector<Enum> optionalPositionalEnumerations;
    parser.addPositional(optionalPositionalEnumerations, "optPosEnums", "Optional positional enumerations",
                         {{"value1", Enum::VALUE1}, {"value2", Enum::VALUE2}},
                         over9000::cmd_line_args::OPTIONAL);

#ifdef _WIN32
    parser.printHelp(std::wcerr);
#else
    parser.printHelp(std::cerr);
#endif

    parser.parse(argc, argv);

    dump("Flag", flag);
    dump("String", string);
    dump("ASCII string", ascii);
    dump("Integer", integer);
    dump("Enumeration", enumeration);
    dump("Optional string", optString);
    dump("Optional integer", optInteger);
    dump("Optional enumeration", optEnumeration);
    dump("Strings", strings);
    dump("Integers", integers);
    dump("Enumerations", enumerations);
    dump("Optional strings", optStrings);
    dump("Optional integers", optIntegers);
    dump("Optional enumerations", optEnumerations);
    dump("Positional string", positionalString);
    dump("Positional integer", positionalInteger);
    dump("Optional positional enumerations", optionalPositionalEnumerations);

    // E.g. the command line

    // cmd-line-args-sample -f --integer 1 --string="A B C" --ascii "a b c" --enum value1 \
    //     --strings=a -s b -s c --integers 1 -i 2 --enums value1 -e value2 \
    //     --optString --optString --optInteger -1 --optEnum=value2 \
    //     --optStrings a --optStrings b --optIntegers=9000 --optEnums value1 \
    //     --posStr=OK -7 value1 value2

    // should result in the output:

    // Flag: true
    // String: A B C
    // ASCII string: a b c
    // Integer: 1
    // Enumeration: VALUE1
    // Optional string: --optString
    // Optional integer: -1
    // Optional enumeration: VALUE2
    // Strings: [a, b, c]
    // Integers: [1, 2]
    // Enumerations: [VALUE1, VALUE2]
    // Optional strings: [a, b]
    // Optional integers: [9000]
    // Optional enumerations: [VALUE1]
    // Positional string: --posStr=OK
    // Positional integer: -7
    // Positional enumerations: [VALUE1, VALUE2]
}
catch (const over9000::cmd_line_args::Error& e)
{
#ifdef _WIN32
    std::wcerr << e << std::endl;
#else
    std::cerr << e << std::endl;
#endif
}
catch (const std::exception& e)
{
    std::cerr << e.what() << std::endl;
}
