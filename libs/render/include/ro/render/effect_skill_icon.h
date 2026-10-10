#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace ro::render {

// L'ICÔNE D'UNE COMPÉTENCE, et le chemin de son image.
//
// ⚠ CE NOM DE FICHIER N'EXISTE NI DANS L'EXÉCUTABLE NI AU SITE D'APPEL. Le
// client le COMPOSE à l'exécution : il demande au Lua le nom de la compétence —
// « MG_FIREBOLT » —, le met dans le format `유저인터페이스\item\%s.bmp`, et charge
// le fichier ainsi formé. Le code du client ne dit donc pas quelle image il
// montre : seuls le Lua et l'archive le disent.
//
// ⚠ ET CHAQUE CHEMIN A ÉTÉ CONFRONTÉ À L'ARCHIVE. Une table de noms peut être
// juste et les images absentes ; l'effet serait alors muet et rien ne le dirait.
// Le relevé n'écrit une entrée que s'il a trouvé le fichier — voir
// `scripts/releve_icones_de_competence.py`.
struct EffectSkillIcon {
  std::uint32_t skill_id = 0;
  // LE NOM QUE LE LUA DU CLIENT DONNE à cette compétence. Il TRACE : d'une icône
  // qui ne s'affiche pas, on remonte à la table du client sans rouvrir l'archive.
  std::string_view skill_name;
  std::string_view texture;
};

// Toutes les icônes relevées, triées par identifiant de compétence.
[[nodiscard]] std::span<const EffectSkillIcon> EffectSkillIconTable();

// L'image d'une compétence, ou une vue VIDE quand la table ne la porte pas.
//
// ⚠ LA TABLE NE PORTE PAS TOUTES LES COMPÉTENCES DU JEU, et ce n'est pas un
// manque : elle porte celles que les EFFETS montrent, c'est-à-dire les deux
// plages que le tirage du client couvre et la compétence que sa seconde branche
// nomme. En relever deux mille pour en montrer vingt-six serait une table que
// personne ne lit.
[[nodiscard]] std::string_view EffectSkillIconFor(std::uint32_t skill_id);

}  // namespace ro::render
