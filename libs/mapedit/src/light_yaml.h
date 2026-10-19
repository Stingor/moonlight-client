#pragma once

#include <optional>
#include <string>
#include <string_view>

#include <yaml-cpp/yaml.h>

#include "ro/mapedit/map_edit.h"

namespace ro::mapedit::detail {

// LES LUMIÈRES DU DOCUMENT : celles de la section « lights », et celles de la
// section « model_lights » du document des lumières de modèle.
//
// Chaque lecture rend le motif de son refus, vide quand elle est bonne.

// La plage d'heures sous la clé « hours » : deux heures de 0 à 24 ; rien sans
// la clé.
[[nodiscard]] std::string ReadHours(const YAML::Node& entry, const std::string& where,
                                    std::optional<LitHours>& hours);
// La plage, sous sa clé, dans une entrée de liste ; rien sans plage.
[[nodiscard]] std::string HoursText(const std::optional<LitHours>& hours);

// Une lumière : position, couleur, portée strictement positive et raison sont
// dues.
[[nodiscard]] std::string ReadLight(const YAML::Node& entry, const std::string& where,
                                    AddedLight& light);
// Une lumière en entrée de liste, sa position en tête.
[[nodiscard]] std::string LightEntryText(const AddedLight& light);

// La section « model_lights » de `whole` — absente, rien —, que seul le
// document `kModelLightsDocument` peut porter.
[[nodiscard]] std::string ReadModelLights(const YAML::Node& whole, std::string_view map_name,
                                          MapEdit& edit);
// Le texte de la section, sa clé comprise ; rien sans lumière de modèle.
[[nodiscard]] std::string ModelLightsYaml(const MapEdit& edit);

}  // namespace ro::mapedit::detail
