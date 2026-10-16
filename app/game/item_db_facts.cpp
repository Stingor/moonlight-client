// Les faits d'un objet que son script et la pet_db donnent. Voir
// `item_db_block.h`.

#include <array>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/ui/database_window.h"

#include "game/item_db_block.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::vfs::ServerItem;

constexpr std::size_t kLineBytes = 256;
constexpr std::string_view kListSeparator = ", ";
constexpr std::uint32_t kMsPerSecond = 1000;
constexpr std::uint32_t kMsPerMinute = 60 * kMsPerSecond;
constexpr std::uint32_t kMsPerHour = 60 * kMsPerMinute;
// Les familiers qu'une nourriture nomme ; au-delà, elle les compte.
constexpr std::size_t kMostNamedPets = 6;

[[nodiscard]] std::string Joined(const std::vector<std::string>& parts) {
  std::string joined;
  for (const std::string& part : parts) {
    if (!joined.empty()) {
      joined += kListSeparator;
    }
    joined += part;
  }
  return joined;
}

// ── LES NOMS ────────────────────────────────────────────────────────────────

[[nodiscard]] std::string NumberedName(std::uint32_t entry_id) {
  return "#" + std::to_string(entry_id);
}

// Un familier : le nom de l'espèce, puis celui du monstre que le serveur montre.
[[nodiscard]] std::string MonsterName(std::uint32_t monster_id, const ItemDbNames& names) {
  if (names.pets != nullptr) {
    if (const std::string_view species = names.pets->SpeciesOf(monster_id); !species.empty()) {
      return std::string(species);
    }
  }
  if (names.db != nullptr) {
    if (const ro::vfs::ServerMonster* monster = names.db->FindMonster(monster_id);
        monster != nullptr && !monster->name.empty()) {
      return monster->name;
    }
  }
  return NumberedName(monster_id);
}

// Un objet : le nom du client, puis celui du serveur.
[[nodiscard]] std::string ItemName(std::uint32_t item_id, const ItemDbNames& names) {
  if (names.items != nullptr) {
    if (const ro::vfs::ItemEntry* entry = names.items->Find(item_id);
        entry != nullptr && !entry->name.empty()) {
      return entry->name;
    }
  }
  if (names.db != nullptr) {
    if (const ServerItem* item = names.db->FindItem(item_id);
        item != nullptr && !item->name.empty()) {
      return item->name;
    }
  }
  return NumberedName(item_id);
}

[[nodiscard]] std::string MonsterNames(std::span<const std::uint32_t> monster_ids,
                                       const ItemDbNames& names) {
  std::vector<std::string> shown;
  for (const std::uint32_t monster_id : monster_ids) {
    shown.push_back(MonsterName(monster_id, names));
  }
  return Joined(shown);
}

// Les états que lève un `sc_end`, nommés comme dans le jeu. Un état qu'elle
// ignore s'écrit par la constante du serveur.
[[nodiscard]] const char* KnownStatusName(std::string_view status) {
  if (status == "POISON") {
    return Tr("Poison");
  }
  if (status == "DPOISON") {
    return Tr("Poison mortel");
  }
  if (status == "SILENCE") {
    return Tr("Silence");
  }
  if (status == "CURSE") {
    return Tr("Malédiction");
  }
  if (status == "BLIND") {
    return Tr("Cécité");
  }
  if (status == "CONFUSION") {
    return Tr("Confusion");
  }
  if (status == "HALLUCINATION") {
    return Tr("Hallucination");
  }
  if (status == "BLEEDING") {
    return Tr("Saignement");
  }
  if (status == "SLEEP") {
    return Tr("Sommeil");
  }
  if (status == "STONE") {
    return Tr("Pétrification");
  }
  if (status == "FREEZE") {
    return Tr("Gel");
  }
  if (status == "STUN") {
    return Tr("Étourdissement");
  }
  if (status == "ORCISH") {
    return Tr("Tête d'orc");
  }
  if (status == "CHANGEUNDEAD") {
    return Tr("Corps de mort-vivant");
  }
  return nullptr;
}

[[nodiscard]] std::string StatusName(std::string_view status) {
  const char* const known = KnownStatusName(status);
  return known != nullptr ? std::string(known) : std::string(status);
}

// ── LES VALEURS ─────────────────────────────────────────────────────────────
//
// ⚠ CHAQUE FORMAT EST UN APPEL À `Tr` SUR UN LITTÉRAL : c'est ce qui laisse le
// compilateur vérifier ses arguments.

using Text = std::array<char, kLineBytes>;

// « 325 ~ 405 PV, 40 ~ 60 SP », « 5 % des PV » ; vide pour un soin nul.
[[nodiscard]] std::string HealText(const ScriptHeal& heal) {
  std::vector<std::string> parts;
  Text text{};
  const auto hp_least = static_cast<unsigned>(heal.hp_least);
  const auto hp_most = static_cast<unsigned>(heal.hp_most);
  const auto sp_least = static_cast<unsigned>(heal.sp_least);
  const auto sp_most = static_cast<unsigned>(heal.sp_most);
  if (heal.percent) {
    if (hp_most > 0) {
      std::snprintf(text.data(), text.size(), Tr("%u %% des PV"), hp_most);
      parts.emplace_back(text.data());
    }
    if (sp_most > 0) {
      std::snprintf(text.data(), text.size(), Tr("%u %% des SP"), sp_most);
      parts.emplace_back(text.data());
    }
    return Joined(parts);
  }
  if (hp_most > 0) {
    if (hp_least == hp_most) {
      std::snprintf(text.data(), text.size(), Tr("%u PV"), hp_most);
    } else {
      std::snprintf(text.data(), text.size(), Tr("%u ~ %u PV"), hp_least, hp_most);
    }
    parts.emplace_back(text.data());
  }
  if (sp_most > 0) {
    if (sp_least == sp_most) {
      std::snprintf(text.data(), text.size(), Tr("%u SP"), sp_most);
    } else {
      std::snprintf(text.data(), text.size(), Tr("%u ~ %u SP"), sp_least, sp_most);
    }
    parts.emplace_back(text.data());
  }
  return Joined(parts);
}

