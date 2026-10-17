// Les faits d'un objet que son script et la pet_db donnent. Voir
// `item_db_block.h`.

#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/sim/equip_attributes.h"
#include "ro/ui/database_window.h"
#include "ro/vfs/server_db.h"

#include "character/equip_attribute_lines.h"
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

// ── L'EFFET DES ÉTATS POSÉS ─────────────────────────────────────────────────

// Le niveau de `bonus`, la commande à un seul argument.
constexpr std::uint8_t kBonusCommandLevel = 1;

// Vrai quand le texte d'un bonus est celui d'un type que le client ne sait pas
// nommer : « Bonus #n », suivi de sa valeur.
[[nodiscard]] bool Unnamed(std::string_view text, std::uint16_t bonus_type) {
  Text numbered{};
  std::snprintf(numbered.data(), numbered.size(), Tr("Bonus #%d"), static_cast<int>(bonus_type));
  const std::string_view prefix(numbered.data());
  return text.starts_with(prefix) && (text.size() == prefix.size() || text[prefix.size()] == ' ');
}

// LE TEXTE DES BONUS QUE DONNENT LES ÉTATS POSÉS, ou vide. Chaque bonus se
// nomme comme dans la case des attributs d'équipement. 🔴 TOUT OU RIEN : un
// état absent de la status_db, dont le script ne se lit pas entier, ou un bonus
// que le client ne sait pas nommer taisent la ligne entière.
[[nodiscard]] std::string EffectText(const ItemScriptFacts& facts, const ItemDbNames& names) {
  if (names.db == nullptr || facts.starts.empty()) {
    return {};
  }
  std::vector<std::string> parts;
  for (const ScriptStart& start : facts.starts) {
    const ro::vfs::ServerStatus* const status = names.db->FindStatus(start.status);
    if (status == nullptr || status->bonuses.empty()) {
      return {};
    }
    for (const ro::vfs::ServerStatusBonus& bonus : status->bonuses) {
      std::int32_t value = bonus.fixed_value;
      if (bonus.value_rank != ro::vfs::kServerStatusFixedValue) {
        value = start.values[bonus.value_rank - 1];
        if (bonus.negated) {
          if (value == std::numeric_limits<std::int32_t>::min()) {
            return {};
          }
          value = -value;
        }
      }
      const ro::sim::EquipBonus one{
          .level = kBonusCommandLevel, .type = bonus.bonus_type, .values = {value}};
      std::string text = EquipBonusText(one, EquipAttributeNames{});
      if (Unnamed(text, bonus.bonus_type)) {
        return {};
      }
      if (!text.empty()) {
        parts.push_back(std::move(text));
      }
    }
  }
  return Joined(parts);
}

// ── LE TYPE, LES DONS ET LES INVOCATIONS ────────────────────────────────────

// Un texte réduit à ses lettres et chiffres, sans casse : « Container » et
// « container » se valent, « Dead-Branch » et « DeadBranch » aussi.
[[nodiscard]] std::string Packed(std::string_view text) {
  std::string packed;
  for (const char letter : text) {
    if (std::isalnum(static_cast<unsigned char>(letter))) {
      packed += static_cast<char>(std::tolower(static_cast<unsigned char>(letter)));
    }
  }
  return packed;
}

// Les drapeaux de `Flags` qui qualifient le type de l'objet ; les autres —
// achat en boutique d'achat… — n'en disent rien.
[[nodiscard]] const char* FlagLabel(std::string_view flag) {
  const std::string packed = Packed(flag);
  if (packed == "container") {
    return Tr("Conteneur");
  }
  if (packed == "deadbranch") {
    return Tr("Invocation");
  }
  if (packed == "noconsume") {
    return Tr("Non consommé");
  }
  return nullptr;
}

// Le lien d'objet du texte balisé (`ro/ui/rich_text.h`).
constexpr std::string_view kItemLinkOpen = "<ITEM>";
constexpr std::string_view kItemLinkInfo = "<INFO>";
constexpr std::string_view kItemLinkClose = "</INFO></ITEM>";

[[nodiscard]] std::string ItemLink(std::uint32_t item_id, const std::string& shown) {
  std::string link(kItemLinkOpen);
  link += shown;
  link += kItemLinkInfo;
  link += std::to_string(item_id);
  link += kItemLinkClose;
  return link;
}

// L'objet que le serveur nomme ainsi par son nom Aegis, sans égard à la casse.
[[nodiscard]] const ServerItem* ItemByAegis(std::string_view aegis_name, const ItemDbNames& names) {
  if (names.db == nullptr) {
    return nullptr;
  }
  const std::string wanted = Packed(aegis_name);
  for (const ServerItem& item : names.db->items()) {
    if (item.aegis_name.size() == aegis_name.size() && Packed(item.aegis_name) == wanted) {
      return &item;
    }
  }
  return nullptr;
}

