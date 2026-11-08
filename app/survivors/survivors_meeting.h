#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>

#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_ground.h"
#include "ro/sim/survivors_rules.h"

#include "survivors/survivors_book.h"
#include "survivors/survivors_save.h"
#include "survivors/survivors_stage.h"

// LA RENCONTRE : un corps attend sur son arène, à une place fixe, tant que sa
// classe n'est pas débloquée, et le héros qui le rejoint la débloque. Certains
// repartent à une heure de la partie : il faut les rejoindre avant.
//
// 🔴 HORS DE LA SIMULATION : le corps, sa place et la distance vivent ici. Rien
// n'y tire au sort et rien n'entre dans une empreinte : les monstres passent à
// travers, et le rejeu, le classement et les parties en or n'en voient rien.
namespace moonclient {

// UNE RENCONTRE : la classe qui attend, l'exploit qui la débloque, son arène,
// sa place depuis le départ, et l'heure de la partie où elle repart (0 : elle
// attend toute la partie).
struct SurvivorsMeetingRule {
  std::string_view class_key;
  std::string_view feat_key;
  std::string_view arena_key;
  std::int32_t east_cells = 0;
  std::int32_t north_cells = 0;
  std::uint64_t leaves_at_run_ms = 0;
  // Les zeny de la partie que le héros doit porter pour qu'il le suive ; 0 :
  // rien n'est demandé.
  std::uint32_t zeny_wanted = 0;
};
// L'arène où le Voleur attend.
inline constexpr std::string_view kMeetingArenaKey = "midgard_plains";
// L'Archer guette dans le désert, et repart à 5:00.
inline constexpr std::string_view kArcherMeetingArenaKey = "morroc_desert";
inline constexpr std::uint64_t kArcherLeavesAtRunMs = 300000;
// Le Marchand attend sur la neige, et ne suit qu'un héros qui a de quoi payer.
inline constexpr std::string_view kMerchantMeetingArenaKey = "lutie_snowfield";
inline constexpr std::uint32_t kMerchantZenyWanted = 150;
// Le Voleur, à 150 cases vers le nord-est ; l'Archer, à une centaine vers le
// sud-ouest ; le Marchand, à une centaine vers le sud-est.
inline constexpr std::array<SurvivorsMeetingRule, 3> kShippedMeetings{{
    {.class_key = "thief",
     .feat_key = kMeetingFeat,
     .arena_key = kMeetingArenaKey,
     .east_cells = 120,
     .north_cells = 90},
    {.class_key = "archer",
     .feat_key = kArcherMeetingFeat,
     .arena_key = kArcherMeetingArenaKey,
     .east_cells = -80,
     .north_cells = -60,
     .leaves_at_run_ms = kArcherLeavesAtRunMs},
    {.class_key = "merchant",
     .feat_key = kMerchantMeetingFeat,
     .arena_key = kMerchantMeetingArenaKey,
     .east_cells = 70,
     .north_cells = -75,
     .zeny_wanted = kMerchantZenyWanted},
}};
// La rencontre de cette classe, ou rien.
[[nodiscard]] const SurvivorsMeetingRule* MeetingOfClass(std::string_view class_key) noexcept;
// Une place qui n'est pas praticable cède à la plus proche qui l'est, dans cette
// limite.
inline constexpr std::int32_t kMeetingSearchCells = 32;
// La distance où le héros le rejoint.
inline constexpr std::int32_t kMeetingReachCells = 2;
// Rejoint, il s'efface pendant cette durée, et une ligne le dit pendant celle-là.
inline constexpr std::uint64_t kMeetingFarewellMs = 1000;
inline constexpr std::uint64_t kMeetingNoticeMs = 6000;

// LA CLASSE QUI ATTEND LE HÉROS de ce départ : une partie libre, seule, sur
// l'arène d'une rencontre, et une classe qu'elle débloque et que le carnet n'a
// pas encore. Nulle sinon.
//
// ⚠ EN COOP, PERSONNE N'ATTEND : une partie d'équipe se joue à armes égales, et
// le carnet d'un seul n'y change rien.
[[nodiscard]] const ro::sim::ClassRule* ClassAwaitingMeeting(const ro::sim::SurvivorsRules& rules,
                                                             const ro::sim::ArenaRule& arena,
                                                             const SurvivorsRunBoard& board,
                                                             const SurvivorsSave& save);

// LA PLACE DU CORPS de la rencontre `meeting` sur le sol généré `ground`, pour
// un héros parti de `start` : son décalage, ou la case praticable la plus
// proche. Rien sur un sol borné, ou sans case praticable à portée.
//
// Elle interroge une copie du sol : celui de la partie garde son cache intact.
[[nodiscard]] std::optional<ro::sim::FieldPoint> MeetingPlaceOf(
    const ro::sim::SurvivorsGround& ground, ro::sim::FieldPoint start,
    const SurvivorsMeetingRule& meeting);

// CE QUE LE CORPS MONTRE à cet instant.
struct SurvivorsMeetingBody {
  ro::sim::FieldPoint at;
  // De 1 au plein, à 0 une fois parti.
  float opacity = 1.0F;
};

// LE CORPS D'UNE PARTIE : absent, il attend, puis il s'efface une fois rejoint,
// ou une fois l'heure de son départ passée.
class SurvivorsMeeting {
 public:
  void Clear() noexcept { *this = SurvivorsMeeting{}; }
  // Il attend en `at`, jusqu'à l'heure de partie `leaves_at_run_ms` (0 : toute
  // la partie), un héros qui porte `zeny_wanted` zeny de la partie.
  void Place(ro::sim::FieldPoint at, std::uint64_t leaves_at_run_ms,
             std::uint32_t zeny_wanted = 0) noexcept;
  // Le héros est à `hero`, à l'heure de partie `run_ms`, avec `hero_zeny` zeny
  // de la partie : vrai à l'image où il le rejoint, une seule fois, s'il porte
  // la somme demandée. L'heure passée, il est parti sans lui.
  [[nodiscard]] bool Follow(ro::sim::FieldPoint hero, std::uint64_t run_ms, std::uint64_t now_ms,
                            std::uint32_t hero_zeny = 0) noexcept;
  // Les zeny qui manquent encore au héros qui en porte `hero_zeny`.
  [[nodiscard]] std::uint32_t ZenyMissing(std::uint32_t hero_zeny) const noexcept {
    return hero_zeny < zeny_wanted_ ? zeny_wanted_ - hero_zeny : 0;
  }
  // Là où le corps attend ; rien dès qu'il est rejoint.
  [[nodiscard]] std::optional<ro::sim::FieldPoint> waiting_at() const noexcept;
  // Le corps à dessiner à `now_ms` ; rien une fois parti.
  [[nodiscard]] std::optional<SurvivorsMeetingBody> BodyAt(std::uint64_t now_ms) const noexcept;
  // Vrai tant que la ligne de la rencontre se dit.
  [[nodiscard]] bool TellingAt(std::uint64_t now_ms) const noexcept;

 private:
  std::optional<ro::sim::FieldPoint> at_;
  std::optional<std::uint64_t> met_ms_;
  std::optional<std::uint64_t> left_ms_;
  std::uint64_t leaves_at_run_ms_ = 0;
  std::uint32_t zeny_wanted_ = 0;
};

}  // namespace moonclient
