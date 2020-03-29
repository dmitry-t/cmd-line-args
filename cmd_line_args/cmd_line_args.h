// Command line argument parser
//
// Copyright 2020, Dmitry Tarakanov
// SPDX-License-Identifier: MIT
//
#pragma once

#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace over9000 {

class Parser;

namespace details {

class Param
{
public:
    Param(std::string longName, std::string help)
        : longName_(std::move(longName)), help_(std::move(help))
    {
    }

    Param& shortName(char name)
    {
        shortName_ = name;
        return *this;
    }

    Param& optional()
    {
        optional_ = true;
        return *this;
    }

    Param& flag()
    {
        flag_ = true;
        optional_ = true;
        return *this;
    }

protected:
    friend class ::over9000::Parser;

    virtual bool isList() const = 0;
    virtual void parse(std::istream& stream) = 0;

    std::string longName_;
    size_t index_ = 0;
    std::string help_;
    char shortName_ = '\0';
    bool optional_ = false;
    bool flag_ = false;
    bool parsed_ = false;
};

template<class T>
struct Converter
{
    void operator()(std::istream& stream, T& value) { stream >> value; }
};

template<>
struct Converter<std::string>
{
    void operator()(std::istream& stream, std::string& value) { std::getline(stream, value); }
};

template<class T>
struct EnumConverter
{
    void operator()(std::istream& stream, T& value)
    {
        std::string string;
        std::getline(stream, string);
        auto iter = values.find(string);
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

    void parse(std::istream& stream) override
    {
        if (parsed_)
        {
            throw std::runtime_error("Repeated argument: --" + longName_);
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

    void parse(std::istream& stream) override
    {
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
        namedParams_.push_back(std::make_unique<details::ParamImpl<T, details::EnumConverter<T>>>(
            value, longName, std::move(help), details::EnumConverter<T>{std::move(enumValues)}));
        paramsByLongName_.emplace(longName, namedParams_.back().get());
        return *namedParams_.back();
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
    details::Param& addParam(T& value, std::string help)
    {
        static_assert(!std::is_enum<T>(), "Missing enum values");

        return addPositionalParam(std::make_unique<details::ParamImpl<T, details::Converter<T>>>(
            value, "", std::move(help), details::Converter<T>{}));
    }

    template<class T>
    details::Param& addParam(std::vector<T>& value, std::string help)
    {
        static_assert(!std::is_enum<T>(), "Missing enum values");

        return addPositionalParam(
            std::make_unique<details::ParamImpl<std::vector<T>, details::Converter<T>>>(
                value, "", std::move(help), details::Converter<T>{}));
    }

    template<class T>
    details::Param& addParam(T& value, std::string help,
                             std::unordered_map<std::string, T> enumValues)
    {
        return addPositionalParam(
            std::make_unique<details::ParamImpl<T, details::EnumConverter<T>>>(
                value, "", std::move(help), details::EnumConverter<T>{std::move(enumValues)}));
    }

    template<class T>
    details::Param& addParam(std::vector<T>& value, std::string help,
                             std::unordered_map<std::string, T> enumValues)
    {
        return addPositionalParam(
            std::make_unique<details::ParamImpl<std::vector<T>, details::EnumConverter<T>>>(
                value, "", std::move(help), details::EnumConverter<T>{std::move(enumValues)}));
    }

    void parse(int argc, const char* argv[])
    {
        // Check short name uniqueness
        for (const auto& param : namedParams_)
        {
            if (param->shortName_ != '\0')
            {
                auto iter = paramsByShortName_.find(param->shortName_);
                if (iter != paramsByShortName_.end())
                {
                    throw std::runtime_error("Repeated short name parameter: -" +
                                             param->shortName_);
                }

                paramsByShortName_.emplace(param->shortName_, param.get());
            }
        }

        for (const auto& param : positionalParams_)
        {
            if (param->flag_)
            {
                throw std::runtime_error("Flag positional parameter #" +
                                         std::to_string(param->index_));
            }
        }

        for (size_t i = 1; i < positionalParams_.size(); ++i)
        {
            auto& param = positionalParams_[i - 1];
            if (param->isList())
            {
                throw std::runtime_error("Not last positional list parameter #" +
                                         std::to_string(param->index_));
            }
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
                throw std::runtime_error(
                    "Optional positional parameter #" + std::to_string(optionalParam->index_) +
                    " followed by non-optional #" + std::to_string(param->index_));
            }
        }

        std::stringstream stream;
        size_t currentPositionalPos = 0;
        details::Param* currentNamedParam = nullptr;
        for (auto* argIter = argv + 1; argIter != argv + argc; ++argIter)
        {
            if (argIter == nullptr)
            {
                throw std::runtime_error("Bad argument #" + std::to_string(argIter - argv + 1));
            }

            std::string arg(*argIter);

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
                    throw std::runtime_error("Unexpected positional argument: " + arg);
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
                auto iter = paramsByShortName_.find(arg[1]);
                if (iter == paramsByShortName_.end())
                {
                    throw std::runtime_error("Unexpected argument: " + arg);
                }

                if (iter->second->flag_)
                {
                    parseArg(*iter->second, "1", stream);
                    continue;
                }

                currentNamedParam = iter->second;
                continue;
            }

            if (arg[1] != '-')
            {
                throw std::runtime_error("Bad argument: " + arg);
            }

            // --long-opt[=value| value|]
            size_t equalPos = arg.find('=');
            if (equalPos == std::string::npos)
            {
                // --long-opt[ value]
                arg.erase(0, 2);
                auto iter = paramsByLongName_.find(arg);
                if (iter == paramsByLongName_.end())
                {
                    throw std::runtime_error("Unexpected argument: --" + arg);
                }

                if (iter->second->flag_)
                {
                    parseArg(*iter->second, "1", stream);
                    continue;
                }

                currentNamedParam = iter->second;
                continue;
            }

            // --long-opt=value
            auto argName = arg;
            arg.erase(0, equalPos + 1);
            argName.erase(equalPos);
            argName.erase(0, 2);
            auto iter = paramsByLongName_.find(argName);
            if (iter == paramsByLongName_.end())
            {
                throw std::runtime_error("Unexpected argument: --" + arg);
            }
            parseArg(*iter->second, arg, stream);
        }

        for (const auto& param : namedParams_)
        {
            if (!param->parsed_ && !param->optional_)
            {
                throw std::runtime_error("Missing argument: --" + param->longName_);
            }
        }

        for (const auto& param : positionalParams_)
        {
            if (!param->parsed_ && !param->optional_)
            {
                throw std::runtime_error("Missing positional argument #" +
                                         std::to_string(param->index_));
            }
        }
    }

private:
    details::Param& addNamedParam(std::unique_ptr<details::Param> param)
    {
        auto iter = paramsByLongName_.find(param->longName_);
        if (iter != paramsByLongName_.end())
        {
            throw std::runtime_error("Repeated long name parameter: --" + param->longName_);
        }

        paramsByLongName_.emplace(param->longName_, param.get());
        namedParams_.push_back(std::move(param));
        return *namedParams_.back();
    }

    details::Param& addPositionalParam(std::unique_ptr<details::Param> param)
    {
        positionalParams_.push_back(std::move(param));
        positionalParams_.back()->index_ = positionalParams_.size(); // 1-based index
        return *positionalParams_.back();
    }

    void parseArg(details::Param& param, const std::string& arg, std::stringstream& stream)
    {
        stream.str(arg);
        stream.clear();
        param.parse(stream);
        if (stream.fail() || !stream.eof())
        {
            if (param.longName_.empty())
            {
                throw std::runtime_error("Bad positional argument #" +
                                         std::to_string(param.index_) + ": " + arg);
            }

            throw std::runtime_error("Bad argument --" + param.longName_ + ": " + arg);
        }
    }

    std::unordered_map<char, details::Param*> paramsByShortName_;
    std::unordered_map<std::string, details::Param*> paramsByLongName_;
    std::vector<std::unique_ptr<details::Param>> namedParams_;
    std::vector<std::unique_ptr<details::Param>> positionalParams_;
};

} // namespace over9000
