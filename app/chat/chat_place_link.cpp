#include "chat/chat_place_link.h"

#include <algorithm>
#include <utility>

#include "ro/core/i18n.h"

namespace moonclient {
namespace {

constexpr char kLabelOpen = '[';
constexpr char kLabelClose = ']';
constexpr char kCoordinateSeparator = ',';
constexpr std::string_view kGatExtension = ".gat";

// Deux caractères, poids faible d'abord.
void AppendCoordinate(std::string& out, int value) {
  const int bounded = std::clamp(value, 0, kPlaceLinkCoordinateMax);
  out.push_back(kPlaceLinkDigits[static_cast<std::size_t>(bounded % kPlaceLinkRadix)]);
  out.push_back(kPlaceLinkDigits[static_cast<std::size_t>(bounded / kPlaceLinkRadix)]);
}

// Le chiffre base 62 du caractère, ou -1.
[[nodiscard]] int DigitOf(char letter) {
  const std::size_t at = kPlaceLinkDigits.find(letter);
  return at == std::string_view::npos ? -1 : static_cast<int>(at);
}

// Lit deux caractères, poids faible d'abord. Faux si l'un n'est pas du base 62.
[[nodiscard]] bool ReadCoordinate(std::string_view digits, int* value) {
  const int low = DigitOf(digits[0]);
  const int high = DigitOf(digits[1]);
  if (low < 0 || high < 0) {
    return false;
  }
  *value = low + high * kPlaceLinkRadix;
  return true;
}

}  // namespace

std::string BuildPlaceLink(const PlaceLink& link) {
  if (link.map.empty() || link.map.find_first_of("<>") != std::string::npos) {
    return {};
  }
  std::string tag(kPlaceLinkOpen);
  tag += link.map;
  AppendCoordinate(tag, link.x);
  AppendCoordinate(tag, link.y);
  tag += kPlaceLinkClose;
  return tag;
}

bool ParsePlaceLink(std::string_view text, PlaceLink* out, std::size_t* after) {
  if (after != nullptr) {
    *after = 0;
  }
  if (!text.starts_with(kPlaceLinkOpen)) {
    return false;
  }
  const std::size_t body_at = kPlaceLinkOpen.size();
  const std::size_t close = text.find(kPlaceLinkClose, body_at);
  if (after != nullptr) {
    *after = close == std::string_view::npos ? text.size() : close + kPlaceLinkClose.size();
  }
  if (close == std::string_view::npos) {
    return false;
  }
  const std::string_view body = text.substr(body_at, close - body_at);
  // Au moins un caractère de nom, plus les quatre des coordonnées.
  if (body.size() <= kPlaceLinkTailLength) {
    return false;
  }
  const std::size_t tail_at = body.size() - kPlaceLinkTailLength;
  PlaceLink read;
  if (!ReadCoordinate(body.substr(tail_at, kPlaceLinkCoordinateDigits), &read.x) ||
      !ReadCoordinate(body.substr(tail_at + kPlaceLinkCoordinateDigits), &read.y)) {
    return false;
  }
  read.map = std::string(body.substr(0, tail_at));
  if (out != nullptr) {
    *out = std::move(read);
  }
  return true;
}

std::size_t FindPlaceLink(std::string_view text, std::size_t from) {
  return text.find(kPlaceLinkOpen, from);
}

std::string PlaceLinkLabel(const PlaceLink& link, std::string_view shown_name) {
  std::string label(1, kLabelOpen);
  label += shown_name.empty() ? std::string_view(link.map) : shown_name;
  if (link.x != 0 || link.y != 0) {
    label.push_back(' ');
    label += std::to_string(link.x);
    label.push_back(kCoordinateSeparator);
    label += std::to_string(link.y);
  }
  label.push_back(kLabelClose);
  return label;
}

std::string PlaceShownName(const ro::vfs::MapDisplayNames* names, std::string_view map) {
  if (names == nullptr || map.empty()) {
    return {};
  }
  // La table est indexée par le fichier `.gat`.
  std::string gat_name(map);
  gat_name += kGatExtension;
  return std::string(names->DisplayNameOf(gat_name).value_or(std::string_view()));
}

std::string PlaceLinkLabel(const PlaceLink& link, const ro::vfs::MapDisplayNames* names) {
  return PlaceLinkLabel(link, PlaceShownName(names, link.map));
}

std::string PlaceDestinationLine(const PlaceLink& link, const ro::vfs::MapDisplayNames* names) {
  // ⚠ L'ESPACE EST POSÉE ICI, PAS DANS LE TEXTE : un catalogue rogne les espaces
  // de bord, et « Destination : » les perdrait à la relecture.
  std::string line(ro::core::Tr("Destination :"));
  line.push_back(' ');
  line += PlaceLinkLabel(link, names);
  return line;
}

ro::ui::NavigationPlaceTip PlaceTipText(const PlaceLink& link,
                                        const ro::vfs::MapDisplayNames* names) {
  return ro::ui::NavigationPlaceTip{
      .label = PlaceShownName(names, link.map), .map_name = link.map, .x = link.x, .y = link.y};
}

}  // namespace moonclient
