#pragma once

#include <optional>
#include <span>
#include <vector>

#include "ro/formats/rsw.h"
#include "ro/mapedit/map_edit.h"

namespace ro::render {

// LES SOURCES DE SON À LEVER : celles du RSW, telles que leurs retouches les
// posent — déplacées, masquées, leur volume, leur portée ou leur cycle
// changés, réservées à la nuit ; une retouche qui ne vise plus sa source ne
// s'applique pas —, puis les sons que le document ajoute. Les « nuit
// seulement » sonnent selon `LitAtNight` : leur plage d'heures quand le ciel a
// une heure `hour`, sinon la nuit `night` qui tombe.
//
// Les noms de fichiers sortent en CP949, comme le RSW les cite : la liste se
// lève comme celle du fichier.
[[nodiscard]] std::vector<formats::RswSound> ActiveMapSounds(
    std::span<const formats::RswSound> map_sounds, const mapedit::MapEdit& document, float night,
    std::optional<float> hour = std::nullopt);

// Les deux listes portent-elles les mêmes sources, champ par champ ? Une liste
// qui change se relève ; une liste égale se laisse sonner.
[[nodiscard]] bool SameMapSounds(std::span<const formats::RswSound> left,
                                 std::span<const formats::RswSound> right);

}  // namespace ro::render
