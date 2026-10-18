#include "ro/vfs/item_words.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

#include "ro/core/i18n.h"
#include "ro/core/text.h"
#include "ro/vfs/achievement_words.h"
#include "ro/vfs/battle_words.h"
#include "ro/vfs/cash_shop_words.h"
#include "ro/vfs/chat_room_words.h"
#include "ro/vfs/craft_words.h"
#include "ro/vfs/encoding.h"
#include "ro/vfs/monster_words.h"
#include "ro/vfs/talkbox_words.h"
#include "ro/vfs/trade_words.h"
#include "ro/vfs/warp_list_words.h"
#include "ro/vfs/warp_memo_words.h"
#include "ro/vfs/whisper_words.h"

#include "message_word.h"
#include "text_table.h"

namespace ro::vfs {
namespace {

// Les noms sous lesquels un gabarit fautif se journalise.
constexpr std::string_view kObtainedName = "le suffixe du bandeau d'obtention";
constexpr std::string_view kGotItemName = "la ligne d'un ramassage";
constexpr std::string_view kDroppedItemName = "la ligne d'un objet jeté";

// ── Le suffixe du bandeau d'obtention ────────────────────────────────────────
//
// Une conversion, et une seule : la quantité ramassée.
constexpr int kObtainedConversionCount = 1;

// Le plus long suffixe qu'on imprime. Celui du client en fait quinze octets, et
// au-delà de ce plafond `snprintf` TRONQUE plutôt que de déborder.
constexpr std::size_t kObtainedTextCapacity = 256;

// Les trois forges que le client reconnaît, par leur nombre d'étoiles.
constexpr std::size_t kForgeStarsOne = 5;
constexpr std::size_t kForgeStarsTwo = 10;
constexpr std::size_t kForgeStarsThree = 15;

void ReadCardWords(ItemWords& words, std::string_view table) {
  for (const std::string_view line : SplitLines(table)) {
    std::uint32_t card_id = 0;
    std::string_view word;
    if (!SplitTableRow(line, card_id, word)) {
      continue;
    }
    words.cards[card_id] = CardWord{.text = Cp949ToUtf8Lossy(word), .after_name = false};
  }
}

// 🔴 CE FICHIER NE PORTE AUCUN MOT — il nomme ceux qui CHANGENT DE CÔTÉ. Le
// chargeur du client fait exactement ce déplacement, et rien d'autre. Une carte
// qu'il nomme sans qu'elle figure dans la table des mots n'a simplement rien à
// déplacer.
void ReadCardWordSides(ItemWords& words, std::string_view table) {
  for (const std::string_view line : SplitLines(table)) {
    // ⚠ CELLE-CI N'A PAS DE SECOND CHAMP — son dièse ferme une valeur toujours
    // vide —, mais elle a la même tête : l'identifiant, puis un dièse.
    std::uint32_t card_id = 0;
    std::string_view unused;
    if (!SplitTableRow(line, card_id, unused)) {
      continue;
    }
    const auto found = words.cards.find(card_id);
    if (found != words.cards.end()) {
      found->second.after_name = true;
    }
  }
}

// Imprime un gabarit PRINTF dont `FormatTakesOneInt` a déjà dit qu'il ne porte
// qu'une conversion entière. Rend une chaîne vide si la libc refuse d'écrire —
// ce qu'un tel gabarit ne peut pas provoquer, puisque sa conversion imprime au
// moins un chiffre.
[[nodiscard]] std::string PrintOneInt(const char* format, int amount) {
  std::array<char, kObtainedTextCapacity> buffer{};
  // ⚠ C'EST UN GABARIT PRINTF, celui du client, et `std::format` ne lit pas
  // « %d » : `std::vformat` jetterait sur un texte ici parfaitement valide.
  //
  // `-Wformat-nonliteral` est donc éteint ICI, sur cette ligne, et nulle part
  // ailleurs : ce qui rend l'appel sûr est le crible de `FormatTakesOneInt`, et
  // l'avertissement doit continuer d'attraper partout ailleurs un format venu
  // d'une archive ou du réseau.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
  const int written = std::snprintf(buffer.data(), buffer.size(), format, amount);
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
  return written > 0 ? std::string(buffer.data()) : std::string();
}

// Imprime un gabarit PRINTF dont `FormatTakesStringThenInt` a déjà dit qu'il
// porte une conversion de chaîne puis une conversion entière, dans cet ordre.
//
// ⚠ ON MESURE AVANT D'ÉCRIRE, et c'est ce qui le distingue de `PrintOneInt` :
// la longueur du nom d'un objet n'est pas bornée — il porte jusqu'à quatre mots
// de cartes et le nom de son forgeron —, donc aucun plafond choisi au jugé ne
// tiendrait sans tronquer le nom d'un joueur.
[[nodiscard]] std::string PrintStringThenInt(const char* format, const char* text, int amount) {
  // ⚠ C'EST UN GABARIT PRINTF, celui du client, et `-Wformat-nonliteral` est
  // éteint SUR LA SEULE LIGNE qui le consomme : ce qui rend l'appel sûr est le
  // crible de `FormatTakesStringThenInt`, et l'avertissement doit continuer
  // d'attraper partout ailleurs un format venu d'une archive ou du réseau.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
  const int needed = std::snprintf(nullptr, 0, format, text, amount);
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
  if (needed <= 0) {
    return {};
  }
  std::string printed(static_cast<std::size_t>(needed), '\0');
  // La taille passée compte le zéro final, que `printed` porte déjà à son
  // dernier rang : `snprintf` le réécrit à sa place, sans déborder.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
  std::snprintf(printed.data(), printed.size() + 1, format, text, amount);
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
  return printed;
}

}  // namespace

const CardWord* ItemWords::FindCard(std::uint32_t card_id) const {
  const auto found = cards.find(card_id);
  return found != cards.end() ? &found->second : nullptr;
}

std::string_view ItemWords::RepeatedWord(std::size_t identical) const {
  return identical < repeated.size() ? std::string_view(repeated[identical]) : std::string_view();
}

std::string_view ItemWords::PossessiveWord(std::uint32_t item_id, std::size_t element) const {
  if (item_id == kWeddingRingItemIdFirst || item_id == kWeddingRingItemIdSecond) {
    return wedding_possessive;
  }
  return element < possessive.size() ? std::string_view(possessive[element]) : std::string_view();
}

std::string_view ItemWords::ForgeStrengthWord(std::size_t stars) const {
  return stars < forge_strength.size() ? std::string_view(forge_strength[stars])
                                       : std::string_view();
}

bool FormatTakesOneInt(std::string_view format) {
  int conversions = 0;
  for (std::size_t at = 0; at < format.size(); ++at) {
    if (format[at] != '%') {
      continue;
    }
    ++at;
    // Un « % » en dernier caractère n'ouvre rien : le gabarit est tronqué.
    if (at >= format.size()) {
      return false;
    }
    // « %% » est un pour-cent littéral, et n'attend donc aucun argument.
    if (format[at] == '%') {
      continue;
    }
    if (format[at] != 'd' && format[at] != 'i') {
      return false;
    }
    if (++conversions > kObtainedConversionCount) {
      return false;
    }
  }
  return conversions == kObtainedConversionCount;
}

std::string ObtainedText(const ItemWords& words, int amount) {
  if (FormatTakesOneInt(words.obtained_suffix)) {
    return PrintOneInt(words.obtained_suffix.c_str(), amount);
  }
  NoteMissingWord(kObtainedName);
  return {};
}

bool FormatTakesStringThenInt(std::string_view format) {
  bool string_seen = false;
  bool int_seen = false;
  for (std::size_t at = 0; at < format.size(); ++at) {
    if (format[at] != '%') {
      continue;
    }
    ++at;
    // Un « % » en dernier caractère n'ouvre rien : le gabarit est tronqué.
    if (at >= format.size()) {
      return false;
    }
    // « %% » est un pour-cent littéral, et n'attend donc aucun argument.
    if (format[at] == '%') {
      continue;
    }
    if (format[at] == 's') {
      // En double, ou APRÈS l'entier : dans les deux cas le gabarit n'est pas
      // celui qu'on sait imprimer.
      if (string_seen || int_seen) {
        return false;
      }
      string_seen = true;
      continue;
    }
    if (format[at] == 'd' || format[at] == 'i') {
      // Avant la chaîne, ou en double : les deux moitiés se recouvrent sur
      // « %d %s », et elles ne se recouvrent QUE là. Retirer l'une laisse
      // passer « %s %d %d » ou « %d » seul.
      if (!string_seen || int_seen) {
        return false;
      }
      int_seen = true;
      continue;
    }
    // Toute autre conversion, largeur ou drapeau compris : on ne sait pas quels
    // arguments elle réclame, donc on refuse le gabarit entier.
    return false;
  }
  return string_seen && int_seen;
}

std::string GotItemText(const ItemWords& words, std::string_view item_name, int amount) {
  if (!FormatTakesStringThenInt(words.pickup.got_item)) {
    NoteMissingWord(kGotItemName);
    return {};
  }
  // ⚠ `snprintf` VEUT UNE CHAÎNE TERMINÉE PAR ZÉRO, et une vue n'en est pas
  // une : le nom se matérialise ici.
  const std::string name(item_name);
  return PrintStringThenInt(words.pickup.got_item.c_str(), name.c_str(), amount);
}

std::string DroppedItemText(const ItemWords& words, std::string_view item_name, int amount) {
  if (!FormatTakesStringThenInt(words.pickup.dropped_item)) {
    NoteMissingWord(kDroppedItemName);
    return {};
  }
  const std::string name(item_name);
  return PrintStringThenInt(words.pickup.dropped_item.c_str(), name.c_str(), amount);
}

std::string_view PickupRefusalText(const ItemWords& words, ItemPickupRefusal refusal) {
  switch (refusal) {
    case ItemPickupRefusal::kCannotGet:
      return words.pickup.cannot_get;
    case ItemPickupRefusal::kTooHeavy:
      return words.pickup.too_heavy;
    case ItemPickupRefusal::kOverWeightLimit:
      return words.pickup.over_weight_limit;
    case ItemPickupRefusal::kOneKindLimit:
      return words.pickup.one_kind_limit;
    case ItemPickupRefusal::kSameItemLimit:
      return words.pickup.same_item_limit;
  }
  // Un genre que l'énumération ne nomme pas n'existe pas : le client, lui, se
  // tait sur un code qu'il ne reconnaît pas, et c'est l'appelant qui décide de
  // ne pas venir jusqu'ici.
  return {};
}

std::string_view CardInsertText(const ItemWords& words, bool inserted) {
  return inserted ? words.card_inserted : words.card_refused;
}

std::string_view LoveSkillText(const ItemWords& words) { return words.love_skill; }

std::string_view EquipShownText(const ItemWords& words, bool shown) {
  return shown ? words.equip_shown : words.equip_hidden;
}

std::string_view CartListTitle(const ItemWords& words) { return words.cart_list_title; }

void FillMessageWords(ItemWords& words) {
  words.repeated[2] = core::Tr("Double");
  words.repeated[3] = core::Tr("Triple");
  words.repeated[4] = core::Tr("Quadruple");
  // LE POSSESSIF PORTE SON ESPACE FINAL : il se colle devant le nom de l'objet.
  words.possessive = {Edged("", core::Tr(":"), " "), Edged("", core::Tr(": Glace"), " "),
                      Edged("", core::Tr(": Terre"), " "), Edged("", core::Tr(": Feu"), " "),
                      Edged("", core::Tr(": Vent"), " ")};
  words.wedding_possessive = core::Tr("Avec M./Mme");
  words.forge_strength[kForgeStarsOne] = Edged("", core::Tr("Très robuste"), " ");
  words.forge_strength[kForgeStarsTwo] = Edged("", core::Tr("Très très robuste"), " ");
  words.forge_strength[kForgeStarsThree] = Edged("", core::Tr("Très très très robuste"), " ");
  words.nameless_creator = core::Tr("Sans nom");
  words.beloved_pet = Edged("", core::Tr("Bien-aimé"), " ");
  // LES RANGS D'INTIMITÉ PORTENT UN ESPACE DE TÊTE, comme dans le client.
  words.pet_intimacy = {Edged(" ", core::Tr("Distant"), ""), Edged(" ", core::Tr("Timide"), ""),
                        Edged(" ", core::Tr("Neutre"), ""), Edged(" ", core::Tr("Cordial"), ""),
                        Edged(" ", core::Tr("Loyal"), "")};
  words.obtained_suffix = Edged(" ", core::Tr("- %d obtenu."), "");
  words.pickup.cannot_get = core::Tr("Vous ne pouvez pas ramasser cet item.");
  words.pickup.too_heavy = core::Tr("Vous ne pouvez plus porter d'items : vous êtes en surcharge.");
  words.pickup.over_weight_limit =
      core::Tr("Vous ne pouvez pas prendre cet item : la limite de poids serait dépassée.");
  words.pickup.one_kind_limit =
      core::Tr("Vous ne pouvez pas porter plus de 30 000 items d'un même type.");
  words.pickup.same_item_limit = core::Tr("Vous avez dépassé la quantité maximale du même item.");
  words.pickup.got_item = core::Tr("Vous avez obtenu %s (%d).");
  words.pickup.dropped_item = core::Tr("Vous avez jeté %s (%d).");
  words.pickup.damaged_mark = core::Tr("(cassé)");
  words.card_inserted = core::Tr("L'insertion de la carte a réussi.");
  words.card_refused = core::Tr("L'insertion de la carte a échoué.");
  words.love_skill = core::Tr("Je t'aime.");
  words.equip_hidden = core::Tr("Informations d'équipement non publiques.");
  words.equip_shown = core::Tr("Informations d'équipement publiques.");
  words.cart_list_title = core::Tr("Liste du cart");
  FillBattleWords(words.battle);
  FillMonsterWords(words.monster);
  FillWarpListWords(words.warp_list);
  FillWarpMemoWords(words.warp_memo);
  FillTalkboxWords(words.talkbox);
  FillCashShopWords(words.cash_shop);
  FillCraftWords(words.craft);
  FillTradeWords(words.trade);
  FillChatRoomWords(words.chat_room);
  FillWhisperWords(words.whisper);
  FillAchievementWords(words.achievement);
}

core::Result<ItemWords> ReadItemWords(Vfs& vfs) {
  ItemWords words;

  FillMessageWords(words);

  auto card_words = ReadOptionalFile(vfs, kCardWordPath);
  if (!card_words) {
    return std::unexpected(card_words.error());
  }
  ReadCardWords(words, core::AsText(*card_words));

  // ⚠ L'ORDRE COMPTE : les côtés se posent sur des mots déjà lus. Inverser les
  // deux lectures perdrait tous les déplacements sans rien signaler.
  auto sides = ReadOptionalFile(vfs, kCardWordAfterPath);
  if (!sides) {
    return std::unexpected(sides.error());
  }
  ReadCardWordSides(words, core::AsText(*sides));

  return words;
}

}  // namespace ro::vfs
