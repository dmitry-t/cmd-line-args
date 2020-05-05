// Command line argument parser
//
// Copyright 2020, Dmitry Tarakanov
// SPDX-License-Identifier: MIT
//
#pragma once

#include <algorithm>
#include <exception>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace over9000 {
namespace cmd_line_args {

#ifdef _WIN32
using Char = wchar_t;
#else
using Char = char;
#endif // _WIN32

class ParseError : public std::exception
{
public:
    template<class T>
    ParseError operator<<(const T& value)
    {
        stream_ << value;
        return std::move(*this);
    }

#ifdef _WIN32
    ParseError operator<<(const std::string& value)
    {
        stream_ << std::wstring(value.begin(), value.end());
        return std::move(*this);
    }
#endif // _WIN32

    const char* what() const noexcept override
    {
        message_.clear();
        try
        {
            auto string = message();
            message_.assign(string.begin(), string.end());
        }
        catch (...)
        {
        }
        return message_.c_str();
    }

    std::basic_string<Char> message() const noexcept
    {
        try
        {
            return stream_.str();
        }
        catch (...)
        {
            return {};
        }
    }

private:
    std::basic_ostringstream<Char> stream_;
    mutable std::string message_;
};

inline std::basic_ostream<Char>& operator<<(std::basic_ostream<Char>& lhs, const ParseError& rhs)
{
    lhs << rhs.message();
    return lhs;
}

class Parser;

namespace details {

class Param
{
public:
    Param(std::string longName, std::string help)
        : longName_(std::move(longName)), help_(std::move(help))
    {
    }

    /// Sets parameter short name. The name must be unique.
    ///
    Param& shortName(char name);

    Param& optional()
    {
        optional_ = true;
        return *this;
    }

    Param& flag()
    {
        if (index_ != 0)
        {
            throw ParseError() << "Flag positional parameter #" << index_;
        }

        flag_ = true;
        optional_ = true;
        return *this;
    }

protected:
    friend class ::over9000::cmd_line_args::Parser;

    virtual bool isList() const = 0;
    virtual void parse(std::basic_istream<Char>& stream) = 0;

    std::string longName_;
    std::string help_;
    char shortName_ = '\0';
    size_t index_ = 0; // 0 for named params, 1-based index for positional params
    bool optional_ = false;
    bool flag_ = false;
    bool parsed_ = false;
    Parser* parser_ = nullptr;
};

#ifdef _WIN32

std::string toASCII(const std::wstring& string, const char* what)
{
    std::string ascii;
    ascii.reserve(string.size());

    for (wchar_t c : string)
    {
        if (c > 127)
        {
            throw ParseError() << "Not an ASCII " << what << " string: " << string;
        }
        ascii += static_cast<char>(c);
    }
    return ascii;
}

std::wstring fromASCII(const std::string& string)
{
    return {string.begin(), string.end()};
}

#else

std::string toASCII(std::string string, const char* what)
{
    return std::move(string);
}

std::string fromASCII(std::string string)
{
    return std::move(string);
}

#endif

template<class T>
struct Converter
{
    void operator()(std::basic_istream<Char>& stream, T& value) { stream >> value; }
};

template<>
struct Converter<std::basic_string<Char>>
{
    void operator()(std::basic_istream<Char>& stream, std::basic_string<Char>& value)
    {
        std::getline(stream, value);
    }
};

#ifdef _WIN32

template<>
struct Converter<std::string>
{
    void operator()(std::basic_istream<Char>& stream, std::string& value)
    {
        std::basic_string<Char> string;
        std::getline(stream, string);
        value = toASCII(string, "argument value");
    }
};

#endif

template<class T>
struct EnumConverter
{
    void operator()(std::basic_istream<Char>& stream, T& value)
    {
        std::basic_string<Char> string;
        std::getline(stream, string);

        std::string ascii_string(string.begin(), string.end());
        auto iter = values.find(ascii_string);
        if (iter != values.end())
        {
            value = iter->second;
        }
        else
        {
            stream.setstate(std::ios_base::failbit);
        }
    }

