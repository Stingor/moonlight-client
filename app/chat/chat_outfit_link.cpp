#include "chat/chat_outfit_link.h"

#include <optional>

#include "ro/core/i18n.h"
#include "ro/net/style_outfits.h"

namespace moonclient {
namespace {

constexpr char kSeparator = ':';
constexpr char kBracket = '<';
// Ce qui remplace, dans une étiquette, ce qui couperait la balise.
constexpr char kSafe = ' ';

}  // namespace

std::string BuildOutfitLink(std::string_view label, std::string_view code) {
  const std::optional<std::string> normalized = ro::net::NormalizeOutfitCode(code);
  if (!normalized.has_value()) {
    return {};
  }
  std::string tag(kOutfitLinkOpen);
  for (const char letter : label) {
    tag.push_back(letter == kSeparator || letter == kBracket ? kSafe : letter);
  }
  tag.push_back(kSeparator);
  tag += *normalized;
  tag += kOutfitLinkClose;
  return tag;
}

std::size_t FindOutfitLink(std::string_view text, std::size_t from) {
  return from >= text.size() ? std::string_view::npos : text.find(kOutfitLinkOpen, from);
}

bool ParseOutfitLink(std::string_view text, OutfitLink* out, std::size_t* after) {
  if (after != nullptr) {
    *after = 0;
  }
  if (out == nullptr || !text.starts_with(kOutfitLinkOpen)) {
    return false;
  }
  const std::size_t close = text.find(kOutfitLinkClose, kOutfitLinkOpen.size());
  if (close == std::string_view::npos) {
    return false;
  }
  if (after != nullptr) {
    *after = close + kOutfitLinkClose.size();
  }
  const std::string_view body = text.substr(kOutfitLinkOpen.size(), close - kOutfitLinkOpen.size());
  // ⚠ LE DERNIER DEUX-POINTS SÉPARE : le code n'en porte jamais, l'étiquette
  // nettoyée non plus, mais une balise forgée à la main pourrait en glisser un
  // dans l'étiquette.
  const std::size_t split = body.rfind(kSeparator);
  if (split == std::string_view::npos) {
    return false;
  }
  const std::optional<std::string> code = ro::net::NormalizeOutfitCode(body.substr(split + 1));
  if (!code.has_value()) {
    return false;
  }
  out->label = std::string(body.substr(0, split));
  out->code = *code;
  return true;
}

std::string OutfitLinkLabel(std::string_view label) {
  std::string shown = "[";
  shown += ro::core::Tr("Tenue");
  // ⚠ SANS ÉTIQUETTE, PAS DE DEUX-POINTS : « [Tenue : ] » se lirait comme un
  // nom qui manque.
  if (!label.empty()) {
    shown += " : ";
    shown.append(label);
  }
  shown.push_back(']');
  return shown;
}

}  // namespace moonclient
