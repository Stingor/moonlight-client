#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>

namespace ro::sim {

// Ce que la navigation doit viser.
enum class NavigationTarget : std::uint8_t {
  // Une cellule précise de la carte.
  kCell = 0,
  // La carte seule, sans cellule.
  kMap = 1,
  // Un PNJ, désigné par ses coordonnées.
  kNpc = 2,
  // Un monstre, désigné par son identifiant.
  kMonster = 3,
};

// Les services de transport que le trajet peut emprunter.
struct NavigationServices {
  // Les services de transport des villes.
  bool transport = false;
  // L'avion.
  bool airship = false;
  // Les parchemins.
  bool scrolls = false;
};

// Une demande de navigation qu'un script adresse au joueur (`navigateto`).
struct NavigationRequest {
  NavigationTarget target = NavigationTarget::kCell;
  NavigationServices services;
  // Vrai : ne pas ouvrir la fenêtre de navigation, seulement lancer le trajet.
  bool hide_window = false;
  // Le nom interne de la carte visée, sans extension.
  std::string map;
  // La cellule visée ; nulle quand la cible n'en porte pas.
  std::uint16_t cell_x = 0;
  std::uint16_t cell_y = 0;
  // Le monstre visé ; nul hors de `NavigationTarget::kMonster`.
  std::uint16_t monster_id = 0;
};

// Combien de demandes attendent au plus : une demande de plus chasse la plus
// ancienne, qu'une nouvelle commande remplacerait de toute façon.
inline constexpr std::size_t kMaxPendingNavigationRequests = 16;

// Les demandes de navigation reçues et pas encore prises en charge.
//
// Le réseau les pose, et l'application les prend : `Take` rend la plus
// ancienne, puis la suivante, jusqu'à ce que la file soit vide.
class NavigationRequests {
 public:
  void Push(NavigationRequest request);

  // La plus ancienne demande, retirée de la file ; rien si la file est vide.
  [[nodiscard]] std::optional<NavigationRequest> Take();

  [[nodiscard]] std::size_t pending() const noexcept { return queue_.size(); }
  [[nodiscard]] bool empty() const noexcept { return queue_.empty(); }
  void Clear() noexcept { queue_.clear(); }

 private:
  std::deque<NavigationRequest> queue_;
};

}  // namespace ro::sim
