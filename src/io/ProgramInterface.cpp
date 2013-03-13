/* ------------------------------------------------------------------------

 Infomap software package for multi-level network clustering

 * Copyright (c) 2013
 * For license and authors, see README.txt or http://www.mapequation.org

------------------------------------------------------------------------ */

#include "ProgramInterface.h"
#include <iostream>
#include <cstdlib>
#include "../io/convert.h"

ProgramInterface::ProgramInterface(std::string name, std::string shortDescription, std::string version)
: m_programName(name),
  m_shortProgramDescription(shortDescription),
  m_programVersion(version),
  m_programDescription(""),
  m_executableName(""),
  m_displayHelp(false),
  m_displayVersion(false),
  m_negateNextOption(false)
{
	addOptionArgument(m_displayHelp, 'h', "help", "Prints this help message.");
	addOptionArgument(m_displayVersion, 'V', "version", "Display program version information.");
	addOptionArgument(m_negateNextOption, 'n', "negate-next", "Set the next (no-argument) option to false.");
}

ProgramInterface::~ProgramInterface()
{
	for (unsigned int i = 0; i < m_nonOptionArguments.size(); ++i)
		delete m_nonOptionArguments[i];
	for (unsigned int i = 0; i < m_optionArguments.size(); ++i)
		delete m_optionArguments[i];
}

void ProgramInterface::exitWithUsage()
{
	std::cout << "Name:" << std::endl;
	std::cout << "        " << m_programName << " - " << m_shortProgramDescription << std::endl;
	std::cout << "\nUsage:" << std::endl;
	std::cout << "        " << m_executableName;
	for (unsigned int i = 0; i < m_nonOptionArguments.size(); ++i)
		std::cout << " [" << m_nonOptionArguments[i]->variableName << "]";
	if (!m_optionArguments.empty())
		std::cout << " [OPTIONS]";
	std::cout << std::endl;

	if (m_programDescription != "")
		std::cout << "\nDescription: " << m_programDescription << std::endl;

	for (unsigned int i = 0; i < m_nonOptionArguments.size(); ++i)
		std::cout << "\n[" << m_nonOptionArguments[i]->variableName << "]\n    " << m_nonOptionArguments[i]->description << std::endl;

	if (!m_optionArguments.empty())
		std::cout << "\n[OPTIONS]" << std::endl;

	// First stringify the options part to get the maximum length
	std::deque<std::string> optionStrings(m_optionArguments.size());
	std::string::size_type maxLength = 0;
	for (unsigned int i = 0; i < m_optionArguments.size(); ++i)
	{
		Option& opt = *m_optionArguments[i];
		if (opt.requireArgument)
			optionStrings[i] = io::Str() << "  -" << opt.shortName << "<" << opt.argumentName <<
			"> --" << opt.longName << "=<" << opt.argumentName << ">";
		else
			optionStrings[i] = io::Str() << "  -" << opt.shortName << "    --" << opt.longName;
		if (optionStrings[i].length() > maxLength)
			maxLength = optionStrings[i].length();
	}

	for (unsigned int i = 0; i < m_optionArguments.size(); ++i)
	{
		Option& opt = *m_optionArguments[i];
		std::string::size_type numSpaces = maxLength + 3 - optionStrings[i].length();
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
	std::cerr << message << "Run with argument '-h' for usage information." << std::endl;
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
	for (unsigned int i = 0; i < m_optionArguments.size(); ++i)
	{
		Option& opt = *m_optionArguments[i];
		option o = {opt.longName.c_str(), opt.requireArgument ? required_argument : no_argument, 0, opt.shortName};
		longOptions.push_back(o);
		shortOptions += opt.shortName;
		if (opt.requireArgument)
			shortOptions += ":";
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
		if (m_displayHelp)
		{
			exitWithUsage();
		}
		else if (m_displayVersion)
		{
			exitWithVersionInformation();
		}
		if (parsed)
			continue;
		switch (c)
		{
		case 0:
			std::cout << "long option " << longOptions[option_index].name << " with arg " <<
			(optarg? optarg : "void") << std::endl;
			break;
		case '?':
			exitWithError("");
			break;
		default:
			std::cout << "?? getopt returned character code '" << c << "' ??" << std::endl;
			break;
		}
		std::cout << "  ---- " << std::endl;
	}
	// non-option ARGV-elements:
	unsigned int argsLeft = argc - optind;
	if (argsLeft != m_nonOptionArguments.size())
		exitWithError("Argument error. ");

	for (unsigned int i = 0; i < m_nonOptionArguments.size(); ++i)
	{
		if (!m_nonOptionArguments[i]->parse(argv[optind++]))
			exitWithError("Argument error. ");
	}

}

