#pragma once

#include <filesystem>

#include "ro/core/error.h"

namespace moonclient::patcher {

// Le chemin de l'exécutable qui tourne, tel que le système l'a chargé.
[[nodiscard]] ro::core::Result<std::filesystem::path> RunningExecutablePath();

// RELANCE LE CLIENT avec la même ligne de commande, puis rend la main : à
// l'appelant de quitter aussitôt.
//
// C'est ce qui suit un patch qui a remplacé l'exécutable : celui qui tourne
// est l'ancien, et la chaîne décrit le nouveau. ⚠ LE NOUVEAU PROCESSUS DÉMARRE
// PENDANT QUE L'ANCIEN S'ARRÊTE : il ne doit pas attendre de lui qu'il ait
// libéré quoi que ce soit — le verrou d'installation est déjà rendu, et
// l'ancien exécutable porte un autre nom.
[[nodiscard]] ro::core::Result<void> RelaunchSelf();

}  // namespace moonclient::patcher
