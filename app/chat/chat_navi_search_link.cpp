// Le lien d'une recherche de navigation, `<NAVS>`. Voir l'en-tête pour sa forme.

#include "chat/chat_navi_search_link.h"

#include <array>
#include <cstdio>
#include <optional>
#include <string>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/text.h"
#include "ro/sim/navi_search.h"

namespace moonclient {
namespace {

constexpr char kFieldSeparator = ':';
// Ce qu'un contexte ne porte jamais : il occupe un champ du MILIEU, et une
// espace ou une tabulation le ferait relire comme le début du terme.
constexpr std::string_view kContextRefused = "<>: \t";
// Ce qu'un terme ne porte jamais : il couperait la balise à la relecture.
constexpr std::string_view kTermRefused = "<>";
// Ce que le segment d'une carte ne porte pas : sinon, c'est le terme qui
// commence.
constexpr std::string_view kBlanks = " \t";
// LA BALISE ENTIÈRE TIENT EN 255 OCTETS, ou elle ne s'écrit pas : c'est la
// borne des autres écrivains de la balise, et une ligne de conversation n'en
// porte pas davantage.
constexpr std::size_t kNaviSearchLinkMostBytes = 255;
// Les blancs qu'une famille peut porter en tête, comme une lecture de nombre
// du C les saute.
constexpr std::string_view kLeadingBlanks = " \t\n\v\f\r";
constexpr int kDecimalBase = 10;
// Au-delà, une famille est inconnue de toute façon : la lecture s'arrête là
// plutôt que de déborder.
constexpr int kFamilyReadCeiling = 1000;
// Le libellé composé : un nom, une carte lisible et leur habillage.
constexpr std::size_t kLabelBytes = 224;
constexpr std::string_view kGatExtension = ".gat";

// LA FAMILLE TELLE QU'UNE LECTURE DE NOMBRE DU C LA LIT : les blancs de tête
// sautés, un signe, puis les chiffres jusqu'au premier qui n'en est pas un.
// Rien de numérique se lit zéro.
[[nodiscard]] int ReadFamily(std::string_view field) {
  std::size_t at = field.find_first_not_of(kLeadingBlanks);
  if (at == std::string_view::npos) {
    return 0;
  }
  bool negative = false;
  if (field[at] == '+' || field[at] == '-') {
    negative = field[at] == '-';
    ++at;
  }
  int value = 0;
  while (at < field.size() && field[at] >= '0' && field[at] <= '9') {
    value = value * kDecimalBase + (field[at] - '0');
    if (value >= kFamilyReadCeiling) {
      value = kFamilyReadCeiling;
      break;
    }
    ++at;
  }
  return negative ? -value : value;
}

[[nodiscard]] bool KnownFamily(int family) {
  return family >= static_cast<int>(NaviSearchFamily::kMap) &&
         family <= static_cast<int>(NaviSearchFamily::kMonster);
}

[[nodiscard]] bool UsableContext(std::string_view map) {
  return !map.empty() && map.find_first_of(kContextRefused) == std::string_view::npos;
}

[[nodiscard]] bool UsableTerm(std::string_view term) {
  return !term.empty() && term.find_first_of(kTermRefused) == std::string_view::npos;
}

// Le nom lisible d'une carte, son nom interne quand la table l'ignore.
[[nodiscard]] std::string MapShown(std::string_view map, const ro::vfs::MapDisplayNames* names) {
  if (names != nullptr && !map.empty()) {
    std::string gat_name(map);
    gat_name += kGatExtension;
    if (const std::optional<std::string_view> shown = names->DisplayNameOf(gat_name);
        shown.has_value() && !shown->empty()) {
      return std::string(*shown);
    }
  }
  return std::string(map);
}

}  // namespace

std::string BuildNaviSearchLink(const NaviSearchLink& link) {
  if (!UsableTerm(link.term) || !KnownFamily(static_cast<int>(link.family))) {
    return {};
  }
  std::string wire(kNaviSearchLinkOpen);
  wire += std::to_string(static_cast<unsigned>(link.family));
  wire.push_back(kFieldSeparator);
  // 🔴 LE CHAMP CARTE S'ÉCRIT MÊME VIDE : `2::Baphomet`.
  if (UsableContext(link.map)) {
    wire += link.map;
  }
  wire.push_back(kFieldSeparator);
  wire += link.term;
  wire += kNaviSearchLinkClose;
  if (wire.size() > kNaviSearchLinkMostBytes) {
    return {};
  }
  return wire;
}

std::string NpcSearchTerm(std::string_view npc_name) {
  NaviSearchLink link{.family = NaviSearchFamily::kNpc,
                      .map = {},
                      .term = std::string(ro::sim::VisibleNpcName(npc_name))};
  if (BuildNaviSearchLink(link).empty()) {
    return {};
  }
  return std::move(link.term);
}

NaviSearchLinkRead ParseNaviSearchLink(std::string_view text, NaviSearchLink* out,
                                       std::size_t* after, std::string_view* raw_term) {
  if (!text.starts_with(kNaviSearchLinkOpen)) {
    return NaviSearchLinkRead::kNoTag;
  }
  const std::size_t body_at = kNaviSearchLinkOpen.size();
  const std::size_t close_at = text.find(kNaviSearchLinkClose, body_at);
  const std::string_view body = close_at == std::string_view::npos
                                    ? std::string_view()
                                    : text.substr(body_at, close_at - body_at);
  const std::size_t first = body.find(kFieldSeparator);
  if (close_at == std::string_view::npos || first == std::string_view::npos) {
    if (after != nullptr) {
      *after = body_at;
    }
    return NaviSearchLinkRead::kNotATag;
  }
  if (after != nullptr) {
    *after = close_at + kNaviSearchLinkClose.size();
  }
  const int family = ReadFamily(body.substr(0, first));
  // LE SEGMENT DU MILIEU n'est une carte que sans blanc : sinon, tout ce qui
  // suit le premier `:` est le terme, comme dans la forme à deux champs.
  std::string_view map;
  std::string_view term = body.substr(first + 1);
  if (const std::size_t second = term.find(kFieldSeparator);
      second != std::string_view::npos &&
      term.substr(0, second).find_first_of(kBlanks) == std::string_view::npos) {
    map = term.substr(0, second);
    term = term.substr(second + 1);
  }
  if (raw_term != nullptr) {
    *raw_term = term;
  }
  if (term.empty()) {
    return NaviSearchLinkRead::kDropped;
  }
  // ⚠ UNE FAMILLE INCONNUE VIENT D'UN ÉCRIVAIN PLUS RÉCENT : on ne devine pas,
  // le terme se montre en texte.
  if (!KnownFamily(family) || !UsableTerm(term)) {
    return NaviSearchLinkRead::kAsText;
  }
  if (out != nullptr) {
    out->family = static_cast<NaviSearchFamily>(family);
    // Un contexte mal formé se perd, la recherche reste : elle part sans carte.
    out->map = UsableContext(map) ? std::string(map) : std::string();
    out->term = std::string(term);
  }
  return NaviSearchLinkRead::kRead;
}

std::size_t FindNaviSearchLink(std::string_view text, std::size_t from) {
  if (from >= text.size()) {
    return std::string_view::npos;
  }
  return text.find(kNaviSearchLinkOpen, from);
}

std::string NaviSearchLinkLabel(const NaviSearchLink& link, const ro::vfs::MapDisplayNames* names) {
  if (link.term.empty()) {
    return {};
  }
  // UNE CARTE VOYAGE SOUS SON NOM INTERNE, et se montre sous son nom lisible ;
  // un PNJ, sans la partie cachée de son nom.
  std::string term = link.term;
  if (link.family == NaviSearchFamily::kMap) {
    term = MapShown(link.term, names);
  } else if (link.family == NaviSearchFamily::kNpc) {
    term = std::string(ro::sim::VisibleNpcName(link.term));
  }
  const std::string context = MapShown(link.map, names);
  const bool bounded = !context.empty();
  std::array<char, kLabelBytes> label{};
  switch (link.family) {
    case NaviSearchFamily::kMap:
      if (bounded) {
        std::snprintf(label.data(), label.size(), ro::core::Tr("[Carte: %s (%s)]"), term.c_str(),
                      context.c_str());
      } else {
        std::snprintf(label.data(), label.size(), ro::core::Tr("[Carte: %s]"), term.c_str());
      }
      break;
    case NaviSearchFamily::kNpc:
      if (bounded) {
        std::snprintf(label.data(), label.size(), ro::core::Tr("[PNJ: %s (%s)]"), term.c_str(),
                      context.c_str());
      } else {
        std::snprintf(label.data(), label.size(), ro::core::Tr("[PNJ: %s]"), term.c_str());
      }
      break;
    case NaviSearchFamily::kMonster:
      if (bounded) {
        std::snprintf(label.data(), label.size(), ro::core::Tr("[Monstre: %s (%s)]"), term.c_str(),
                      context.c_str());
      } else {
        std::snprintf(label.data(), label.size(), ro::core::Tr("[Monstre: %s]"), term.c_str());
      }
      break;
  }
  // ⚠ LA COUPE DE `snprintf` TOMBE À L'OCTET : un nom accentué y perdrait la
  // moitié d'un caractère.
  return std::string(ro::core::WithoutDanglingUtf8(label.data()));
}

ro::ui::NaviSearchMenuFacts NaviSearchFactsOf(const NaviSearchLink& link) {
  return ro::ui::NaviSearchMenuFacts{.family = static_cast<std::uint8_t>(link.family),
                                     .bounded = !link.map.empty()};
}

}  // namespace moonclient
