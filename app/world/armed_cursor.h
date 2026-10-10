#pragma once

#include <cstdint>
#include <string>

#include "ro/render/cursor.h"
#include "ro/sim/actor.h"

namespace ro::sim {
class SkillTree;
}  // namespace ro::sim

namespace moonclient {

// Ce que le grimoire attend qu'on désigne, une fois une compétence choisie.
enum class SkillAim : std::uint8_t {
  // Rien n'attend : le clic va au monde comme d'habitude.
  kNothing,
  // Une compétence attend qu'on désigne QUELQU'UN.
  kTarget,
  // Une compétence attend qu'on désigne une CASE.
  kGround,
  // Une compétence attend qu'on désigne un PIÈGE POSÉ — une unité au sol.
  //
  // ⚠ LE CLIENT L'ARME DANS UN MODE À PART, ni celui du sol ni celui d'une
  // cible : seul le clic sur une unité au sol la lance. Un clic sur un monstre
  // ou un joueur la désarme sans rien faire, un clic au sol la désarme et fait
  // MARCHER vers la case.
  kTrap,
};

// LA FORME DU CURSEUR QUAND UNE COMPÉTENCE ATTEND SA CIBLE.
//
// 🔴 C'EST L'ANNEAU, ET JAMAIS L'ÉPÉE. L'épée annonce le coup ORDINAIRE, celui
// que l'armement vient justement de suspendre : la montrer promet le geste
// qu'on a désactivé. Le client ne la pose nulle part tant qu'une compétence
// attend.
//
// ⚠ ET IL EN A DEUX, dix et onze — le MÊME anneau, celui de onze tournant deux
// fois plus vite. C'est l'accélération qui dit « celle-là est une cible » ;
// l'anneau lent dit seulement « désigne ».
//
// ⚠ `otherwise` EST CE QU'ON MONTRERAIT SANS ARMEMENT, et il se calcule de toute
// façon : la forme ordinaire reste la réponse dans le cas ordinaire, qui est
// aussi le plus fréquent. C'est aussi lui qui CLASSE ce qui est sous la souris,
// comme les branches par catégorie du client.
[[nodiscard]] ro::render::CursorShape ArmedCursor(SkillAim aim, const ro::sim::Actor* hovered,
                                                  ro::render::CursorShape otherwise);

// LE RANG AUQUEL LA COMPÉTENCE ARMÉE PARTIRA, APRÈS UN CRAN DE MOLETTE.
//
// 🔴 LA MOLETTE NE ZOOME PLUS TANT QU'UNE COMPÉTENCE EST ARMÉE. Le routeur de
// molette du client s'arrête net dès que le mode de ciblage est posé, AVANT
// toute autre question — avant même de demander si le rang de celle-ci se
// règle. La rendre à la caméra pour une compétence qui ne se dose pas ferait
// reculer la vue au milieu d'une visée.
//
// ⚠ UN CRAN PAR IMAGE, ET LE SIGNE SEUL COMPTE. Le client ne lit que le SIGNE
// du cumul de l'image et envoie son message avec un pas de UN : trois crans
// d'un coup montent d'un rang, pas de trois.
//
// ⚠ ET TOUT NE SE DOSE PAS : `level_matters` dit si la compétence figure sur la
// liste blanche que portent les tables du client. Sans elle, le rang changerait
// sous le joueur là où il ne veut rien dire.
//
// Les bornes sont celles du client : le rang APPRIS en haut, UN en bas. Un rang
// appris nul ne borne rien : la compétence n'est pas au grimoire, et il n'y a
// rien à régler.
[[nodiscard]] std::uint16_t ArmedLevelAfterWheel(std::uint16_t armed_level,
                                                 std::uint16_t learned_level, bool level_matters,
                                                 float wheel_steps);

// CE QUE L'ÉTIQUETTE DE CIBLAGE ÉCRIT : la compétence, et le rang qui partira.
//
// Le client pose là une bulle de texte tant qu'une compétence attend sa cible
// et n'y met QUE le nom affiché des tables : son rang, lui, se dessine en
// chiffres au CURSEUR — voir `ro/render/armed_skill.h`.
//
// `with_level` ajoute le rang derrière le nom. Qui garde les deux affichages ne
// veut pas forcément le rang deux fois, et c'est un réglage.
//
// ⚠ SANS TABLES DU CLIENT, ON DIT LE NUMÉRO, comme la bannière d'annonce : un
// bandeau sans nom ressemblerait à une compétence sans nom, et les deux se
// corrigent à des endroits opposés.
[[nodiscard]] std::string ArmedBannerText(const ro::sim::SkillTree* skills, std::uint16_t skill_id,
                                          std::uint16_t level, bool with_level);

}  // namespace moonclient
