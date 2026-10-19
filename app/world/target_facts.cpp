#include "world/target_facts.h"

#include <cctype>
#include <cstddef>
#include <string>

#include "ro/sim/world.h"

#include "world/entity_traits.h"

namespace moonclient {
namespace {

// La classe de monstre, telle que le serveur la numérote.
constexpr std::uint8_t kMvpClass = 1;
constexpr std::uint8_t kGuardianClass = 2;

constexpr std::string_view kLevelMark = "Lv. ";
constexpr std::string_view kHealthMark = "HP: ";
constexpr std::string_view kSeparator = " · ";
constexpr int kDecimal = 10;
constexpr std::uint32_t kPercentScale = 100;

// Le nom d'un trait, ou rien quand aucune famille ne le connaît : la plaque ne
// montre pas de « ? ».
[[nodiscard]] std::string_view NamedOrEmpty(std::string_view name) {
  return name == kUnnamedTarget ? std::string_view() : name;
}

// Ajoute un morceau à « a · b · c ». Un morceau vide ne laisse aucune trace.
void AppendPart(std::string& into, std::string_view part) {
  if (part.empty()) {
    return;
  }
  if (!into.empty()) {
    into += kSeparator;
  }
  into += part;
}

// Un entier en tête de `text`, à la façon de `%d` : blancs, signe, chiffres.
// Rend la suite du texte, ou rien s'il n'y a pas de chiffre.
[[nodiscard]] std::optional<std::string_view> ReadNumber(std::string_view text, long long& value) {
  std::size_t at = 0;
  while (at < text.size() && std::isspace(static_cast<unsigned char>(text[at])) != 0) {
    ++at;
  }
  bool negative = false;
  if (at < text.size() && (text[at] == '-' || text[at] == '+')) {
    negative = text[at] == '-';
    ++at;
  }
  const std::size_t digits_from = at;
  long long read = 0;
  while (at < text.size() && std::isdigit(static_cast<unsigned char>(text[at])) != 0) {
    read = read * kDecimal + (text[at] - '0');
    ++at;
  }
  if (at == digits_from) {
    return std::nullopt;
  }
  value = negative ? -read : read;
  return text.substr(at);
}

}  // namespace

MobPlate ParseMobPlate(std::string_view plate) {
  MobPlate read;
  if (const std::size_t at = plate.find(kLevelMark); at != std::string_view::npos) {
    long long level = 0;
    if (ReadNumber(plate.substr(at + kLevelMark.size()), level).has_value() && level > 0) {
      read.level = static_cast<int>(level);
    }
  }
  if (const std::size_t at = plate.find(kHealthMark); at != std::string_view::npos) {
    const std::string_view after = plate.substr(at + kHealthMark.size());
    long long current = 0;
    long long most = 0;
    const std::optional<std::string_view> rest = ReadNumber(after, current);
    bool absolute = false;
    if (rest.has_value() && !rest->empty() && rest->front() == '/' && current >= 0) {
      if (ReadNumber(rest->substr(1), most).has_value() && most > 0) {
        read.health = static_cast<std::uint32_t>(current);
        read.max_health = static_cast<std::uint32_t>(most);
        absolute = true;
      }
    }
    if (!absolute && rest.has_value() && !rest->empty() && rest->front() == '%' && current >= 0) {
      read.health_percent = static_cast<int>(current);
    }
  }
  return read;
}

TargetFacts FactsOfTarget(const ro::sim::World& world, const ro::vfs::ItemWords& words) {
  TargetFacts facts;
  const ro::sim::Actor* actor = world.selected();
  if (actor == nullptr) {
    return facts;
  }
  facts.present = true;
  facts.name = actor->name;

  const ro::sim::TargetDetail& detail = world.target_detail();
  // Une réponse du serveur pour CETTE entité, et qui la décrit.
  const bool told = detail.id == actor->id && detail.told && detail.reachable;

  facts.is_monster = actor->kind == ro::sim::ActorKind::kMonster;
  facts.is_player = actor->kind == ro::sim::ActorKind::kPlayer;
  // Dès que le serveur a parlé, c'est lui qui classe : il connaît le type réel.
  if (told) {
    facts.is_monster = detail.kind == ro::sim::TargetKind::kMonster;
    facts.is_player = detail.kind == ro::sim::TargetKind::kPlayer;
    facts.is_npc = detail.kind == ro::sim::TargetKind::kNpc;
  }
  facts.ally = told && detail.magic.has_value() && detail.max_magic.has_value();

  const ro::sim::NameCard* card = actor->card.has_value() ? &*actor->card : nullptr;
  if (facts.is_player && card != nullptr) {
    facts.guild = card->banner;
  }
  MobPlate plate;
  if (facts.is_monster && card != nullptr) {
    plate = ParseMobPlate(card->aside);
  }

  if (told && detail.level.has_value()) {
    facts.level = detail.level;
  } else if (plate.level.has_value()) {
    facts.level = static_cast<std::int16_t>(*plate.level);
  }

  // Les sources de PV, par fiabilité décroissante : les PV exacts du serveur,
  // la jauge que le client tient, la plaque chiffrée, puis un pourcentage —
  // celui du serveur, ou celui de la plaque. Un pourcentage ne passe jamais
  // devant un chiffre exact : il ne dit que la jauge.
  const bool told_health = told && detail.health.has_value() && detail.max_health.has_value();
  if (told_health && !detail.health_in_percent) {
    facts.health = detail.health;
    facts.max_health = detail.max_health;
  } else if (actor->max_health > 0 && actor->health >= 0) {
    facts.health = static_cast<std::uint32_t>(actor->health);
    facts.max_health = static_cast<std::uint32_t>(actor->max_health);
  } else if (plate.health.has_value() && plate.max_health.has_value()) {
    facts.health = plate.health;
    facts.max_health = plate.max_health;
  } else if (told_health) {
    facts.health = detail.health;
    facts.max_health = detail.max_health;
    facts.health_in_percent = true;
  } else if (plate.health_percent.has_value()) {
    facts.health = static_cast<std::uint32_t>(*plate.health_percent);
    facts.max_health = kPercentScale;
    facts.health_in_percent = true;
  }
  if (told && detail.magic.has_value() && detail.max_magic.has_value()) {
    facts.magic = detail.magic;
    facts.max_magic = detail.max_magic;
  }

  if (told && detail.nature.has_value() && !facts.is_npc) {
    const ro::sim::TargetDetail::Nature& nature = *detail.nature;
    AppendPart(facts.species, NamedOrEmpty(RaceName(words, nature.race)));
    const std::string_view element = NamedOrEmpty(ElementName(words, nature.element));
    if (!element.empty()) {
      // Le niveau d'élément se colle à l'élément, comme partout dans le jeu.
      AppendPart(facts.species, nature.element_level > 0 ? std::string(element) + " " +
                                                               std::to_string(nature.element_level)
                                                         : std::string(element));
    }
    AppendPart(facts.species, NamedOrEmpty(SizeName(words, nature.size)));
    if (nature.monster_class == kMvpClass) {
      AppendPart(facts.species, "MVP");
    } else if (nature.monster_class == kGuardianClass) {
      AppendPart(facts.species, "Guardian");
    }
  } else if (facts.is_monster && card != nullptr) {
    // La plaque porte les mêmes notions en toutes lettres : la race dans la
    // guilde, l'élément dans le rang.
    AppendPart(facts.species, card->banner);
    AppendPart(facts.species, card->badge);
  }
  return facts;
}

}  // namespace moonclient
