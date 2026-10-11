#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "ro/core/error.h"
#include "ro/sim/skill_book.h"
#include "ro/sim/skill_tree.h"
#include "ro/sim/world_fwd.h"

namespace moonclient {

// CE QUE LE GRIMOIRE ÉCRIT DANS LA CONVERSATION quand le joueur arme ou lance
// un skill, et quand la demande ne part pas.
//
// ⚠ L'ARMEMENT ET LE LANCER ONT LEUR CASE À EUX, « Mes skills (visée et
// lancer) » : chaque sort en écrit une ligne, si bien qu'un combat en aligne
// des dizaines. La case est décochée d'office ; le joueur qui veut ce journal
// la coche. Un ÉCHEC D'ENVOI, lui, garde la case d'un message du serveur : il
// est rare, et c'est justement celui qu'on doit lire.

// LE NOM QU'UN SKILL MONTRE AU JOUEUR : celui des tables du client — le même
// que la fenêtre des skills et la barre de raccourcis —, sinon le nom interne
// que le serveur envoie, sinon son numéro.
//
// `node` peut être nul : le serveur accorde des skills que les tables ne
// portent pas, et une installation peut n'avoir aucune table.
[[nodiscard]] std::string SkillShownName(const ro::sim::SkillNode* node,
                                         const ro::sim::KnownSkill& skill);

// « Nom : choisissez une cible. », une case, ou un piège, selon ce que le
// skill vise.
[[nodiscard]] std::string SkillArmedLine(std::string_view shown_name,
                                         const ro::sim::KnownSkill& skill);

// « Nom lancé. »
[[nodiscard]] std::string SkillLaunchedLine(std::string_view shown_name);

// La demande de lancer n'est pas partie, et pourquoi.
[[nodiscard]] std::string SkillNotSentLine(std::string_view shown_name, const ro::core::Error& why);

// La marche d'approche vers la cible n'est pas partie, et pourquoi.
[[nodiscard]] std::string SkillApproachNotSentLine(std::string_view shown_name,
                                                   const ro::core::Error& why);

// Écrit une ligne d'armement ou de lancer, dans la case « Mes skills ». Le
// locuteur est ZÉRO : elle ne vient de personne.
void RecordSkillUse(ro::sim::World& world, std::string text, std::uint32_t now_ms);

}  // namespace moonclient
