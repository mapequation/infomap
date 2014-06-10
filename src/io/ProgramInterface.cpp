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

ProgramInterface::ProgramInterface(std::string name, std::string shortDescription, std::string version)
: m_programName(name),
  m_shortProgramDescription(shortDescription),
  m_programVersion(version),
  m_programDescription(""),
  m_executableName(""),
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
	std::cout << "Name:" << std::endl;
	std::cout << "        " << m_programName << " - " << m_shortProgramDescription << std::endl;
	std::cout << "\nUsage:" << std::endl;
	std::cout << "        " << m_executableName;
	for (unsigned int i = 0; i < m_nonOptionArguments.size(); ++i)
		if (showAdvanced || !m_nonOptionArguments[i]->isAdvanced)
			std::cout << " " << m_nonOptionArguments[i]->variableName;
	if (!m_optionArguments.empty())
		std::cout << " [options]";
	std::cout << std::endl;

	if (m_programDescription != "")
		std::cout << "\nDescription: " << m_programDescription << std::endl;

	for (unsigned int i = 0; i < m_nonOptionArguments.size(); ++i)
		if (showAdvanced || !m_nonOptionArguments[i]->isAdvanced)
			std::cout << "\n[" << m_nonOptionArguments[i]->variableName << "]\n    " << m_nonOptionArguments[i]->description << std::endl;

	if (!m_optionArguments.empty())
		std::cout << "\n[options]" << std::endl;

	// First stringify the options part to get the maximum length
	std::deque<std::string> optionStrings(m_optionArguments.size());
	std::string::size_type maxLength = 0;
	for (unsigned int i = 0; i < m_optionArguments.size(); ++i)
	{
		Option& opt = *m_optionArguments[i];
		bool haveShort = opt.shortName != '\0';
		std::string optArg = opt.requireArgument ? (io::Str() << "<" << opt.argumentName << ">") : std::string(3, ' ');
		std::string shortOption = haveShort ? (io::Str() <<  "  -" << opt.shortName << optArg) : std::string(7, ' ');
		optionStrings[i] = io::Str() << shortOption << " --" << opt.longName << (opt.requireArgument? '=' : ' ') << optArg;
		if (optionStrings[i].length() > maxLength)
			maxLength = optionStrings[i].length();
	}

	for (unsigned int i = 0; i < m_optionArguments.size(); ++i)
	{
		Option& opt = *m_optionArguments[i];
		std::string::size_type numSpaces = maxLength + 3 - optionStrings[i].length();
		if (showAdvanced || !opt.isAdvanced)
			std::cout << optionStrings[i] << std::string(numSpaces, ' ') << opt.description << "\n";
	}
	std::cout << std::endl;
	std::exit(0);
}

void ProgramInterface::exitWithVersionInformation()
{
	std::cout << m_programName << " version " << m_programVersion << std::endl;
	std::cout << "See www.mapequation.org for terms of use." << std::endl;
	std::exit(0);
}

void ProgramInterface::exitWithError(std::string message)
{
	std::cout << m_programName << " version " << m_programVersion << std::endl;
	std::cerr << message;
	std::cout << "\nUsage: " << m_executableName;
	for (unsigned int i = 0; i < m_nonOptionArguments.size(); ++i)
		if (!m_nonOptionArguments[i]->isAdvanced)
			std::cout << " " << m_nonOptionArguments[i]->variableName;
	if (!m_optionArguments.empty())
		std::cout << " [options]";
	std::cout << ". Run with option '-h' for more information." << std::endl;
	std::exit(1);
}

