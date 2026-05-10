/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include "ProgramInterface.h"
#include "../utils/Log.h"

#include <map>
#include <utility>

namespace infomap {

const std::string ArgType::integer = "integer";
const std::string ArgType::number = "number";
const std::string ArgType::string = "string";
const std::string ArgType::path = "path";
const std::string ArgType::probability = "probability";
const std::string ArgType::option = "option";
const std::string ArgType::list = "list";

const std::unordered_map<std::string, char> ArgType::toShort = {
  { "integer", 'n' },
  { "number", 'f' },
  { "string", 's' },
  { "path", 'p' },
  { "probability", 'P' },
  { "option", 'o' },
  { "list", 'l' },
};

namespace {

  using ShortOptionMap = std::map<char, Option*>;
  using LongOptionMap = std::map<std::string, Option*>;
  using NonOptionArguments = std::deque<std::unique_ptr<TargetBase>>;

  struct OptionLookup {
    ShortOptionMap shortOptions;
    LongOptionMap longOptions;
  };

  OptionLookup buildOptionLookup(std::deque<std::unique_ptr<Option>>& options)
  {
    OptionLookup lookup;
    for (auto& optionArgument : options) {
      auto& opt = *optionArgument;
      if (opt.shortName != '\0') {
        auto inserted = lookup.shortOptions.insert(std::make_pair(opt.shortName, &opt));
        if (!inserted.second)
          throw std::runtime_error(io::Str() << "Duplication of option '" << opt.shortName << "'");
      }

      auto inserted = lookup.longOptions.insert(std::make_pair(opt.longName, &opt));
      if (!inserted.second)
        throw std::runtime_error(io::Str() << "Duplication of option \"" << opt.longName << "\"");
    }
    return lookup;
  }

  std::vector<std::string> tokenizeArgs(const std::string& args)
  {
    std::vector<std::string> flags;
    std::istringstream argStream(args);
    std::string arg;
    while (!(argStream >> arg).fail())
      flags.push_back(arg);
    return flags;
  }

  void parseOptionValue(Option& opt, const std::string& value)
  {
    if (!opt.parse(value))
      throw std::runtime_error(io::Str() << "Cannot parse '" << value << "' as argument to option '" << opt.longName << "'. ");
  }

  void parseLongOption(const std::string& arg, const LongOptionMap& longOptions, const std::vector<std::string>& flags, unsigned int& i)
  {
    if (arg.length() < 3)
      throw std::runtime_error("Illegal argument '--'");

    std::string longOpt = arg.substr(2);
    bool flagValue = true;
    auto it = longOptions.find(longOpt);
    if (it == longOptions.end()) {
      // Unrecognized option, check if it negates a recognised option with the '--no-' prefix
      if (longOpt.compare(0, 3, "no-") == 0 && longOptions.find(std::string(longOpt, 3)) != longOptions.end()) {
        longOpt = std::string(longOpt, 3);
        it = longOptions.find(longOpt);
        flagValue = false;
      } else {
        throw std::runtime_error(io::Str() << "Unrecognized option: '--" << longOpt << "'");
      }
    }

    auto& opt = *it->second;
    if (!opt.requireArgument || opt.incrementalArgument) {
      opt.set(flagValue);
      return;
    }

    const auto numArgsLeft = flags.size() - i - 1;
    if (numArgsLeft == 0)
      throw std::runtime_error(io::Str() << "Option '" << opt.longName << "' requires argument");

    ++i;
    parseOptionValue(opt, flags[i]);
  }

  void parseShortOptions(const std::string& arg, const ShortOptionMap& shortOptions, const std::vector<std::string>& flags, unsigned int& i)
  {
    for (unsigned int j = 1; j < arg.length(); ++j) {
      const auto optionName = arg[j];
      const auto numCharsLeft = arg.length() - j - 1;
      auto it = shortOptions.find(optionName);
      if (it == shortOptions.end())
        throw std::runtime_error(io::Str() << "Unrecognized option: '-" << optionName << "'");

      auto& opt = *it->second;
      if (!opt.requireArgument || opt.incrementalArgument) {
        opt.set(true);
        continue;
      }

      std::string optArg;
      const auto numArgsLeft = flags.size() - i - 1;
      if (numCharsLeft > 0) {
        optArg = arg.substr(j + 1);
        j = arg.length() - 1;
      } else if (numArgsLeft > 0) {
        ++i;
        optArg = flags[i];
      } else {
        throw std::runtime_error(io::Str() << "Option '" << opt.longName << "' requires argument");
      }

      parseOptionValue(opt, optArg);
    }
  }

