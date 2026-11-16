#pragma once

#include "ro/ui/staff_tools.h"

#include "game/settings_file.h"

namespace moonclient {

// LES CLÉS DE LA CAMÉRA que l'établi règle : ses bornes et son relevé.
// ⚠ LA VALEUR DU MEMBRE EST LE DÉFAUT.
void DeclareStaffCamera(SettingsFile& into, ro::ui::StaffToolsLook& look);

}  // namespace moonclient
