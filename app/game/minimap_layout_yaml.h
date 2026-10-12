#pragma once

#include <yaml-cpp/yaml.h>

#include "ro/ui/minimap_settings.h"

namespace moonclient {

// ── LA PRÉSENTATION DE LA MINIMAP EN YAML ──────────────────────────────────
//
// Une table, la même sous chaque profil de `ui_layouts.yml` et sous
// « minimap » de `navigation.yml` : deux écritures divergeraient au premier
// champ ajouté.

// Lit les champs présents de `node`, une table ; les autres gardent ce que
// `into` porte. Les valeurs hors bornes — un fichier s'édite à la main — sont
// ramenées. ⚠ PEUT LEVER `YAML::Exception` sur une valeur illisible.
void ReadMinimapLayout(const YAML::Node& node, ro::ui::MinimapLayout& into);

// Écrit la table entière, `BeginMap` à `EndMap` compris.
void WriteMinimapLayout(YAML::Emitter& out, const ro::ui::MinimapLayout& from);

}  // namespace moonclient
