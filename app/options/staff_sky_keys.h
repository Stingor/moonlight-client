#pragma once

#include "ro/ui/staff_tools.h"

#include "game/settings_file.h"

namespace moonclient {

// LES CLÉS DE LA COURSE DES ASTRES que l'établi règle : voir
// `StaffToolsLook::sky_course`. ⚠ LA VALEUR DU MEMBRE EST LE DÉFAUT.
void DeclareStaffSkyCourse(SettingsFile& into, ro::ui::StaffToolsLook& look);

}  // namespace moonclient
