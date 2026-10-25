#pragma once

#include <cstdint>

#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_journal.h"
#include "ro/sim/survivors_news.h"
#include "ro/sim/survivors_run.h"

namespace ro::sim {

// Au-delà, les pas dus sont perdus : une image figée ne rattrape pas des
// secondes d'un coup.
inline constexpr std::uint32_t kMostCatchUpSteps = 12;

// UNE PARTIE JOUÉE EN TEMPS RÉEL : des millisecondes entières converties en pas
// fixes, et le journal tenu au fil des entrées.
class SurvivorsPlay {
 public:
  explicit SurvivorsPlay(SurvivorsRun run);
  // Les pas dus pour `elapsed_ms`, au plus kMostCatchUpSteps (le reste est perdu).
  // Hors kRunning : rien, et le reste est remis à zéro. Rend le nombre de pas.
  // Le cap reçu se ramène dans [0, kHeadingSteps) (kNoHeading reste tel quel) ;
  // un cap qui change s'inscrit au journal avant son pas ; un choix ouvert
  // pendant la boucle l'arrête et remet le reste à zéro.
  std::uint32_t Advance(std::uint32_t elapsed_ms, SurvivorsInput input);
  // Inscrite au pas courant, si la partie l'accepte.
  [[nodiscard]] bool Answer(SurvivorsAnswer answer);
  // Inscrit au pas courant, et le pas final devient ce pas. Sans effet, et sans
  // rien inscrire, sur une partie finie : une entrée après la fin rendrait le
  // journal irrejouable.
  void Abandon();
  // Les nouvelles accumulées depuis le dernier appel.
  [[nodiscard]] SurvivorsNews TakeNews() { return run_.TakeNews(); }
  // En lecture seule : un pas, une réponse ou un abandon qui contourneraient le
  // journal le rendraient irrejouable.
  [[nodiscard]] const SurvivorsRun& run() const noexcept { return run_; }
  [[nodiscard]] const SurvivorsJournal& journal() const noexcept { return journal_; }
  // Part du pas suivant déjà écoulée, en millièmes : l'interpolation du dessin.
  [[nodiscard]] std::uint32_t blend_per_mille() const noexcept;

 private:
  SurvivorsRun run_;
  SurvivorsJournal journal_;
  std::uint32_t pending_ms_ = 0;
  std::uint8_t last_heading_ = kNoHeading;
};

}  // namespace ro::sim