  void parseNonOptionArguments(std::deque<std::string>& nonOpts, NonOptionArguments& nonOptionArguments, unsigned int numRequiredArguments)
  {
    if (nonOpts.size() < numRequiredArguments)
      throw std::runtime_error("Missing required arguments.");

    unsigned int i = 0;
    unsigned int numVectorArguments = nonOpts.size() - (nonOptionArguments.size() - 1);
    while (!nonOpts.empty()) {
      std::string arg = nonOpts.front();
      nonOpts.pop_front();
      if (nonOptionArguments[i]->isOptionalVector && numVectorArguments == 0)
        ++i;
      if (!nonOptionArguments[i]->parse(arg))
        throw std::runtime_error("Argument error.");
      if (!nonOptionArguments[i]->isOptionalVector || --numVectorArguments == 0)
        ++i;
    }
  }

  std::string join(const std::vector<std::string>& values, const std::string& separator)
  {
    std::ostringstream out;
    for (unsigned int i = 0; i < values.size(); ++i) {
      if (i > 0)
        out << separator;
      out << values[i];
    }
    return out.str();
  }

  std::string bashWords(const std::vector<std::string>& values)
  {
    return join(values, " ");
  }

  std::string zshDescription(std::string value)
  {
    for (char& c : value) {
      if (c == '\'' || c == '[' || c == ']' || c == ':' || c == '\\')
        c = ' ';
    }
    return value;
  }

  std::string zshAction(const Option& opt)
  {
    if (!opt.choices.empty())
      return io::Str() << "(" << join(opt.choices, " ") << ")";
    if (opt.argumentName == ArgType::path)
      return "_files";
    return "";
  }

  std::vector<std::string> visibleOptionWords(const std::deque<std::unique_ptr<Option>>& options)
  {
    std::vector<std::string> words;
    for (const auto& optionArgument : options) {
      const auto& opt = *optionArgument;
      if (opt.hidden)
        continue;
      if (opt.shortName != '\0')
        words.push_back(io::Str() << "-" << opt.shortName);
      words.push_back(io::Str() << "--" << opt.longName);
    }
    return words;
  }

  std::vector<std::string> valueOptionWords(const std::deque<std::unique_ptr<Option>>& options)
  {
    std::vector<std::string> words;
    for (const auto& optionArgument : options) {
      const auto& opt = *optionArgument;
      if (opt.hidden || !opt.requireArgument)
        continue;
      if (opt.shortName != '\0')
        words.push_back(io::Str() << "-" << opt.shortName);
      words.push_back(io::Str() << "--" << opt.longName);
    }
    return words;
  }

