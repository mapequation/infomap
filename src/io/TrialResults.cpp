/*******************************************************************************
 Infomap software package for multi-level network clustering
 Copyright (c) 2013, 2014 Daniel Edler, Anton Holmgren, Martin Rosvall

 This file is part of the Infomap software package.
 See file LICENSE_GPLv3.txt for full license details.
 For more information, see <http://www.mapequation.org>
 ******************************************************************************/

#include "TrialResults.h"

#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <cctype>

namespace infomap {

namespace {

  // Local JSON string escaper — identical pattern to RunReport.cpp / ParameterCatalog.cpp.
  std::string jsonString(const std::string& value)
  {
    std::ostringstream escaped;
    escaped << '"';
    for (char c : value) {
      switch (c) {
      case '"':
      case '\\':
        escaped << '\\' << c;
        break;
      case '\b':
        escaped << "\\b";
        break;
      case '\f':
        escaped << "\\f";
        break;
      case '\n':
        escaped << "\\n";
        break;
      case '\r':
        escaped << "\\r";
        break;
      case '\t':
        escaped << "\\t";
        break;
      default:
        escaped << c;
        break;
      }
    }
    escaped << '"';
    return escaped.str();
  }

  // -----------------------------------------------------------------------
  // Minimal hand-written parser for the fixed TrialResultsFile JSON schema.
  // Does NOT attempt to be a general JSON parser — it scans for known keys
  // and reads their scalar or array values.  Whitespace-tolerant.
  // -----------------------------------------------------------------------

  // Skip whitespace starting at pos.
  void skipWhitespace(const std::string& s, std::size_t& pos)
  {
    while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) {
      ++pos;
    }
  }

  // Expect a specific character at pos (after skipping whitespace).
  // Advances pos past it. Throws on mismatch.
  void expect(const std::string& s, std::size_t& pos, char c, const std::string& source)
  {
    skipWhitespace(s, pos);
    if (pos >= s.size() || s[pos] != c) {
      throw std::runtime_error("TrialResults parse error in '" + source + "': expected '" + c + "' at position " + std::to_string(pos));
    }
    ++pos;
  }

  // Parse a quoted JSON string. pos must point to the opening '"'.
  // Returns the unescaped content and advances pos past the closing '"'.
  std::string parseJsonString(const std::string& s, std::size_t& pos, const std::string& source)
  {
    skipWhitespace(s, pos);
    if (pos >= s.size() || s[pos] != '"') {
      throw std::runtime_error("TrialResults parse error in '" + source + "': expected '\"' at position " + std::to_string(pos));
    }
    ++pos; // skip opening quote
    std::string result;
    while (pos < s.size() && s[pos] != '"') {
      if (s[pos] == '\\') {
        ++pos;
        if (pos >= s.size()) {
          throw std::runtime_error("TrialResults parse error in '" + source + "': unexpected end in escape");
        }
        switch (s[pos]) {
        case '"':
        case '\\':
        case '/':
          result += s[pos];
          break;
        case 'b':
          result += '\b';
          break;
        case 'f':
          result += '\f';
          break;
        case 'n':
          result += '\n';
          break;
        case 'r':
          result += '\r';
          break;
        case 't':
          result += '\t';
          break;
        default:
          result += s[pos];
          break;
        }
      } else {
        result += s[pos];
      }
      ++pos;
    }
    if (pos >= s.size()) {
      throw std::runtime_error("TrialResults parse error in '" + source + "': unterminated string");
    }
    ++pos; // skip closing quote
    return result;
  }