    std::unordered_map<std::string, T> values;
};

template<class T, class Converter>
class ParamImpl : public Param
{
public:
    ParamImpl(T& value, std::string longName, std::string help, Converter converter)
        : Param(std::move(longName), std::move(help))
        , converter_(std::move(converter))
        , value_(&value)
    {
    }

    bool isList() const override { return false; }

    void parse(std::basic_istream<Char>& stream) override
    {
        if (parsed_)
        {
            throw ParseError() << "Repeated argument: --" << longName_;
        }
        converter_(stream, *value_);
        parsed_ = true;
    }

private:
    Converter converter_;
    T* value_ = nullptr;
};

template<class T, class Converter>
class ParamImpl<std::vector<T>, Converter> : public Param
{
public:
    ParamImpl(std::vector<T>& value, std::string longName, std::string help, Converter converter)
        : Param(std::move(longName), std::move(help))
        , converter_(std::move(converter))
        , value_(&value)
    {
    }

    bool isList() const override { return true; }

    void parse(std::basic_istream<Char>& stream) override
    {
        // Handle repeated Parser::parse() calls
        if (!parsed_)
        {
            value_->clear();
        }

        T value;
        converter_(stream, value);
        value_->push_back(value);
        parsed_ = true;
    }

private:
    Converter converter_;
    std::vector<T>* value_ = nullptr;
};

} // namespace details

class Parser
{
public:
    explicit Parser(std::string description) : description_(std::move(description)) {}

    template<class T>
    details::Param& addParam(T& value, const std::string& longName, std::string help)
    {
        static_assert(!std::is_enum<T>(), "Missing enum values");

        return addNamedParam(std::make_unique<details::ParamImpl<T, details::Converter<T>>>(
            value, longName, std::move(help), details::Converter<T>{}));
    }

    template<class T>
    details::Param& addParam(std::vector<T>& value, const std::string& longName, std::string help)
    {
        static_assert(!std::is_enum<T>(), "Missing enum values");

        return addNamedParam(
            std::make_unique<details::ParamImpl<std::vector<T>, details::Converter<T>>>(
                value, longName, std::move(help), details::Converter<T>{}));
    }

    template<class T>
    details::Param& addParam(T& value, const std::string& longName, std::string help,
                             std::unordered_map<std::string, T> enumValues)
    {
        return addNamedParam(std::make_unique<details::ParamImpl<T, details::EnumConverter<T>>>(
            value, longName, std::move(help), details::EnumConverter<T>{std::move(enumValues)}));
    }

    template<class T>
    details::Param& addParam(std::vector<T>& value, const std::string& longName, std::string help,
                             std::unordered_map<std::string, T> enumValues)
    {
        return addNamedParam(
            std::make_unique<details::ParamImpl<std::vector<T>, details::EnumConverter<T>>>(
                value, longName, std::move(help),
                details::EnumConverter<T>{std::move(enumValues)}));
    }

    template<class T>
    details::Param& addPositionalParam(T& value, const std::string& longName, std::string help)
    {
        static_assert(!std::is_enum<T>(), "Missing enum values");

        return addPositionalParam(std::make_unique<details::ParamImpl<T, details::Converter<T>>>(
            value, longName, std::move(help), details::Converter<T>{}));
    }

    template<class T>
    details::Param& addPositionalParam(std::vector<T>& value, const std::string& longName,
                                       std::string help)
    {
        static_assert(!std::is_enum<T>(), "Missing enum values");

        return addPositionalParam(
            std::make_unique<details::ParamImpl<std::vector<T>, details::Converter<T>>>(
                value, longName, std::move(help), details::Converter<T>{}));
    }