  std::vector<std::string> pathOptionWords(const std::deque<std::unique_ptr<Option>>& options)
  {
    std::vector<std::string> words;
    for (const auto& optionArgument : options) {
      const auto& opt = *optionArgument;
      if (opt.hidden || opt.argumentName != ArgType::path)
        continue;
      if (opt.shortName != '\0')
        words.push_back(io::Str() << "-" << opt.shortName);
      words.push_back(io::Str() << "--" << opt.longName);
    }
    return words;
  }

} // namespace

ProgramInterface::ProgramInterface(std::string name, std::string shortDescription, std::string version)
    : m_programName(std::move(name)),
      m_shortProgramDescription(std::move(shortDescription)),
      m_programVersion(std::move(version))
{
  addIncrementalOptionArgument(m_displayHelp, 'h', "help", "Prints this help message. Use -hh to show advanced options.", "About");
  addOptionArgument(m_displayVersion, 'V', "version", "Display program version information.", "About");
  addOptionArgument(m_completionShell, "completion", "Print shell completion script for bash or zsh.", ArgType::option, "About").setChoices({ "bash", "zsh" });
  addOptionArgument(m_printJsonParameters, "print-json-parameters", "Print Infomap parameters in JSON.", "About").setHidden(true);
}

void ProgramInterface::exitWithUsage(bool showAdvanced) const
{
  Log() << "Name:\n";
  Log() << "        " << m_programName << " - " << m_shortProgramDescription << '\n';
  Log() << "\nUsage:\n";
  Log() << "        " << m_executableName;
  for (auto& nonOptionArgument : m_nonOptionArguments)
    if (showAdvanced || !nonOptionArgument->isAdvanced)
      Log() << " " << nonOptionArgument->variableName;
  if (!m_optionArguments.empty())
    Log() << " [options]";
  Log() << '\n';

  if (!m_programDescription.empty())
    Log() << "\nDescription:\n        " << m_programDescription << '\n';

  for (auto& nonOptionArgument : m_nonOptionArguments)
    if (showAdvanced || !nonOptionArgument->isAdvanced)
      Log() << "\n[" << nonOptionArgument->variableName << "]\n    " << nonOptionArgument->description << '\n';

  if (!m_optionArguments.empty())
    Log() << "\n[options]\n";

  // First stringify the options part to get the maximum length
  std::deque<std::string> optionStrings(m_optionArguments.size());
  std::string::size_type maxLength = 0;
  for (unsigned int i = 0; i < m_optionArguments.size(); ++i) {
    auto& opt = *m_optionArguments[i];
    bool haveShort = opt.shortName != '\0';
    std::string optArgShort = opt.requireArgument ? (io::Str() << "<" << ArgType::toShort.at(opt.argumentName) << ">") : opt.incrementalArgument ? "[+]"
                                                                                                                                                 : std::string(3, ' ');
    std::string optArgLong = opt.requireArgument ? (io::Str() << "<" << opt.argumentName << ">") : opt.incrementalArgument ? "[+]"
                                                                                                                           : std::string(3, ' ');
    std::string shortOption = haveShort ? (io::Str() << "  -" << opt.shortName << optArgShort) : std::string(7, ' ');
    optionStrings[i] = io::Str() << shortOption << " --" << opt.longName << " " << optArgLong;
    if (optionStrings[i].length() > maxLength)
      maxLength = optionStrings[i].length();
  }

  std::vector<std::string> groups { "About" };
  for (auto& group : m_groups) {
    if (group != "About")
      groups.push_back(group);
  }
  if (m_groups.empty())
    groups.emplace_back("All");

  for (const auto& group : groups) {
    if (group != "All") {
      Log() << "\n"
            << group << "\n";
      Log() << std::string(group.length(), '-') << "\n";
    }
    for (unsigned int i = 0; i < m_optionArguments.size(); ++i) {
      auto& opt = *m_optionArguments[i];
      if (group == "All" || opt.group == group) {
        std::string::size_type numSpaces = maxLength + 3 - optionStrings[i].length();
        if (showAdvanced || !opt.isAdvanced) {
          Log() << optionStrings[i] << std::string(numSpaces, ' ') << opt.description;
          if (!opt.printNumericValue().empty())
            Log() << " (Default: " << opt.printNumericValue() << ")";
          Log() << "\n";
        }
      }
    }
  }
  Log() << '\n';
  throw CleanExit {};
}

void ProgramInterface::exitWithVersionInformation() const
{
  Log() << m_programName << " version " << m_programVersion;
#ifdef _OPENMP
  Log() << " compiled with OpenMP";
#endif
  Log() << '\n';
  Log() << "See www.mapequation.org for terms of use.\n";
  throw CleanExit {};
}

void ProgramInterface::exitWithError(const std::string& message) const
{
  Log() << m_programName << " version " << m_programVersion;
#ifdef _OPENMP
  Log() << " compiled with OpenMP";
#endif
  Log() << '\n';
  Log() << "Usage: " << m_executableName;
  for (auto& nonOptionArgument : m_nonOptionArguments)
    if (!nonOptionArgument->isAdvanced)
      Log() << " " << nonOptionArgument->variableName;
  if (!m_optionArguments.empty())
    Log() << " [options]";
  Log() << ". Run with option '-h' for more information.\n";
  throw std::runtime_error(message);
}

void ProgramInterface::printBashCompletion() const
{
  const auto options = visibleOptionWords(m_optionArguments);
  const auto valueOptions = valueOptionWords(m_optionArguments);
  const auto pathOptions = pathOptionWords(m_optionArguments);

  Log() << "# bash completion for Infomap\n";
  Log() << "_infomap_completion()\n";
  Log() << "{\n";
  Log() << "  local cur prev position\n";
  Log() << "  COMPREPLY=()\n";
  Log() << "  cur=\"${COMP_WORDS[COMP_CWORD]}\"\n";
  Log() << "  prev=\"${COMP_WORDS[COMP_CWORD-1]}\"\n";
  Log() << "\n";
  Log() << "  __infomap_words() { COMPREPLY=( $(compgen -W \"$1\" -- \"$cur\") ); }\n";
  Log() << "  __infomap_files() { if declare -F _filedir >/dev/null; then _filedir; else COMPREPLY=( $(compgen -f -- \"$cur\") ); fi; }\n";
  Log() << "  __infomap_dirs() { if declare -F _filedir >/dev/null; then _filedir -d; else COMPREPLY=( $(compgen -d -- \"$cur\") ); fi; }\n";
  Log() << "  __infomap_option_requires_value() {\n";
  Log() << "    case \"$1\" in\n";
  Log() << "      " << join(valueOptions, "|") << ") return 0 ;;\n";
  Log() << "    esac\n";
  Log() << "    return 1\n";
  Log() << "  }\n";
  Log() << "  __infomap_position() {\n";
  Log() << "    local i word count=0\n";
  Log() << "    for ((i = 1; i < COMP_CWORD; ++i)); do\n";
  Log() << "      word=\"${COMP_WORDS[i]}\"\n";
  Log() << "      if __infomap_option_requires_value \"$word\"; then ((++i)); continue; fi\n";
  Log() << "      [[ \"$word\" == -* ]] && continue\n";
  Log() << "      ((++count))\n";
  Log() << "    done\n";
  Log() << "    printf '%s' \"$count\"\n";
  Log() << "  }\n";
  Log() << "\n";
  Log() << "  case \"$prev\" in\n";
  Log() << "    --completion) __infomap_words \"bash zsh\"; return ;;\n";
  for (const auto& optionArgument : m_optionArguments) {
    const auto& opt = *optionArgument;
    if (opt.hidden || opt.choices.empty() || opt.longName == "completion")
      continue;
    std::vector<std::string> words;
    if (opt.shortName != '\0')
      words.push_back(io::Str() << "-" << opt.shortName);
    words.push_back(io::Str() << "--" << opt.longName);
    Log() << "    " << join(words, "|") << ") __infomap_words \"" << bashWords(opt.choices) << "\"; return ;;\n";
  }
  if (!pathOptions.empty())
    Log() << "    " << join(pathOptions, "|") << ") __infomap_files; return ;;\n";
  Log() << "  esac\n";
  Log() << "\n";
  Log() << "  case \"$cur\" in\n";
  Log() << "    -*) __infomap_words \"" << bashWords(options) << "\"; return ;;\n";
  Log() << "  esac\n";
  Log() << "\n";
  Log() << "  position=\"$(__infomap_position)\"\n";
  Log() << "  if [[ \"$position\" -ge 1 ]]; then\n";
  Log() << "    __infomap_dirs\n";
  Log() << "  else\n";
  Log() << "    __infomap_files\n";
  Log() << "  fi\n";
  Log() << "}\n";
  Log() << "\n";
  Log() << "complete -F _infomap_completion Infomap infomap\n";
}

void ProgramInterface::printZshCompletion() const
{
  Log() << "#compdef Infomap infomap\n";
  Log() << "\n";
  Log() << "_infomap() {\n";
  Log() << "  _arguments -C \\\n";
  for (const auto& optionArgument : m_optionArguments) {
    const auto& opt = *optionArgument;
    if (opt.hidden)
      continue;

    const auto desc = zshDescription(opt.description);
    const auto action = zshAction(opt);
    if (opt.shortName != '\0') {
      Log() << "    '(-" << opt.shortName << " --" << opt.longName << ")'{-" << opt.shortName << ",--" << opt.longName << "}'[" << desc << "]";
    } else {
      Log() << "    '--" << opt.longName << "[" << desc << "]";
    }
    if (opt.requireArgument) {
      Log() << ":" << opt.argumentName << ":";
      if (!action.empty())
        Log() << action;
    }
    Log() << "'";
    Log() << " \\\n";
  }
  Log() << "    '1:network file:_files' \\\n";
  Log() << "    '2:output directory:_files -/'\n";
  Log() << "}\n";
  Log() << "\n";
  Log() << "_infomap \"$@\"\n";
}

void ProgramInterface::exitWithCompletion() const
{
  if (m_completionShell == "bash") {
    printBashCompletion();
  } else if (m_completionShell == "zsh") {
    printZshCompletion();
  } else {
    throw std::runtime_error(io::Str() << "Unsupported completion shell '" << m_completionShell << "'. Supported shells: bash, zsh.");
  }

  throw CleanExit {};
}

std::string toJson(const std::string& key, const std::string& value)
{
  return io::Str() << '"' << key << "\": \"" << value << '"';
}

std::string toJson(const std::string& key, bool value)
{
  return io::Str() << '"' << key << "\": " << (value ? "true" : "false");
}

template <typename Value>
std::string toJson(const std::string& key, Value value)
{
  return io::Str() << '"' << key << "\": " << value;
}

std::string toJson(const Option& opt)
{
  std::ostringstream json;
  json << "{ "
       << toJson("long", std::string(io::Str() << "--" << opt.longName)) << ", "
       << toJson("short", opt.shortName != '\0' ? std::string(io::Str() << "-" << opt.shortName) : "") << ", "
       << toJson("description", opt.description) << ", "
       << toJson("group", opt.group) << ", "
       << toJson("required", opt.requireArgument) << ", "
       << toJson("advanced", opt.isAdvanced) << ", "
       << toJson("incremental", opt.incrementalArgument) << ", "
       << (opt.requireArgument
               ? (io::Str() << toJson("longType", opt.argumentName) << ", "
                            << toJson("shortType", std::string(1, ArgType::toShort.at(opt.argumentName))) << ", "
                            << toJson("default", opt.printValue()))
               : toJson("default", false));
  if (!opt.choices.empty()) {
    json << ", \"choices\": [";
    for (unsigned int i = 0; i < opt.choices.size(); ++i) {
      if (i > 0)
        json << ", ";
      json << '"' << opt.choices[i] << '"';
    }
    json << "]";
  }
  json << " }";
  return json.str();
}

void ProgramInterface::exitWithJsonParameters() const
{
  Log() << "{\n  \"parameters\": [\n";

  for (unsigned int i = 0; i < m_optionArguments.size(); ++i) {
    auto& opt = *m_optionArguments[i];
    if (opt.hidden)
      continue;
    Log() << "    " << toJson(opt);
    if (i < m_optionArguments.size() - 1) {
      Log() << ",\n";
    } else {
      Log() << "\n";
    }
  }
  Log() << "  ]\n}";

  throw CleanExit {};
}

void ProgramInterface::parseArgs(const std::string& args)
{
  const auto optionLookup = buildOptionLookup(m_optionArguments);
  const auto flags = tokenizeArgs(args);

  std::deque<std::string> nonOpts;
  try {
    for (unsigned int i = 0; i < flags.size(); ++i) {
      const std::string& arg = flags[i];
      if (arg.empty())
        throw std::runtime_error("Illegal argument ''");

      if (arg[0] != '-') {
        nonOpts.push_back(arg);
      } else {
        if (arg.length() < 2)
          throw std::runtime_error("Illegal argument '-'");

        if (arg[1] == '-') {
          parseLongOption(arg, optionLookup.longOptions, flags, i);
        } else {
          parseShortOptions(arg, optionLookup.shortOptions, flags, i);
        }
      }
      if (m_displayHelp > 0)
        exitWithUsage(m_displayHelp > 1);
      if (m_displayVersion)
        exitWithVersionInformation();
      if (!m_completionShell.empty())
        exitWithCompletion();
      if (m_printJsonParameters)
        exitWithJsonParameters();
    }
  } catch (std::exception& e) {
    exitWithError(e.what());
  }

  try {
    parseNonOptionArguments(nonOpts, m_nonOptionArguments, numRequiredArguments());
  } catch (std::exception& e) {
    exitWithError(e.what());
  }
}

std::vector<ParsedOption> ProgramInterface::getUsedOptionArguments() const
{
  std::vector<ParsedOption> opts;
  unsigned int numFlags = m_optionArguments.size();
  for (unsigned int i = 0; i < numFlags; ++i) {
    auto& opt = *m_optionArguments[i];
    if (opt.used && opt.longName != "negate-next")
      opts.emplace_back(opt);
  }
  return opts;
}

} // namespace infomap
