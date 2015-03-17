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

#include <getopt.h>
#include <vector>
#include <deque>
#include <string>
#include <sstream>
#include <stdexcept>
#include <iostream>
#include "convert.h"

#ifdef NS_INFOMAP
namespace infomap
{
#endif

struct OptionConflictError : std::logic_error
{
	OptionConflictError(std::string const& s) : std::logic_error(s) {}
};

struct Option
{
	Option(char shortName, std::string longName, std::string desc, bool isAdvanced, bool requireArgument = false,
			std::string argName = "")
	: shortName(shortName),
	  longName(longName),
	  description(desc),
	  isAdvanced(isAdvanced),
	  requireArgument(requireArgument),
	  incrementalArgument(false),
	  argumentName(argName),
	  used(false),
	  negated(false)
	{}
	virtual ~Option() {}
	virtual bool parse(std::string const&) { used = true; return true; }
	virtual void set(bool value) { used = true; negated = !value; };
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
	bool isAdvanced;
	bool requireArgument;
	bool incrementalArgument;
	std::string argumentName;
	bool used;
	bool negated;
};

struct IncrementalOption : Option
{
	IncrementalOption(unsigned int& target, char shortName, std::string longName, std::string desc, bool isAdvanced)
	: Option(shortName, longName, desc, isAdvanced, false), target(target)
	{ incrementalArgument = true; }
	virtual bool parse(std::string const&  value) {	Option::parse(value); return ++target; }
	virtual void set(bool value) { Option::set(value); if (value) { ++target; } else if (target > 0) {--target;} }
	virtual std::ostream& printValue(std::ostream& out) const { return out << target; }
	virtual std::string printValue() const { return io::Str() << target; }
	virtual std::string printNumericValue() const { return ""; }

	unsigned int& target;
};

template<typename T>
struct ArgumentOption : Option
{
	ArgumentOption(T& target, char shortName, std::string longName, std::string desc, bool isAdvanced, std::string argName)
	: Option(shortName, longName, desc, isAdvanced, true, argName), target(target)
	{}
	virtual bool parse(std::string const&  value) {	Option::parse(value); return io::stringToValue(value, target); }
	virtual std::string printValue() const { return io::Str() << target; }
	virtual std::ostream& printValue(std::ostream& out) const { return out << target; }
	virtual std::string printNumericValue() const { return TypeInfo<T>::isNumeric()? printValue() : ""; }

	T& target;
};

template<>
struct ArgumentOption<bool> : Option
{
	ArgumentOption(bool& target, char shortName, std::string longName, std::string desc, bool isAdvanced)
	: Option(shortName, longName, desc, isAdvanced, false), target(target)
	{}
	virtual bool parse(std::string const&  value) {	Option::parse(value); return target = true; }
	virtual void set(bool value) { Option::set(value); target = value; }
	virtual std::ostream& printValue(std::ostream& out) const { return out << target; }
	virtual std::string printValue() const { return io::Str() << target; }
	virtual std::string printNumericValue() const { return ""; }

	bool& target;
};

struct ParsedOption
{
	ParsedOption(const Option& opt) :
		shortName(opt.shortName),
		longName(opt.longName),
		description(opt.description),
		isAdvanced(opt.isAdvanced),
		requireArgument(opt.requireArgument),
		incrementalArgument(opt.incrementalArgument),
		argumentName(opt.argumentName),
		negated(opt.negated),
		value(opt.printValue())
	{}

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
	bool isAdvanced;
	bool requireArgument;
	bool incrementalArgument;
	std::string argumentName;
	bool negated;
	std::string value;
};

struct TargetBase
{
	TargetBase(std::string variableName, std::string desc, bool isAdvanced)
	: variableName(variableName), description(desc), isOptionalVector(false), isAdvanced(isAdvanced)
	{}
	virtual ~TargetBase() {}
	virtual bool parse(std::string const& value) = 0;

	std::string variableName;
	std::string description;
	bool isOptionalVector;
	bool isAdvanced;
};

template<typename T>
struct Target : TargetBase
{
	Target(T& target, std::string variableName, std::string desc, bool isAdvanced)
	: TargetBase(variableName, desc, isAdvanced), target(target)
	{}
	virtual ~Target() {}

