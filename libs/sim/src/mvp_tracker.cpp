// Le carnet de chasse MVP : l'état reçu du serveur et ses règles d'échéance.

#include "ro/sim/mvp_tracker.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "ro/core/ascii.h"

namespace ro::sim {

namespace {

constexpr std::int64_t kMsPerSecond = 1000;
constexpr std::int64_t kSecondsPerMinute = 60;

// LES HEURES VIENNENT DU FIL, sans borne : une somme ou un écart qui sortirait
// de l'int64 s'arrête à sa limite au lieu de déborder. Une heure aberrante reste
// aberrante, mais l'ordre entre les instants tient.
[[nodiscard]] constexpr std::int64_t SaturatingAdd(std::int64_t left, std::int64_t right) noexcept {
  constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();
  constexpr std::int64_t kMin = std::numeric_limits<std::int64_t>::min();
  if (right > 0 && left > kMax - right) {
    return kMax;
  }
  if (right < 0 && left < kMin - right) {
    return kMin;
  }
  return left + right;
}

[[nodiscard]] constexpr std::int64_t SaturatingSub(std::int64_t left, std::int64_t right) noexcept {
  constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();
  constexpr std::int64_t kMin = std::numeric_limits<std::int64_t>::min();
  if (right < 0 && left > kMax + right) {
    return kMax;
  }
  if (right > 0 && left < kMin + right) {
    return kMin;
  }
  return left - right;
}

}  // namespace

void MvpTrackerBook::NoteServerTime(std::int64_t server_time_s, std::int64_t local_now_s) noexcept {
  // Une heure nulle ou négative n'est pas une heure : l'écart connu reste.
  if (server_time_s <= 0) {
    return;
  }
  clock_offset_s_ = SaturatingSub(server_time_s, local_now_s);
  clock_known_ = true;
}

void MvpTrackerBook::ReceiveCatalogue(std::vector<MvpSlot> slots) {
  slots_ = std::move(slots);
  catalog_known_ = true;
  ++catalog_serial_;
}

void MvpTrackerBook::ReceiveObservations(
    std::vector<std::pair<std::uint16_t, MvpObservation>> entries, bool replace_all) {
  if (replace_all) {
    observations_.clear();
  }
  for (auto& [slot_id, observation] : entries) {
    observations_[slot_id] = std::move(observation);
  }
}

void MvpTrackerBook::ReceiveFavorites(std::vector<std::uint16_t> slot_ids) {
  favorites_ = std::move(slot_ids);
}

void MvpTrackerBook::ReceiveGroup(MvpGroup group, bool members_complete) {
  group_ = std::move(group);
  extensions_seen_ = extensions_seen_ || group_.self_user_id.has_value();
  // Sortir d'un groupe rend caduc tout ce qu'il savait. Une réponse tronquée,
  // elle, ne prouve rien : ce qu'on savait reste.
  if (members_complete && group_.group_id == 0) {
    observations_.clear();
  }
}

void MvpTrackerBook::ReceiveInvitation(std::uint32_t group_id, std::string group_name,
                                       std::optional<MvpInviteOrigin> origin) {
  invite_group_id_ = group_id;
  invite_name_ = std::move(group_name);
  // L'origine est celle de CETTE trame : une trame sans queue n'hérite pas de
  // la précédente.
  invite_origin_ = origin.value_or(MvpInviteOrigin::kUnknown);
  extensions_seen_ = extensions_seen_ || origin.has_value();
}

void MvpTrackerBook::ReceiveResult(std::uint8_t result_code, std::uint32_t now_ms,
                                   std::optional<MvpResultTag> tag) noexcept {
  last_result_ = result_code;
  last_result_ms_ = now_ms;
  // L'étiquette ne prouve pas la forme étendue : le serveur la rend à toute
  // commande qui portait un identifiant, que la session ait annoncé la
  // capacité ou non.
  last_result_tag_ = tag;
  ++result_serial_;
}

void MvpTrackerBook::ClearInvitation() noexcept {
  invite_group_id_ = 0;
  invite_name_.clear();
  invite_origin_ = MvpInviteOrigin::kUnknown;
}

void MvpTrackerBook::Clear() noexcept {
  slots_.clear();
  observations_.clear();
  favorites_.clear();
  group_ = MvpGroup();
  catalog_known_ = false;
  // ⚠ LE NUMÉRO DE CATALOGUE AVANCE, il ne revient jamais en arrière : qui le
  // compare au dernier vu doit voir que le catalogue a changé — il s'est vidé.
  ++catalog_serial_;
  clock_known_ = false;
  clock_offset_s_ = 0;
  // ⚠ UNE NOUVELLE SESSION SE PROUVE À NOUVEAU : le serveur peut avoir changé.
  extensions_seen_ = false;
  ClearInvitation();
}

std::int64_t MvpTrackerBook::ServerNow(std::int64_t local_now_s) const noexcept {
  return SaturatingAdd(local_now_s, clock_offset_s_);
}

const MvpSlot* MvpTrackerBook::FindSlot(std::uint16_t slot_id) const noexcept {
  const auto found = std::ranges::find(slots_, slot_id, &MvpSlot::slot_id);
  return found != slots_.end() ? &*found : nullptr;
}

const MvpSlot* MvpTrackerBook::FindSlotFor(std::uint16_t mob_id,
                                           std::string_view map) const noexcept {
  if (map.empty()) {
    return nullptr;
  }
  const MvpSlot* scripted = nullptr;
  for (const MvpSlot& slot : slots_) {
    // UNE CARTE SE COMPARE SANS LA CASSE, en ASCII seulement : un lien collé à
    // la main ou une carte relue ailleurs peut porter des majuscules que le
    // catalogue n'a pas.
    if (!core::AsciiEqualsFolded(slot.map, map)) {
      continue;
    }
    // Le créneau du monstre lui-même passe avant le créneau scripté de la
    // carte, quel que soit leur rang au catalogue.
    if (slot.mob_id == mob_id) {
      return &slot;
    }
    if (slot.mob_id == 0 && scripted == nullptr) {
      scripted = &slot;
    }
  }
  return scripted;
}

const MvpObservation* MvpTrackerBook::FindObservation(std::uint16_t slot_id) const noexcept {
  const auto found = observations_.find(slot_id);
  return found != observations_.end() ? &found->second : nullptr;
}

bool MvpTrackerBook::IsFavorite(std::uint16_t slot_id) const noexcept {
  return std::ranges::find(favorites_, slot_id) != favorites_.end();
}

std::optional<MvpWindow> MvpTrackerBook::WindowOf(std::uint16_t slot_id) const noexcept {
  const MvpObservation* observation = FindObservation(slot_id);
  if (observation == nullptr) {
    return std::nullopt;
  }
  // Mérité : un instant, pas une plage.
  if (observation->exact_respawn_s != 0) {
    return MvpWindow{.from_s = observation->exact_respawn_s,
                     .to_s = observation->exact_respawn_s,
                     .exact = true};
  }
  const MvpSlot* slot = FindSlot(slot_id);
  if (slot == nullptr || observation->kill_time_s == 0) {
    return std::nullopt;
  }
  // LA LOI, jamais le tirage : des millisecondes ramenées en secondes ENTIÈRES.
  const std::int64_t from_s = SaturatingAdd(
      observation->kill_time_s, static_cast<std::int64_t>(slot->delay1_ms) / kMsPerSecond);
  return MvpWindow{
      .from_s = from_s,
      .to_s = SaturatingAdd(from_s, static_cast<std::int64_t>(slot->delay2_ms) / kMsPerSecond),
      .exact = false};
}

std::vector<MvpTomb> MvpTombsOn(const MvpTrackerBook& book, std::string_view map,
                                std::int64_t server_now_s) {
  std::vector<MvpTomb> tombs;
  for (const auto& [slot_id, observation] : book.observations()) {
    // -1 : position inconnue. La cellule (0, 0), elle, est une vraie place.
    if (observation.tomb_x < 0 || observation.tomb_y < 0) {
      continue;
    }
    const MvpSlot* slot = book.FindSlot(slot_id);
    if (slot == nullptr || !core::AsciiEqualsFolded(slot->map, map)) {
      continue;
    }
    const std::optional<MvpWindow> window = book.WindowOf(slot_id);
    // Une fenêtre entièrement passée ne dit plus rien de la tombe.
    if (!window.has_value() || SaturatingAdd(window->to_s, kMvpTombLifetimeS) < server_now_s) {
      continue;
    }
    tombs.push_back(MvpTomb{.cell_x = observation.tomb_x,
                            .cell_y = observation.tomb_y,
                            .exact = window->exact,
                            .slot_id = slot_id});
  }
  return tombs;
}

bool MvpBadgeLit(const MvpTrackerBook& book, std::int64_t server_now_s, int lead_min) {
  const std::int64_t lead_s = static_cast<std::int64_t>(lead_min) * kSecondsPerMinute;
  for (const std::uint16_t slot_id : book.favorites()) {
    const std::optional<MvpWindow> window = book.WindowOf(slot_id);
    if (!window.has_value()) {
      continue;
    }
    // Bornée en haut : passé sa fenêtre, le MVP est déjà repris ou déjà tué par
    // d'autres, et un créneau oublié cesse d'allumer l'icône.
    const std::int64_t until_s = window->exact ? SaturatingAdd(window->from_s, kMvpBadgeExactFreshS)
                                               : SaturatingAdd(window->to_s, kMvpBadgeWindowFreshS);
    if (SaturatingAdd(server_now_s, lead_s) >= window->from_s && server_now_s <= until_s) {
      return true;
    }
  }
  return false;
}

}  // namespace ro::sim
