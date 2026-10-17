#pragma once

#include "ro/mapedit/map_edit.h"

namespace moonclient {

// LES RAISONS PAR DÉFAUT : chaque retouche ou ajout du document qui n'a pas de
// raison en reçoit une générique, dans la langue du joueur, qui dit ce qu'il
// est — « Retouche de l'effet #47 ». La raison reste due, mais n'est jamais
// vide ; le staff la remplace en saisissant la sienne.
[[nodiscard]] ro::mapedit::MapEdit WithReasons(ro::mapedit::MapEdit document);

}  // namespace moonclient
