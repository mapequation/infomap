/**********************************************************************************

 Infomap software package for multi-level network clustering

 Copyright (c) 2013, 2014 Daniel Edler, Martin Rosvall

 For more information, see <http://www.mapequation.org>


 This file is part of Infomap software package.

 Infomap software package is free software: you can redistribute it and/or modify
 it under the terms of the GNU Affero General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 Infomap software package is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Affero General Public License for more details.

 You should have received a copy of the GNU Affero General Public License
 along with Infomap software package.  If not, see <http://www.gnu.org/licenses/>.

**********************************************************************************/


#ifndef PROGRAMINTERFACE_H_
#define PROGRAMINTERFACE_H_

#include <utility>
#include <vector>
#include <deque>
#include <string>
#include <sstream>
#include <iostream>
#include <unordered_map>
#include "convert.h"
#include "../utils/exceptions.h"

namespace infomap {

struct ArgType {
  static const std::string integer;
  static const std::string number;
  static const std::string string;
  static const std::string path;
  static const std::string probability;
  static const std::string option;
  static const std::string list;
  static const std::unordered_map<std::string, char> toShort;
};

struct Option {
  Option(char shortName, std::string longName, std::string desc, std::string group, bool isAdvanced, bool requireArgument = false, std::string argName = "")
      : shortName(shortName),
        longName(std::move(longName)),
        description(std::move(desc)),
        group(std::move(group)),
        isAdvanced(isAdvanced),
        requireArgument(requireArgument),
        incrementalArgument(false),
        argumentName(std::move(argName)) {}

  virtual ~Option() = default;

  virtual bool parse(std::string const&)
  {
    used = true;
    return true;
  }

  virtual void set(bool value)
  {
    used = true;
    negated = !value;
  };

  Option& setHidden(bool value)
  {
    hidden = value;
    return *this;
  }

  virtual std::ostream& printValue(std::ostream& out) const { return out; }
  virtual std::string printValue() const { return ""; }
  virtual std::string printNumericValue() const { return ""; }

  friend std::ostream& operator<<(std::ostream& out, const Option& option)
  {
    out << option.longName;
    if (option.requireArgument) {
      out << " = ";
      option.printValue(out);
    }
    return out;
  }

  char shortName;
  std::string longName;
  std::string description;
  std::string group;
  bool isAdvanced;
  bool requireArgument;
  bool incrementalArgument;
  std::string argumentName;
  bool hidden = false;
  bool used = false;
  bool negated = false;
};

struct IncrementalOption : Option {
  IncrementalOption(unsigned int& target, char shortName, std::string longName, std::string desc, std::string group, bool isAdvanced)
      : Option(shortName, std::move(longName), std::move(desc), std::move(group), isAdvanced, false), target(target)
  {
    incrementalArgument = true;
  }

  bool parse(std::string const& value) override
  {
    Option::parse(value);
    return ++target;
  }

  void set(bool value) override
  {
    Option::set(value);
    if (value) {
      ++target;
    } else if (target > 0) {
      --target;
    }
  }

  std::ostream& printValue(std::ostream& out) const override { return out << target; }
  std::string printValue() const override { return io::Str() << target; }

  unsigned int& target;
};

template <typename T>
struct ArgumentOption : Option {
  ArgumentOption(T& target, char shortName, std::string longName, std::string desc, std::string group, bool isAdvanced, std::string argName)
      : Option(shortName, std::move(longName), std::move(desc), std::move(group), isAdvanced, true, std::move(argName)), target(target) {}

  bool parse(std::string const& value) override
  {
    Option::parse(value);
    return io::stringToValue(value, target);
  }

  std::string printValue() const override { return io::Str() << target; }
  std::ostream& printValue(std::ostream& out) const override { return out << target; }
  std::string printNumericValue() const override { return TypeInfo<T>::isNumeric() ? printValue() : ""; }

  T& target;
};

template <>
struct ArgumentOption<bool> : Option {
  ArgumentOption(bool& target, char shortName, std::string longName, std::string desc, std::string group, bool isAdvanced)
      : Option(shortName, std::move(longName), std::move(desc), std::move(group), isAdvanced, false), target(target) {}

  bool parse(std::string const& value) override
  {
    Option::parse(value);
    return target = true;
  }

  void set(bool value) override
  {
    Option::set(value);
    target = value;
  }

  std::ostream& printValue(std::ostream& out) const override { return out << target; }
  std::string printValue() const override { return io::Str() << target; }
  std::string printNumericValue() const override { return ""; }

  bool& target;
};

struct ParsedOption {
  explicit ParsedOption(const Option& opt)
      : shortName(opt.shortName),
        longName(opt.longName),
        description(opt.description),
        group(opt.group),
        isAdvanced(opt.isAdvanced),
        requireArgument(opt.requireArgument),
        incrementalArgument(opt.incrementalArgument),
        argumentName(opt.argumentName),
        negated(opt.negated),
        value(opt.printValue()) {}

  friend std::ostream& operator<<(std::ostream& out, const ParsedOption& option)
  {
    if (option.negated)
      out << "no ";
    out << option.longName;
    if (option.requireArgument)
      out << " = " << option.value;
    return out;
  }

  char shortName;
  std::string longName;
  std::string description;
  std::string group;
  bool isAdvanced;
  bool requireArgument;
  bool incrementalArgument;
  std::string argumentName;
  bool negated;
  std::string value;
};

struct TargetBase {
  TargetBase(std::string variableName, std::string desc, std::string group, bool isAdvanced)
      : variableName(variableName), description(desc), group(group), isOptionalVector(false), isAdvanced(isAdvanced) {}