    template<class T>
    details::Param& addPositionalParam(T& value, const std::string& longName, std::string help,
                                       std::unordered_map<std::string, T> enumValues)
    {
        return addPositionalParam(
            std::make_unique<details::ParamImpl<T, details::EnumConverter<T>>>(
                value, longName, std::move(help),
                details::EnumConverter<T>{std::move(enumValues)}));
    }

    template<class T>
    details::Param& addPositionalParam(std::vector<T>& value, const std::string& longName,
                                       std::string help,
                                       std::unordered_map<std::string, T> enumValues)
    {
        return addPositionalParam(
            std::make_unique<details::ParamImpl<std::vector<T>, details::EnumConverter<T>>>(
                value, longName, std::move(help),
                details::EnumConverter<T>{std::move(enumValues)}));
    }

    void parse(int argc, const Char* argv[])
    {
        // Calculate the executable base name

        exeName_ = argv[0];

#ifdef _WIN32
        auto slashPos = exeName_.find_last_of(L"\\/");
#else
        auto slashPos = exeName_.find_last_of("/");
#endif //_WIN32

        if (slashPos != std::basic_string<Char>::npos)
        {
            exeName_.erase(0, slashPos + 1);
        }

        // Reset paremeter states

        for (const auto& param : namedParams_)
        {
            param->parsed_ = false;
        }

        for (const auto& param : positionalParams_)
        {
            param->parsed_ = false;
        }

        details::Param* optionalParam = nullptr;
        for (const auto& param : positionalParams_)
        {
            if (param->optional_)
            {
                optionalParam = param.get();
            }
            else if (optionalParam != nullptr)
            {
                throw ParseError() << "Optional positional parameter #" << optionalParam->index_
                                   << " followed by non-optional #" << param->index_;
            }
        }

        std::basic_stringstream<Char> stream;
        size_t currentPositionalPos = 0;
        details::Param* currentNamedParam = nullptr;
        for (auto* argIter = argv + 1; argIter != argv + argc; ++argIter)
        {
            if (argIter == nullptr)
            {
                throw ParseError() << "Bad argument #" << (argIter - argv + 1);
            }

            std::basic_string<Char> arg(*argIter);

            if (currentNamedParam != nullptr)
            {
                parseArg(*currentNamedParam, arg, stream);
                currentNamedParam = nullptr;
                continue;
            }

            if (arg.size() < 2 || arg[0] != '-')
            {
                if (currentPositionalPos >= positionalParams_.size())
                {
                    throw ParseError() << "Unexpected positional argument: " << arg;
                }

                parseArg(*positionalParams_[currentPositionalPos], arg, stream);
                if (!positionalParams_[currentPositionalPos]->isList())
                {
                    ++currentPositionalPos;
                }
                continue;
            }

            if (arg.size() == 2)
            {
                // -s[ value]
                auto iter = paramsByShortName_.find(static_cast<char>(arg[1]));
                if (iter == paramsByShortName_.end())
                {
                    throw ParseError() << "Unexpected argument: " << arg;
                }

                if (iter->second->flag_)
                {
#ifdef _WIN32
                    parseArg(*iter->second, L"1", stream);
#else
                    parseArg(*iter->second, "1", stream);
#endif
                    continue;
                }

                currentNamedParam = iter->second;
                continue;
            }

            if (arg[1] != '-')
            {
                throw ParseError() << "Bad argument: " << arg;
            }

            // --long-opt[=value| value|]
            size_t equalPos = arg.find('=');
            if (equalPos == std::string::npos)
            {
                // --long-opt[ value]
                auto argName = details::toASCII(arg, "argument name");
                argName.erase(0, 2);
                auto iter = paramsByLongName_.find(argName);
                if (iter == paramsByLongName_.end())
                {
                    throw ParseError() << "Unexpected argument: --" << argName;
                }

                if (iter->second->flag_)
                {
#ifdef _WIN32
                    parseArg(*iter->second, L"1", stream);
#else
                    parseArg(*iter->second, "1", stream);
#endif
                    continue;
                }

                currentNamedParam = iter->second;
                continue;
            }

            // --long-opt=value
            auto argName = details::toASCII(arg, "argument name");
            arg.erase(0, equalPos + 1);
            argName.erase(equalPos);
            argName.erase(0, 2);
            auto iter = paramsByLongName_.find(argName);
            if (iter == paramsByLongName_.end())
            {
                throw ParseError() << "Unexpected argument: --" << argName;
            }
            parseArg(*iter->second, arg, stream);
        }

        for (const auto& param : namedParams_)
        {
            if (!param->parsed_ && !param->optional_)
            {
                throw ParseError() << "Missing argument: --" << param->longName_;
            }
        }

        for (const auto& param : positionalParams_)
        {
            if (!param->parsed_ && !param->optional_)
            {
                throw ParseError() << "Missing positional argument #" << param->index_;
            }
        }
    }

