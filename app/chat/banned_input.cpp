#include "chat/banned_input.h"

#include "ro/core/i18n.h"

namespace moonclient {
namespace {

// Le refus que le client écrit en rouge, et le saumon de celui d'un nom de
// guilde.
constexpr std::uint32_t kRefusalRed = 0xFF0000;
constexpr std::uint32_t kGuildNameRefusalSalmon = 0xFF966E;

}  // namespace

ro::sim::BannedWords LoadBannedWords(ro::vfs::Vfs* vfs) {
  if (vfs == nullptr) {
    return {};
  }
  auto bytes = vfs->Read(ro::sim::kBannedWordsPath);
  if (!bytes) {
    // ⚠ SANS FICHIER, RIEN NE SE MASQUE : le client d'origine fait de même.
    return {};
  }
  return ro::sim::BannedWords::FromMannerText(
      std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
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

std::uint32_t BannedInputColor(BannedInput input) {
  return input == BannedInput::kGuildName ? kGuildNameRefusalSalmon : kRefusalRed;
}

}  // namespace moonclient
