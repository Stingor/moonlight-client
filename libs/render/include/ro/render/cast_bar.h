#pragma once

#include <cstdint>

namespace ro::render {

// LA RÈGLE DE LA BARRE D'INCANTATION : quand une compétence en montre une.
//
// La barre elle-même se peint dans l'interface — voir `ro/ui/cast_bars.h`. Ce
// qui vient d'ici, c'est ce que SkillImpl répond quand on lui demande si une
// incantation en pose une.

// Les deux compétences qui n'ont PAS de barre, quelle que soit leur durée.
//
// Le client les nomme par leur numéro, et rien ne les distingue par ailleurs :
// ce sont Brandish Spear et Bowling Bash, dont le temps annoncé est un délai
// d'après-coup et non une incantation. C'est SkillImpl qui applique la
// règle ; leurs numéros vivent ici parce que c'est ici qu'ils veulent dire
// quelque chose.
inline constexpr std::uint16_t kBrandishSpearSkill = 57;
inline constexpr std::uint16_t kBowlingBashSkill = 62;

}  // namespace ro::render