    void printHelp(std::basic_ostream<Char>& stream)
    {
        printDescription(stream);
        printUsage(stream);
        printOptions(stream);
    }

    void printDescription(std::basic_ostream<Char>& stream)
    {
        stream << details::fromASCII(description_) << "\n\n";
    }

    void printUsage(std::basic_ostream<Char>& stream)
    {
        const size_t MAX_WIDTH = 80;
        std::ostringstream output;
        std::ostringstream paramOutput;

        stream << "Usage: " << exeName_;

        size_t usageIndent = 7 + exeName_.size(); // "Usage: exeName"

        size_t width = usageIndent;

        auto identUsage = [&] {
            output << std::setfill(' ') << std::setw(usageIndent) << "";
            width = usageIndent;
        };

        auto outputUsage = [&] {
            auto paramString = paramOutput.str();

            if (!paramString.empty())
            {
                paramOutput.str({});
                if (width + paramString.size() > MAX_WIDTH)
                {
                    output << "\n";
                    identUsage();
                }
                output << paramString;
                width += paramString.size();
            }
        };

        for (const auto& param : namedParams_)
        {
            paramOutput << " ";

            if (param->optional_)
            {
                paramOutput << "[";
            }
            else if (param->shortName_ != '\0')
            {
                paramOutput << "(";
            }

            if (param->shortName_ != '\0')
            {
                paramOutput << "-" << param->shortName_ << " <" << param->longName_ << "> | ";
            }

            paramOutput << "--" << param->longName_;

            if (!param->flag_)
            {
                paramOutput << " <" << param->longName_ << ">";
            }

            if (param->isList())
            {
                paramOutput << " ...";
            }

            if (param->optional_)
            {
                paramOutput << "]";
            }
            else if (param->shortName_ != '\0')
            {
                paramOutput << ")";
            }

            outputUsage();
        }

        for (const auto& param : positionalParams_)
        {
            paramOutput << " ";

            if (param->optional_)
            {
                paramOutput << "[";
            }

            paramOutput << "<" << param->longName_ << ">";

            if (param->isList())
            {
                paramOutput << " ...";
            }

            if (param->optional_)
            {
                paramOutput << "]";
            }

            outputUsage();
        }

        outputUsage();

        stream << details::fromASCII(output.str()) << "\n";
    }