// Une durée dans la plus grande unité qui la tombe juste.
[[nodiscard]] std::string DurationText(std::uint32_t duration_ms) {
  Text text{};
  if (duration_ms % kMsPerHour == 0) {
    std::snprintf(text.data(), text.size(), Tr("%u heure(s)"),
                  static_cast<unsigned>(duration_ms / kMsPerHour));
  } else if (duration_ms % kMsPerMinute == 0) {
    std::snprintf(text.data(), text.size(), Tr("%u minute(s)"),
                  static_cast<unsigned>(duration_ms / kMsPerMinute));
  } else if (duration_ms % kMsPerSecond == 0) {
    std::snprintf(text.data(), text.size(), Tr("%u seconde(s)"),
                  static_cast<unsigned>(duration_ms / kMsPerSecond));
  } else {
    std::snprintf(text.data(), text.size(), Tr("%u ms"), static_cast<unsigned>(duration_ms));
  }
  return text.data();
}

// Le nom traduit d'un élément, celui du filtre de la base : son rang plus un,
// le zéro y valant « Tous ».
[[nodiscard]] const char* ElementLabel(std::uint8_t element) {
  return ro::ui::DatabaseFilterChoice(element + 1, ro::ui::kDatabaseElements);
}

}  // namespace

void AppendItemDbScriptFacts(std::vector<std::string>& lines, const ServerItem& item,
                             const ItemScriptFacts& facts, const ItemDbNames& names) {
  if (facts.attack_element != kNoScriptElement) {
    lines.push_back(ItemDbLabelled(Tr("Élément :"), ElementLabel(facts.attack_element)));
  }
  if (facts.defense_element != kNoScriptElement) {
    lines.push_back(ItemDbLabelled(Tr("Élément d'armure :"), ElementLabel(facts.defense_element)));
  }
  if (facts.heal.has_value()) {
    if (const std::string heal = HealText(*facts.heal); !heal.empty()) {
      lines.push_back(ItemDbLabelled(Tr("Soin :"), heal));
    }
  }
  if (!facts.cured.empty()) {
    std::vector<std::string> cured;
    for (const std::string& status : facts.cured) {
      cured.push_back(StatusName(status));
    }
    lines.push_back(ItemDbLabelled(Tr("Guérit :"), Joined(cured)));
  }
  if (facts.effect_duration_ms > 0) {
    lines.push_back(ItemDbLabelled(Tr("Durée :"), DurationText(facts.effect_duration_ms)));
  }
  if (item.reuse_delay_ms > 0) {
    lines.push_back(
        ItemDbLabelled(Tr("Délai de réutilisation :"), DurationText(item.reuse_delay_ms)));
  }
  if (names.pets == nullptr) {
    return;
  }
  // LA CAPTURE : l'appât capture le monstre dont il est le `TameItem`.
  if (facts.captures) {
    const std::uint32_t lure = facts.lure_item_id != 0 ? facts.lure_item_id : item.item_id;
    if (const std::span<const std::uint32_t> tamed = names.pets->TamedBy(lure); !tamed.empty()) {
      lines.push_back(ItemDbLabelled(Tr("Capture :"), MonsterNames(tamed, names)));
    }
  }
  // L'ŒUF : le familier qui en sort, et ce qu'il mange.
  if (const std::uint32_t pet_class = names.pets->ClassOfEgg(item.item_id); pet_class != 0) {
    lines.push_back(ItemDbLabelled(Tr("Familier :"), MonsterName(pet_class, names)));
    if (const std::uint32_t food = names.pets->FoodOf(pet_class); food != 0) {
      lines.push_back(ItemDbLabelled(Tr("Nourriture :"), ItemName(food, names)));
    }
  }
  // L'ACCESSOIRE : les familiers qui le portent.
  if (const std::span<const std::uint32_t> wearers = names.pets->WornBy(item.item_id);
      !wearers.empty()) {
    lines.push_back(ItemDbLabelled(Tr("Familier :"), MonsterNames(wearers, names)));
  }
  // LA NOURRITURE : les familiers qui la mangent, comptés au-delà d'une ligne.
  if (const std::vector<std::uint32_t> fed = names.pets->FedBy(item.item_id); !fed.empty()) {
    if (fed.size() <= kMostNamedPets) {
      lines.push_back(ItemDbLabelled(Tr("Nourrit :"), MonsterNames(fed, names)));
    } else {
      Text count{};
      std::snprintf(count.data(), count.size(), Tr("%zu familiers"), fed.size());
      lines.push_back(ItemDbLabelled(Tr("Nourrit :"), count.data()));
    }
  }
}

}  // namespace moonclient
