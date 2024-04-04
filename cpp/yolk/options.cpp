#include "yolk/yolk.h"
#include "yolk/options.h"

void egg::yolk::OptionParser::Rules::parse(Options& options, const std::string& key, const std::string* value) {
  auto rule = this->find(key);
  if (rule == this->end()) {
    // Unknown key
    if (!key.empty()) {
      throw egg::ovum::Exception("Unrecognized option: '--{option}'").with("option", key);
    } else if (value != nullptr) {
      throw egg::ovum::Exception("Unexpected argument: '{argument}'").with("argument", *value);
    } else {
      throw egg::ovum::Exception("Unexpected argument");
    }
  }
  if (key.empty()) {
    // 'value'
    assert(value != nullptr);
    options.emplace(key, *value);
  } else if (value == nullptr) {
    // '--key'
    options.emplace(key, std::string());
  } else {
    // '--key=value'
    options.emplace(key, *value);
  }
}

egg::yolk::OptionParser& egg::yolk::OptionParser::withExtraneousArguments(Occurrences occurrences) {
  this->rules.emplace(std::piecewise_construct, std::forward_as_tuple(), std::forward_as_tuple(occurrences));
  return *this;
}

egg::yolk::OptionParser& egg::yolk::OptionParser::withOption(const std::string& option, Occurrences occurrences) {
  this->rules.emplace(std::piecewise_construct, std::forward_as_tuple(option), std::forward_as_tuple(occurrences));
  return *this;
}

egg::yolk::Options egg::yolk::OptionParser::parse() {
  Options options{};
  auto hyphens = false;
  for (auto& argument : this->arguments) {
    if (hyphens || !argument.starts_with("--")) {
      // See https://www.gnu.org/software/bash/manual/html_node/Shell-Builtin-Commands.html
      this->rules.parse(options, {}, &argument);
    } else if (argument.size() == 2) {
      // '--' signifies the end of options
      hyphens = true;
    } else {
      auto equals = argument.find('=');
      if (equals == std::string::npos) {
        // '--key'
        this->rules.parse(options, argument.substr(2), nullptr);
      } else {
        // '--key=value'
        // '--=value' is a synonym for 'value'
        auto key = argument.substr(2, equals - 2);
        auto value = argument.substr(equals + 1);
        this->rules.parse(options, key, &value);
      }
    }
  }
  for (auto& rule : this->rules) {
    auto count = options.count(rule.first);
    switch (rule.second.occurrences) {
    case Occurrences::ZeroOrOne:
      if (count > 1) {
        if (rule.first.empty()) {
          throw egg::ovum::Exception("At most one argument was expected").with("arguments", std::to_string(count));
        }
        throw egg::ovum::Exception("At most one occurrence of '--{option}' was expected").with("option", rule.first).with("occurrences", std::to_string(count));
      }
      break;
    case Occurrences::ZeroOrMore:
    default:
      break;
    case Occurrences::One:
      if (rule.first.empty()) {
        throw egg::ovum::Exception("Exactly one argument was expected").with("arguments", std::to_string(count));
      }
      if (count != 1) {
        throw egg::ovum::Exception("Exactly one occurrence of '--{option}' was expected").with("option", rule.first).with("occurrences", std::to_string(count));
      }
      break;
    case Occurrences::OneOrMore:
      if (rule.first.empty()) {
        throw egg::ovum::Exception("At least one argument was expected").with("arguments", std::to_string(count));
      }
      if (count < 1) {
        throw egg::ovum::Exception("At least one occurrence of '--{option}' was expected").with("option", rule.first).with("occurrences", std::to_string(count));
      }
      break;
    }
  }
  return options;
}