    void printOptions(std::basic_ostream<Char>& stream)
    {
        size_t maxHelpIndent = 0;

        for (const auto& param : namedParams_)
        {
            size_t helpIdent = 0;
            if (param->shortName_ != '\0')
            {
                helpIdent += 4; // "-s, "
            }

            helpIdent += 2 + param->longName_.size(); // "--name"

            if (!param->flag_)
            {
                helpIdent += 3 + param->longName_.size(); // " <name>"
            }

            maxHelpIndent = std::max(maxHelpIndent, helpIdent);
        }

        for (const auto& param : positionalParams_)
        {
            size_t helpIdent = 2 + param->longName_.size(); // "<name>"
            maxHelpIndent = std::max(maxHelpIndent, helpIdent);
        }

        size_t INDENT = 4;
        maxHelpIndent += INDENT + 1;

        std::ostringstream output;
        std::ostringstream paramOutput;

        output << "Options:\n";

        auto outputHelp = [&](details::Param& param) {
            auto paramString = paramOutput.str();
            paramOutput.str({});

            output << paramString << std::setfill(' ')
                   << std::setw(maxHelpIndent - paramString.size()) << "" << param.help_ << "\n";
        };

        for (const auto& param : namedParams_)
        {
            paramOutput << std::setfill(' ') << std::setw(INDENT) << "";
            if (param->shortName_ != '\0')
            {
                paramOutput << "-" << param->shortName_ << ", ";
            }
            paramOutput << "--" << param->longName_;
            if (!param->flag_)
            {
                paramOutput << " <" << param->longName_ << ">";
            }

            outputHelp(*param);
        }

        for (const auto& param : positionalParams_)
        {
            paramOutput << std::setfill(' ') << std::setw(INDENT) << ""
                        << "<" << param->longName_ << ">";

            outputHelp(*param);
        }

        stream << details::fromASCII(output.str());
    }

private:
    friend class over9000::cmd_line_args::details::Param;

    // Called by Param::shortName()
    void setShortName(details::Param* param, char shortName)
    {
        auto iter = paramsByShortName_.find(shortName);
        if (iter != paramsByShortName_.end())
        {
            throw ParseError() << "Repeated short name parameter: -" << shortName;
        }

        paramsByShortName_.emplace(shortName, param);
    }

    details::Param& addNamedParam(std::unique_ptr<details::Param> param)
    {
        if (param->longName_.size() < 2)
        {
            throw ParseError() << "Too short long name parameter: --" << param->longName_;
        }

        auto iter = paramsByLongName_.find(param->longName_);
        if (iter != paramsByLongName_.end())
        {
            throw ParseError() << "Repeated long name parameter: --" << param->longName_;
        }

        param->parser_ = this;
        paramsByLongName_.emplace(param->longName_, param.get());
        namedParams_.push_back(std::move(param));
        return *namedParams_.back();
    }

    details::Param& addPositionalParam(std::unique_ptr<details::Param> param)
    {
        size_t param_index = positionalParams_.size(); // 1-based index

        if (param->isList() && !positionalParams_.empty() && positionalParams_.back()->isList())
        {
            throw ParseError() << "Repeated positional list parameter #" << (param_index + 1);
        }

        positionalParams_.push_back(std::move(param));
        positionalParams_.back()->index_ = param_index;
        return *positionalParams_.back();
    }

    void parseArg(details::Param& param, const std::basic_string<Char>& arg,
                  std::basic_stringstream<Char>& stream)
    {
        stream.str(arg);
        stream.clear();
        param.parse(stream);
        if (stream.fail() || !stream.eof())
        {
            if (param.index_ != 0)
            {
                throw ParseError() << "Bad positional argument #" << param.index_ << ": " << arg;
            }

            throw ParseError() << "Bad argument --" << param.longName_ << ": " << arg;
        }
    }

    std::string description_;
    std::unordered_map<char, details::Param*> paramsByShortName_;
    std::unordered_map<std::string, details::Param*> paramsByLongName_;
    std::vector<std::unique_ptr<details::Param>> namedParams_;
    std::vector<std::unique_ptr<details::Param>> positionalParams_;
    std::basic_string<Char> exeName_;
};

namespace details {

Param& Param::shortName(char name)
{
    if (index_ != 0)
    {
        throw ParseError() << "Short name for positional parameter: #" << this->index_;
    }

    if (name <= ' ' || name > 127)
    {
        throw ParseError() << "Bad short name for parameter: --" << longName_;
    }

    shortName_ = name;
    parser_->setShortName(this, name);
    return *this;
}

} // namespace details

} // namespace cmd_line_args
} // namespace over9000
