#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>

#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_ground.h"
#include "ro/sim/survivors_rules.h"

#include "survivors/survivors_save.h"
#include "survivors/survivors_stage.h"

// LA RENCONTRE : un corps attend sur la prairie, à une place fixe, tant que sa
// classe n'est pas débloquée, et le héros qui le rejoint la débloque.
//
// 🔴 HORS DE LA SIMULATION : le corps, sa place et la distance vivent ici. Rien
// n'y tire au sort et rien n'entre dans une empreinte : les monstres passent à
// travers, et le rejeu, le classement et les parties en or n'en voient rien.
namespace moonclient {

// La prairie, seule arène où il attend.
inline constexpr std::string_view kMeetingArenaKey = "midgard_plains";
// SA PLACE, depuis le départ : 150 cases, vers le nord-est.
inline constexpr std::int32_t kMeetingEastCells = 120;
inline constexpr std::int32_t kMeetingNorthCells = 90;
// Une place qui n'est pas praticable cède à la plus proche qui l'est, dans cette
// limite.
inline constexpr std::int32_t kMeetingSearchCells = 32;
// La distance où le héros le rejoint.
inline constexpr std::int32_t kMeetingReachCells = 2;
// Rejoint, il s'efface pendant cette durée, et une ligne le dit pendant celle-là.
inline constexpr std::uint64_t kMeetingFarewellMs = 1000;
inline constexpr std::uint64_t kMeetingNoticeMs = 6000;

// LA CLASSE QUI ATTEND LE HÉROS de ce départ : une partie libre, seule, sur la
// prairie, et une classe que la rencontre débloque et que le carnet n'a pas
// encore. Nulle sinon.
//
// ⚠ EN COOP, PERSONNE N'ATTEND : une partie d'équipe se joue à armes égales, et
// le carnet d'un seul n'y change rien.
[[nodiscard]] const ro::sim::ClassRule* ClassAwaitingMeeting(const ro::sim::SurvivorsRules& rules,
                                                             const ro::sim::ArenaRule& arena,
                                                             const SurvivorsRunBoard& board,
                                                             const SurvivorsSave& save);

// LA PLACE DU CORPS sur le sol généré `ground`, pour un héros parti de `start` :
// le décalage fixe, ou la case praticable la plus proche. Rien sur un sol borné,
// ou sans case praticable à portée.
//
// Elle interroge une copie du sol : celui de la partie garde son cache intact.
[[nodiscard]] std::optional<ro::sim::FieldPoint> MeetingPlaceOf(
    const ro::sim::SurvivorsGround& ground, ro::sim::FieldPoint start);

// CE QUE LE CORPS MONTRE à cet instant.
struct SurvivorsMeetingBody {
  ro::sim::FieldPoint at;
  // De 1 au plein, à 0 une fois parti.
  float opacity = 1.0F;
};

// LE CORPS D'UNE PARTIE : absent, il attend, puis il s'efface une fois rejoint.
class SurvivorsMeeting {
 public:
  void Clear() noexcept { *this = SurvivorsMeeting{}; }
  void Place(ro::sim::FieldPoint at) noexcept;
  // Le héros est à `hero` : vrai à l'image où il le rejoint, une seule fois.
  [[nodiscard]] bool Follow(ro::sim::FieldPoint hero, std::uint64_t now_ms) noexcept;
  // Là où le corps attend ; rien dès qu'il est rejoint.
  [[nodiscard]] std::optional<ro::sim::FieldPoint> waiting_at() const noexcept;
  // Le corps à dessiner à `now_ms` ; rien une fois parti.
  [[nodiscard]] std::optional<SurvivorsMeetingBody> BodyAt(std::uint64_t now_ms) const noexcept;
  // Vrai tant que la ligne de la rencontre se dit.
  [[nodiscard]] bool TellingAt(std::uint64_t now_ms) const noexcept;

 private:
  std::optional<ro::sim::FieldPoint> at_;
  std::optional<std::uint64_t> met_ms_;
};

}  // namespace moonclient
