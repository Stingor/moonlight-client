#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/formats/rsw.h"
#include "ro/mapedit/map_edit.h"

namespace ro::mapedit {

// LES LUMIÈRES DE MODÈLE SUR UNE CARTE : voir docs/adr/0126.
//
// Une lumière de modèle se tient dans le REPÈRE DE LA POSE : celui du RSW,
// tourné et mis à l'échelle comme la pose, et centré sur sa position. Sans
// rotation et à l'échelle 1, sa position est donc l'écart à la position de la
// pose, en unités du RSW, y négatif vers le haut. Le recentrage du maillage
// n'y entre pas : il dépend du fichier, et la lumière doit tenir à la pose.

// LA CLÉ D'UN FICHIER DE MODÈLE : ses lettres ASCII en minuscules et ses
// barres obliques à l'endroit des barres inverses. Deux poses du même fichier
// ont la même clé, quelle que soit la casse que le RSW leur donne.
[[nodiscard]] std::string ModelFileKey(std::string_view model_file);

// Où luit `offset`, dans le repère du RSW, sur la pose `placement`.
[[nodiscard]] std::array<float, 3> ModelLightPosition(const formats::RswModel& placement,
                                                      const std::array<float, 3>& offset);

// L'écart qui luit en `position`, dans le repère du RSW, sur la pose
// `placement` : l'inverse de `ModelLightPosition`. Un axe d'échelle nulle
// écrase tout ce qu'il porte : l'écart y vaut zéro.
[[nodiscard]] std::array<float, 3> ModelLightOffset(const formats::RswModel& placement,
                                                    const std::array<float, 3>& position);

// LES LUMIÈRES DE MODÈLE DE `book` SUR LA CARTE, dans le repère du RSW : pour
// chaque pose de `models` dont la clé `model_keys` — même rang — en porte,
// dans l'ordre des poses puis du document. Une pose masquée ou éteinte —
// `hidden`, `unlit`, rangs croissants — n'en porte aucune.
[[nodiscard]] std::vector<AddedLight> ModelLightsOn(const MapEdit& book,
                                                    std::span<const formats::RswModel> models,
                                                    std::span<const std::string> model_keys,
                                                    std::span<const std::size_t> hidden,
                                                    std::span<const std::size_t> unlit);

}  // namespace ro::mapedit
