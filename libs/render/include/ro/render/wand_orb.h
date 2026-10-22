#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "ro/formats/image.h"
#include "ro/render/actor_sprite.h"
#include "ro/render/effect_frame.h"
#include "ro/render/element_glow.h"

namespace ro::render {

// LA BOULE D'ÉNERGIE DE LA BAGUETTE : ce que devient le trait d'une baguette
// quand le joueur le demande. Un cœur petit, presque blanc, dans un halo doux
// qui pulse ; derrière lui, une courte traînée d'étincelles qui s'estompent ;
// à l'arrivée, un bref éclat qui s'élargit et disparaît.
//
// ⚠ AUCUNE IMAGE N'EST LUE : un seul disque radial doux, fait en code, sert à
// toutes les pièces. Ses quatre ACTIONS sont les quatre pièces, et les
// échelles de leurs calques en font la forme — la pulsation du halo, le
// rétrécissement d'une étincelle, l'élargissement de l'éclat. La couleur et le
// fondu sont ceux du nœud qui porte la pièce : un nœud de sprite recouvre la
// teinte de ses calques.

// LE CHEMIN SOUS LEQUEL LE SPRITE FAIT EN CODE SE RANGE parmi les sprites
// lus : il ne commence pas par le dossier des données, et aucune archive ne
// peut donc le fournir à sa place.
inline constexpr std::string_view kWandOrbSpritePath = "procedural\\wand_orb";

// LA COULEUR ARCANE, celle d'une attaque neutre ou sans élément connu : un
// bleu-violet. En rouge, vert, bleu.
inline constexpr std::array<std::uint8_t, 3> kWandOrbArcaneRgb = {130, 110, 255};

// La couleur de la boule : celle de la teinte élémentaire, dans la table de la
// lueur élémentaire, ou l'arcane sans teinte.
[[nodiscard]] std::array<std::uint8_t, 3> WandOrbRgb(std::optional<ElementHue> hue) noexcept;

// LE CŒUR TIRE VERS LE BLANC : cette part du chemin de la couleur au blanc.
inline constexpr float kWandOrbCoreWhiteShare = 0.75F;
[[nodiscard]] std::array<std::uint8_t, 3> WandOrbCoreRgb(std::array<std::uint8_t, 3> rgb) noexcept;

// ── Le disque ─────────────────────────────────────────────────────────────

// Le côté du disque, en pixels de sprite : toutes les tailles s'y rapportent.
inline constexpr std::int32_t kWandOrbDiscPixels = 64;
// L'EXPOSANT DE SA DÉCROISSANCE : l'alpha vaut `(1 − r)^n`, `r` allant de zéro
// au centre à un au bord. Deux : un centre plein, un bord qui s'évanouit.
inline constexpr float kWandOrbDiscFalloffExponent = 2.0F;

// Le disque blanc, son alpha décroissant du centre au bord, en RGBA8.
[[nodiscard]] formats::Image WandOrbDiscImage();

// ── Les pièces ────────────────────────────────────────────────────────────

// LES QUATRE PIÈCES, et le rang de l'action du sprite qui dessine chacune.
enum class WandOrbPart : std::uint8_t { kCore, kHalo, kSpark, kBurst };
inline constexpr std::size_t kWandOrbPartCount = 4;
[[nodiscard]] constexpr std::int32_t WandOrbActionOf(WandOrbPart part) noexcept {
  return static_cast<std::int32_t>(part);
}

// Les diamètres, en pixels de sprite.
inline constexpr float kWandOrbCoreDiameterPixels = 12.0F;
inline constexpr float kWandOrbHaloDiameterPixels = 44.0F;
inline constexpr float kWandOrbSparkDiameterPixels = 9.0F;
inline constexpr float kWandOrbBurstStartDiameterPixels = 20.0F;
inline constexpr float kWandOrbBurstEndDiameterPixels = 76.0F;

// LE HALO PULSE : son diamètre oscille de cette part autour du sien, sur ce
// nombre d'images tenues chacune ce nombre de tics.
inline constexpr float kWandOrbHaloPulseShare = 0.15F;
inline constexpr std::int32_t kWandOrbHaloPulseFrames = 8;
inline constexpr std::int32_t kWandOrbHaloFrameHoldTicks = 2;

// LES ÉTINCELLES : une semée tous les tant de tics du vol, jamais plus que le
// plafond — c'est ce qui borne le coût d'un vol lent —, chacune vivant ses
// images d'un tic, de son diamètre à cette part de lui, à un écart tiré du
// point du vol où elle naît, en unités du monde.
inline constexpr std::int32_t kWandOrbSparkPeriodTicks = 3;
inline constexpr std::int32_t kWandOrbSparkCountMax = 12;
inline constexpr std::int32_t kWandOrbSparkFrames = 7;
inline constexpr float kWandOrbSparkEndShare = 0.3F;
inline constexpr float kWandOrbSparkJitterUnits = 1.5F;

// L'ÉCLAT D'IMPACT : sa durée, sous un plafond qu'elle ne dépasse jamais, et
// ses images d'un tic chacune.
inline constexpr float kWandOrbBurstCeilingMs = 250.0F;
inline constexpr float kWandOrbBurstMs = 192.0F;
static_assert(kWandOrbBurstMs < kWandOrbBurstCeilingMs);
inline constexpr std::int32_t kWandOrbBurstFrames =
    static_cast<std::int32_t>(kWandOrbBurstMs) / kEffectTickMs;

// Les alphas de départ, sur deux cent cinquante-cinq.
inline constexpr float kWandOrbCoreAlpha = 255.0F;
inline constexpr float kWandOrbHaloAlpha = 190.0F;
inline constexpr float kWandOrbSparkAlpha = 200.0F;
inline constexpr float kWandOrbBurstAlpha = 220.0F;

// LE SPRITE FAIT EN CODE : le disque pour seule image, et une action par pièce.
// Une étincelle et l'éclat ouvrent leur action sur une image VIDE, tenue tant
// que la pièce attend son tour : voir `WandOrbPiece::hidden_ticks`.
[[nodiscard]] LoadedActorSprite WandOrbSprite();

// UNE PIÈCE À SEMER : un nœud de sprite, ajouté au décor.
struct WandOrbPiece {
  WandOrbPart part = WandOrbPart::kCore;
  glm::vec3 birth{0.0F};
  // Où elle vole, en ligne droite et hauteur comprise, pendant toute sa vie ;
  // rien : elle reste où elle naît.
  std::optional<glm::vec3> flies_to;
  std::int32_t life_ticks = 1;
  // COMBIEN DE TICS ELLE ATTEND, invisible sur l'image vide de son action,
  // avant de se montrer : une étincelle, le passage de la boule ; l'éclat, son
  // arrivée.
  std::int32_t hidden_ticks = 0;
  std::int32_t frame_hold_ticks = 1;
  bool loops_frames = true;
  std::array<std::uint8_t, 3> rgb = {};
  float alpha = 0.0F;
  // En combien de tics, à la fin de sa vie, elle s'efface ; zéro : jamais.
  std::int32_t fade_out_ticks = 0;
  // Seul le cœur porte l'ombre d'un projectile : le reste est de la lumière.
  bool casts_shadow = false;
};

// LES PIÈCES D'UN TIR de `from` à `to` en `flight_ticks` tics, de cette
// couleur. Le cœur et le halo volent ensemble ; chaque étincelle naît au point
// où la boule passe à son tic, et ne bouge plus ; l'éclat attend l'arrivée.
// `generator` tire l'écart des étincelles.
[[nodiscard]] std::vector<WandOrbPiece> WandOrbPieces(glm::vec3 from, glm::vec3 to,
                                                      std::int32_t flight_ticks,
                                                      std::array<std::uint8_t, 3> rgb,
                                                      std::uint32_t& generator);

}  // namespace ro::render
