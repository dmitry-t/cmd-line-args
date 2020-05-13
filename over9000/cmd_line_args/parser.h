// Command line argument parser
//
// Copyright 2020, Dmitry Tarakanov
// SPDX-License-Identifier: MIT
//
#pragma once

#include <algorithm>
#include <exception>
#include <iomanip>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace over9000 {
namespace cmd_line_args {

#ifdef _WIN32
using Char = wchar_t;
#else
using Char = char;
#endif // _WIN32

class Error : public std::exception
{
public:
    template<class T>
    Error operator<<(const T& value)
    {
        stream_ << value;
        return std::move(*this);
    }

#ifdef _WIN32
    Error operator<<(const std::string& value)
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

inline std::basic_ostream<Char>& operator<<(std::basic_ostream<Char>& lhs, const Error& rhs)
{
    lhs << rhs.message();
    return lhs;
}

class Parser;

enum class ParamType
{
    REQUIRED,
    OPTIONAL,
};

constexpr ParamType REQUIRED = ParamType::REQUIRED;
constexpr ParamType OPTIONAL = ParamType::OPTIONAL;

namespace details {

class Param
{
public:
    Param(std::string longName, char shortName, std::string help, ParamType type, bool flag)
        : longName_(std::move(longName))
        , shortName_(shortName)
        , help_(std::move(help))
        , optional_(type == ParamType::OPTIONAL)
        , flag_(flag)
    {
        if (shortName != '\0' && shortName <= ' ')
        {
            throw Error() << "Bad short name for parameter: --" << longName_;
        }
    }

    friend std::basic_ostream<Char>& operator<<(std::basic_ostream<Char>& lhs, const Param& rhs)
    {
        if (rhs.index_ != 0) // Positional
        {
            lhs << "#" << rhs.index_ << " ";
        }
        else if (rhs.shortName_ != '\0')
        {
            lhs << "-" << rhs.shortName_ << "/";
        }
        lhs << "--" << std::basic_string<Char>(rhs.longName_.begin(), rhs.longName_.end());
        return lhs;
    }

protected:
    friend class ::over9000::cmd_line_args::Parser;

    virtual bool isList() const = 0;
    virtual void parse(std::basic_istream<Char>& stream) = 0;
    virtual std::string getValidValues() const = 0;

    std::string longName_;
    char shortName_ = '\0';
    std::string help_;
    size_t index_ = 0; // 0 for named params, 1-based index for positional params
    bool optional_ = false;
    bool flag_ = false;
    bool parsed_ = false;
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
            throw Error() << "Not an ASCII " << what << " string: " << string;
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
    std::string getValidValues() const { return {}; }
};

template<>
struct Converter<std::basic_string<Char>>
{
    void operator()(std::basic_istream<Char>& stream, std::basic_string<Char>& value)
    {
        std::getline(stream, value);
    }

    std::string getValidValues() const { return {}; }
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

    std::string getValidValues() const { return {}; }
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

    std::string getValidValues() const
    {
        std::ostringstream stream;
        const char* delimiter = "";
        for (const auto& v : values)
        {
            stream << delimiter << v.first;
            delimiter = ", ";
        }
        return stream.str();
    }

    std::map<std::string, T> values;
};

template<class T>
struct TypeTraits
{
    using ValueType = T;
    using EnumValuesType = std::map<std::string, T>;
};

template<class T>
struct TypeTraits<std::vector<T>>
{
    using ValueType = T;
    using EnumValuesType = std::map<std::string, T>;
};

template<class T, class Converter>
class ParamImpl : public Param
{
public:
    ParamImpl(T& value, std::string longName, char shortName, std::string help, ParamType type,
              bool flag, Converter converter)
        : Param(std::move(longName), shortName, std::move(help), type, flag)
        , converter_(std::move(converter))
        , value_(&value)
    {
    }

    bool isList() const override { return false; }

    void parse(std::basic_istream<Char>& stream) override
    {
        converter_(stream, *value_);
        parsed_ = true;
    }

    std::string getValidValues() const override { return converter_.getValidValues(); }

private:
    Converter converter_;
    T* value_ = nullptr;
};

template<class T, class Converter>
class ParamImpl<std::vector<T>, Converter> : public Param
{
public:
    ParamImpl(std::vector<T>& value, std::string longName, char shortName, std::string help,
              ParamType type, bool flag, Converter converter)
        : Param(std::move(longName), shortName, std::move(help), type, flag)
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

    std::string getValidValues() const override { return converter_.getValidValues(); }

private:
    Converter converter_;
    std::vector<T>* value_ = nullptr;
};

} // namespace details

/// Command line arguments parser.
///
class Parser
{
public:
    explicit Parser(std::string description) : description_(std::move(description)) {}

