#include "chat/chat_layer_link.h"

#include "ro/core/i18n.h"
#include "ro/proto/chat_limits.h"
#include "ro/sim/style_layer_code.h"

namespace moonclient {
namespace {

constexpr char kSeparator = ':';
constexpr char kBracket = '<';
// Ce qui remplace, dans une étiquette, ce qui couperait la balise.
constexpr char kSafe = ' ';

}  // namespace

std::string BuildLayerLink(std::string_view label, std::string_view code) {
  if (!ro::sim::DecodeStyleLayerCode(code).has_value()) {
    return {};
  }
  std::string tag(kLayerLinkOpen);
  for (const char letter : label) {
    tag.push_back(letter == kSeparator || letter == kBracket ? kSafe : letter);
  }
  tag.push_back(kSeparator);
  tag.append(code);
  tag += kLayerLinkClose;
  // 🔴 SEULE, LA BALISE DOIT TENIR DANS UNE LIGNE : au-delà, le serveur jette
  // le message entier, sans un mot.
  if (tag.size() > ro::proto::kChatTextMax) {
    return {};
  }
  return tag;
}

std::size_t FindLayerLink(std::string_view text, std::size_t from) {
  return from >= text.size() ? std::string_view::npos : text.find(kLayerLinkOpen, from);
}

bool ParseLayerLink(std::string_view text, LayerLink* out, std::size_t* after) {
  if (after != nullptr) {
    *after = 0;
  }
  if (out == nullptr || !text.starts_with(kLayerLinkOpen)) {
    return false;
  }
  const std::size_t close = text.find(kLayerLinkClose, kLayerLinkOpen.size());
  if (close == std::string_view::npos) {
    return false;
  }
  if (after != nullptr) {
    *after = close + kLayerLinkClose.size();
  }
  const std::string_view body = text.substr(kLayerLinkOpen.size(), close - kLayerLinkOpen.size());
  const std::size_t split = body.find(kSeparator);
  if (split == std::string_view::npos) {
    return false;
  }
  const std::string_view code = body.substr(split + 1);
  if (!ro::sim::DecodeStyleLayerCode(code).has_value()) {
    return false;
  }
  out->label = std::string(body.substr(0, split));
  out->code = std::string(code);
  return true;
}

std::string LayerLinkLabel(std::string_view label) {
  std::string shown = "[";
  shown += ro::core::Tr("Personnalisation");
  if (!label.empty()) {
    shown += " : ";
    shown.append(label);
  }
  shown.push_back(']');
  return shown;
}

}  // namespace moonclient
