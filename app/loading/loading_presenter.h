#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "ro/core/load_progress.h"

namespace moonclient {

class ClientShell;

// L'ÉCRAN DE CHARGEMENT PENDANT UN CHARGEMENT BLOQUANT : il vit le temps du
// chargement, reçoit son avancement et le présente, à son heure.
//
// 🔴 AUCUNE RÉENTRANCE. Une présentation vide la file du système, peint
// l'écran et l'envoie — rien d'autre. Elle n'appelle ni l'image du jeu, ni la
// liaison réseau, ni le moindre événement : ce qui se construit est à moitié
// construit, et rien ne doit le regarder. Les événements restent dans la file
// pour l'image qui suit le chargement ; une demande de fermeture s'y note au
// journal et attend son tour.
//
// ⚠ RIEN NE PARAÎT AVANT LE SEUIL : un chargement court ne coûte qu'un relevé
// d'horloge par étape. Voir `ro::core::LoadPacing`.
//
// ⚠ PAS DE FIL DE FOND : la carte graphique n'a qu'un contexte, et tout ce qui
// s'y téléverse passe par le fil principal. L'écran se peint donc ENTRE deux
// morceaux du chargement, aux coutures que ses étapes déclarent.
class LoadingPresenter final : public ro::core::LoadObserver {
 public:
  // `what` nomme le chargement dans le journal. `already_shown` : l'écran est
  // déjà à l'écran, et le reste sans seuil ni fondu.
  LoadingPresenter(ClientShell& shell, std::span<const ro::core::LoadStepWeight> plan,
                   std::string_view what, bool already_shown = false);
  ~LoadingPresenter() override;

  // L'avancement à passer aux chargeurs.
  [[nodiscard]] ro::core::LoadProgress* progress() noexcept { return &progress_; }

  void Changed(const ro::core::LoadProgress& progress) override;

 private:
  void Present(std::uint32_t now_ms);

  ClientShell& shell_;
  ro::core::LoadProgress progress_;
  ro::core::LoadPacing pacing_;
  std::string what_;
  std::uint32_t started_ms_ = 0;
  // Ce que l'écran a coûté : ses images et leur temps cumulé.
  std::uint32_t presented_frames_ = 0;
  std::uint64_t presenting_ms_ = 0;
  bool quit_noted_ = false;
};

}  // namespace moonclient
