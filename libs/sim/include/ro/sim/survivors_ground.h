#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "ro/sim/survivors_terrain.h"
#include "ro/sim/walk_map.h"

namespace ro::sim {

// LE SOL D'UNE PARTIE : ce que la simulation demande quand elle veut savoir si
// une case se franchit. Il est de deux sortes.
//
//   · BORNÉ : une carte finie (un GAT, les piliers des essais). Il répond
//     exactement ce que répond son `WalkMap`, hors de la carte compris.
//   · GÉNÉRÉ : le monde sans bord d'une règle de terrain, de
//     kTerrainWorldCells de côté. Un morceau se génère à la première question
//     qui le touche et se garde dans un cache à correspondance directe ; une
//     éviction le régénère à l'identique. AUCUNE RÉPONSE NE DÉPEND DU CACHE :
//     sa taille, son histoire et l'ordre des questions ne changent rien, ce
//     que les essais prouvent en jouant la même partie avec un cache d'un seul
//     morceau.
//
// Le cache vit derrière des méthodes `const` : il ne fait pas partie de l'état
// de la partie, seulement de son coût. Un sol généré garde l'adresse de sa
// règle, qui doit lui survivre (elle vit dans les règles de la partie).
// Un sol ne se partage pas entre deux fils.

// Le côté du cache d'un sol généré, en morceaux : 16 × 16 morceaux couvrent
// 512 cases, bien au-delà de ce que la partie interroge autour du héros.
inline constexpr std::int32_t kGroundCacheSideChunks = 16;
// Le plus grand côté de cache permis : 64 × 64 morceaux d'environ 1 Kio, soit
// environ 4 Mio. Au-delà, le cache coûterait sans rien servir.
inline constexpr std::int32_t kMostGroundCacheSideChunks = 64;

class SurvivorsGround {
 public:
  // Un sol borné vide : rien ne s'y franchit.
  SurvivorsGround() = default;

  [[nodiscard]] static SurvivorsGround Bounded(WalkMap map);
  // `cache_side_chunks` est ramené dans [1, kMostGroundCacheSideChunks].
  [[nodiscard]] static SurvivorsGround Generated(
      const TerrainRule& rule, std::uint64_t run_seed,
      std::int32_t cache_side_chunks = kGroundCacheSideChunks);
  // Le sol garde l'adresse de sa règle : une règle temporaire le laisserait
  // pointer dans le vide.
  static SurvivorsGround Generated(const TerrainRule&& rule, std::uint64_t run_seed,
                                   std::int32_t cache_side_chunks = kGroundCacheSideChunks) =
      delete;

  // Faux hors du sol. La voie du sol borné reste en ligne : c'est la question
  // que la partie pose le plus souvent.
  [[nodiscard]] bool Passable(std::int32_t x, std::int32_t y) const {
    if (bounded_) return map_.Passable(x, y);
    return GeneratedPassable(x, y);
  }
  // En cases : celles de la carte, ou kTerrainWorldCells pour un sol généré.
  [[nodiscard]] std::int32_t width() const noexcept;
  [[nodiscard]] std::int32_t height() const noexcept;
  [[nodiscard]] bool bounded() const noexcept { return bounded_; }
  // La carte d'un sol borné ; vide pour un sol généré.
  [[nodiscard]] const WalkMap& map() const noexcept { return map_; }
  // La règle et la graine d'un sol généré : ce qu'un rejeu redérive des
  // règles et du départ. nullptr et 0 pour un sol borné.
  [[nodiscard]] const TerrainRule* rule() const noexcept { return rule_; }
  [[nodiscard]] std::uint64_t run_seed() const noexcept { return run_seed_; }
  // Le côté du cache d'un sol généré, une fois borné ; 0 pour un sol borné.
  [[nodiscard]] std::int32_t cache_side_chunks() const noexcept { return cache_side_chunks_; }
  // Le morceau (chunk_x, chunk_y) d'un sol généré, tel que
  // GenerateTerrainChunk le rend ; nullptr pour un sol borné ou hors du monde.
  // Le pointeur ne vaut que jusqu'à la prochaine question posée au sol.
  [[nodiscard]] const TerrainChunk* ChunkAt(std::int32_t chunk_x, std::int32_t chunk_y) const;
  // Le nombre de morceaux générés depuis la création du sol, régénérations
  // comprises : ce que le banc mesure.
  [[nodiscard]] std::uint64_t chunks_generated() const noexcept { return chunks_generated_; }

 private:
  struct CachedChunk {
    bool filled = false;
    TerrainChunk chunk;
  };

  [[nodiscard]] bool GeneratedPassable(std::int32_t x, std::int32_t y) const;
  // Le morceau en cache, généré au besoin ; les coordonnées sont dans le monde.
  [[nodiscard]] const TerrainChunk& Cached(std::int32_t chunk_x, std::int32_t chunk_y) const;

  bool bounded_ = true;
  WalkMap map_;
  const TerrainRule* rule_ = nullptr;
  std::uint64_t run_seed_ = 0;
  std::int32_t cache_side_chunks_ = 0;
  mutable std::vector<CachedChunk> cache_;
  mutable std::uint64_t chunks_generated_ = 0;
};

}  // namespace ro::sim
