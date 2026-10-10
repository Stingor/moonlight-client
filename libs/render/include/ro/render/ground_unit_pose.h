#pragma once

#include <cstdint>

#include "ro/render/model_mesh.h"

namespace ro::render {

// L'image du modèle d'une UNITÉ AU SOL — un piège, un leurre.
//
// Le modèle se pose FIGÉ sur son image de pose, et n'en bouge que lorsqu'il
// reçoit l'ordre de jouer — le piège se referme : il joue alors son animation
// UNE fois, à la vitesse 1, et reste sur sa dernière image.
//
// Données pures : l'ordre de jouer vient du monde, l'image se calcule ici, et
// la pose des nœuds se fait par `ModelClock::Hold`.

// Ce qu'un modèle d'unité retient d'un pas à l'autre.
struct GroundUnitPlay {
  // Le compteur d'images de la première famille. ⚠ IL PART DE ZÉRO, et non de
  // l'image de pose : le premier pas après l'ordre de jouer montre donc
  // l'image 33, et non 36.
  std::int32_t counted_frame = 0;
  // L'image montrée.
  float frame = 0.0F;
};

// L'image de pose. Le même nombre, 3, est donné aux deux familles : la première
// le lit comme une image entière, la seconde comme un temps en millisecondes,
// qu'elle convertit par sa cadence.
[[nodiscard]] float GroundUnitRestFrame(const ModelRig& rig);

// Le modèle tout juste bâti : au repos, sur son image de pose.
[[nodiscard]] GroundUnitPlay GroundUnitAtRest(const ModelRig& rig);

// Un pas logique : l'image d'après pour un modèle qui a reçu l'ordre de jouer,
// l'image inchangée sinon.
//
// La première famille avance d'un cran par pas ; la seconde lit le temps écoulé
// depuis que le modèle a été BÂTI — et non depuis l'ordre de jouer : un piège
// posé depuis plus longtemps que son animation saute droit à sa dernière image.
void StepGroundUnit(const ModelRig& rig, bool sprung, std::uint32_t built_ms, std::uint32_t now_ms,
                    GroundUnitPlay& play);

}  // namespace ro::render
