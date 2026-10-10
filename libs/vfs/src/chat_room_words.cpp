// Les mots des salons de discussion. Voir l'en-tête.

#include "ro/vfs/chat_room_words.h"

#include <array>
#include <string>
#include <tuple>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/vfs/item_words.h"
#include "ro/vfs/trade_words.h"

#include "message_word.h"

namespace ro::vfs {
namespace {

// Le nom sous lequel un gabarit fautif se journalise.
constexpr std::string_view kTemplateName = "un gabarit des salons";

// La conversion d'un nom, et l'échappement d'un pourcent.
constexpr std::string_view kNameConversion = "%s";
constexpr std::string_view kEscapedPercent = "%%";

}  // namespace

std::string_view ChatRoomText(const ItemWords& words, ChatRoomWord word) {
  const auto rank = static_cast<std::size_t>(word);
  if (rank >= kChatRoomWordCount) {
    return {};
  }
  return words.chat_room.texts[rank];
}

std::string ChatRoomTextWith(const ItemWords& words, ChatRoomWord word, std::string_view name) {
  const std::string_view format = ChatRoomText(words, word);
  if (format.empty()) {
    return {};
  }
  if (TradeFormatShape(format) != "s") {
    return std::string(NoteMissingWord(kTemplateName));
  }
  std::string out;
  out.reserve(format.size() + name.size());
  for (std::size_t at = 0; at < format.size(); ++at) {
    if (format.substr(at).starts_with(kEscapedPercent)) {
      out.push_back('%');
      ++at;
    } else if (format.substr(at).starts_with(kNameConversion)) {
      out.append(name);
      ++at;
    } else {
      out.push_back(format[at]);
    }
  }
  return out;
}

void FillChatRoomWords(ChatRoomWords& words) {
  // DANS L'ORDRE DE `ChatRoomWord` : le rang de l'un est l'indice de l'autre.
  auto texts = std::to_array<std::string>({
      core::Tr("Veuillez saisir le titre du salon."),
      core::Tr("Veuillez saisir le mot de passe."),
      core::Tr("Veuillez saisir le mot de passe. Il doit compter au moins 4 caractères."),
      core::Tr("Le salon a bien été créé."),
      core::Tr("Nombre maximal de salons atteint."),
      core::Tr("Un salon du même nom existe déjà."),
      core::Tr("Le salon est plein."),
      core::Tr("Vous avez été expulsé de ce salon."),
      core::Tr("Priv. :"),
      core::Tr("Publ. :"),
      core::Tr("Créer un salon de chat"),
      core::Tr("Réglages du salon"),
      core::Tr("Expulser le personnage"),
      core::Tr("Céder les droits de chef"),
      core::Tr("pers."),
      core::Tr("(%s) est entré."),
      core::Tr("(%s) est parti."),
      core::Tr("(%s) a été expulsé."),
      core::Tr("Ce serveur n'accepte que les caractères anglais."),
      core::Tr("Privé"),
      core::Tr("Public"),
      core::Tr("Vous n'avez pas le niveau requis."),
      core::Tr("Votre niveau est trop élevé pour cette classe."),
      core::Tr("Cette classe ne convient pas à ce type de travail."),
      core::Tr("Titre :"),
      core::Tr("Limite :"),
      core::Tr("Mot de passe :"),
  });
  // ⚠ UN TEXTE OUBLIÉ NE SE VERRAIT PAS : tous les mots suivants seraient
  // décalés d'un rang. Le compte se vérifie à la compilation.
  static_assert(std::tuple_size_v<decltype(texts)> == kChatRoomWordCount);
  words.texts = std::move(texts);
}

}  // namespace ro::vfs
