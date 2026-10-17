#pragma once

#include <optional>
#include <span>
#include <vector>

#include "ro/formats/rsw.h"
#include "ro/mapedit/map_edit.h"
#include "ro/render/scene_light.h"

namespace ro::render {

// LA NUIT QUI TOMBE : 0 en plein jour, 1 au cœur de la nuit. Le jour est la
// composante la plus claire de `daylight × tint` — le ciel à l'heure ET la nuit
// que le serveur pose. Sur une carte fermée au ciel, `always_night`, rien n'y
// fait le jour.
[[nodiscard]] float NightStrength(const SceneLight& sky, bool always_night);

// À partir de quelle nuit un effet « la nuit seulement » se lève — et se
// couche : la moitié, pour qu'il paraisse au crépuscule et parte à l'aube.
inline constexpr float kEffectNightThreshold = 0.5F;

// LA NUIT D'UN OBJET « la nuit seulement » : sa plage d'heures quand il en a
// une et que le ciel a une heure `hour`, sinon la nuit `night` qui tombe. Un
// effet se lève ou non ; une lumière luit d'autant.
[[nodiscard]] bool LitAtNight(const std::optional<mapedit::LitHours>& hours, float night,
                              std::optional<float> hour);
[[nodiscard]] float NightGlow(const std::optional<mapedit::LitHours>& hours, float night,
                              std::optional<float> hour);

// LES OBJETS D'EFFET À LEVER : ceux du RSW, tels que leurs retouches les posent
// — déplacés, masqués, réservés à la nuit ; une retouche qui ne vise plus son
// objet ne s'applique pas —, puis ceux que le document ajoute. Les « nuit
// seulement » se lèvent tant que `night` atteint le seuil. Un objet ajouté se
// lève comme un objet du RSW : son numéro, sa place, sa cadence, ses
// paramètres. Une plage d'heures se juge à `hour` : voir `LitAtNight`.
[[nodiscard]] std::vector<formats::RswEffect> ActiveMapEffects(
    std::span<const formats::RswEffect> map_objects, const mapedit::MapEdit& document, float night,
    std::optional<float> hour = std::nullopt);

// Les deux listes portent-elles les mêmes objets, champ par champ ? Une liste
// qui change se relève ; une liste égale se laisse vivre.
[[nodiscard]] bool SameMapEffects(std::span<const formats::RswEffect> left,
                                  std::span<const formats::RswEffect> right);

}  // namespace ro::render
