#include "cmd_line_args/cmd_line_args.h"

#include <iostream>
#include <vector>

#ifdef _WIN32

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
void dump(const char* title, T value)
{
    std::cout << title << ": " << std::boolalpha << value << std::endl;
}

template<class T>
void dump(const char* title, const std::vector<T>& values)
{
    std::cout << title << std::boolalpha << ": [";
    const char* delimiter = "";
    for (const auto& value : values)
    {
        std::cout << delimiter << value;
        delimiter = ", ";
    }
    std::cout << "]" << std::endl;
}

int main(int argc, const char* argv[]) try
{
    over9000::Parser parser;

    bool flag = false;
    parser.addParam(flag, "flag", "Flag").shortName('f').flag();

    std::string string;
    parser.addParam(string, "string", "String");

    int integer = 0;
    parser.addParam(integer, "integer", "Integer");

    Enum enumeration = Enum::VALUE0;
    parser.addParam(enumeration, "enum", "Enumeration",
                    {{"value1", Enum::VALUE1}, {"value2", Enum::VALUE2}});

    std::string optString;
    parser.addParam(optString, "optString", "Optional string").optional();

    int optInteger = 0;
    parser.addParam(optInteger, "optInteger", "Optional integer").optional();

    Enum optEnumeration = Enum::VALUE0;
    parser
        .addParam(optEnumeration, "optEnum", "Optional enumeration",
                  {{"value1", Enum::VALUE1}, {"value2", Enum::VALUE2}})
        .optional();

    std::vector<std::string> strings;
    parser.addParam(strings, "strings", "Strings").shortName('s');

    std::vector<int> integers;
    parser.addParam(integers, "integers", "Integers").shortName('i');

    std::vector<Enum> enumerations;
    parser
        .addParam(enumerations, "enums", "Enumerations",
                  {{"value1", Enum::VALUE1}, {"value2", Enum::VALUE2}})
        .shortName('e');

    std::vector<std::string> optStrings;
    parser.addParam(optStrings, "optStrings", "Optional string").optional();

    std::vector<int> optIntegers;
    parser.addParam(optIntegers, "optIntegers", "Optional integers").optional();

    std::vector<Enum> optEnumerations;
    parser
        .addParam(optEnumerations, "optEnums", "Optional enumerations",
                  {{"value1", Enum::VALUE1}, {"value2", Enum::VALUE2}})
        .optional();

    std::string positionalString;
    parser.addParam(positionalString, "Positional string");

    int positionalInteger = 0;
    parser.addParam(positionalInteger, "Positional integer");

    std::vector<Enum> positionalEnumerations;
    parser
        .addParam(positionalEnumerations, "Positional enumerations",
                  {{"value1", Enum::VALUE1}, {"value2", Enum::VALUE2}})
        .optional();

    parser.parse(argc, argv);

    dump("Flag", flag);
    dump("String", string);
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
    dump("Positional enumerations", positionalEnumerations);
}
catch (const std::exception& e)
{
    std::cerr << e.what() << std::endl;
}
