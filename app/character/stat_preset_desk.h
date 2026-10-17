#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "ro/net/connection.h"
#include "ro/sim/stat_presets.h"
#include "ro/sim/stat_raise_run.h"
#include "ro/sim/world.h"
#include "ro/ui/stat_preset_window.h"

namespace moonclient {

// LES PRÉRÉGLAGES DE STATS, VUS DE LA PARTIE : le carnet du compte et son
// fichier, l'aperçu, et la file qui envoie les montées.
//
// ⚠ L'APERÇU SE REFAIT À CHAQUE IMAGE depuis le monde : il montre toujours ce
// que « Confirmer » lancerait, et une montée faite à la main pendant qu'il est
// ouvert s'y voit aussitôt.
class StatPresetDesk {
 public:
  StatPresetDesk(std::filesystem::path client_root, std::uint32_t account_id);

  // Fait avancer l'application en cours : une montée part quand la précédente
  // a reçu sa réponse. Un arrêt se dit dans la conversation et dans la fenêtre.
  void Tick(ro::net::Connection& link, ro::sim::World& world, std::uint32_t now_ms);

  // Ce que la fenêtre a demandé.
  void Apply(const ro::ui::StatPresetAction& asked, const ro::sim::World& world,
             std::uint32_t now_ms);

  // Compose la vue de la fenêtre. ⚠ ELLE POINTE DANS CET OBJET, et vaut
  // jusqu'au prochain appel.
  void Compose(const ro::sim::World& world, ro::ui::StatPresetWindowView& into);

  [[nodiscard]] const ro::sim::StatPresetBook& book() const noexcept { return book_; }

 private:
  void Keep();
  void Say(ro::sim::World& world, std::uint32_t now_ms);

  std::filesystem::path client_root_;
  std::uint32_t account_id_ = 0;
  ro::sim::StatPresetBook book_;
  ro::sim::StatRaiseRun run_;
  bool was_running_ = false;
  // Le préréglage dont l'aperçu est ouvert, par son nom.
  std::optional<std::string> previewed_;
  std::string applied_name_;
  std::string report_;
  std::vector<ro::ui::StatPresetRowView> rows_;
  std::string preview_name_;
};

}  // namespace moonclient
