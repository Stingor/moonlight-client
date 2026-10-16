// Le lecteur de scripts d'objet, par motifs exacts. Voir l'en-tête.

#include "game/item_script_facts.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <limits>
#include <span>
#include <system_error>

namespace moonclient {
namespace {

// ── LES MORCEAUX ────────────────────────────────────────────────────────────

// Une instruction, ou la tête d'un bloc : le texte entre deux `;`, `{` ou `}`,
// et la profondeur d'accolades où il commence.
struct Fragment {
  std::string text;
  int depth = 0;
};

[[nodiscard]] bool SameText(std::string_view left, std::string_view right) {
  return left.size() == right.size() &&
         std::ranges::equal(left, right, [](unsigned char a, unsigned char b) {
           return std::tolower(a) == std::tolower(b);
         });
}

[[nodiscard]] bool StartsWithText(std::string_view text, std::string_view prefix) {
  return text.size() >= prefix.size() && SameText(text.substr(0, prefix.size()), prefix);
}

[[nodiscard]] std::string_view Trimmed(std::string_view text) {
  const auto blank = [](char letter) { return std::isspace(static_cast<unsigned char>(letter)); };
  while (!text.empty() && blank(text.front())) {
    text.remove_prefix(1);
  }
  while (!text.empty() && blank(text.back())) {
    text.remove_suffix(1);
  }
  return text;
}

// Le texte d'un littéral entre guillemets, échappements compris, recopié tel
// quel ; `at` pointe le guillemet ouvrant et finit sur le fermant.
void CopyString(std::string_view script, std::size_t& at, std::string& into) {
  into += script[at];
  for (++at; at < script.size(); ++at) {
    into += script[at];
    if (script[at] == '\\' && at + 1 < script.size()) {
      into += script[++at];
    } else if (script[at] == '"') {
      return;
    }
  }
}

// Les morceaux d'un script, commentaires ôtés. Une chaîne ne coupe rien.
[[nodiscard]] std::vector<Fragment> FragmentsOf(std::string_view script) {
  std::vector<Fragment> fragments;
  std::string current;
  int depth = 0;
  int current_depth = 0;
  const auto emit = [&] {
    if (!Trimmed(current).empty()) {
      fragments.push_back(Fragment{.text = std::string(Trimmed(current)), .depth = current_depth});
    }
    current.clear();
    current_depth = depth;
  };
  for (std::size_t at = 0; at < script.size(); ++at) {
    const char letter = script[at];
    const char next = at + 1 < script.size() ? script[at + 1] : '\0';
    if (letter == '"') {
      CopyString(script, at, current);
    } else if (letter == '/' && next == '/') {
      while (at < script.size() && script[at] != '\n') {
        ++at;
      }
      current += ' ';
    } else if (letter == '/' && next == '*') {
      const std::size_t closing = script.find("*/", at + 2);
      at = closing == std::string_view::npos ? script.size() : closing + 1;
      current += ' ';
    } else if (letter == ';') {
      emit();
    } else if (letter == '{') {
      emit();
      ++depth;
      current_depth = depth;
    } else if (letter == '}') {
      emit();
      depth = std::max(0, depth - 1);
      current_depth = depth;
    } else {
      current += letter;
    }
  }
  emit();
  return fragments;
}

// Les mots d'un morceau, chaînes sautées : commandes, constantes, variables.
[[nodiscard]] std::vector<std::string_view> WordsOf(std::string_view text) {
  std::vector<std::string_view> words;
  const auto in_word = [](char letter) {
    return std::isalnum(static_cast<unsigned char>(letter)) || letter == '_' || letter == '.' ||
           letter == '@' || letter == '$' || letter == '\'';
  };
  std::size_t at = 0;
  while (at < text.size()) {
    if (text[at] == '"') {
      for (++at; at < text.size() && text[at] != '"'; ++at) {
        if (text[at] == '\\') {
          ++at;
        }
      }
      ++at;
    } else if (in_word(text[at])) {
      const std::size_t start = at;
      while (at < text.size() && in_word(text[at])) {
        ++at;
      }
      words.push_back(text.substr(start, at - start));
    } else {
      ++at;
    }
  }
  return words;
}

// ── LES ARGUMENTS ───────────────────────────────────────────────────────────

// Les arguments d'une instruction `commande a,b,…`, coupés aux virgules hors
// parenthèses. Vide quand le morceau n'est pas cette commande suivie d'un blanc.
[[nodiscard]] std::optional<std::vector<std::string_view>> ArgumentsOf(std::string_view text,
                                                                       std::string_view command) {
  if (!StartsWithText(text, command)) {
    return std::nullopt;
  }
  std::string_view rest = text.substr(command.size());
  if (rest.empty()) {
    return std::vector<std::string_view>{};
  }
  if (!std::isspace(static_cast<unsigned char>(rest.front()))) {
    return std::nullopt;
  }
  rest = Trimmed(rest);
  std::vector<std::string_view> arguments;
  int nesting = 0;
  std::size_t start = 0;
  for (std::size_t at = 0; at < rest.size(); ++at) {
    if (rest[at] == '(') {
      ++nesting;
    } else if (rest[at] == ')') {
      --nesting;
    } else if (rest[at] == ',' && nesting == 0) {
      arguments.push_back(Trimmed(rest.substr(start, at - start)));
      start = at + 1;
    }
  }
  arguments.push_back(Trimmed(rest.substr(start)));
  return arguments;
}

// Un entier littéral, signe compris.
[[nodiscard]] std::optional<std::int64_t> IntegerOf(std::string_view text) {
  text = Trimmed(text);
  std::int64_t value = 0;
  const auto [end, failure] = std::from_chars(text.data(), text.data() + text.size(), value);
  if (failure != std::errc() || end != text.data() + text.size() || text.empty()) {
    return std::nullopt;
  }
  return value;
}

// Une plage : un entier, ou `rand(a,b)` de deux entiers — le serveur range
// lui-même les bornes.
struct Range {
  std::int64_t least = 0;
  std::int64_t most = 0;
};

[[nodiscard]] std::optional<Range> RangeOf(std::string_view text) {
  if (const std::optional<std::int64_t> single = IntegerOf(text); single.has_value()) {
    return Range{.least = *single, .most = *single};
  }
  constexpr std::string_view kRand = "rand(";
  text = Trimmed(text);
  if (!StartsWithText(text, kRand) || !text.ends_with(')')) {
    return std::nullopt;
  }
  const std::string_view inside = text.substr(kRand.size(), text.size() - kRand.size() - 1);
  const std::size_t comma = inside.find(',');
  if (comma == std::string_view::npos) {
    return std::nullopt;
  }
  const std::optional<std::int64_t> first = IntegerOf(inside.substr(0, comma));
  const std::optional<std::int64_t> second = IntegerOf(inside.substr(comma + 1));
  if (!first.has_value() || !second.has_value()) {
    return std::nullopt;
  }
  return Range{.least = std::min(*first, *second), .most = std::max(*first, *second)};
}

[[nodiscard]] bool Fits(const Range& range) {
  return range.least >= 0 && range.most <= std::numeric_limits<std::uint32_t>::max();
}

// ── LES FAMILLES ────────────────────────────────────────────────────────────

enum class Family : std::uint8_t {
  kHeal,
  kCure,
  kAttackElement,
  kDefenseElement,
  kCapture,
  kStart
};
inline constexpr std::size_t kFamilyCount = 6;

struct FamilyWord {
  std::string_view word;
  Family family = Family::kHeal;
};

// Les mots qui désignent une famille, où qu'ils paraissent.
constexpr std::array<FamilyWord, 10> kFamilyWords = {{
    {"itemheal", Family::kHeal},
    {"percentheal", Family::kHeal},
    {"heal", Family::kHeal},
    {"sc_end", Family::kCure},
    {"bAtkEle", Family::kAttackElement},
    {"bDefEle", Family::kDefenseElement},
    {"pet", Family::kCapture},
    {"sc_start", Family::kStart},
    {"sc_start2", Family::kStart},
    {"sc_start4", Family::kStart},
}};

// Les mots après lesquels la suite du script peut ne pas s'exécuter.
constexpr std::array<std::string_view, 8> kBreakWords = {
    "end", "return", "goto", "break", "close", "close2", "callsub", "callfunc",
};

// Les constantes d'élément, dans l'ordre du serveur.
constexpr std::array<std::string_view, kScriptElementCount> kElementNames = {
    "Ele_Neutral", "Ele_Water", "Ele_Earth", "Ele_Fire",  "Ele_Wind",
    "Ele_Poison",  "Ele_Holy",  "Ele_Dark",  "Ele_Ghost", "Ele_Undead",
};

// Le nombre d'arguments de chaque forme de `sc_start`, type compris.
struct StartForm {
  std::string_view command;
  std::size_t arguments = 0;
};
constexpr std::array<StartForm, 3> kStartForms = {{
    {"sc_start", 3},
    {"sc_start2", 4},
    {"sc_start4", 6},
}};

constexpr std::string_view kStatusPrefix = "SC_";

struct Reading {
  ItemScriptFacts facts;
  std::array<bool, kFamilyCount> mute{};
  std::size_t heals = 0;
  bool started = false;
  void Mute(Family family) { mute[static_cast<std::size_t>(family)] = true; }
  [[nodiscard]] bool Muted(Family family) const { return mute[static_cast<std::size_t>(family)]; }
};

// Le soin d'une instruction exacte, ou rien.
[[nodiscard]] std::optional<ScriptHeal> HealOf(std::string_view text) {
  for (const std::string_view command : {"itemheal", "percentheal", "heal"}) {
    const auto arguments = ArgumentsOf(text, command);
    if (!arguments.has_value()) {
      continue;
    }
    if (arguments->size() != 2) {
      return std::nullopt;
    }
    const std::optional<Range> hp = RangeOf((*arguments)[0]);
    const std::optional<Range> sp = RangeOf((*arguments)[1]);
    // `percentheal` et `heal` ne tirent pas au sort : `rand` y est une autre
    // forme, et une valeur négative blesse.
    const bool random_allowed = command == "itemheal";
    const auto fixed = [](const Range& range) { return range.least == range.most; };
    if (!hp.has_value() || !sp.has_value() || !Fits(*hp) || !Fits(*sp) ||
        (!random_allowed && (!fixed(*hp) || !fixed(*sp)))) {
      return std::nullopt;
    }
    return ScriptHeal{.percent = command == "percentheal",
                      .hp_least = static_cast<std::uint32_t>(hp->least),
                      .hp_most = static_cast<std::uint32_t>(hp->most),
                      .sp_least = static_cast<std::uint32_t>(sp->least),
                      .sp_most = static_cast<std::uint32_t>(sp->most)};
  }
  return std::nullopt;
}

// L'élément d'un `bonus bAtkEle,Ele_X` ou `bonus bDefEle,Ele_X` exact.
[[nodiscard]] std::uint8_t ElementOf(std::string_view text, std::string_view bonus) {
  const auto arguments = ArgumentsOf(text, "bonus");
  if (!arguments.has_value() || arguments->size() != 2 || !SameText((*arguments)[0], bonus)) {
    return kNoScriptElement;
  }
  for (std::size_t rank = 0; rank < kElementNames.size(); ++rank) {
    if (SameText((*arguments)[1], kElementNames[rank])) {
      return static_cast<std::uint8_t>(rank);
    }
  }
  return kNoScriptElement;
}

// La durée d'un `sc_start` exact, ou rien.
[[nodiscard]] std::optional<std::uint32_t> StartDurationOf(std::string_view text) {
  for (const StartForm& form : kStartForms) {
    const auto arguments = ArgumentsOf(text, form.command);
    if (!arguments.has_value()) {
      continue;
    }
    if (arguments->size() != form.arguments || !StartsWithText((*arguments)[0], kStatusPrefix)) {
      return std::nullopt;
    }
    for (std::size_t rank = 2; rank < arguments->size(); ++rank) {
      if (!IntegerOf((*arguments)[rank]).has_value()) {
        return std::nullopt;
      }
    }
    const std::optional<std::int64_t> tick = IntegerOf((*arguments)[1]);
    if (!tick.has_value() || *tick <= 0 || *tick > std::numeric_limits<std::uint32_t>::max()) {
      return std::nullopt;
    }
    return static_cast<std::uint32_t>(*tick);
  }
  return std::nullopt;
}

void ReadElement(Reading& reading, std::string_view text, Family family, std::string_view bonus,
                 std::uint8_t& into) {
  const std::uint8_t element = ElementOf(text, bonus);
  if (element == kNoScriptElement || (into != kNoScriptElement && into != element)) {
    reading.Mute(family);
    return;
  }
  into = element;
}

// Une instruction posée hors bloc, avant toute rupture : chaque famille qu'elle
// nomme se lit par son motif exact, ou se tait.
void ReadStatement(Reading& reading, std::string_view text, std::span<const Family> named) {
  ItemScriptFacts& facts = reading.facts;
  for (const Family family : named) {
    switch (family) {
      case Family::kHeal:
        if (const std::optional<ScriptHeal> heal = HealOf(text);
            heal.has_value() && ++reading.heals == 1) {
          facts.heal = heal;
        } else {
          reading.Mute(family);
        }
        break;
      case Family::kCure: {
        const auto arguments = ArgumentsOf(text, "sc_end");
        if (!arguments.has_value() || arguments->size() != 1 ||
            !StartsWithText((*arguments)[0], kStatusPrefix) ||
            WordsOf((*arguments)[0]).size() != 1) {
          reading.Mute(family);
          break;
        }
        std::string status((*arguments)[0].substr(kStatusPrefix.size()));
        std::ranges::transform(status, status.begin(), [](unsigned char letter) {
          return static_cast<char>(std::toupper(letter));
        });
        if (std::ranges::find(facts.cured, status) == facts.cured.end()) {
          facts.cured.push_back(std::move(status));
        }
        break;
      }
      case Family::kAttackElement:
        ReadElement(reading, text, family, "bAtkEle", facts.attack_element);
        break;
      case Family::kDefenseElement:
        ReadElement(reading, text, family, "bDefEle", facts.defense_element);
        break;
      case Family::kCapture: {
        const auto arguments = ArgumentsOf(text, "pet");
        if (!arguments.has_value() || arguments->size() > 1) {
          reading.Mute(family);
          break;
        }
        std::uint32_t lure = 0;
        if (arguments->size() == 1) {
          const std::optional<std::int64_t> item = IntegerOf((*arguments)[0]);
          if (!item.has_value() || *item <= 0 ||
              *item > std::numeric_limits<std::uint32_t>::max()) {
            reading.Mute(family);
            break;
          }
          lure = static_cast<std::uint32_t>(*item);
        }
        if (facts.captures && facts.lure_item_id != lure) {
          reading.Mute(family);
          break;
        }
        facts.captures = true;
        facts.lure_item_id = lure;
        break;
      }
      case Family::kStart: {
        const std::optional<std::uint32_t> tick = StartDurationOf(text);
        if (!tick.has_value() || (reading.started && facts.effect_duration_ms != *tick)) {
          reading.Mute(family);
          break;
        }
        reading.started = true;
        facts.effect_duration_ms = *tick;
        break;
      }
    }
  }
}

}  // namespace

ItemScriptFacts ReadItemScript(std::string_view script) {
  Reading reading;
  bool broken = false;
  for (const Fragment& fragment : FragmentsOf(script)) {
    std::vector<Family> named;
    bool breaks = false;
    for (const std::string_view word : WordsOf(fragment.text)) {
      for (const FamilyWord& family_word : kFamilyWords) {
        if (SameText(word, family_word.word) &&
            std::ranges::find(named, family_word.family) == named.end()) {
          named.push_back(family_word.family);
        }
      }
      breaks = breaks || std::ranges::any_of(kBreakWords, [word](std::string_view stop) {
                 return SameText(word, stop);
               });
    }
    // 🔴 HORS BLOC ET AVANT TOUTE RUPTURE, OU RIEN : ailleurs, l'instruction
    // peut ne pas s'exécuter, ou s'exécuter plusieurs fois.
    if (fragment.depth != 0 || broken || breaks) {
      for (const Family family : named) {
        reading.Mute(family);
      }
    } else {
      ReadStatement(reading, fragment.text, named);
    }
    broken = broken || breaks;
  }

  ItemScriptFacts& facts = reading.facts;
  if (reading.Muted(Family::kHeal)) {
    facts.heal.reset();
  }
  if (reading.Muted(Family::kCure)) {
    facts.cured.clear();
  }
  if (reading.Muted(Family::kAttackElement)) {
    facts.attack_element = kNoScriptElement;
  }
  if (reading.Muted(Family::kDefenseElement)) {
    facts.defense_element = kNoScriptElement;
  }
  if (reading.Muted(Family::kCapture)) {
    facts.captures = false;
    facts.lure_item_id = 0;
  }
  if (reading.Muted(Family::kStart)) {
    facts.effect_duration_ms = 0;
  }
  return facts;
}

}  // namespace moonclient