  virtual ~TargetBase() = default;

  virtual bool parse(std::string const& value) = 0;

  std::string variableName;
  std::string description;
  std::string group;
  bool isOptionalVector = false;
  bool isAdvanced;
};

template <typename T>
struct Target : TargetBase {
  Target(T& target, std::string variableName, std::string desc, std::string group, bool isAdvanced)
      : TargetBase(std::move(variableName), std::move(desc), std::move(group), isAdvanced), target(target) {}

  bool parse(std::string const& value) override
  {
    return io::stringToValue(value, target);
  }

  T& target;
};

template <typename T>
struct OptionalTargets : TargetBase {
  OptionalTargets(std::vector<T>& target, std::string variableName, std::string desc, std::string group, bool isAdvanced)
      : TargetBase(std::move(variableName), std::move(desc), std::move(group), isAdvanced), targets(target)
  {
    isOptionalVector = true;
  }

  bool parse(std::string const& value) override
  {
    T target;
    bool ok = io::stringToValue(value, target);
    if (ok)
      targets.push_back(target);
    return ok;
  }

  std::vector<T>& targets;
};

class ProgramInterface {
public:
  ProgramInterface(std::string name, std::string shortDescription, std::string version);
  virtual ~ProgramInterface();

  void addProgramDescription(std::string desc)
  {
    m_programDescription = std::move(desc);
  }

  void setGroups(std::vector<std::string> groups)
  {
    m_groups = std::move(groups);
  }

  template <typename T>
  void addNonOptionArgument(T& target, std::string variableName, std::string desc, std::string group, bool isAdvanced = false)
  {
    m_nonOptionArguments.push_back(new Target<T>(target, std::move(variableName), std::move(desc), std::move(group), isAdvanced));
  }

  template <typename T>
  void addOptionalNonOptionArguments(std::vector<T>& target, std::string variableName, std::string desc, std::string group, bool isAdvanced = false)
  {
    if (m_numOptionalNonOptionArguments != 0)
      throw OptionConflictError("Can't have two non-option vector arguments");
    ++m_numOptionalNonOptionArguments;
    m_nonOptionArguments.push_back(new OptionalTargets<T>(target, std::move(variableName), std::move(desc), std::move(group), isAdvanced));
  }

  Option& addOptionArgument(char shortName, std::string longName, std::string description, std::string group, bool isAdvanced = false)
  {
    auto* o = new Option(shortName, std::move(longName), std::move(description), std::move(group), isAdvanced);
    m_optionArguments.push_back(o);
    return *o;
  }

  Option& addIncrementalOptionArgument(unsigned int& target, char shortName, std::string longName, std::string description, std::string group, bool isAdvanced = false)
  {
    auto* o = new IncrementalOption(target, shortName, std::move(longName), std::move(description), std::move(group), isAdvanced);
    m_optionArguments.push_back(o);
    return *o;
  }

  Option& addOptionArgument(bool& target, char shortName, std::string longName, std::string description, std::string group, bool isAdvanced = false)
  {
    auto* o = new ArgumentOption<bool>(target, shortName, std::move(longName), std::move(description), std::move(group), isAdvanced);
    m_optionArguments.push_back(o);
    return *o;
  }

  // Without shortName
  Option& addOptionArgument(bool& target, std::string longName, std::string description, std::string group, bool isAdvanced = false)
  {
    auto* o = new ArgumentOption<bool>(target, '\0', std::move(longName), std::move(description), std::move(group), isAdvanced);
    m_optionArguments.push_back(o);
    return *o;
  }

  template <typename T>
  Option& addOptionArgument(T& target, char shortName, std::string longName, std::string description, std::string argumentName, std::string group, bool isAdvanced = false)
  {
    auto* o = new ArgumentOption<T>(target, shortName, std::move(longName), std::move(description), std::move(group), isAdvanced, std::move(argumentName));
    m_optionArguments.push_back(o);
    return *o;
  }

  // Without shortName
  template <typename T>
  Option& addOptionArgument(T& target, std::string longName, std::string description, std::string argumentName, std::string group, bool isAdvanced = false)
  {
    auto* o = new ArgumentOption<T>(target, '\0', std::move(longName), std::move(description), std::move(group), isAdvanced, std::move(argumentName));
    m_optionArguments.push_back(o);
    return *o;
  }

  void parseArgs(const std::string& args);

  std::vector<ParsedOption> getUsedOptionArguments();

  unsigned int numRequiredArguments() { return m_nonOptionArguments.size() - m_numOptionalNonOptionArguments; }

private:
  void exitWithUsage(bool showAdvanced);
  void exitWithVersionInformation();
  void exitWithError(std::string message);
  void exitWithJsonParameters();

  static std::string toJson(const std::string& key, const std::string& value);
  static std::string toJson(const std::string& key, int value);
  static std::string toJson(const std::string& key, unsigned int value);
  static std::string toJson(const std::string& key, double value);
  static std::string toJson(const std::string& key, bool value);
  static std::string toJson(const Option& opt);

  std::deque<Option*> m_optionArguments;
  std::deque<TargetBase*> m_nonOptionArguments;
  std::string m_programName = "Infomap";
  std::string m_shortProgramDescription;
  std::string m_programVersion;
  std::string m_programDescription;
  std::vector<std::string> m_groups;
  std::string m_executableName = "Infomap";
  unsigned int m_displayHelp = 0;
  bool m_displayVersion = false;
  bool m_negateNextOption = false;
  bool m_printJsonParameters = false;

  unsigned int m_numOptionalNonOptionArguments = 0;
};

} // namespace infomap

#endif /* ARGPARSER_H_ */
