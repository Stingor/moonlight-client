// Le lien d'un respawn de MVP, `<MVPL>`. Voir l'en-tête pour sa forme.

#include "chat/chat_mvp_link.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <ctime>
#include <format>
#include <limits>
#include <optional>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/local_time.h"
#include "ro/core/printed.h"
#include "ro/core/text.h"
#include "ro/net/mvp_tracker.h"

namespace moonclient {
namespace {

constexpr char kFieldSeparator = ':';
// Les champs qui précèdent le libellé : monstre, carte, mort, instant, deux
// délais, deux coordonnées. Le libellé vient EN DERNIER pour pouvoir porter des
// ':' : on découpe ce nombre de fois, et le reste EST le libellé.
constexpr std::size_t kMvpLinkLeadingFields = 8;
constexpr std::size_t kMonsterField = 0;
constexpr std::size_t kMapField = 1;
constexpr std::size_t kKillField = 2;
constexpr std::size_t kRespawnField = 3;
constexpr std::size_t kDelay1Field = 4;
constexpr std::size_t kDelay2Field = 5;
constexpr std::size_t kTombXField = 6;
constexpr std::size_t kTombYField = 7;
constexpr std::size_t kLabelField = kMvpLinkLeadingFields;

constexpr std::int64_t kSecondsPerMinute = 60;
constexpr std::int64_t kMsPerSecond = 1000;
// Le libellé composé : le nom, la carte et deux heures y tiennent largement.
constexpr std::size_t kMvpLabelBytes = 224;
constexpr std::string_view kSenderMark = " :";

constexpr std::int64_t kDecimalBase = 10;

// UNE LECTURE LAXISTE, celle d'`atoll` : espaces de tête, UN signe facultatif
// ('+' ou '-'), chiffres jusqu'au premier autre caractère, zéro s'il n'y en a
// aucun ; un nombre trop grand SATURE à la borne du type. Le champ se lit ainsi
// comme les autres clients le lisent. La plage d'une HEURE se borne à part :
// voir `LinkSeconds`.
[[nodiscard]] std::int64_t Lax64(std::string_view field) {
  constexpr std::int64_t kLowest = std::numeric_limits<std::int64_t>::min();
  constexpr std::int64_t kHighest = std::numeric_limits<std::int64_t>::max();
  std::size_t at = 0;
  while (at < field.size() && (field[at] == ' ' || field[at] == '\t' || field[at] == '\n' ||
                               field[at] == '\r' || field[at] == '\v' || field[at] == '\f')) {
    ++at;
  }
  bool negative = false;
  if (at < field.size() && (field[at] == '+' || field[at] == '-')) {
    negative = field[at] == '-';
    ++at;
  }
  // ⚠ CUMULÉ EN NÉGATIF : la plus petite valeur n'a pas d'opposée positive.
  std::int64_t value = 0;
  for (; at < field.size() && field[at] >= '0' && field[at] <= '9'; ++at) {
    const std::int64_t digit = field[at] - '0';
    if (value < (kLowest + digit) / kDecimalBase) {
      return negative ? kLowest : kHighest;
    }
    value = value * kDecimalBase - digit;
  }
  if (negative) {
    return value;
  }
  return value == kLowest ? kHighest : -value;
}

// La même lecture, celle d'`atoi` : saturée aux bornes d'un entier de 32 bits,
// AVANT toute conversion vers le type du champ.
[[nodiscard]] std::int32_t Lax32(std::string_view field) {
  return static_cast<std::int32_t>(
      std::clamp<std::int64_t>(Lax64(field), std::numeric_limits<std::int32_t>::min(),
                               std::numeric_limits<std::int32_t>::max()));
}

// 🔴 UNE HEURE DU LIEN SE BORNE COMME CELLES DU FIL : le texte vient de
// n'importe quel joueur, et le libellé, l'échéance et l'aperçu y ajoutent des
// durées sans saturer. Voir `kMvpLatestWireSeconds`.
[[nodiscard]] std::int64_t LinkSeconds(std::string_view field) {
  return std::clamp<std::int64_t>(Lax64(field), 0, ro::net::kMvpLatestWireSeconds);
}

// L'expéditeur en tête de ligne, « Nom : texte », s'il tient en `most_wire_bytes`
// octets DU FIL.
[[nodiscard]] std::string HeadBefore(std::string_view text, std::size_t most_wire_bytes) {
  const std::size_t mark = text.find(kSenderMark);
  if (mark == std::string_view::npos || mark == 0) {
    return {};
  }
  const std::string_view head = text.substr(0, mark);
  if (ro::core::Cp1252FromUtf8(head).size() > most_wire_bytes) {
    return {};
  }
  return std::string(head);
}

// LA TOMBE SE CITE AVANT LE CROCHET FINAL de la forme choisie. Une cellule
// (0, 0) existe : seule une coordonnée négative dit la tombe inconnue.
[[nodiscard]] std::string WithTomb(const MvpLink& link, const char* label) {
  std::string text(label);
  if (link.tomb_x < 0 || link.tomb_y < 0 || text.empty() || text.back() != ']') {
    return text;
  }
  text.insert(text.size() - 1,
              ro::core::Printed(ro::core::Tr(", tombe en %d,%d"), link.tomb_x, link.tomb_y));
  return text;
}

// LA LOI QUE LE CATALOGUE DU LECTEUR PRÊTE à un lien qui n'en porte pas : le
// retour au plus tôt plus l'amplitude, en secondes ENTIÈRES comme la fenêtre du
// carnet. Le créneau scripté de la carte répond pour un monstre tiré. Rien sans
// catalogue reçu, sans créneau, ou sans retour au plus tôt.
[[nodiscard]] std::optional<std::int64_t> CatalogueLawSeconds(
    const MvpLink& link, const ro::sim::MvpTrackerBook* catalogue) {
  if (catalogue == nullptr || !catalogue->catalog_known() ||
      link.monster_id > std::numeric_limits<std::uint16_t>::max()) {
    return std::nullopt;
  }
  const ro::sim::MvpSlot* slot =
      catalogue->FindSlotFor(static_cast<std::uint16_t>(link.monster_id), link.map);
  if (slot == nullptr || slot->delay1_ms == 0) {
    return std::nullopt;
  }
  // ⚠ CHAQUE DÉLAI TRONQUÉ À PART, comme le carnet : la somme tronquée d'un
  // bloc gagnerait une seconde sur lui quand les deux restes la font.
  return static_cast<std::int64_t>(slot->delay1_ms) / kMsPerSecond +
         static_cast<std::int64_t>(slot->delay2_ms) / kMsPerSecond;
}

}  // namespace

std::string BuildMvpLink(const MvpLink& link) {
  if (link.map.empty() || link.name.empty()) {
    return {};
  }
  const std::string body = std::format("{}:{}:{}:{}:{}:{}:{}:{}:{}", link.monster_id, link.map,
                                       link.kill_time_s, link.exact_respawn_s, link.delay1_min,
                                       link.delay2_min, link.tomb_x, link.tomb_y, link.name);
  // ⚠ UN CHEVRON FERMERAIT LA BALISE D'UN AUTRE : aucun lien plutôt qu'un lien
  // qui casse la ligne.
  if (body.find_first_of("<>") != std::string::npos) {
    return {};
  }
  std::string tag(kMvpLinkOpen);
  tag += body;
  tag += kMvpLinkClose;
  return tag;
}

MvpLinkRead ParseMvpLink(std::string_view text, MvpLink* out, std::size_t* after,
                         std::string_view* body) {
  if (!text.starts_with(kMvpLinkOpen)) {
    return MvpLinkRead::kNoTag;
  }
  const std::size_t body_at = kMvpLinkOpen.size();
  const std::size_t close = text.find(kMvpLinkClose, body_at);
  if (close == std::string_view::npos) {
    if (after != nullptr) {
      *after = text.size();
    }
    return MvpLinkRead::kUnclosed;
  }
  const std::string_view inside = text.substr(body_at, close - body_at);
  if (after != nullptr) {
    *after = close + kMvpLinkClose.size();
  }
  if (body != nullptr) {
    *body = inside;
  }
  std::array<std::string_view, kMvpLinkLeadingFields + 1> part{};
  std::size_t at = 0;
  for (std::size_t field = 0; field < kMvpLinkLeadingFields; ++field) {
    const std::size_t colon = inside.find(kFieldSeparator, at);
    if (colon == std::string_view::npos) {
      return MvpLinkRead::kUnreadable;
    }
    part.at(field) = inside.substr(at, colon - at);
    at = colon + 1;
  }
  part.at(kLabelField) = inside.substr(at);
  if (part.at(kMapField).empty() || part.at(kLabelField).empty()) {
    return MvpLinkRead::kUnreadable;
  }
  const std::int64_t kill_s = LinkSeconds(part.at(kKillField));
  const std::int64_t respawn_s = LinkSeconds(part.at(kRespawnField));
  // NI MORT NI INSTANT : le lien n'apprend rien, et c'est son seul contenu.
  if (kill_s <= 0 && respawn_s <= 0) {
    return MvpLinkRead::kUnreadable;
  }
  if (out != nullptr) {
    MvpLink read;
    read.monster_id = static_cast<std::uint32_t>(Lax32(part.at(kMonsterField)));
    read.map = std::string(part.at(kMapField));
    read.kill_time_s = kill_s;
    read.exact_respawn_s = respawn_s;
    read.delay1_min = static_cast<std::uint16_t>(Lax32(part.at(kDelay1Field)));
    read.delay2_min = static_cast<std::uint16_t>(Lax32(part.at(kDelay2Field)));
    read.tomb_x = static_cast<std::int16_t>(Lax32(part.at(kTombXField)));
    read.tomb_y = static_cast<std::int16_t>(Lax32(part.at(kTombYField)));
    read.name = std::string(part.at(kLabelField));
    *out = std::move(read);
  }
  return MvpLinkRead::kRead;
}

std::size_t FindMvpLink(std::string_view text, std::size_t from) {
  return text.find(kMvpLinkOpen, from);
}

std::string MvpClockText(std::int64_t unix_s) {
  if (unix_s <= 0) {
    return {};
  }
  const auto seconds = static_cast<std::time_t>(unix_s);
  std::tm local{};
  if (!ro::core::ToLocalTime(seconds, local)) {
    return {};
  }
  return ro::core::Printed("%02d:%02d", local.tm_hour, local.tm_min);
}

// CE QU'ON MET EN TÊTE, C'EST LE RETOUR : c'est la question posée. L'heure de
// mort ne s'affiche que faute de mieux. Chaque forme exige que ses heures se
// lisent, sinon la suivante répond.
std::string MvpLinkLabel(const MvpLink& link) {
  const char* name = link.name.empty() ? "?" : link.name.c_str();
  const char* map = link.map.empty() ? "?" : link.map.c_str();
  std::array<char, kMvpLabelBytes> label{};
  // UN INSTANT EXACT : quelqu'un a payé un Convex Mirror et l'a partagé.
  if (link.exact_respawn_s > 0) {
    if (const std::string at = MvpClockText(link.exact_respawn_s); !at.empty()) {
      std::snprintf(label.data(), label.size(), ro::core::Tr("[MVP: %s (%s) — retour à %s]"), name,
                    map, at.c_str());
      return WithTomb(link, label.data());
    }
  }
  // SINON LA LOI, appliquée à l'heure de mort : une fenêtre, jamais un point.
  if (link.kill_time_s > 0 && link.delay1_min > 0) {
    const std::int64_t from_s =
        link.kill_time_s + static_cast<std::int64_t>(link.delay1_min) * kSecondsPerMinute;
    const std::int64_t to_s =
        from_s + static_cast<std::int64_t>(link.delay2_min) * kSecondsPerMinute;
    const std::string from = MvpClockText(from_s);
    const std::string to = MvpClockText(to_s);
    if (!from.empty() && !to.empty()) {
      std::snprintf(label.data(), label.size(), ro::core::Tr("[MVP: %s (%s) — retour %s–%s]"), name,
                    map, from.c_str(), to.c_str());
      return WithTomb(link, label.data());
    }
  }
  if (link.kill_time_s > 0) {
    if (const std::string at = MvpClockText(link.kill_time_s); !at.empty()) {
      std::snprintf(label.data(), label.size(), ro::core::Tr("[MVP: %s (%s) — mort à %s]"), name,
                    map, at.c_str());
      return WithTomb(link, label.data());
    }
  }
  std::snprintf(label.data(), label.size(), ro::core::Tr("[MVP: %s (%s)]"), name, map);
  return WithTomb(link, label.data());
}

// UN INSTANT EXACT DÉPASSÉ VEUT DIRE QU'IL EST REVENU : l'information est
// dépensée, exactement comme une fenêtre refermée.
//
// ⚠ UNE LOI PORTÉE PAR LE LIEN PASSE AVANT CELLE DU CATALOGUE : c'est celle que
// l'expéditeur a vue. Le catalogue ne prête la sienne qu'à un lien qui n'en a
// pas.
bool MvpLinkPassed(const MvpLink& link, std::int64_t server_now_s,
                   const ro::sim::MvpTrackerBook* catalogue) {
  if (link.exact_respawn_s > 0) {
    return link.exact_respawn_s < server_now_s;
  }
  if (link.kill_time_s > 0 && link.delay1_min > 0) {
    const std::int64_t to_s =
        link.kill_time_s +
        (static_cast<std::int64_t>(link.delay1_min) + link.delay2_min) * kSecondsPerMinute;
    return to_s < server_now_s;
  }
  if (link.kill_time_s > 0) {
    if (const std::optional<std::int64_t> law_s = CatalogueLawSeconds(link, catalogue)) {
      return link.kill_time_s + *law_s < server_now_s;
    }
    return link.kill_time_s + kMvpLawlessLinkLifetimeS < server_now_s;
  }
  return false;
}

std::string MvpSenderOf(ro::sim::ChatType type, std::string_view text) {
  // SEULS CES TYPES PORTENT UN LOCUTEUR EN TÊTE : une réponse du serveur aussi
  // peut contenir « : », sans que ce qui le précède soit un nom.
  const bool spoken = type == ro::sim::ChatType::kPublicChat ||
                      type == ro::sim::ChatType::kPartyChat ||
                      type == ro::sim::ChatType::kGuildChat || type == ro::sim::ChatType::kClanChat;
  if (!spoken) {
    return {};
  }
  return HeadBefore(text, kMvpSenderMostWireBytes);
}

std::string MvpSenderOfLine(ro::sim::ChatKind kind, ro::sim::ChatType type, std::string_view text) {
  if (kind == ro::sim::ChatKind::kServer) {
    return {};
  }
  return MvpSenderOf(type, text);
}

std::string MvpRoomSenderOf(std::string_view text) {
  return HeadBefore(text, kMvpRoomSenderMostWireBytes);
}

std::string MvpWhisperSenderOf(const ro::sim::WhisperFacts& whisper, std::string_view own_name) {
  switch (whisper.side) {
    case ro::sim::WhisperSide::kReceived:
      return whisper.peer_name;
    case ro::sim::WhisperSide::kSent:
    case ro::sim::WhisperSide::kUndelivered:
      return std::string(own_name);
    case ro::sim::WhisperSide::kNone:
      break;
  }
  return {};
}

std::string MvpPastWhisperSenderOf(const ro::sim::WhisperFacts& whisper) {
  return MvpWhisperSenderOf(whisper, std::string_view());
}

}  // namespace moonclient
