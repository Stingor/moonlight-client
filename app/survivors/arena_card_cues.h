#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "ro/render/item_drop.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_news.h"
#include "ro/sim/survivors_state.h"

#include "survivors/arena_effect_plan.h"

// CE QU'UNE CARTE TOMBÉE FAIT VOIR ET ENTENDRE DANS L'ARÈNE (Q6), décidé sans
// GPU ni son : une lueur tant qu'elle est au sol, un son à sa chute, un éclat
// et un son quand le héros la prend, puis son illustration au HUD.
namespace moonclient {

// LA LUEUR D'UNE CARTE AU SOL : la colonne violette d'un butin rare, sa
// flaque et son son de chute.
inline constexpr std::uint16_t kCardGlowPillar = ro::render::kItemDropPillarPurple;

// LE SON D'UNE CARTE RAMASSÉE : celui de la récompense des mini-jeux.
inline constexpr std::string_view kCardPickupSound = "_heal_effect.wav";

// LES LUEURS ALLUMÉES au plus, les plus anciennes cartes d'abord : une lueur
// dure tant que sa carte reste, et compte parmi les effets vivants.
inline constexpr std::size_t kMostArenaCardGlows = 16;

// Une carte au sol à faire luire : son numéro, qui nomme aussi l'acteur du
// monde privé qui porte sa lueur, et sa place.
struct ArenaCardGlow {
  std::uint32_t serial = 0;
  ro::sim::FieldPoint at;
};

// LES LUEURS DE L'IMAGE, une par carte au sol dans l'ordre de chute, au plus
// `kMostArenaCardGlows`, rangées dans `out` (vidé). Aucune sans effets.
//
// ⚠ « EFFETS RÉDUITS » LES GARDE : une lueur n'est pas l'éclat d'un coup mais
// le seul signe qu'une carte attend, et une carte non ramassée est perdue.
void PlanArenaCardGlows(std::span<const ro::sim::SurvivorsPickup> pickups,
                        ArenaEffectSwitches switches, std::vector<ArenaCardGlow>& out);

// LES SONS DES CARTES d'une image : un son de chute si une carte au moins est
// tombée, un son de ramassage si une au moins a été prise. Vides sinon.
struct ArenaCardSounds {
  std::string_view drop;
  std::string_view pickup;
};
[[nodiscard]] ArenaCardSounds CardSoundsOf(const ro::sim::SurvivorsNews& news) noexcept;

// LA VIGNETTE D'UNE CARTE RAMASSÉE (Q6) : son illustration au HUD pendant
// trois secondes de l'horloge de présentation, qui se fige avec la partie.
inline constexpr std::uint64_t kCardVignetteMs = 3000;
// Les cartes en attente au plus : au-delà, la nouvelle se perd, la carte
// entre quand même à l'Album.
inline constexpr std::size_t kMostCardVignettes = 4;

struct CardVignetteShown {
  std::uint16_t card_item_id = 0;
  // De 0 à 1 le long de ses trois secondes.
  float fraction = 0.0F;
};

// LA FILE DES VIGNETTES : chaque carte ramassée a ses trois secondes à son
// tour, dans l'ordre de ramassage. Une horloge qui recule est celle d'une
// partie neuve : la file se vide.
class CardVignettes {
 public:
  // Les cartes ramassées des nouvelles que l'appelant vient de prendre, une
  // fois chacune.
  void Notice(std::span<const ro::sim::SurvivorsCardNews> found, std::uint64_t now_ms);
  // La vignette à montrer à `now_ms` ; les vignettes finies s'en vont.
  [[nodiscard]] std::optional<CardVignetteShown> ShownAt(std::uint64_t now_ms);
  void Clear() noexcept;
  [[nodiscard]] std::size_t waiting() const noexcept { return waiting_.size(); }

 private:
  // Vide la file si l'horloge a reculé depuis le dernier appel.
  void Follow(std::uint64_t now_ms) noexcept;

  std::vector<std::uint16_t> waiting_;
  // L'instant où la première vignette de la file a paru.
  std::uint64_t front_since_ms_ = 0;
  std::uint64_t last_now_ms_ = 0;
};

}  // namespace moonclient
