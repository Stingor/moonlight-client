#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ro/formats/act.h"
#include "ro/formats/spr.h"
#include "ro/render/actor_sprite.h"

// LES BULLES DU SOMMEIL : le sprite-enfant qui naît au-dessus de tout acteur qui
// s'endort — l'état du corps ou le sommeil profond —, et meurt à son réveil.
//
// Il suit son porteur à vingt unités au-dessus de ses pieds, quelle que soit
// sa taille, sans ombre, et se peint en avant de toute la scène : ni le décor
// ni un corps ne le cachent. Son animation tourne sans fin, à la cadence que
// son fichier déclare.
namespace ro::render {

inline constexpr std::string_view kSleepBubblesSpritePath = "data\\sprite\\이팩트\\Status-sleep";

// De combien les bulles flottent au-dessus des pieds de leur porteur, en
// unités du monde.
inline constexpr float kSleepBubblesRiseUnits = 20.0F;

// L'action que les bulles jouent : la première, vue de la direction zéro.
inline constexpr std::size_t kSleepBubblesAction = 0;

// L'opacité des bulles quand le joueur rend les invocations translucides : le
// même réglage, et la même valeur, que pour tout sprite-enfant.
inline constexpr std::uint8_t kSleepBubblesTranslucentAlpha = 50;
inline constexpr std::uint8_t kSleepBubblesOpaqueAlpha = 255;

// Les bulles, `asleep_ms` après l'endormissement : l'action zéro, image après
// image à sa cadence déclarée — jamais sous un tic —, en boucle. Vide pour un
// fichier sans action zéro.
[[nodiscard]] ComposedActor ComposeSleepBubbles(const formats::SprSprite& sprite,
                                                const formats::ActAnimation& animation,
                                                float asleep_ms);

// LA MÊME TENUE POUR UNE AUTRE ACTION : un sprite-enfant qui se tient au-dessus
// de son porteur tant qu'un état dure, `action_index` image après image à sa
// cadence déclarée, en boucle. Les points du silence la partagent. Vide pour
// une action que le fichier ne porte pas.
[[nodiscard]] ComposedActor ComposeHeldChildSprite(const formats::SprSprite& sprite,
                                                   const formats::ActAnimation& animation,
                                                   std::size_t action_index, float held_ms);

}  // namespace ro::render