  // Parse an unsigned long value (integer, no quotes). Advances pos.
  unsigned long parseUlong(const std::string& s, std::size_t& pos, const std::string& source)
  {
    skipWhitespace(s, pos);
    if (pos >= s.size() || !std::isdigit(static_cast<unsigned char>(s[pos]))) {
      throw std::runtime_error("TrialResults parse error in '" + source + "': expected integer at position " + std::to_string(pos));
    }
    unsigned long value = 0;
    while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) {
      value = value * 10 + static_cast<unsigned long>(s[pos] - '0');
      ++pos;
    }
    return value;
  }

  // Parse a double value (may be negative, may have decimal point/exponent). Advances pos.
  double parseDouble(const std::string& s, std::size_t& pos, const std::string& source)
  {
    skipWhitespace(s, pos);
    const std::size_t start = pos;
    if (pos < s.size() && (s[pos] == '-' || s[pos] == '+')) {
      ++pos;
    }
    while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) {
      ++pos;
    }
    if (pos < s.size() && s[pos] == '.') {
      ++pos;
      while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) {
        ++pos;
      }
    }
    if (pos < s.size() && (s[pos] == 'e' || s[pos] == 'E')) {
      ++pos;
      if (pos < s.size() && (s[pos] == '+' || s[pos] == '-')) {
        ++pos;
      }
      while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) {
        ++pos;
      }
    }
    if (pos == start) {
      throw std::runtime_error("TrialResults parse error in '" + source + "': expected number at position " + std::to_string(pos));
    }
    return std::stod(s.substr(start, pos - start));
  }

  // Parse an int (possibly negative). Advances pos.
  int parseInt(const std::string& s, std::size_t& pos, const std::string& source)
  {
    skipWhitespace(s, pos);
    const std::size_t start = pos;
    if (pos < s.size() && s[pos] == '-') {
      ++pos;
    }
    while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) {
      ++pos;
    }
    if (pos == start) {
      throw std::runtime_error("TrialResults parse error in '" + source + "': expected integer at position " + std::to_string(pos));
    }
    return std::stoi(s.substr(start, pos - start));
  }

  // Scan forward to find the next occurrence of key (a quoted JSON key) and
  // advance pos to just after the ':'. Returns false if not found.
  bool findKey(const std::string& s, std::size_t& pos, const std::string& key)
  {
    // Build search pattern: "key":
    const std::string pattern = "\"" + key + "\"";
    const std::size_t found = s.find(pattern, pos);
    if (found == std::string::npos) {
      return false;
    }
    pos = found + pattern.size();
    // skip whitespace then ':'
    while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) {
      ++pos;
    }
    if (pos >= s.size() || s[pos] != ':') {
      return false;
    }
    ++pos; // skip ':'
    return true;
  }

  // Parse the trials array, starting with pos at/before the '['.
  std::vector<TrialResultEntry> parseTrialsArray(const std::string& s, std::size_t& pos, const std::string& source)
  {
    std::vector<TrialResultEntry> trials;
    expect(s, pos, '[', source);
    skipWhitespace(s, pos);
    if (pos < s.size() && s[pos] == ']') {
      ++pos;
      return trials;
    }

    while (pos < s.size()) {
      // Each trial is a JSON object { "trial":N, "seed":N, "codelength":D,
      //   "num_top_modules":N, "num_levels":N, "thread":N, "time_s":D }
      expect(s, pos, '{', source);

      TrialResultEntry entry;
      // Required keys must be present so a malformed/partially-written shard
      // cannot win merge selection with default 0 values.
      bool seenTrial = false;
      bool seenSeed = false;
      bool seenCodelength = false;
      // Parse all key/value pairs in this object without assuming order.
      bool first = true;
      while (true) {
        skipWhitespace(s, pos);
        if (pos >= s.size()) {
          throw std::runtime_error("TrialResults parse error in '" + source + "': unexpected end inside trial object");
        }
        if (s[pos] == '}') {
          ++pos;
          break;
        }
        if (!first) {
          expect(s, pos, ',', source);
        }
        first = false;

        // Read key
        const std::string key = parseJsonString(s, pos, source);
        expect(s, pos, ':', source);

        if (key == "trial") {
          entry.trial = static_cast<unsigned int>(parseUlong(s, pos, source));
          seenTrial = true;
        } else if (key == "seed") {
          entry.seed = parseUlong(s, pos, source);
          seenSeed = true;
        } else if (key == "codelength") {
          entry.codelength = parseDouble(s, pos, source);
          seenCodelength = true;
        } else if (key == "num_top_modules") {
          entry.numTopModules = static_cast<unsigned int>(parseUlong(s, pos, source));
        } else if (key == "num_levels") {
          entry.numLevels = static_cast<unsigned int>(parseUlong(s, pos, source));
        } else if (key == "thread") {
          entry.thread = parseInt(s, pos, source);
        } else if (key == "time_s") {
          entry.timeSec = parseDouble(s, pos, source);
        } else {
          // Unknown key — skip its value (string or number; objects/arrays not expected)
          skipWhitespace(s, pos);
          if (pos < s.size() && s[pos] == '"') {
            parseJsonString(s, pos, source);
          } else {
            // Skip a number-like token
            while (pos < s.size() && s[pos] != ',' && s[pos] != '}') {
              ++pos;
            }
          }
        }
      }
      if (!seenTrial || !seenSeed || !seenCodelength) {
        throw std::runtime_error("TrialResults parse error in '" + source
                                 + "': trial object is missing a required field (trial, seed, codelength)");
      }
      trials.push_back(entry);

      skipWhitespace(s, pos);
      if (pos >= s.size()) break;
      if (s[pos] == ']') {
        ++pos;
        break;
      }
      if (s[pos] == ',') {
        ++pos;
      }
    }
    return trials;
  }

} // namespace

