#pragma once

// CE QUE LA FENÊTRE « BASIC INFO » MONTRE, croisé depuis le monde : la vie de
// l'acteur, le reste du statut du joueur, et le nom de sa classe.

#include <string>

#include "ro/sim/world_fwd.h"
#include "ro/ui/basic_info_window.h"
#include "ro/vfs/job_names.h"

namespace moonclient {

// Recompose `into` ; rend faux, et le laisse vide, sans personnage en jeu.
//
// ⚠ CE QUE LE SERVEUR N'A PAS DIT VAUT ZÉRO : c'est la forme sous laquelle la
// fenêtre du jeu le montre avant le premier paquet — des jauges vides et
// muettes, des niveaux à zéro.
[[nodiscard]] bool ComposeBasicInfo(ro::ui::BasicInfoView& into, const ro::sim::World& world,
                                    const ro::vfs::JobNames& jobs, std::string toggle_keys);

}  // namespace moonclient
