#include "chat/banned_input.h"

#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/core/text.h"
#include "ro/vfs/banned_word_list.h"

namespace moonclient {
namespace {

// Le refus que le client écrit en rouge, et le saumon de celui d'un nom de
// guilde.
constexpr std::uint32_t kRefusalRed = 0xFF0000;
constexpr std::uint32_t kGuildNameRefusalSalmon = 0xFF966E;

// Le refus d'un nom qui porte une balise de lien d'objet, le même pour un
// personnage et pour un compagnon. Rien quand il n'en porte aucune.
std::optional<std::string> ItemLinkTagRefusal(std::string_view utf8_name) {
  if (utf8_name.find(kItemLinkOpeningTag) == std::string_view::npos &&
      utf8_name.find(kItemLinkClosingTag) == std::string_view::npos) {
    return std::nullopt;
  }
  return std::string(ro::core::Tr("Un nom qui porte cette balise ne peut pas être employé."));
}

}  // namespace

ro::sim::BannedWords LoadBannedWords(ro::vfs::Vfs* vfs) {
  if (vfs == nullptr) {
    return {};
  }
  // ⚠ SANS TABLE, RIEN NE SE MASQUE : le client d'origine fait de même.
  auto words = ro::vfs::ReadBannedWordList(*vfs);
  if (!words) {
    ro::core::LogWarning("{} — rien ne se masquera", ro::core::Describe(words.error()));
    return {};
  }
  return ro::sim::BannedWords(std::move(*words));
}

std::optional<std::string> BannedInputRefusal(const ro::sim::BannedWords& banned,
                                              std::string_view utf8_text, BannedInput input) {
  if (!banned.Contains(utf8_text)) {
    return std::nullopt;
  }
  switch (input) {
    case BannedInput::kCharacterName:
    case BannedInput::kSentence:
      return std::string(ro::core::Tr("Langage grossier détecté."));
    case BannedInput::kGuildName:
      return std::string(
          ro::core::Tr("Un nom qui contient des mots interdits ne peut pas être enregistré."));
    case BannedInput::kMail:
      return std::string(ro::core::Tr("Grossièreté détectée.\nVérifiez votre texte."));
    case BannedInput::kCompanionName:
      return std::string(ro::core::Tr("Ce nom n'est pas disponible."));
    case BannedInput::kTalkbox:
      return std::string(ro::core::Tr("Filtre de discussion : non, ça ne passera pas."));
  }
  return std::string(ro::core::Tr("Langage grossier détecté."));
}

std::optional<std::string> CharacterNameRefusal(const ro::sim::BannedWords& banned,
                                                std::string_view utf8_name) {
  if (ro::core::Cp1252FromUtf8(utf8_name).size() < kCharacterNameMinBytes) {
    return std::string(ro::core::Tr("Le nom du personnage doit compter au moins 4 caractères."));
  }
  if (std::optional<std::string> refused = ItemLinkTagRefusal(utf8_name); refused.has_value()) {
    return refused;
  }
  return BannedInputRefusal(banned, utf8_name, BannedInput::kCharacterName);
}

std::optional<std::string> CompanionNameRefusal(const ro::sim::BannedWords* banned,
                                                std::string_view utf8_name) {
  if (std::optional<std::string> refused = ItemLinkTagRefusal(utf8_name); refused.has_value()) {
    return refused;
  }
  if (banned == nullptr) {
    return std::nullopt;
  }
  return BannedInputRefusal(*banned, utf8_name, BannedInput::kCompanionName);
}

std::uint32_t BannedInputColor(BannedInput input) {
  return input == BannedInput::kGuildName ? kGuildNameRefusalSalmon : kRefusalRed;
}

}  // namespace moonclient
