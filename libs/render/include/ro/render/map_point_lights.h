#pragma once

#include <optional>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "ro/mapedit/map_edit.h"
#include "ro/render/map_effect_math.h"
#include "ro/render/point_lights.h"
#include "ro/render/scene_light.h"

namespace ro::render {

struct MapScene;

// LES LUMIÈRES PONCTUELLES DE L'IMAGE : celles du document, passées au monde,
// leur intensité de l'instant, rangées de la plus proche de `eye` à la plus lointaine. Voir
// docs/adr/0119.
//
// Une lumière « la nuit seulement » vaut `1 − jour`, où le jour est la
// composante la plus claire de `daylight × tint` : le ciel à l'heure ET la
// nuit que le serveur pose. Sur une carte fermée au ciel — intérieur, donjon,
// `always_night` —, elle brille pleinement : rien n'y fait le jour. Sa plage
// d'heures, quand elle en a une, se juge à l'heure du ciel `hour` : voir
// `NightGlow`.
[[nodiscard]] PointLightSet MapPointLights(const std::vector<ro::mapedit::AddedLight>& lights,
                                           const MapEffectFrame& frame, const SceneLight& sky,
                                           bool always_night, glm::vec3 eye, double time_ms = 0.0,
                                           std::optional<float> hour = std::nullopt);

// TOUTES LES LUMIÈRES DU DOCUMENT : les siennes, puis la lueur de chaque effet
// qui en porte une, à sa place, avec la règle de nuit de l'effet — ceux qu'il
// ajoute, et les effets de la carte `map_objects` que ses retouches font luire
// là où elles les posent ; un effet masqué ne luit pas.
[[nodiscard]] std::vector<ro::mapedit::AddedLight> DocumentLights(
    const ro::mapedit::MapEdit& edit, std::span<const ro::formats::RswEffect> map_objects = {});

// LES LUMIÈRES DE LA CARTE AFFICHÉE : celles de son document `edit`, puis
// celles que ses modèles portent selon le document des lumières de modèle
// `model_book` — voir `mapedit::ModelLightsOn` et docs/adr/0126 —, puis la
// lueur des effets de modèle qui en portent une (docs/adr/0129).
[[nodiscard]] std::vector<ro::mapedit::AddedLight> SceneLights(
    const ro::mapedit::MapEdit& edit, const ro::mapedit::MapEdit& model_book,
    const MapScene& scene);

// LA LUEUR D'UN EFFET LUIT À LA HAUTEUR DE SA FLAMME, pas à son ancre : un
// effet s'ancre au pied du feu, souvent au ras du sol ou juste dessous, où sa
// lumière ombrerait tout ce qui le domine. Elle monte de tant d'unités du RSW
// — la hauteur d'une vasque de brasero au-dessus de son pied.
inline constexpr float kEffectGlowLiftWorld = 12.0F;

// LE VACILLEMENT d'une lumière à l'instant `time_ms`, facteur de son intensité
// entre `1 − kFlickerDepth` et 1 : deux ondes de périodes premières entre
// elles, décalées par le rang `slot` pour que deux torches voisines ne
// battent pas ensemble. Déterministe : la même image donne la même lumière.
inline constexpr float kFlickerDepth = 0.35F;
[[nodiscard]] float FlickerFactor(std::size_t slot, double time_ms);

// LE BALANCEMENT D'UNE FLAMME QUI VACILLE à l'instant `time_ms`, en unités du
// monde : elle monte et descend d'au plus `kFlameSwayWorld`, sur deux ondes
// décalées par le rang `slot`, et les ombres qu'elle porte s'allongent et
// raccourcissent.
//
// ⚠ JAMAIS DE CÔTÉ : un brasier se tient souvent contre un mur, et une flamme
// qui s'y balancerait passerait derrière lui. Le mur s'éteindrait alors d'un
// bloc, puis se rallumerait. Une flamme qui monte reste à la même distance des
// murs. Il ne change que tous les
// `kFlameSwayStepMs` : chaque pas repeint la carte d'ombre de la flamme. À
// trente pas par seconde, un pas ne déplace le bord d'une ombre que d'une
// fraction de sa pénombre.
inline constexpr float kFlameSwayWorld = 0.6F;
inline constexpr double kFlameSwayStepMs = 1000.0 / 30.0;
[[nodiscard]] glm::vec3 FlameSway(std::size_t slot, double time_ms);

// LA PÉNOMBRE D'UNE FLAMME QUI VACILLE, en part de la distance à la flamme :
// une flamme est une source large, et le bord de son ombre se fond au lieu de
// trancher. C'est ce qui fait passer une surface de l'ombre à la lumière en
// douceur quand la flamme se balance. Voir `PointLight::shadow_spread`.
inline constexpr float kFlameShadowSpread = 0.06F;

}  // namespace ro::render
