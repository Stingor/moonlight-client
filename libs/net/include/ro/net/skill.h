#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/packed_position.h"
#include "ro/sim/skill_book.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// Les COMPÉTENCES : ce que le serveur dit qu'on sait faire, et les deux
// demandes qui servent à le faire.
//
// Comme pour la marche et les coups, LE SERVEUR EST AUTORITAIRE : lancer ne
// change rien au monde ici. Le serveur répond par ce qu'il annonce à tout le
// monde — une incantation, un coup, une unité posée — ou par rien du tout
// quand il refuse : pas assez de points de magie, cible hors de portée, temps
// d'attente non écoulé. L'erreur rendue ne concerne que l'ÉMISSION.

// « Je lance cette compétence sur celui-ci. »
//
// C'est aussi par là que passe une compétence lancée SUR SOI : le serveur
// remplace alors la cible par le demandeur, quoi qu'on lui écrive. Une
// compétence qui vise une CASE, en revanche, est écartée d'entrée et sans un
// mot ; voir RequestSkillOnGround.
[[nodiscard]] core::Result<void> RequestSkillOnTarget(Connection& link, std::uint16_t skill_id,
                                                      std::uint16_t skill_level,
                                                      std::uint32_t target_id);

// « Je lance cette compétence sur cette case. » — une zone, un mur, un piège.
//
// Le serveur ne rapproche personne : une case hors de portée ne donne rien.
// MONTER UN RANG. ⚠ UN APPEL PAR RANG : le paquet ne porte pas de niveau, et le
// serveur revalide chaque montée. C'est ce que fait le client.
[[nodiscard]] core::Result<void> RequestSkillUp(Connection& link, std::uint16_t skill_id);

[[nodiscard]] core::Result<void> RequestSkillOnGround(Connection& link, std::uint16_t skill_id,
                                                      std::uint16_t skill_level,
                                                      sim::CellPosition where);

// « Je pose cette compétence sur cette case, avec ce texte. » — Talkie Box.
//
// `text` est en UTF-8, comme tout ce que le joueur tape ; il part dans la page
// du jeu, coupé à ce que le paquet porte.
[[nodiscard]] core::Result<void> RequestSkillOnGroundWithText(Connection& link,
                                                              std::uint16_t skill_id,
                                                              std::uint16_t skill_level,
                                                              sim::CellPosition where,
                                                              std::string_view text);

// Applique au grimoire une trame qui parle des compétences apprises.
//
// Cinq paquets l'alimentent : la LISTE entière, qui arrive à chaque entrée sur
// une carte ; l'AJOUT d'une compétence seule, qu'un objet peut donner ; sa MISE
// À JOUR, quand un rang monte ; son RETRAIT ; et le TEMPS D'ATTENTE que le
// serveur pose après un lancement réussi.
//
// ⚠ La mise à jour ne porte PAS le nom de la compétence : voir SkillBook::Learn,
// qui garde celui qu'on avait plutôt que de laisser une ligne sans titre.
//
// Cette fonction vit à part d'ApplyFrame parce que le grimoire n'est pas le
// monde : il ne parle ni d'acteurs ni de cases.
//
// `now_ms` est l'horloge de l'APPELANT, et ne sert qu'à dater une échéance :
// tout le reste de ce que porte le grimoire est hors du temps.
[[nodiscard]] FrameOutcome ApplySkillFrame(const Frame& frame, sim::SkillBook& book,
                                           std::uint32_t now_ms);

// L'ISSUE D'UN `ZC_ACK_TOUSESKILL` (0x0110) QUI DIT UN ÉCHEC. Le même paquet
// peut aussi annoncer qu'un skill est bien parti.
inline constexpr std::uint8_t kSkillUseFailed = 0;

// LE SKILL QUE CETTE TRAME DIT REFUSÉ : un `ZC_ACK_TOUSESKILL` en échec. Vide
// pour toute autre trame. Le serveur l'adresse au maître pour un skill de
// compagnon aussi, avec l'identifiant du skill.
[[nodiscard]] std::optional<std::uint16_t> RefusedSkillOf(const Frame& frame);

}  // namespace ro::net
