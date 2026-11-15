#pragma once

#include "ro/ui/staff_tools.h"

#include "game/settings_file.h"

namespace moonclient {

// LES CLÉS DE LA CAMÉRA que l'établi règle : ses bornes, son relevé et la
// hauteur de l'œil à la première personne. ⚠ LA VALEUR DU MEMBRE EST LE DÉFAUT.
void DeclareStaffCamera(SettingsFile& into, ro::ui::StaffToolsLook& look);

}  // namespace moonclient
