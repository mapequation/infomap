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


#include "ProgramInterface.h"
#include <iostream>
#include <cstdlib>
#include <map>
#include <utility>
#include "../io/convert.h"
#include "../utils/Logger.h"

#ifdef NS_INFOMAP
namespace infomap
{
#endif

ProgramInterface::ProgramInterface(std::string name, std::string shortDescription, std::string version)
: m_programName(name),
  m_shortProgramDescription(shortDescription),
  m_programVersion(version),
  m_programDescription(""),
  m_executableName("Infomap"),
  m_displayHelp(false),
  m_displayVersion(false),
  m_negateNextOption(false),
  m_numOptionalNonOptionArguments(0)
{
	addIncrementalOptionArgument(m_displayHelp, 'h', "help", "Prints this help message. Use -hh to show advanced options.");
	addOptionArgument(m_displayVersion, 'V', "version", "Display program version information.");
	addOptionArgument(m_negateNextOption, 'n', "negate-next", "Set the next (no-argument) option to false.", true);
}

ProgramInterface::~ProgramInterface()
{
	for (unsigned int i = 0; i < m_nonOptionArguments.size(); ++i)
		delete m_nonOptionArguments[i];
	for (unsigned int i = 0; i < m_optionArguments.size(); ++i)
		delete m_optionArguments[i];
}

void ProgramInterface::exitWithUsage(bool showAdvanced)
{
	Log() << "Name:" << std::endl;
	Log() << "        " << m_programName << " - " << m_shortProgramDescription << std::endl;
	Log() << "\nUsage:" << std::endl;
	Log() << "        " << m_executableName;
	for (unsigned int i = 0; i < m_nonOptionArguments.size(); ++i)
		if (showAdvanced || !m_nonOptionArguments[i]->isAdvanced)
			Log() << " " << m_nonOptionArguments[i]->variableName;
	if (!m_optionArguments.empty())
		Log() << " [options]";
	Log() << std::endl;

	if (m_programDescription != "")
		Log() << "\nDescription:\n        " << m_programDescription << std::endl;

	for (unsigned int i = 0; i < m_nonOptionArguments.size(); ++i)
		if (showAdvanced || !m_nonOptionArguments[i]->isAdvanced)
			Log() << "\n[" << m_nonOptionArguments[i]->variableName << "]\n    " << m_nonOptionArguments[i]->description << std::endl;

	if (!m_optionArguments.empty())
		Log() << "\n[options]" << std::endl;

	// First stringify the options part to get the maximum length
	std::deque<std::string> optionStrings(m_optionArguments.size());
	std::string::size_type maxLength = 0;
	for (unsigned int i = 0; i < m_optionArguments.size(); ++i)
	{
		Option& opt = *m_optionArguments[i];
		bool haveShort = opt.shortName != '\0';
		std::string optArg = opt.requireArgument ? (io::Str() << "<" << opt.argumentName << ">") :
				opt.incrementalArgument? "[+]" : std::string(3, ' ');
		std::string shortOption = haveShort ? (io::Str() <<  "  -" << opt.shortName << optArg) : std::string(7, ' ');
		optionStrings[i] = io::Str() << shortOption << " --" << opt.longName << (opt.requireArgument? ' ' : ' ') << optArg;
		if (optionStrings[i].length() > maxLength)
			maxLength = optionStrings[i].length();
	}

	for (unsigned int i = 0; i < m_optionArguments.size(); ++i)
	{
		Option& opt = *m_optionArguments[i];
		std::string::size_type numSpaces = maxLength + 3 - optionStrings[i].length();
		if (showAdvanced || !opt.isAdvanced) {
			Log() << optionStrings[i] << std::string(numSpaces, ' ') << opt.description;
			if (!opt.printNumericValue().empty())
				Log() << " (Default: " << opt.printNumericValue() << ")";
			Log() << "\n";
		}
	}
	Log() << std::endl;
	std::exit(0);
}

void ProgramInterface::exitWithVersionInformation()
{
	Log() << m_programName << " version " << m_programVersion << std::endl;
	Log() << "See www.mapequation.org for more info." << std::endl;
	std::exit(0);
}

void ProgramInterface::exitWithError(std::string message)
{
	Log() << m_programName << " version " << m_programVersion << std::endl;
	std::cerr << message;
	Log() << "\nUsage: " << m_executableName;
	for (unsigned int i = 0; i < m_nonOptionArguments.size(); ++i)
		if (!m_nonOptionArguments[i]->isAdvanced)
			Log() << " " << m_nonOptionArguments[i]->variableName;
	if (!m_optionArguments.empty())
		Log() << " [options]";
	Log() << ". Run with option '-h' for more information." << std::endl;
	std::exit(1);
}

void ProgramInterface::parseArgs(const std::string& args)
{
	// Map the options on short and long name, and check for duplication
	std::map<char, Option*> shortOptionMap;
	std::map<std::string, Option*> longOptionMap;
	for (unsigned int i = 0; i < m_optionArguments.size(); ++i)
	{
		Option& opt = *m_optionArguments[i];
		if (opt.shortName != '\0')
		{
			std::map<char, Option*>::iterator it = shortOptionMap.find(opt.shortName);
			if (it != shortOptionMap.end())
				throw OptionConflictError(io::Str() << "Duplication of option '" << opt.shortName << "'");
			shortOptionMap.insert(std::make_pair(opt.shortName, &opt));
		}

		std::map<std::string, Option*>::iterator it = longOptionMap.find(opt.longName);
		if (it != longOptionMap.end())
			throw OptionConflictError(io::Str() << "Duplication of option \"" << opt.longName << "\"");
		longOptionMap.insert(std::make_pair(opt.longName, &opt));
	}

	// Split the flags on whitespace
	std::vector<std::string> flags;
	std::istringstream argStream(args);
	std::string arg;
	while (!(argStream >> arg).fail())
		flags.push_back(arg);

	std::deque<std::string> nonOpts;
	try
	{
		for (unsigned int i = 0; i < flags.size(); ++i)
		{
			bool negate = m_negateNextOption;
			m_negateNextOption = false;
			unsigned int numArgsLeft = flags.size() - i - 1;

			const std::string& arg = flags[i];
			if (arg.length() == 0)
				throw InputSyntaxError("Illegal argument ''");

			if (arg[0] != '-') {
				nonOpts.push_back(arg);
			}
			else
			{
				if (arg.length() < 2)
					throw InputSyntaxError("Illegal argument '-'");

				if (arg[1] == '-')
				{
					// Long option
					if (arg.length() < 3)
						throw InputSyntaxError("Illegal argument '--'");
					std::string longOpt = arg.substr(2);
					std::map<std::string, Option*>::iterator it = longOptionMap.find(longOpt);
					if (it == longOptionMap.end())
						throw InputDomainError(io::Str() << "Unrecognized option: '--" << longOpt << "'");
					Option& opt = *it->second;
					if (!opt.requireArgument || opt.incrementalArgument)
						opt.set(!negate);
					else
					{
						if (numArgsLeft == 0)
							throw InputDomainError(io::Str() << "Option '" << opt.longName << "' requires argument");
						++i;
						if (!opt.parse(flags[i]))
							throw InputDomainError(io::Str() << "Cannot parse '" << flags[i] << "' as argument to option '" <<
									opt.longName << "'. ");
					}
				}
				else
				{
					// Short option(s)
					for (unsigned int j = 1; j < arg.length(); ++j)
					{
						negate = m_negateNextOption;
						m_negateNextOption = false;
						char o = arg[j];
						unsigned int numCharsLeft = arg.length() - j - 1;
						std::map<char, Option*>::iterator it = shortOptionMap.find(o);
						if (it == shortOptionMap.end())
							throw InputDomainError(io::Str() << "Unrecognized option: '-" << o << "'");
						Option& opt = *it->second;
						if (!opt.requireArgument || opt.incrementalArgument)
							opt.set(!negate);
						else
						{
							std::string optArg;
							if (numCharsLeft > 0)
							{
								optArg = arg.substr(j + 1);
								j = arg.length() - 1;
							}
							else if (numArgsLeft)
							{
								++i;
								optArg = flags[i];
							}
							else
								throw InputDomainError(io::Str() << "Option '" << opt.longName << "' requires argument");

							if (!opt.parse(optArg))
								throw InputDomainError(io::Str() << "Cannot parse '" << optArg << "' as argument to option '" <<
										opt.longName << "'. ");
						}
					}
				}

			}
			if (m_displayHelp > 0)
				exitWithUsage(m_displayHelp > 1);
			if (m_displayVersion)
				exitWithVersionInformation();
		}
	}
	catch (std::exception& e)
	{
		exitWithError(e.what());
	}

	if (nonOpts.size() < numRequiredArguments())
		exitWithError("Missing required arguments.");

	unsigned int i = 0;
	unsigned int numVectorArguments = nonOpts.size() - (m_nonOptionArguments.size() - 1);
	while (!nonOpts.empty())
	{
		std::string arg = nonOpts.front();
		nonOpts.pop_front();
		if (m_nonOptionArguments[i]->isOptionalVector && numVectorArguments == 0)
			++i;
		if (!m_nonOptionArguments[i]->parse(arg))
			exitWithError("Argument error.");
		if (!m_nonOptionArguments[i]->isOptionalVector || --numVectorArguments == 0)
			++i;
	}
}

std::vector<ParsedOption> ProgramInterface::getUsedOptionArguments()
{
	std::vector<ParsedOption> opts;
	unsigned int numFlags = m_optionArguments.size();
	for (unsigned int i = 0; i < numFlags; ++i)
	{
		Option& opt = *m_optionArguments[i];
		if (opt.used && opt.longName != "negate-next")
			opts.push_back(ParsedOption(opt));
	}
	return opts;
}

#ifdef NS_INFOMAP
}
#endif
