#include "ro/vfs/battle_words.h"

#include <cstddef>
#include <cstdio>

#include "ro/core/i18n.h"

#include "message_word.h"

namespace ro::vfs {
namespace {

// Les noms sous lesquels un gabarit fautif se journalise.
constexpr std::string_view kIReceivedName = "la ligne du coup reçu";
constexpr std::string_view kPartyReceivedName = "la ligne du coup reçu par le groupe";
constexpr std::string_view kIGaveName = "la ligne du coup porté";
constexpr std::string_view kPartyGaveName = "la ligne du coup porté par le groupe";

// Les conversions que chaque ligne attend.
constexpr std::string_view kOneNameThenDamage = "sd";
constexpr std::string_view kTwoNamesThenDamage = "ssd";

// Imprime un gabarit que `FormatTakesExactly` a déjà criblé.
//
// ⚠ `-Wformat-nonliteral` EST ÉTEINT SUR CES SEULES LIGNES : c'est le crible qui
// rend l'appel sûr, et l'avertissement doit continuer d'attraper partout
// ailleurs un format venu d'une archive ou du réseau. On mesure avant d'écrire :
// un nom de joueur n'a pas de longueur bornée qu'on voudrait deviner.
template <typename... Args>
[[nodiscard]] std::string Printed(const char* format, Args... args) {
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
  const int needed = std::snprintf(nullptr, 0, format, args...);
  if (needed <= 0) {
    return {};
  }
  std::string printed(static_cast<std::size_t>(needed), '\0');
  std::snprintf(printed.data(), printed.size() + 1, format, args...);
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
  return printed;
}

}  // namespace

bool FormatTakesExactly(std::string_view format, std::string_view conversions) {
  std::size_t seen = 0;
  for (std::size_t at = 0; at < format.size(); ++at) {
    if (format[at] != '%') {
      continue;
    }
    ++at;
    // Un « % » en dernier caractère n'ouvre rien : le gabarit est tronqué.
    if (at >= format.size()) {
      return false;
    }
    if (format[at] == '%') {
      continue;
    }
    const char asked = format[at] == 'i' ? 'd' : format[at];
    if (seen >= conversions.size() || conversions[seen] != asked) {
      return false;
    }
    ++seen;
  }
  return seen == conversions.size();
}

std::string_view SomeoneText(const ItemWords& words) { return words.battle.someone; }

std::string BattleLineText(const ItemWords& words, BattleLine line, std::string_view first,
                           std::string_view second, int damage) {
  const std::string first_name(first);
  const std::string second_name(second);
  // Un gabarit absent, ou qui ne porte pas les conversions attendues, ne se
  // devine pas : la ligne est vide.
  const auto one_name = [&](const std::string& read, std::string_view name) {
    if (!FormatTakesExactly(read, kOneNameThenDamage)) {
      NoteMissingWord(name);
      return std::string();
    }
    return Printed(read.c_str(), first_name.c_str(), damage);
  };
  const auto two_names = [&](const std::string& read, std::string_view name) {
    if (!FormatTakesExactly(read, kTwoNamesThenDamage)) {
      NoteMissingWord(name);
      return std::string();
    }
    return Printed(read.c_str(), first_name.c_str(), second_name.c_str(), damage);
  };
  switch (line) {
    case BattleLine::kIReceived:
      return one_name(words.battle.i_received, kIReceivedName);
    case BattleLine::kPartyReceived:
      return two_names(words.battle.party_received, kPartyReceivedName);
    case BattleLine::kIGave:
      return one_name(words.battle.i_gave, kIGaveName);
    case BattleLine::kPartyGave:
      return two_names(words.battle.party_gave, kPartyGaveName);
  }
  return {};
}

void FillBattleWords(BattleWords& words) {
  words.someone = core::Tr("Quelqu'un");
  // L'ESPACE DE TÊTE APPARTIENT À LA LIGNE, comme dans le client.
  words.i_received = Edged(" ", core::Tr("[%s] vous inflige '%d' points de dégâts."), "");
  words.party_received =
      Edged(" ", core::Tr("[%s] est attaqué par [%s] : '%d' points de dégâts."), "");
  words.i_gave = Edged(" ", core::Tr("[%s] subit '%d' points de dégâts."), "");
  words.party_gave = Edged(" ", core::Tr("[%s] attaque [%s] : '%d' points de dégâts."), "");
}

}  // namespace ro::vfs
