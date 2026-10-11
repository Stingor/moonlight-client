#pragma once

// Ce que les classes du gestionnaire d'effets partagent entre leurs fichiers :
// la particule en quad, la matrice du client, et la fabrique de chacune.

#include <array>
#include <cstdint>
#include <memory>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_class.h"
#include "ro/render/map_effect_math.h"

namespace ro::render::effect_class {

inline constexpr float kClientPi = 3.141592F;
inline constexpr float kHalfTurnDegrees = 180.0F;
inline constexpr float kFullTurnDegrees = 360.0F;
inline constexpr std::int32_t kTickMs = 16;

[[nodiscard]] inline float Radians(float degrees) { return degrees / kHalfTurnDegrees * kClientPi; }

// UN DÉCALAGE PRIS DANS LE PLAN DE LA CAMÉRA : le vecteur tourné du tangage
// puis du lacet, comme un panneau face à elle. `{0, -h, 0}` monte de `h` à
// l'écran, quel que soit l'angle de vue.
[[nodiscard]] inline glm::vec3 CameraPlaneOffset(const EffectClassView* view, glm::vec3 offset) {
  if (view == nullptr) {
    return offset;
  }
  NativeMatrix rotation = kNativeIdentity;
  MultiplyRotationX(rotation, Radians(view->pitch_degrees));
  MultiplyRotationY(rotation, Radians(view->yaw_degrees));
  return Transform(offset, rotation);
}

// LES MOTS DE RENDU que les classes passent à la file du client : le sol, peint
// avant les corps, et l'ordinaire.
inline constexpr std::uint32_t kGroundRenderWord = 13;
inline constexpr std::uint32_t kOrdinaryRenderWord = 5;

// LES FACTEURS DE MÉLANGE d'une particule neuve : la source par son alpha, la
// destination telle quelle — une ADDITION. Six pour la destination donne le
// mélange ordinaire.
inline constexpr std::int32_t kBlendSourceAlpha = 5;
inline constexpr std::int32_t kBlendOne = 2;
inline constexpr std::int32_t kBlendInverseSourceAlpha = 6;

// UNE PARTICULE EN QUAD : quatre coins de repos, une matrice propre, une place,
// et ce que le dessin lui applique.
//
// ⚠ L'ALPHA EST UN ENTIER NON BORNÉ : la couleur d'un coin se bâtit sur trente-
// deux bits, si bien qu'au-delà de 255 ou sous zéro seul l'octet bas compte.
struct QuadParticle {
  std::string_view texture;
  // Haut-gauche, haut-droit, bas-gauche, bas-droit, dans le repère local.
  std::array<glm::vec3, 4> corners = {glm::vec3{-1.0F, 1.0F, 1.0F}, glm::vec3{1.0F, 1.0F, 1.0F},
                                      glm::vec3{-1.0F, -1.0F, 1.0F}, glm::vec3{1.0F, -1.0F, 1.0F}};
  NativeMatrix matrix = kNativeIdentity;
  glm::vec3 position{0.0F, 0.0F, 0.0F};
  std::array<std::int32_t, 3> color = {255, 255, 255};
  std::int32_t alpha = 255;
  std::int32_t blend_source = kBlendSourceAlpha;
  std::int32_t blend_destination = kBlendOne;
  std::uint32_t render_word = kOrdinaryRenderWord;
};

// La portion de texture d'un quad : les deux abscisses et les deux ordonnées.
struct QuadUv {
  float v_top = 0.0F;
  float v_bottom = 1.0F;
  float u_left = 0.0F;
  float u_right = 1.0F;
};

// Le quad d'une particule : coins · matrice, puis — hors repère du monde — la
// rotation du lacet de la caméra, puis la place.
void SubmitQuad(const QuadParticle& particle, const QuadUv& uv, bool world_space,
                const EffectClassView& view, EffectClassOutput& output);

// L'octet de couleur qu'un entier non borné laisse dans le mot du client.
[[nodiscard]] constexpr std::uint8_t LowByte(std::int32_t value) {
  constexpr std::uint32_t kByteMask = 0xFFU;
  return static_cast<std::uint8_t>(static_cast<std::uint32_t>(value) & kByteMask);
}

// Ce qu'un sprite fait de son image quand l'action est jouée jusqu'au bout.
// `kManual` n'avance jamais seul : sa classe choisit l'image à chaque tic.
enum class SpritePlay : std::uint8_t { kLoop, kOnce, kHold, kManual };

// UNE PARTICULE EN SPRITE : son image avance à chaque dessin quand son âge
// tombe sur sa période — à chaque dessin pour une période nulle.
struct SpriteParticle {
  std::string_view sprite;
  glm::vec3 position{0.0F};
  glm::vec3 offset{0.0F};
  std::int32_t age = 0;
  std::int32_t frame = 0;
  float period = 0.0F;
  SpritePlay play = SpritePlay::kLoop;
  bool spent = false;
  float scale = 1.0F;
  float spin_degrees = 0.0F;
  std::int32_t alpha = 255;
  std::uint32_t render_word = 0;
  bool keeps_layer_color = false;
  // L'action du `.act` que le sprite joue.
  std::int32_t action = 0;
  // Voir `EffectClassSpriteDraw::depth_bias`.
  float depth_bias = 0.0F;
  // L'échelle que le porteur impose en plus de la sienne : voir
  // `EffectClassTick::owner_shrink_scale`.
  float owner_scale = 1.0F;
  // Voir `EffectClassSpriteDraw::screen_lift_pixels`.
  float screen_lift_pixels = 0.0F;
};

// L'image suivante d'un sprite, selon sa période et son mode de lecture ; un
// sprite joué une fois est DÉPENSÉ passé sa dernière image.
void AdvanceSpriteFrame(SpriteParticle& particle, const EffectClassTick& tick);
// Le sprite à peindre, sauf s'il est dépensé.
void DrawSprite(const SpriteParticle& particle, EffectClassOutput& output);

// L'angle d'ÉCRAN, en degrés dans le sens des aiguilles d'une montre, de la
// direction qui va de `from` à `to` : `acos(dx / longueur)`, retourné quand le
// point visé est plus haut à l'écran. Vide quand l'un n'est pas visible.
[[nodiscard]] std::optional<float> ScreenAngleDegrees(const EffectClassView& view, glm::vec3 from,
                                                      glm::vec3 to);

// LES FABRIQUES, une par classe portée.
using Maker = std::unique_ptr<EffectClassInstance> (*)(const EffectClassBirth&,
                                                       const EffectClassView&);
std::unique_ptr<EffectClassInstance> MakeFriggSong(const EffectClassBirth&, const EffectClassView&);
std::unique_ptr<EffectClassInstance> MakeBindTrap(const EffectClassBirth&, const EffectClassView&);
std::unique_ptr<EffectClassInstance> MakeJupitelThunderHit(const EffectClassBirth&,
                                                           const EffectClassView&);
std::unique_ptr<EffectClassInstance> MakeRotateItem(const EffectClassBirth&,
                                                    const EffectClassView&);
std::unique_ptr<EffectClassInstance> MakeRotateLine(const EffectClassBirth&,
                                                    const EffectClassView&);
std::unique_ptr<EffectClassInstance> MakeStarEmperor(const EffectClassBirth&,
                                                     const EffectClassView&);
std::unique_ptr<EffectClassInstance> MakeLeashRings(const EffectClassBirth&,
                                                    const EffectClassView&);
std::unique_ptr<EffectClassInstance> MakePetitio(const EffectClassBirth&, const EffectClassView&);
std::unique_ptr<EffectClassInstance> MakeSavageImpact(const EffectClassBirth&,
                                                      const EffectClassView&);
std::unique_ptr<EffectClassInstance> MakeEternalSlash(const EffectClassBirth&,
                                                      const EffectClassView&);
std::unique_ptr<EffectClassInstance> MakeCrimsonArrow(const EffectClassBirth&,
                                                      const EffectClassView&);
std::unique_ptr<EffectClassInstance> MakeStormCannon(const EffectClassBirth&,
                                                     const EffectClassView&);
std::unique_ptr<EffectClassInstance> MakeShadowExceed(const EffectClassBirth&,
                                                      const EffectClassView&);
std::unique_ptr<EffectClassInstance> MakeFloralFlareRoad(const EffectClassBirth&,
                                                         const EffectClassView&);
std::unique_ptr<EffectClassInstance> MakeAstralStrike(const EffectClassBirth&,
                                                      const EffectClassView&);
std::unique_ptr<EffectClassInstance> MakePneumaticus(const EffectClassBirth&,
                                                     const EffectClassView&);
std::unique_ptr<EffectClassInstance> MakeRainOfCrystal(const EffectClassBirth&,
                                                       const EffectClassView&);
std::unique_ptr<EffectClassInstance> MakeTriangleShot(const EffectClassBirth&,
                                                      const EffectClassView&);
std::unique_ptr<EffectClassInstance> MakeCartCannon(const EffectClassBirth&,
                                                    const EffectClassView&);
std::unique_ptr<EffectClassInstance> MakeAxeBoomerang(const EffectClassBirth&,
                                                      const EffectClassView&);
std::unique_ptr<EffectClassInstance> MakeAcidifiedThrow(const EffectClassBirth&,
                                                        const EffectClassView&);
std::unique_ptr<EffectClassInstance> MakeCaneOfEvilEye(const EffectClassBirth&,
                                                       const EffectClassView&);
std::unique_ptr<EffectClassInstance> MakeScatter(const EffectClassBirth&, const EffectClassView&);
std::unique_ptr<EffectClassInstance> MakeStemSpear(const EffectClassBirth&, const EffectClassView&);
std::unique_ptr<EffectClassInstance> MakeSvgSpirit(const EffectClassBirth&, const EffectClassView&);
std::unique_ptr<EffectClassInstance> MakePowdering(const EffectClassBirth&, const EffectClassView&);
std::unique_ptr<EffectClassInstance> MakeSpriteEffect(const EffectClassBirth&,
                                                      const EffectClassView&);
std::unique_ptr<EffectClassInstance> MakeSpriteMable(const EffectClassBirth&,
                                                     const EffectClassView&);
std::unique_ptr<EffectClassInstance> MakeTunaParty(const EffectClassBirth&, const EffectClassView&);
std::unique_ptr<EffectClassInstance> MakeBearerSprite(const EffectClassBirth&,
                                                      const EffectClassView&);
std::unique_ptr<EffectClassInstance> MakeSunStance(const EffectClassBirth&, const EffectClassView&);
std::unique_ptr<EffectClassInstance> MakeSoulUnity(const EffectClassBirth&, const EffectClassView&);
std::unique_ptr<EffectClassInstance> MakeBearerStr(const EffectClassBirth&, const EffectClassView&);
std::unique_ptr<EffectClassInstance> MakeEternalSlashCount(const EffectClassBirth&,
                                                           const EffectClassView&);
std::unique_ptr<EffectClassInstance> MakeServantWeapons(const EffectClassBirth&,
                                                        const EffectClassView&);
std::unique_ptr<EffectClassInstance> MakeShadowSpheres(const EffectClassBirth&,
                                                       const EffectClassView&);
std::unique_ptr<EffectClassInstance> MakeMeteor(const EffectClassBirth&, const EffectClassView&);
std::unique_ptr<EffectClassInstance> MakeEnergyOrb(const EffectClassBirth&, const EffectClassView&);
std::unique_ptr<EffectClassInstance> MakeThrowItem(const EffectClassBirth&, const EffectClassView&);
std::unique_ptr<EffectClassInstance> MakeCherryBlossom(const EffectClassBirth&,
                                                       const EffectClassView&);
std::unique_ptr<EffectClassInstance> MakeFullScreen(const EffectClassBirth&,
                                                    const EffectClassView&);
std::unique_ptr<EffectClassInstance> MakeGaleStorm(const EffectClassBirth&, const EffectClassView&);
std::unique_ptr<EffectClassInstance> MakeJupitelThunderStorm(const EffectClassBirth&,
                                                             const EffectClassView&);

}  // namespace ro::render::effect_class
