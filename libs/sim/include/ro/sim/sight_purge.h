#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "ro/sim/world_fwd.h"

namespace ro::sim {

// ── LES RESTES DU MONDE ─────────────────────────────────────────────────────
//
// Deux sortes d'acteurs que rien n'a retirés : ceux restés hors de la vue du
// serveur, et les monstres restés debout à zéro PV.
//
// HORS DE LA VUE.
//
// Le serveur ne fait connaître que les acteurs du carré de `view_cells` cases
// autour du personnage, et dit la sortie de chacun. Un acteur qui reste
// DURABLEMENT au-delà ne peut donc être qu'un reste que rien n'a retiré : il se
// dessinait à l'écran, et l'IA d'un compagnon le comptait parmi les monstres.
//
// ⚠ UNE MARGE ET UN DÉLAI, sans quoi la purge mordrait sur du vivant : un
// acteur qui marche peut déborder le carré le temps que sa sortie arrive.
// ⚠ ET RIEN TANT QUE LE SERVEUR N'A PAS DIT SON RAYON (`view_cells` nul) :
// supposer celui d'un serveur d'usine purgerait ce qu'un serveur plus large
// montre légitimement.

// Les cases au-delà du rayon de vue avant qu'un acteur soit suspect.
inline constexpr int kSightPurgeMarginCells = 2;
// Le temps qu'il doit rester au-delà, d'affilée, pour être purgé.
inline constexpr std::uint32_t kSightPurgeGraceMs = 3000;

// UN MONSTRE À ZÉRO PV RESTÉ DEBOUT : pour le serveur, il est mort, et sa mort
// suit le dernier coup de quelques millisecondes. Au-delà de ce délai, elle
// n'a pas été reçue, et le monstre est un reste. ⚠ ZÉRO EXACT : une jauge à
// « 0 % » peut cacher un ou deux PV bien vivants.
inline constexpr std::uint32_t kZeroHealthGraceMs = 2000;

// Pourquoi un acteur a été purgé.
enum class PurgeReason : std::uint8_t { kBeyondSight, kZeroHealth };

// Un acteur purgé, et où il était par rapport au personnage.
struct PurgedActor {
  std::uint32_t actor_id = 0;
  std::uint16_t job = 0;
  int dx_cells = 0;
  int dy_cells = 0;
  PurgeReason reason = PurgeReason::kBeyondSight;
};

class SightPurge {
 public:
  // Retire du monde les acteurs restés au-delà de `view_cells` + la marge
  // depuis `kSightPurgeGraceMs`, et les monstres restés debout à zéro PV
  // depuis `kZeroHealthGraceMs` ; les rend pour le journal. Le personnage
  // lui-même n'est jamais touché.
  std::vector<PurgedActor> Sweep(World& world, int view_cells, std::uint32_t now_ms);

 private:
  // Depuis quand chaque acteur suspect est au-delà.
  std::unordered_map<std::uint32_t, std::uint32_t> beyond_since_ms_;
  // Depuis quand chaque monstre à zéro PV est resté debout.
  std::unordered_map<std::uint32_t, std::uint32_t> zero_since_ms_;
};

}  // namespace ro::sim
