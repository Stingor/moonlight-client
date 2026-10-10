#pragma once

#include <cstdint>

#include "ro/core/error.h"

#include "patcher/patch_run.h"

namespace moonclient {

class ClientShell;

namespace patcher {

// COMMENT S'ACHÈVE L'ÉTAPE DE MISE À JOUR.
enum class PatchStageEnd : std::uint8_t {
  kUpToDate,      // tout est appliqué : on ouvre les archives
  kRestart,       // l'exécutable a changé : le client doit se relancer
  kPlayOutdated,  // le patch a échoué et le joueur joue quand même
  kQuit,          // le joueur a fermé la fenêtre ou choisi de quitter
};

// L'ÉTAPE DE MISE À JOUR : entre la fenêtre et les archives.
//
// Le patcher tourne sur un fil à lui ; cette boucle dessine sa progression
// et sert les événements. Sur un échec, le joueur choisit : réessayer, jouer
// quand même, ou quitter.
//
// ⚠ LE VERROU DE L'INSTALLATION SE PREND ICI, pour toute la marche : deux
// clients lancés ensemble ne patchent pas le même dossier en même temps.
[[nodiscard]] ro::core::Result<PatchStageEnd> RunPatchStage(ClientShell& shell,
                                                            const PatchRunSettings& settings);

// LA MISE À JOUR DE L'INSTALLATION DE `shell`, telle que le démarrage la
// demande : l'étape, puis la relance quand l'exécutable a changé. Sur
// `kRestart`, le nouveau client est déjà lancé, et l'appelant n'a plus qu'à
// quitter.
[[nodiscard]] ro::core::Result<PatchStageEnd> UpdateInstall(ClientShell& shell,
                                                            const PatcherConfig& config);

}  // namespace patcher
}  // namespace moonclient