    /// Registers a named parameter.
    /// A corresponding command line argument may be passed as follows (s is a shortName):
    /// - --longName value
    /// - --longName=value
    /// - -s value
    ///
    template<class T>
    void addParam(T& value, std::string longName, char shortName, std::string help,
                  ParamType type = ParamType::REQUIRED)
    {
        addParam(makeParam(value, std::move(longName), shortName, std::move(help), type, false));
    }

    /// Registers a named parameter.
    /// A corresponding command line argument may be passed as follows:
    /// - --longName value
    /// - --longName=value
    ///
    template<class T>
    void addParam(T& value, std::string longName, std::string help,
                  ParamType type = ParamType::REQUIRED)
    {
        addParam(value, std::move(longName), '\0', std::move(help), type);
    }

    /// Registers a named parameter with enumerated allowed values.
    /// A corresponding command line argument may be passed as follows (s is a shortName):
    /// - --longName value
    /// - --longName=value
    /// - -s value
    ///
    template<class T>
    void addParam(T& value, std::string longName, char shortName, std::string help,
                  typename details::TypeTraits<T>::EnumValuesType enumValues,
                  ParamType type = ParamType::REQUIRED)
    {
        addParam(makeEnumParam(value, std::move(longName), shortName, std::move(help), type, false,
                               std::move(enumValues)));
    }

    /// Registers a named parameter with enumerated allowed values.
    /// A corresponding command line argument may be passed as follows:
    /// - --longName value
    /// - --longName=value
    ///
    template<class T>
    void addParam(T& value, std::string longName, std::string help,
                  typename details::TypeTraits<T>::EnumValuesType enumValues,
                  ParamType type = ParamType::REQUIRED)
    {
        addParam(value, std::move(longName), '\0', std::move(help), std::move(enumValues), type);
    }

    /// Registers a named flag parameter.
    /// A corresponding command line argument may be passed as follows (s is a shortName):
    /// - --longName
    /// - -s
    ///
    template<class T>
    void addFlag(T& value, std::string longName, char shortName, std::string help)
    {
        static_assert(std::is_integral<T>::value, "Value must be of integral type");
        addParam(makeParam(value, std::move(longName), shortName, std::move(help), OPTIONAL, true));
    }

    /// Registers a named flag parameter.
    /// A corresponding command line argument may be passed as follows:
    /// - --longName
    ///
    template<class T>
    void addFlag(T& value, std::string longName, std::string help)
    {
        addFlag(value, std::move(longName), '\0', std::move(help));
    }

    /// Registers a positional parameter.
    ///
    template<class T>
    void addPositional(T& value, std::string longName, std::string help,
                       ParamType type = ParamType::REQUIRED)
    {
        addPositional(makeParam(value, std::move(longName), '\0', std::move(help), type, false));
    }

    /// Registers a positional parameter with enumerated allowed values.
    ///
    template<class T>
    void addPositional(T& value, std::string longName, std::string help,
                       typename details::TypeTraits<T>::EnumValuesType enumValues,
                       ParamType type = ParamType::REQUIRED)
    {
        addPositional(makeEnumParam(value, std::move(longName), '\0', std::move(help), type, false,
                                    std::move(enumValues)));
    }

    /// Parses the command line arguments.
    ///
    void parse(int argc, const Char* const argv[])
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