void ProgramInterface::parseArgs(int argc, char** argv)
{
	m_executableName = argv[0];
	size_t pos = m_executableName.find_last_of("/");
	if (pos != std::string::npos)
		m_executableName = m_executableName.substr(pos + 1);

	std::vector<option> longOptions;
	std::string shortOptions;
	std::map<char, Option*> shortOptionMap;
	std::map<std::string, Option*> longOptionMap;
	for (unsigned int i = 0; i < m_optionArguments.size(); ++i)
	{
		Option& opt = *m_optionArguments[i];
		option o = {opt.longName.c_str(), opt.requireArgument ? required_argument : no_argument, 0, opt.shortName};
		longOptions.push_back(o);
		if (opt.shortName != '\0')
		{
			shortOptions += opt.shortName;
			std::map<char, Option*>::iterator it = shortOptionMap.find(opt.shortName);
			if (it != shortOptionMap.end())
				throw OptionConflictError(io::Str() << "Duplication of option '" << opt.shortName << "'");
			shortOptionMap.insert(std::make_pair(opt.shortName, &opt));
		}
		if (opt.requireArgument)
			shortOptions += ":";

		std::map<std::string, Option*>::iterator it = longOptionMap.find(opt.longName);
		if (it != longOptionMap.end())
			throw OptionConflictError(io::Str() << "Duplication of option \"" << opt.longName << "\"");
		longOptionMap.insert(std::make_pair(opt.longName, &opt));
	}
	option o = {NULL, 0, NULL, 0}; //TODO: Why add this?
	longOptions.push_back(o);


	int c, option_index = 0;
	while ((c = getopt_long(argc, argv, shortOptions.c_str(), &longOptions[0], &option_index)) != -1)
	{
		bool parsed = false;
		bool negate = m_negateNextOption;
		m_negateNextOption = false;
		for (unsigned int i = 0; i < m_optionArguments.size(); ++i)
		{
			Option& opt = *m_optionArguments[i];
			if (opt.shortName == '\0')
				continue;
			if ((int)opt.shortName == c)
			{
				if (!opt.requireArgument || opt.incrementalArgument)
				{
					opt.set(!negate);
				}
				else
				{
					if (!opt.parse(optarg))
						exitWithError(Str() << "Cannot parse argument '" << optarg << "' to option '" <<
								opt.longName << "'. ");
				}
				parsed = true;
				break;
			}
		}
		if (m_displayVersion)
		{
			exitWithVersionInformation();
		}
		if (parsed)
			continue;
		switch (c)
		{
		case 0: // Long option without a corresponding short option
		{
//			std::cout << "long option " << longOptions[option_index].name << " with arg " <<
//			(optarg? optarg : "void") << std::endl;
			Option& longOpt = *longOptionMap[longOptions[option_index].name];
			if (!longOpt.requireArgument || longOpt.incrementalArgument)
			{
				longOpt.set(!negate);
			}
			else
			{
				if (!longOpt.parse(optarg))
					exitWithError(Str() << "Cannot parse argument '" << optarg << "' to option '" <<
							longOpt.longName << "'. ");
			}
			parsed = true;
			break;
		}
		case '?':
			exitWithError("");
			break;
		default:
			std::cout << "?? getopt returned character code '" << c << "' ??" << std::endl;
			break;
		}
//		std::cout << "  ---- " << std::endl;
	}

	if (m_displayHelp > 0)
	{
		exitWithUsage(m_displayHelp > 1);
	}

	// non-option ARGV-elements:
	unsigned int argsLeft = argc - optind;

	if (m_numOptionalNonOptionArguments == 0)
	{
		if (argsLeft != m_nonOptionArguments.size())
			exitWithError("Argument error. ");
	}
	else
	{
		if (argsLeft < m_nonOptionArguments.size() - 1)
			exitWithError("Argument error. ");
	}

	if (m_nonOptionArguments.size() == 0)
		return;

	unsigned int numVectorArguments = argsLeft - (m_nonOptionArguments.size() - 1);

	std::deque<std::string> argvLeft;
	while (argvLeft.size() != argsLeft)
		argvLeft.push_back(argv[optind++]);

//	for (unsigned int i = 0; i < m_nonOptionArguments.size(); ++i, --argsLeft)
	unsigned int i = 0;
	while (!argvLeft.empty())
	{
		std::string arg = argvLeft.front();
		argvLeft.pop_front();
		if (m_nonOptionArguments[i]->isOptionalVector && numVectorArguments == 0)
			++i;
		if (!m_nonOptionArguments[i]->parse(arg))
			exitWithError("Argument error. ");
		if (!m_nonOptionArguments[i]->isOptionalVector || --numVectorArguments == 0)
			++i;
	}

}

