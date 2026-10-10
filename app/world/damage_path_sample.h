#pragma once

#include <cstdint>
#include <map>
#include <optional>

#include <imgui.h>

#include <glm/glm.hpp>

#include "ro/render/actor_sprite.h"
#include "ro/render/camera.h"
#include "ro/render/gl_texture.h"
#include "ro/ui/entity_sample.h"
#include "ro/ui/path_canvas.h"

#include "world/hit_popup_look.h"
#include "world/hit_popups.h"

namespace moonclient {

// ── LE CADRE D'ESSAI DES DÉGÂTS, dans le volet de droite de la section ─────
//
// Une cible, et le coup qui montre le type choisi, rejoué sans cesse : les
// nombres naissent, suivent leur trajectoire et s'éteignent pendant qu'on en
// tire les ancres. Ils passent par une seconde instance de `HitPopups`, donc
// par les mêmes règles et les mêmes compositions que le jeu.

// La cible que le cadre montre pour ce type : le personnage pour ce qu'il
// reçoit — un coup, un soin, le « Miss » qu'il porte quand il rate —, un
// monstre pour le reste.
[[nodiscard]] ro::ui::EntitySampleShown PathTargetShown(DamagePath path) noexcept;

// LE SOL DU CADRE, vu par la caméra du jeu : ce qu'un pas d'une unité vers
// l'est puis vers le nord déplace à l'écran autour de `ground`, ramené en pixels
// de sprite par `sprite_scale`. Sans projection possible, un sol vu de face.
[[nodiscard]] HitPopups::SampleGround SampleGroundOf(const ro::render::Camera& camera,
                                                     float aspect_ratio, float viewport_width,
                                                     float viewport_height, const glm::vec3& ground,
                                                     float sprite_scale, float body_degrees);

// CE QUE LA PARTIE DONNE AU CADRE, en plus du réglage.
struct DamagePathStage {
  // La cible, déjà en vignette.
  ro::ui::EntitySample target;
  // Pixels d'écran par pixel de sprite, ceux du jeu au zoom courant.
  float sprite_scale = 1.0F;
  HitPopups::SampleGround ground;
  // La police des dégâts écrits en police.
  ImFont* font = nullptr;
};

class DamagePathSampler {
 public:
  // Le cadre à cet instant. `game_numbers` prête ses sprites : sans eux, pas de
  // nombres — la cible et les ancres restent.
  [[nodiscard]] ro::ui::PathSample Compose(const std::optional<HitPopups>& game_numbers,
                                           const HitPopupLook& look, DamagePathStage stage,
                                           std::uint32_t now_ms);

 private:
  // La texture d'une image de sprite, téléversée à sa première apparition.
  [[nodiscard]] ImTextureID TextureOf(const ro::render::SpriteImageKey& key);

  // OÙ LE COUP DE CE TYPE NAÎT CHEZ LE CLIENT, et le chemin que son allure
  // seule lui fait faire, dérive comprise : voir `ro::ui::PathSample::birth` et
  // `ro::ui::PathSample::allure`. Relevés en le faisant vivre, sans trajectoire
  // à lui, dans une instance d'essai.
  void TraceAllure(DamagePath path, const HitPopupLook& look, const HitPopups::SampleGround& ground,
                   ro::ui::PathSample& sample) const;

  std::optional<HitPopups> numbers_;
  DamagePath shown_ = DamagePath::kOnMonster;
  std::uint32_t next_spawn_ms_ = 0;
  std::map<ro::render::SpriteImageKey, ro::render::GlTexture> textures_;
};

}  // namespace moonclient
