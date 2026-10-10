#pragma once

#include <cstdint>
#include <map>

namespace ro::sim {

// L'effet qui RETIRE la marque d'un PNJ (`QTYPE_NONE` du serveur).
inline constexpr std::uint16_t kQuestMarkerRemovedEffect = 9999;
// La couleur d'une marque que la minimap ne montre pas.
inline constexpr std::uint16_t kQuestMarkerNoColor = 0;
// La case d'une marque retirée : hors de toute carte.
inline constexpr std::int16_t kQuestMarkerNoCell = -1;

// LA MARQUE DE QUÊTE D'UN PNJ : sa case, l'effet qui choisit l'image au-dessus
// de lui, et la couleur qui choisit son icône sur la minimap.
struct QuestMarker {
  std::int16_t cell_x = kQuestMarkerNoCell;
  std::int16_t cell_y = kQuestMarkerNoCell;
  std::uint16_t effect = kQuestMarkerRemovedEffect;
  std::uint16_t color = kQuestMarkerNoColor;

  // La marque se montre au-dessus du PNJ.
  [[nodiscard]] bool shown() const noexcept { return effect != kQuestMarkerRemovedEffect; }
  // Sa case est sur la carte : la minimap peut la situer.
  [[nodiscard]] bool placed() const noexcept { return cell_x >= 0 && cell_y >= 0; }

  [[nodiscard]] friend bool operator==(const QuestMarker&, const QuestMarker&) = default;
};

// LES MARQUES DE QUÊTE DE LA CARTE, par identifiant de PNJ.
//
// ⚠ À PART DES ACTEURS : le serveur les envoie TOUTES à l'entrée sur la carte,
// que le PNJ soit en vue ou non, puis seulement quand une condition change. Une
// marque reçue avant l'apparition du PNJ se garde, et un PNJ qui sort de vue
// puis revient retrouve la sienne, que rien ne redira.
//
// Une seconde annonce pour le même PNJ REMPLACE la première. Le retrait
// (`kQuestMarkerRemovedEffect`) ne supprime pas l'entrée : il la marque
// retirée, sans case. Seule l'entrée sur une carte vide le tout.
class QuestMarkers {
 public:
  // Note ce que le serveur annonce pour ce PNJ.
  void Tell(std::uint32_t npc_id, QuestMarker marker);

  // La marque à montrer au-dessus de ce PNJ, ou nul s'il n'en a pas ou qu'elle
  // est retirée.
  [[nodiscard]] const QuestMarker* ShownFor(std::uint32_t npc_id) const;

  // Toutes les entrées, retirées comprises.
  [[nodiscard]] const std::map<std::uint32_t, QuestMarker>& all() const noexcept {
    return markers_;
  }

  void Clear() noexcept { markers_.clear(); }

 private:
  std::map<std::uint32_t, QuestMarker> markers_;
};

}  // namespace ro::sim
