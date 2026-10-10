#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include "ro/formats/act.h"
#include "ro/formats/spr.h"
#include "ro/render/actor_sprite.h"

namespace ro::render {

// La bulle qu'un acteur fait paraître au-dessus de sa tête.
//
// Un seul sprite les porte toutes, une ACTION par émoticône. Partout ici,
// `emotion` est le numéro du PROTOCOLE — emotion_type, dans src/map/clif.hpp —
// et `EmotionAction` le traduit en action du sprite.
inline constexpr std::string_view kEmotionSpritePath = "data\\sprite\\이팩트\\emotion";

// L'action du sprite qui dessine ce numéro, ou rien s'il n'en a pas.
//
// 🔴 CE N'EST PAS LE NUMÉRO LUI-MÊME : les deux divergent de 4 à 14, puis à
// partir de 34. Voir la table dans la source.
[[nodiscard]] std::optional<std::size_t> EmotionAction(std::uint16_t emotion);

// Combien de temps une émoticône vit : son animation entière, jouée UNE fois.
// Zéro pour une émoticône que le sprite ne porte pas.
[[nodiscard]] float EmotionLifeMs(const formats::ActAnimation& animation, std::uint16_t emotion);

// L'émoticône à cet instant, en pixels de sprite autour de son ancre.
//
// Vide une fois l'animation finie : c'est ainsi qu'une bulle s'éteint, sans que
// rien n'ait à la retirer. L'écart qui la sépare de son ancre est celui du
// fichier ; c'est à l'appelant de poser cette ancre au sommet de son porteur.
[[nodiscard]] ComposedActor ComposeEmotion(const formats::SprSprite& sprite,
                                           const formats::ActAnimation& animation,
                                           std::uint16_t emotion, float elapsed_ms);

}  // namespace ro::render