        std::basic_stringstream<Char> stream;
        size_t currentPositionalPos = 0;
        details::Param* currentNamedParam = nullptr;
        for (auto* argIter = argv + 1; argIter != argv + argc; ++argIter)
        {
            if (argIter == nullptr)
            {
                throw Error() << "Bad argument #" << (argIter - argv + 1);
            }

            std::basic_string<Char> arg(*argIter);

            if (currentNamedParam != nullptr)
            {
                parseArg(*currentNamedParam, arg, stream);
                currentNamedParam = nullptr;
                continue;
            }

            if (arg.size() == 2 && arg[0] == '-')
            {
                // -s[ value]
                auto iter = paramsByShortName_.find(static_cast<char>(arg[1]));
                if (iter != paramsByShortName_.end())
                {
                    if (!iter->second->parsed_ || iter->second->isList())
                    {
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
                }
            }

            if (arg.size() > 2 && arg[0] == '-' && arg[1] == '-')
            {
                // --long-opt[=value| value|]
                size_t equalPos = arg.find('=');
                if (equalPos == std::string::npos)
                {
                    // --long-opt[ value]
                    auto argName = details::toASCII(arg, "argument name");
                    argName.erase(0, 2);
                    auto iter = paramsByLongName_.find(argName);
                    if (iter != paramsByLongName_.end())
                    {
                        if (!iter->second->parsed_ || iter->second->isList())
                        {
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
                    }
                }
                else
                {
                    // --long-opt=value
                    auto argName = details::toASCII(arg, "argument name");
                    argName.erase(equalPos);
                    argName.erase(0, 2);
                    auto iter = paramsByLongName_.find(argName);
                    if (iter != paramsByLongName_.end())
                    {
                        if (!iter->second->parsed_ || iter->second->isList())
                        {
                            arg.erase(0, equalPos + 1);
                            parseArg(*iter->second, arg, stream);
                            continue;
                        }
                    }
                }
            }

            if (currentPositionalPos >= positionalParams_.size())
            {
                throw Error() << "Unexpected argument: " << arg;
            }

            parseArg(*positionalParams_[currentPositionalPos], arg, stream);
            if (!positionalParams_[currentPositionalPos]->isList())
            {
                ++currentPositionalPos;
            }
        }

        for (const auto& param : namedParams_)
        {
            if (!param->parsed_ && !param->optional_)
            {
                throw Error() << "Missing argument: " << *param;
            }
        }

        for (const auto& param : positionalParams_)
        {
            if (!param->parsed_ && !param->optional_)
            {
                throw Error() << "Missing positional argument " << *param;
            }
        }
    }

    /// Prints full help on all registered parameters.
    ///
    void printHelp(std::basic_ostream<Char>& stream)
    {
        printDescription(stream);
        printUsage(stream);
        printParams(stream);
    }

    /// Prints the program description.
    ///
    void printDescription(std::basic_ostream<Char>& stream)
    {
        stream << details::fromASCII(description_) << "\n\n";
    }

    /// Prints command line usage.
    ///
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

    /// Prints command line parameters.
    ///
    void printParams(std::basic_ostream<Char>& stream)
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
                   << std::setw(maxHelpIndent - paramString.size()) << "" << param.help_;

            auto validValues = param.getValidValues();
            if (!validValues.empty())
            {
                output << ". Valid values: " << validValues;
            }

            output << "\n";
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

    template<class T>
    static std::unique_ptr<details::Param> makeParam(T& value, std::string longName, char shortName,
                                                     std::string help, ParamType type, bool flag)
    {
        using ValueType = typename details::TypeTraits<T>::ValueType;
        static_assert(!std::is_enum<ValueType>(), "Missing enum values");
        return std::make_unique<details::ParamImpl<T, details::Converter<ValueType>>>(
            value, std::move(longName), shortName, std::move(help), type, flag,
            details::Converter<ValueType>());
    }

    template<class T>
    static std::unique_ptr<details::Param> makeEnumParam(
        T& value, std::string longName, char shortName, std::string help, ParamType type, bool flag,
        typename details::TypeTraits<T>::EnumValuesType enumValues)
    {
        using ValueType = typename details::TypeTraits<T>::ValueType;
        return std::make_unique<details::ParamImpl<T, details::EnumConverter<ValueType>>>(
            value, std::move(longName), shortName, std::move(help), type, false,
            details::EnumConverter<ValueType>{std::move(enumValues)});
    }

    void addParam(std::unique_ptr<details::Param> param)
    {
        if (param->longName_.size() < 2)
        {
            throw Error() << "Too short long name parameter: " << *param;
        }

        auto iter = paramsByLongName_.find(param->longName_);
        if (iter != paramsByLongName_.end())
        {
            throw Error() << "Repeated parameter long name: " << *param;
        }

        if (param->shortName_ != '\0')
        {
            auto iter = paramsByShortName_.find(param->shortName_);
            if (iter != paramsByShortName_.end())
            {
                throw Error() << "Repeated parameter short name: " << *param;
            }

            paramsByShortName_.emplace(param->shortName_, param.get());
        }

        paramsByLongName_.emplace(param->longName_, param.get());
        namedParams_.push_back(std::move(param));
    }

    void addPositional(std::unique_ptr<details::Param> param)
    {
        param->index_ = positionalParams_.size(); // 1-based index

        if (!positionalParams_.empty() && positionalParams_.back()->optional_)
        {
            throw Error() << "Optional positional parameter " << *positionalParams_.back()
                          << " followed by another positional parameter " << *param;
        }

        if (!positionalParams_.empty() && positionalParams_.back()->isList())
        {
            throw Error() << "Positional list parameter " << *positionalParams_.back()
                          << " followed by another positional parameter " << *param;
        }

        positionalParams_.push_back(std::move(param));
    }

    void parseArg(details::Param& param, const std::basic_string<Char>& arg,
                  std::basic_stringstream<Char>& stream)
    {
        stream.str(arg);
        stream.clear();
        param.parse(stream);
        if (stream.fail() || !stream.eof())
        {
            auto validValues = param.getValidValues();
            if (!validValues.empty())
            {
                validValues.insert(0, ". Valid values: ");
            }

            if (param.index_ != 0)
            {
                throw Error() << "Bad positional argument " << param << ": " << arg << validValues;
            }

            throw Error() << "Bad argument " << param << ": " << arg << validValues;
        }
    }

    std::string description_;
    std::map<char, details::Param*> paramsByShortName_;
    std::map<std::string, details::Param*> paramsByLongName_;
    std::vector<std::unique_ptr<details::Param>> namedParams_;
    std::vector<std::unique_ptr<details::Param>> positionalParams_;
    std::basic_string<Char> exeName_;
};

} // namespace cmd_line_args
} // namespace over9000
