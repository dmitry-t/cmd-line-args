// Command line argument parser
//
// Copyright 2020, Dmitry Tarakanov
// SPDX-License-Identifier: MIT
//
#include "over9000/cmd_line_args/parser.h"

#include "gtest/gtest.h"
#include <codecvt>
#include <locale>
#include <string>

namespace {

using over9000::cmd_line_args::ParseError;
using over9000::cmd_line_args::Parser;

struct Tests : testing::Test
{
    Parser parser;

    Tests() : parser("Description") {}

    void parse(const std::vector<const char*>& args)
    {
#ifdef _WIN32
        std::vector<std::wstring> wstrings;
        wstrings.reserve(args.size());
        std::vector<const wchar_t*> wargs;
        wargs.reserve(args.size());
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        for (auto* arg : args)
        {
            wstrings.push_back(converter.from_bytes(arg));
            wargs.push_back(wstrings.back().c_str());
        }
        parser.parse(static_cast<int>(wargs.size()), wargs.data());
#else
        parser.parse(static_cast<int>(args.size()), args.data());
#endif
    }
};

TEST_F(Tests, stringParams)
{
    std::string s1;
    parser.addParam(s1, "string1", "String");

    std::string s2;
    parser.addParam(s2, "string2", "String").shortName('s');

    std::string s3;
    parser.addParam(s3, "string3", "String").shortName('3');

    parse({"exe", "--string1", "a b c", "-s", "s2", "--string3=s3"});

    ASSERT_EQ("a b c", s1);
    ASSERT_EQ("s2", s2);
    ASSERT_EQ("s3", s3);

    parse({"exe", "--string1", "s1", "--string3=a b c", "-s", "a b c"});

    ASSERT_EQ("s1", s1);
    ASSERT_EQ("a b c", s2);
    ASSERT_EQ("a b c", s3);
}

TEST_F(Tests, tooShortLongNameThrows)
{
    std::string s;
    ASSERT_THROW(parser.addParam(s, "s", "String"), ParseError);
}

TEST_F(Tests, badArgumentNameThrows)
{
    std::string s;
    parser.addParam(s, "string", "String").shortName('s');

    ASSERT_THROW(parse({"exe", "-string", "s"}), ParseError);
    ASSERT_THROW(parse({"exe", "--s", "s"}), ParseError);
}

TEST_F(Tests, missingArgumentThrows)
{
    std::string s1;
    parser.addParam(s1, "s1", "String");

    std::string s2;
    parser.addParam(s2, "s2", "String").shortName('s');

    ASSERT_THROW(parse({"exe", "--s2", "s2"}), ParseError);
}

TEST_F(Tests, repeatedParameterThrows)
{
    std::string s1;
    parser.addParam(s1, "string1", "String 1.1").shortName('s');

    std::string s2;
    ASSERT_THROW(parser.addParam(s2, "string1", "String 1.2"), ParseError);

    std::string s3;
    ASSERT_THROW(parser.addParam(s2, "string2", "String 2").shortName('s'), ParseError);
}

TEST_F(Tests, repeatedArgumentThrows)
{
    std::string s;
    parser.addParam(s, "string", "String").shortName('s');

    ASSERT_THROW(parse({"exe", "--string=1", "a", "-s", "b"}), ParseError);
    ASSERT_THROW(parse({"exe", "-s", "a", "--string", "b"}), ParseError);
    ASSERT_THROW(parse({"exe", "--string", "a", "--string=b"}), ParseError);
}

TEST_F(Tests, optionalStringParams)
{
    std::string s1 = "s1";
    parser.addParam(s1, "string1", "String ").optional();

    std::string s2 = "s2";
    parser.addParam(s2, "string2", "String 2 ").shortName('s').optional();

    std::string s3 = "s3";
    parser.addParam(s3, "string3", "String 3").shortName('3').optional();

    parse({"exe", "--string1", "a b c"});

    ASSERT_EQ("a b c", s1);
    ASSERT_EQ("s2", s2);
    ASSERT_EQ("s3", s3);

    parse({"exe", "--string1", "s1", "-s", "a b c"});

    ASSERT_EQ("s1", s1);
    ASSERT_EQ("a b c", s2);
    ASSERT_EQ("s3", s3);
}

TEST_F(Tests, intParams)
{
    int i1 = 1;
    parser.addParam(i1, "int1", "Integer 1");

    int i2 = 2;
    parser.addParam(i2, "int2", "Integer 2").shortName('i');

    int i3 = 3;
    parser.addParam(i3, "int3", "Integer 3").shortName('3');

    parse({"exe", "--int1", "10", "-i", "20", "--int3=30"});

    ASSERT_EQ(10, i1);
    ASSERT_EQ(20, i2);
    ASSERT_EQ(30, i3);

    parse({"exe", "--int1", "-10", "-i", "-20", "--int3=-30"});

    ASSERT_EQ(-10, i1);
    ASSERT_EQ(-20, i2);
    ASSERT_EQ(-30, i3);
}

TEST_F(Tests, optionalIntParams)
{
    int i1 = 1;
    parser.addParam(i1, "int1", "Integer 1").optional();

    int i2 = 2;
    parser.addParam(i2, "int2", "Integer 2").shortName('i').optional();

    int i3 = 3;
    parser.addParam(i3, "int3", "Integer 3").shortName('3').optional();

    parse({"exe", "--int1", "10"});

    ASSERT_EQ(10, i1);
    ASSERT_EQ(2, i2);
    ASSERT_EQ(3, i3);

    parse({"exe", "-3", "-30"});

    ASSERT_EQ(10, i1);
    ASSERT_EQ(2, i2);
    ASSERT_EQ(-30, i3);
}

enum class Enum
{
    VALUE0 = 0,
    VALUE1 = 1,
    VALUE2 = 2,
    VALUE3 = 3,
};

TEST_F(Tests, enumParams)
{
    Enum e1 = Enum::VALUE0;
    parser.addParam(e1, "enum1", "Enum 1",
                    {
                        {"0", Enum::VALUE0},
                        {"1", Enum::VALUE1},
                        {"2", Enum::VALUE2},
                        {"3", Enum::VALUE3},
                    });

    Enum e2 = Enum::VALUE0;
    parser.addParam(e2, "enum2", "Enum 2",
                    {
                        {"V0", Enum::VALUE0},
                        {"V1", Enum::VALUE1},
                        {"V2", Enum::VALUE2},
                        {"V3", Enum::VALUE3},
                    }).shortName('e');

    Enum e3 = Enum::VALUE0;
    parser.addParam(e3, "enum3", "Enum 3",
                    {
                        {"-0", Enum::VALUE0},
                        {"-1", Enum::VALUE1},
                        {"-2", Enum::VALUE2},
                        {"-3", Enum::VALUE3},
                    });

    parse({"exe", "--enum1", "1", "-e", "V2", "--enum3=-3"});

    ASSERT_EQ(Enum::VALUE1, e1);
    ASSERT_EQ(Enum::VALUE2, e2);
    ASSERT_EQ(Enum::VALUE3, e3);

    parse({"exe", "--enum3", "-2", "-e", "V1", "--enum1=0"});

    ASSERT_EQ(Enum::VALUE0, e1);
    ASSERT_EQ(Enum::VALUE1, e2);
    ASSERT_EQ(Enum::VALUE2, e3);
}

TEST_F(Tests, positionalStringParams)
{
    std::string required;
    parser.addParam(required, "required", "Required");

    std::string optional;
    parser.addParam(optional, "optional", "Optional").optional();

    std::string positional;
    parser.addPositionalParam(positional, "positional", "Positional");

    parse({"exe", "--required", "1", "2"});

    ASSERT_EQ("1", required);
    ASSERT_EQ("", optional);
    ASSERT_EQ("2", positional);

    parse({"exe", "b", "--required", "a"});

    ASSERT_EQ("a", required);
    ASSERT_EQ("", optional);
    ASSERT_EQ("b", positional);

    parse({"exe", "--optional", "O", "P", "--required", "R"});

    ASSERT_EQ("R", required);
    ASSERT_EQ("O", optional);
    ASSERT_EQ("P", positional);
}

TEST_F(Tests, optionalPositionalStringParams)
{
    std::string required;
    parser.addParam(required, "required", "Required");

    std::string optional;
    parser.addParam(optional, "optional", "Optional").optional();

    std::string positional;
    parser.addPositionalParam(positional, "positional", "Positional").optional();

    parse({"exe", "--required", "req", "--optional", "opt"});

    ASSERT_EQ("req", required);
    ASSERT_EQ("opt", optional);
    ASSERT_EQ("", positional);

    parse({"exe", "--required", "R", "--optional", "O", "P"});

    ASSERT_EQ("R", required);
    ASSERT_EQ("O", optional);
    ASSERT_EQ("P", positional);
}

TEST_F(Tests, twoPositionalListsThrow)
{
    std::vector<int> list1;
    parser.addPositionalParam(list1, "list1", "List 1");

    std::vector<int> list2;
    ASSERT_THROW(parser.addPositionalParam(list1, "list2", "List 2"), ParseError);
}

} // namespace
