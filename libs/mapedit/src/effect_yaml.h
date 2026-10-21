#pragma once

#include <optional>
#include <string>
#include <string_view>

#include <yaml-cpp/yaml.h>

#include "ro/mapedit/map_edit.h"

namespace ro::mapedit::detail {

// LES EFFETS DU DOCUMENT : ceux de la section « effects », et ceux de la
// section « model_effects » du document des lumières de modèle.
//
// Chaque lecture rend le motif de son refus, vide quand elle est bonne.

// La lueur d'un effet : couleur, portée strictement positive, vacillement,
// ombre ; les champs absents gardent le défaut d'`EffectGlow`.
[[nodiscard]] std::string ReadGlow(const YAML::Node& glow, const std::string& where,
                                   EffectGlow& lit);
// La lueur d'un effet, sous sa clé ; rien sans lueur.
[[nodiscard]] std::string GlowText(const std::optional<EffectGlow>& glow);

// Un effet : numéro, position et raison sont dus.
[[nodiscard]] std::string ReadEffect(const YAML::Node& entry, const std::string& where,
                                     AddedEffect& effect);
// Un effet en entrée de liste, son numéro en tête.
[[nodiscard]] std::string EffectEntryText(const AddedEffect& effect);

// La section « model_effects » de `whole` — absente, rien —, que seul le
// document `kModelLightsDocument` peut porter.
[[nodiscard]] std::string ReadModelEffects(const YAML::Node& whole, std::string_view map_name,
                                           MapEdit& edit);
// Le texte de la section, sa clé comprise ; rien sans effet de modèle.
[[nodiscard]] std::string ModelEffectsYaml(const MapEdit& edit);

}  // namespace ro::mapedit::detail
