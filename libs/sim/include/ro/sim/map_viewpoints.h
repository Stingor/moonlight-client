#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace ro::sim {

// Combien de temps un repère éphémère reste posé, en millisecondes.
inline constexpr std::uint32_t kEphemeralViewpointLifetimeMs = 15'000;

// Les huit bits de poids fort d'une couleur ARGB : l'opacité pleine.
inline constexpr std::uint32_t kViewpointOpaqueAlpha = 0xFF000000U;
// Les trois octets de couleur que le serveur envoie.
inline constexpr std::uint32_t kViewpointRgbMask = 0x00FFFFFFU;

// Un repère qu'un script pose sur la carte (la commande `viewpoint`).
struct Viewpoint {
  // Le PNJ dont le script a posé le repère, et le numéro de point qu'il lui a
  // donné : le couple est la clé. Deux PNJ peuvent employer le même numéro sans
  // se marcher dessus.
  std::uint32_t npc_id = 0;
  std::int32_t point_id = 0;
  // La cellule visée.
  std::int16_t cell_x = 0;
  std::int16_t cell_y = 0;
  // 0xAARRGGBB, TOUJOURS OPAQUE : le serveur n'envoie que les trois octets
  // 0x00RRGGBB, et un alpha nul ferait disparaître le repère.
  std::uint32_t color_argb = kViewpointOpaqueAlpha;
  // Vrai : le repère dure jusqu'à ce qu'un script le retire ou que la carte
  // change. Faux : il expire `kEphemeralViewpointLifetimeMs` après sa pose.
  bool permanent = false;
  // L'instant de la pose, sur l'horloge de l'appelant : l'interface s'en sert
  // pour faire clignoter le repère.
  std::uint32_t placed_ms = 0;
};

// La couleur opaque qui correspond aux trois octets 0x00RRGGBB du serveur.
[[nodiscard]] constexpr std::uint32_t OpaqueViewpointColor(std::uint32_t rgb) noexcept {
  return kViewpointOpaqueAlpha | (rgb & kViewpointRgbMask);
}

// Les repères de la carte courante, par ordre de pose.
//
// Cet état n'appartient à aucune fenêtre : le réseau le remplit, l'interface le
// lit. Il se vide à chaque changement de carte.
class MapViewpoints {
 public:
  // Pose un repère ; un repère de même clé est REMPLACÉ, à sa place dans
  // l'ordre de pose.
  void Place(const Viewpoint& viewpoint);

  // Retire le repère de cette clé. Faux s'il n'y en avait pas.
  bool Remove(std::uint32_t npc_id, std::int32_t point_id);

  // Retire tout : à appeler quand la carte change.
  void Clear() noexcept { viewpoints_.clear(); }

  // Retire les repères éphémères posés depuis `kEphemeralViewpointLifetimeMs`
  // ou plus. Rend le nombre de repères retirés.
  std::size_t PurgeExpired(std::uint32_t now_ms);

  [[nodiscard]] std::span<const Viewpoint> viewpoints() const noexcept { return viewpoints_; }
  [[nodiscard]] const Viewpoint* Find(std::uint32_t npc_id, std::int32_t point_id) const noexcept;
  [[nodiscard]] bool empty() const noexcept { return viewpoints_.empty(); }

 private:
  std::vector<Viewpoint> viewpoints_;
};

}  // namespace ro::sim
