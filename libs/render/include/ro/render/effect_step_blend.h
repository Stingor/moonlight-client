#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_screen_quad.h"

// LE LISSAGE DES EFFETS ENTRE DEUX PAS FIXES.
//
// Les effets avancent par pas de seize millisecondes (`kEffectTickMs`). Sur un
// écran plus rapide que le pas, plusieurs images montrent le même état, puis
// l'effet saute au suivant. Le lissage montre, entre deux pas, un état MÊLÉ :
// celui du pas précédent vers celui du pas courant, dans la proportion du pas
// déjà écoulé. La simulation n'en sait rien : elle fait les mêmes pas, tire le
// même aléa et rend le même état ; seul ce qu'on peint change.
//
// 🔴 CE QUI EST CONTINU SE MÊLE, CE QUI EST DISCRET SUIT LE PAS COURANT. Les
// coins (donc la place, la rotation et l'échelle d'un quad) et la teinte (donc
// l'alpha) se mêlent. L'image d'une texture, ses coordonnées de texture, son
// mélange et sa profondeur sont ceux du pas courant : mêler deux images d'une
// planche peindrait la frontière entre elles. Une figure qui naît au pas
// courant se montre telle quelle, une figure morte a déjà disparu, et deux
// pas dont la figure n'a pas la même forme — pas le même nombre de quads, pas
// la même image — ne se mêlent pas : le pas courant s'affiche seul. Une
// particule qui renaît dans un emplacement recyclé n'est pas celle d'avant :
// son étiquette change, et elle ne se mêle pas (`BlendTaggedEffectWorldQuads`).
// L'image d'un fichier `.str` reste celle du pas courant ; seule sa
// déformation se place entre deux images (`EffectLayerAt`).
//
// QUI LISSE : la troupe procédurale, ses nœuds d'écran et sa grille de visée,
// les classes d'effets, les effets de la carte (`MapEffects::SetSmoothSteps`)
// et les fichiers `.str`.
namespace ro::render {

// La part du pas écoulée depuis le dernier pas, dans [0, 1] : zéro montre le pas
// précédent, un le pas courant. `owed_ms` est le reste d'un pas entamé.
[[nodiscard]] float EffectStepAlpha(std::uint32_t owed_ms) noexcept;

// La part d'image d'un fichier `.str` écoulée après `elapsed_ms`, dans [0, 1[ :
// ce qui reste de la division par le pas. Zéro pour une durée nulle ou négative.
[[nodiscard]] float EffectFrameFraction(float elapsed_ms) noexcept;

// Un point du pas précédent, déplacé de `previous_shift`, mêlé à celui du pas
// courant. Le déplacement est celui que le porteur a fait entre les deux : un
// effet accroché qui marche ne doit pas traîner derrière lui.
[[nodiscard]] glm::vec3 BlendEffectPoint(glm::vec3 previous, glm::vec3 previous_shift,
                                         glm::vec3 current, float alpha) noexcept;

// Deux teintes mêlées, canal par canal, arrondies.
[[nodiscard]] std::array<std::uint8_t, 4> BlendEffectTint(
    const std::array<std::uint8_t, 4>& previous, const std::array<std::uint8_t, 4>& current,
    float alpha) noexcept;

// Mêle un quad du pas précédent au quad du pas courant, EN PLACE : coins, point
// de recentrage et teinte. Le reste est celui du pas courant.
void BlendEffectWorldQuad(const EffectWorldQuad& previous, glm::vec3 previous_shift, float alpha,
                          EffectWorldQuad& current) noexcept;

// Mêle un lot entier, quad à quad. FAUX, et le lot courant intact, quand les
// deux lots n'ont pas la même forme : pas le même nombre de quads, ou un quad
// qui ne peint pas la même image.
bool BlendEffectWorldQuads(std::span<const EffectWorldQuad> previous, glm::vec3 previous_shift,
                           float alpha, std::span<EffectWorldQuad> current) noexcept;

// Mêle les quads d'un lot ÉTIQUETÉ, un par particule : seuls se mêlent deux
// quads de même étiquette, et un quad sans pareil s'affiche tel quel. Une
// particule née ou morte au dernier pas change d'étiquette, et ne traîne donc
// jamais de son ancienne place à la nouvelle. Les deux listes sont rangées par
// étiquette croissante. Rend le nombre de quads mêlés.
std::size_t BlendTaggedEffectWorldQuads(std::span<const EffectWorldQuad> previous,
                                        std::span<const std::uint64_t> previous_tags,
                                        glm::vec3 previous_shift, float alpha,
                                        std::span<EffectWorldQuad> current,
                                        std::span<const std::uint64_t> current_tags) noexcept;

// Un nombre mêlé : une échelle, un alpha, une hauteur.
[[nodiscard]] float MixEffectValue(float previous, float current, float alpha) noexcept;

// Un angle mêlé par le PLUS COURT CHEMIN : de 359 à 1 degré, on passe par 0 et
// non par 180.
[[nodiscard]] float MixEffectDegrees(float previous, float current, float alpha) noexcept;

// Les mêmes règles pour les quads d'ÉCRAN, en pixels : coins et teinte se
// mêlent, le décalage est celui du porteur à l'écran. FAUX, et le lot courant
// intact, quand les formes diffèrent — nombre, image ou mélange.
bool BlendEffectScreenQuads(std::span<const EffectScreenQuad> previous, glm::vec2 previous_shift,
                            float alpha, std::span<EffectScreenQuad> current) noexcept;

}  // namespace ro::render