// Le nom lisible d'un groupe, tiré de sa constante : « BlueBox » donne
// « Blue Box » ; un trait bas sépare deux mots comme un blanc.
[[nodiscard]] std::string ReadableGroup(std::string_view constant) {
  std::string shown;
  for (std::size_t at = 0; at < constant.size(); ++at) {
    const auto letter = static_cast<unsigned char>(constant[at]);
    const bool cut = letter == '_' || (at > 0 && std::isupper(letter) &&
                                       std::islower(static_cast<unsigned char>(constant[at - 1])));
    if (cut && !shown.empty() && shown.back() != ' ') {
      shown += ' ';
    }
    if (letter != '_') {
      shown += static_cast<char>(letter);
    }
  }
  while (!shown.empty() && shown.back() == ' ') {
    shown.pop_back();
  }
  return shown;
}

[[nodiscard]] ItemDbGiftEffect GiftEffectOf(const ScriptGift& gift, const ItemDbNames& names) {
  Text text{};
  ItemDbGiftEffect effect{.count = gift.count};
  const auto count = static_cast<unsigned>(gift.count);
  switch (gift.kind) {
    case ScriptGiftKind::kItem: {
      effect.kind = ItemDbGiftKind::kItem;
      std::uint32_t item_id = gift.item_id;
      if (item_id == 0) {
        const ServerItem* const named = ItemByAegis(gift.aegis_name, names);
        item_id = named != nullptr ? named->item_id : 0;
      }
      effect.name = item_id != 0 ? ItemName(item_id, names) : gift.aegis_name;
      const std::string shown = item_id != 0 ? ItemLink(item_id, effect.name) : effect.name;
      std::snprintf(text.data(), text.size(), Tr("donne %u × %s"), count, shown.c_str());
      break;
    }
    case ScriptGiftKind::kGroup:
      effect.kind = ItemDbGiftKind::kGroup;
      effect.name = ReadableGroup(gift.group);
      std::snprintf(text.data(), text.size(), Tr("donne des items du groupe %s"),
                    effect.name.c_str());
      break;
    case ScriptGiftKind::kRandomGroup:
      effect.kind = ItemDbGiftKind::kRandomGroup;
      effect.name = ReadableGroup(gift.group);
      std::snprintf(text.data(), text.size(), Tr("donne %u item(s) au hasard (groupe %s)"), count,
                    effect.name.c_str());
      break;
  }
  effect.text = text.data();
  return effect;
}

[[nodiscard]] ItemDbGiftEffect SummonEffectOf(const ScriptSummon& summon,
                                              const ItemDbNames& names) {
  Text text{};
  ItemDbGiftEffect effect{.count = summon.count};
  const auto count = static_cast<unsigned>(summon.count);
  if (summon.monster_id != 0) {
    effect.kind = ItemDbGiftKind::kSummon;
    effect.name = MonsterName(summon.monster_id, names);
    std::snprintf(text.data(), text.size(), Tr("invoque %u × %s"), count, effect.name.c_str());
  } else {
    effect.kind = ItemDbGiftKind::kSummonGroup;
    effect.name = ReadableGroup(summon.group);
    std::snprintf(text.data(), text.size(), Tr("invoque %u monstre(s) au hasard (groupe %s)"),
                  count, effect.name.c_str());
  }
  effect.text = text.data();
  return effect;
}

}  // namespace

std::string QualifiedKind(std::string_view kind, const ServerItem& item) {
  std::vector<std::string> qualifiers;
  std::string category = item.category;
  for (const std::string& flag : item.flags) {
    if (const char* const label = FlagLabel(flag); label != nullptr) {
      qualifiers.emplace_back(label);
      // Le drapeau le dit en clair, et traduit : la catégorie se tait.
      if (Packed(category) == Packed(flag)) {
        category.clear();
      }
    }
  }
  Text text{};
  std::string shown(kind);
  if (!category.empty()) {
    std::snprintf(text.data(), text.size(), Tr("%s — %s"), shown.c_str(), category.c_str());
    shown = text.data();
  }
  if (!qualifiers.empty()) {
    const std::string joined = Joined(qualifiers);
    std::snprintf(text.data(), text.size(), Tr("%s (%s)"), shown.c_str(), joined.c_str());
    shown = text.data();
  }
  return shown;
}

std::vector<ItemDbGiftEffect> GiftEffectsOf(const ItemScriptFacts& facts,
                                            const ItemDbNames& names) {
  std::vector<ItemDbGiftEffect> effects;
  for (const ScriptGift& gift : facts.gifts) {
    effects.push_back(GiftEffectOf(gift, names));
  }
  for (const ScriptSummon& summon : facts.summons) {
    effects.push_back(SummonEffectOf(summon, names));
  }
  return effects;
}

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
  if (const std::string effect = EffectText(facts, names); !effect.empty()) {
    lines.push_back(ItemDbLabelled(Tr("Effet :"), effect));
  }
  for (const ItemDbGiftEffect& gift : GiftEffectsOf(facts, names)) {
    lines.push_back(ItemDbLabelled(Tr("Effet :"), gift.text));
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
