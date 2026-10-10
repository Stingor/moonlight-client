#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace ro::render {

// LA TEINTE D'UN RUBAN, par variante.
//
// La famille procédurale sort ses quads par trois primitives, et celle du ruban
// en sert quinze dessinateurs. Elle ne reçoit pas de couleur : elle la CHOISIT, dans un switch
// de cent un cas sur un champ du nœud, et quarante-quatre de ces cas posent
// autre chose que du blanc.
//
// La teinte n'appartient donc ni au nœud ni à la figure : elle appartient à la
// primitive, et une figure de cette famille qui la peindrait elle-même
// montrerait la mauvaise couleur à quarante-quatre effets.
//
// ⚠ LA TEINTE DU NŒUD ÉCRASE CELLE DE LA VARIANTE quand elle est posée. Le
// client lit trois champs de son propriétaire et ne garde la variante que pour
// ceux qui valent moins de zéro. Ce qui est ici est donc un DÉFAUT, pas un
// verdict.

// Ce qu'une variante pose. Une variante absente de la table laisse le blanc et
// le mélange ordinaire — c'est le cas de la majorité.
struct EffectTrailVariant {
  std::uint8_t variant = 0;
  std::array<std::uint8_t, 3> color = {255, 255, 255};
  // L'alpha que le cas impose au ruban, ou ZÉRO quand il laisse celui de la
  // piste. Une seule variante en pose un, et elle le pose bas.
  std::uint8_t forced_alpha = 0;
  // Le facteur de mélange de destination, dans la numérotation de Direct3D.
  // Deux et six ne diffèrent pas d'un cheveu : l'un ajoute, l'autre garde ce
  // qui est derrière proportionnellement à l'alpha de la source.
  std::int32_t blend_destination = 0;
};

// Ce qu'une variante vaut quand son cas ne pose rien.
[[nodiscard]] EffectTrailVariant NeutralEffectTrailVariant();

// La table entière, triée par numéro de variante — pour un test qui la passe
// en revue, jamais pour une recherche linéaire.
[[nodiscard]] std::span<const EffectTrailVariant> EffectTrailVariantTable();

// Ce que la primitive poserait pour ce numéro. Rend le neutre pour un numéro
// que la table ne porte pas, comme le défaut du switch.
[[nodiscard]] EffectTrailVariant EffectTrailVariantFor(std::uint8_t variant);

// ⚠ UNE TEINTE D'INSTANCE ÉCRASE CETTE PALETTE, ET NOUS NE LA PORTONS PAS.
//
// La primitive du ruban remonte du nœud à son PROPRIÉTAIRE, y lit trois octets
// de teinte et un mode de mélange, et chacun remplace la valeur de la variante — sauf à valoir
// MOINS UN, qui veut dire « garde la tienne ». Les deux autres primitives, le quad de particule
// projeté et le ruban RVB, lisent les mêmes quatre champs.
//
// ⚠ CE N'EST PAS UN BÂTISSEUR QUI L'ÉCRIT, et c'est ce qui décide de la portée
// du manque : les deux seules fonctions qui posent ces champs sont le
// constructeur d'instance — qui les met à moins un — et la voie des effets
// d'APPARENCE d'un acteur. Un effet ordinaire garde donc sa variante, et
// l'anneau de EF_HEAL4 est blanc parce que le client le veut blanc.
//
// Ce qui manque est donc la teinte que porte un acteur, pas celle d'un effet.
// Le jour où l'on portera les effets d'apparence, c'est ici que leur couleur
// devra passer.

}  // namespace ro::render