// ---------------------------------------------------------------------------
// serializeTrialResults
// ---------------------------------------------------------------------------

std::string serializeTrialResults(const TrialResultsFile& r)
{
  std::ostringstream out;
  out << std::setprecision(12);
  out << '{'
      << "\"network_fingerprint\":" << jsonString(r.networkFingerprint) << ','
      << "\"config_fingerprint\":" << jsonString(r.configFingerprint) << ','
      << "\"infomap_version\":" << jsonString(r.infomapVersion) << ','
      << "\"base_seed\":" << r.baseSeed << ','
      << "\"trial_offset\":" << r.trialOffset << ','
      << "\"num_trials\":" << r.numTrials << ','
      << "\"best_tree_file\":" << jsonString(r.bestTreeFile) << ','
      << "\"trials\":[";

  bool first = true;
  for (const auto& t : r.trials) {
    if (!first) out << ',';
    first = false;
    out << '{'
        << "\"trial\":" << t.trial << ','
        << "\"seed\":" << t.seed << ','
        << "\"codelength\":" << t.codelength << ','
        << "\"num_top_modules\":" << t.numTopModules << ','
        << "\"num_levels\":" << t.numLevels << ','
        << "\"thread\":" << t.thread << ','
        << "\"time_s\":" << t.timeSec
        << '}';
  }

  out << "]}\n";
  return out.str();
}

// ---------------------------------------------------------------------------
// parseTrialResults
// ---------------------------------------------------------------------------

TrialResultsFile parseTrialResults(const std::string& json, const std::string& sourcePath)
{
  TrialResultsFile result;
  std::size_t pos = 0;

  auto readStringField = [&](const std::string& key, std::string& field) {
    std::size_t search = 0;
    if (findKey(json, search, key)) {
      field = parseJsonString(json, search, sourcePath);
    }
  };

  auto readUlongField = [&](const std::string& key, unsigned long& field) {
    std::size_t search = 0;
    if (findKey(json, search, key)) {
      field = parseUlong(json, search, sourcePath);
    }
  };

  auto readUintField = [&](const std::string& key, unsigned int& field) {
    std::size_t search = 0;
    if (findKey(json, search, key)) {
      field = static_cast<unsigned int>(parseUlong(json, search, sourcePath));
    }
  };

  readStringField("network_fingerprint", result.networkFingerprint);
  readStringField("config_fingerprint", result.configFingerprint);
  readStringField("infomap_version", result.infomapVersion);
  readUlongField("base_seed", result.baseSeed);
  readUintField("trial_offset", result.trialOffset);
  readUintField("num_trials", result.numTrials);
  readStringField("best_tree_file", result.bestTreeFile);

  // Parse the trials array.
  pos = 0;
  if (findKey(json, pos, "trials")) {
    result.trials = parseTrialsArray(json, pos, sourcePath);
  }

  return result;
}

} // namespace infomap