	virtual bool parse(std::string const&  value)
	{
		return io::stringToValue(value, target);
	}

	T& target;
};

template<typename T>
struct OptionalTargets : TargetBase
{
	OptionalTargets(std::vector<T>& target, std::string variableName, std::string desc, bool isAdvanced)
	: TargetBase(variableName, desc, isAdvanced), targets(target)
	{ isOptionalVector = true; }
	virtual ~OptionalTargets() {}

	virtual bool parse(std::string const&  value)
	{
		T target;
		bool ok = io::stringToValue(value, target);
		if (ok)
			targets.push_back(target);
		return ok;
	}
	std::vector<T>& targets;
};

class ProgramInterface
{
public:
	ProgramInterface(std::string name, std::string shortDescription, std::string version);
	virtual ~ProgramInterface();

	void addProgramDescription(std::string desc)
	{
		m_programDescription = desc;
	}

	template<typename T>
	void addNonOptionArgument(T& target, std::string variableName, std::string desc, bool isAdvanced = false)
	{
		TargetBase* t = new Target<T>(target, variableName, desc, isAdvanced);
		m_nonOptionArguments.push_back(t);
	}

	template<typename T>
	void addOptionalNonOptionArguments(std::vector<T>& target, std::string variableName, std::string desc, bool isAdvanced = false)
	{
		if (m_numOptionalNonOptionArguments != 0)
			throw OptionConflictError("Can't have two non-option vector arguments");
		++m_numOptionalNonOptionArguments;
		TargetBase* t = new OptionalTargets<T>(target, variableName, desc, isAdvanced);
		m_nonOptionArguments.push_back(t);
	}

	void addOptionArgument(char shortName, std::string longName, std::string description, bool isAdvanced = false)
	{
		m_optionArguments.push_back(new Option(shortName, longName, description, isAdvanced));
	}

	void addIncrementalOptionArgument(unsigned int& target, char shortName, std::string longName, std::string description, bool isAdvanced = false)
	{
		Option* o = new IncrementalOption(target, shortName, longName, description, isAdvanced);
		m_optionArguments.push_back(o);
	}

	void addOptionArgument(bool& target, char shortName, std::string longName, std::string description, bool isAdvanced = false)
	{
		Option* o = new ArgumentOption<bool>(target, shortName, longName, description, isAdvanced);
		m_optionArguments.push_back(o);
	}

	// Without shortName
	void addOptionArgument(bool& target, std::string longName, std::string description, bool isAdvanced = false)
	{
		Option* o = new ArgumentOption<bool>(target, '\0', longName, description, isAdvanced);
		m_optionArguments.push_back(o);
	}

	template<typename T>
	void addOptionArgument(T& target, char shortName, std::string longName, std::string description, std::string argumentName, bool isAdvanced = false)
	{
		Option* o = new ArgumentOption<T>(target, shortName, longName, description, isAdvanced, argumentName);
		m_optionArguments.push_back(o);
	}

	// Without shortName
	template<typename T>
	void addOptionArgument(T& target, std::string longName, std::string description, std::string argumentName, bool isAdvanced = false)
	{
		Option* o = new ArgumentOption<T>(target, '\0', longName, description, isAdvanced, argumentName);
		m_optionArguments.push_back(o);
	}

	void parseArgs(const std::string& args);

	std::vector<ParsedOption> getUsedOptionArguments();

	unsigned int numRequiredArguments() { return m_nonOptionArguments.size() - m_numOptionalNonOptionArguments; }

private:
	void exitWithUsage(bool showAdvanced);
	void exitWithVersionInformation();
	void exitWithError(std::string message);

	std::vector<option> m_longOptions; // struct option defined in <getopt.h>
	std::deque<Option*> m_optionArguments;
	std::deque<TargetBase*> m_nonOptionArguments;
	std::string m_programName;
	std::string m_shortProgramDescription;
	std::string m_programVersion;
	std::string m_programDescription;
	std::string m_executableName;
	unsigned int m_displayHelp;
	bool m_displayVersion;
	bool m_negateNextOption;

	unsigned int m_numOptionalNonOptionArguments;
};

#ifdef NS_INFOMAP
}
#endif

#endif /* ARGPARSER_H_ */
