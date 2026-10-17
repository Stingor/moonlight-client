#pragma once

#include <vector>

#include "ro/render/effect_renderer.h"
#include "ro/render/map_effects.h"
#include "ro/render/model_clock.h"
#include "ro/render/model_renderer.h"
#include "ro/render/point_shadow_maps.h"
#include "ro/render/scene_post_process.h"
#include "ro/render/sprite_renderer.h"
#include "ro/render/sun_shadow_map.h"
#include "ro/render/terrain_renderer.h"
#include "ro/render/void_shade.h"
#include "ro/render/water_renderer.h"

namespace moonclient {

// LES RENDERERS DE LA PARTIE : le terrain, l'eau, les modèles, les sprites, les
// effets, et ce que la carte leur donne à peindre.
//
// 🔴 UNE BASE DE `Game`, DONC DÉTRUITE APRÈS TOUT MEMBRE : la carte, les effets
// et les calques qui désignent un renderer s'en vont avant lui.
//
// ⚠ AUCUNE MÉTHODE : la partie les lit comme les siens, sous le même nom.
class GameRendering {
 protected:
  // ── LE RENDU ───────────────────────────────────────────────────────────────
  ro::render::TerrainRenderer terrain_;
  // LA CARTE D'OMBRE DU SOLEIL, créée la première fois qu'on la demande.
  // `sun_map_refused_` retient un échec : il ne se retente pas à chaque image.
  ro::render::SunShadowMap sun_map_;
  bool sun_map_refused_ = false;
  // LES CARTES D'OMBRE DES LUMIÈRES PONCTUELLES, créées à la première lumière
  // qui en demande ; même refus retenu.
  ro::render::PointShadowMaps point_maps_;
  bool point_maps_refused_ = false;
  // LE VIDE SOUS LE MONDE, assombri par l'heure ; créé à la première nuit.
  ro::render::VoidShade void_shade_;
  bool void_shade_refused_ = false;
  // LE MONDE PEINT À PART, repeint à travers la page « Graphics ».
  ro::render::ScenePostProcess scene_post_;
  // LA NAPPE D'EAU. Vide sur une carte qui n'en a pas, et elle ne dessine alors
  // rien : la plupart des souterrains sont dans ce cas.
  ro::render::WaterRenderer water_;
  // LES CHEMINÉES (44), LES ÉMETTEURS (974) ET LES ANNEAUX DE VENT (2343) de la
  // carte. Ils ne repartent qu'avec la scène, comme l'horloge des modèles.
  ro::render::MapEffects map_effects_;
  // Leurs lots pour le renderer d'effets, réemployés d'image en image.
  std::vector<ro::render::DrawnWorldQuads> map_effect_quads_;
  // L'HORLOGE DES MODÈLES ANIMÉS — fontaines, moulins, enseignes. Elle part à la
  // première image de chaque carte et s'efface avec le reste, au changement de
  // carte : voir `ForgetWhatPlayed`.
  ro::render::ModelClock model_clock_;
  ro::render::ModelRenderer models_;
  ro::render::ModelRenderer ground_unit_models_;
  ro::render::SpriteRenderer sprites_;
  ro::render::EffectRenderer effect_renderer_;
};

}  // namespace moonclient
