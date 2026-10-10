#include "chat/chat_setting_link.h"

#include "chat/chat_item_link.h"

namespace moonclient {
namespace {

constexpr char kFieldSeparator = ':';

}  // namespace

std::string BuildSettingLink(const SettingLink& link) {
  // ⚠ NI FERMANTE NI OUVRANTE DANS LE LIBELLÉ : l'une couperait la balise,
  // l'autre la ferait lire comme coupée.
  if (!SettingKeyTravels(link.key) || link.fallback.find(kSettingLinkClose) != std::string::npos ||
      link.fallback.find(kSettingLinkOpen) != std::string::npos) {
    return {};
  }
  std::string tag(kSettingLinkOpen);
  tag += link.key;
  tag.push_back(kFieldSeparator);
  tag += link.fallback;
  tag += kSettingLinkClose;
  return tag;
}

SettingLinkRead ParseSettingLink(std::string_view text, SettingLink* out, std::size_t* after) {
  const std::size_t body_at = kSettingLinkOpen.size();
  const std::size_t close = text.find(kSettingLinkClose, body_at);
  // ⚠ UNE OUVRANTE AVANT LA FERMANTE : celle-ci appartient au lien suivant, et
  // la première balise est coupée — l'avaler ferait du lien d'après un libellé.
  const std::size_t next_open = text.find(kSettingLinkOpen, body_at);
  if (!text.starts_with(kSettingLinkOpen) || close == std::string_view::npos || next_open < close) {
    if (after != nullptr) {
      *after = text.starts_with(kSettingLinkOpen) ? body_at : 0;
    }
    return SettingLinkRead::kUnclosed;
  }
  if (after != nullptr) {
    *after = close + kSettingLinkClose.size();
  }
  const std::string_view body = text.substr(body_at, close - body_at);
  const std::size_t colon = body.find(kFieldSeparator);
  if (colon == std::string_view::npos) {
    return SettingLinkRead::kNoSeparator;
  }
  if (out != nullptr) {
    out->key = std::string(body.substr(0, colon));
    out->fallback = std::string(body.substr(colon + 1));
  }
  return SettingLinkRead::kRead;
}

std::size_t FindSettingLink(std::string_view text, std::size_t from) {
  return text.find(kSettingLinkOpen, from);
}

PendingChatLink SettingPendingLink(const SettingDestination& destination) {
  // 🔴 LE LIBELLÉ POSÉ EST CELUI QUE LE FIL RENDRA, chez nous comme chez le
  // lecteur dans sa langue : ce que le joueur voit avant l'envoi est ce que
  // tous liront après.
  return PendingChatLink{
      .shown = SettingLinkLabel(destination),
      .wire = BuildSettingLink(SettingLink{.key = std::string(destination.key),
                                           .fallback = SettingDestinationFallback(destination)}),
      .kind = ro::ui::ChatLinkKind::kSetting};
}

}  // namespace moonclient
