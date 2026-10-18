#pragma once

#include <cstddef>
#include <string>

#include "ro/mapedit/map_edit.h"

namespace moonclient {

// LES RAISONS PAR DÉFAUT : chaque retouche ou ajout du document qui n'a pas de
// raison en reçoit une générique, dans la langue du joueur, qui dit ce qu'il
// est — « Retouche de l'effet #47 ». La raison reste due, mais n'est jamais
// vide ; le staff la remplace en saisissant la sienne.
[[nodiscard]] ro::mapedit::MapEdit WithReasons(ro::mapedit::MapEdit document);

// CE QUI EMPÊCHE D'ENREGISTRER, dans la langue du joueur : la première
// retouche ou le premier ajout sans raison, sprites de décor compris — ou une
// lumière, un son sans portée — que la relecture refuserait ; vide quand tout s'écrit. Les modèles
// ajoutés se nomment par leur rang, après les `original_count` du RSW.
[[nodiscard]] std::string SaveRefusal(const ro::mapedit::MapEdit& document,
                                      std::size_t original_count);

}  // namespace moonclient
