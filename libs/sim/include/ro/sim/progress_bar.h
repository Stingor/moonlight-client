#pragma once

#include <cstdint>

#include "ro/sim/world_fwd.h"

namespace ro::sim {

// ── LA BARRE DE PROGRESSION D'UN SCRIPT ─────────────────────────────────────
//
// 🔴 CE N'EST PAS UNE BARRE À PART : c'est la jauge de l'acteur
// (`Actor::gauge`), celle d'une incantation, posée pour une durée. Elle se
// dessine donc comme une incantation, garde la souris vivante sous un dialogue
// comme une incantation, et s'éteint à son échéance comme elle.
//
// La couleur que le script demande n'y paraît pas : le client ne la lit pas.

// LA RÉPONSE QUE LE SERVEUR ATTEND.
//
// 🔴 SANS ELLE, LE SCRIPT RESTE ARRÊTÉ : `progressbar` suspend le script du
// joueur jusqu'au CZ_PROGRESS, et, tant qu'il attend, chaque pas demandé est
// pris pour une annulation et refusé. Le joueur serait cloué sur place.
//
// La dette naît avec la barre du joueur, et se solde à la PREMIÈRE extinction
// de sa jauge — échéance ou annulation par le serveur : une seule réponse par
// barre. Elle survit au changement de carte, comme la jauge elle-même.
class ProgressReport {
 public:
  // Une barre de script vient d'être posée sur le joueur.
  void Owe() noexcept { owed_ = true; }

  // La jauge du joueur s'éteint. Rend la réponse due si une barre l'attendait.
  void GaugeEnded() noexcept;

  // Vrai UNE fois quand la réponse est due, puis faux jusqu'à la barre suivante.
  [[nodiscard]] bool TakeDue() noexcept;

  [[nodiscard]] bool owed() const noexcept { return owed_; }

 private:
  bool owed_ = false;
  bool due_ = false;
};

inline constexpr std::uint32_t kMsPerProgressSecond = 1000;

// ZC_PROGRESS : la barre au-dessus du joueur, pour `seconds` secondes à partir
// de `now_ms`, et la dette de la réponse. Une barre en cours repart de
// l'instant. Rend faux, sans dette, quand le joueur n'est pas dans le monde.
bool StartOwnProgress(World& world, ProgressReport& report, std::uint32_t seconds,
                      std::uint32_t now_ms);

// ZC_PROGRESS_ACTOR : la barre au-dessus d'un autre acteur, sans réponse due.
// Rend faux pour un acteur absent, le joueur ou un objet au sol.
bool StartActorProgress(World& world, std::uint32_t actor_id, std::uint32_t seconds,
                        std::uint32_t now_ms);

// ZC_PROGRESS_CANCEL : la jauge du joueur s'éteint tout de suite, et la
// réponse devient due si une barre l'attendait.
void CancelOwnProgress(World& world, ProgressReport& report);

// L'ÉCHÉANCE : une fois la jauge du joueur écoulée — ou disparue —, la réponse
// devient due. À appeler à chaque image.
void NoteOwnProgressEnd(const World& world, ProgressReport& report, std::uint32_t now_ms);

}  // namespace ro::sim
