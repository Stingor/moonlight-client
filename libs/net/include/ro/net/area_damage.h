#pragma once

#include <cstdint>

#include "ro/net/world_update.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// LES DÉGÂTS D'UN SORT DE ZONE, que le serveur nous adresse en privé.
//
// 🔴 SANS CE PAQUET, UN COMPTEUR RESTE À ZÉRO SUR STORM GUST. Le paquet
// ordinaire d'un coup porte l'identifiant de sa SOURCE, et pour une compétence
// posée au sol cette source est l'UNITÉ qui frappe, pas le lanceur : rien ne
// nous rattache le coup. Le serveur Moonlight ajoute donc
// `ZC_MOONLIGHT_SKILL_DMG` (0x0F09), qu'il n'envoie qu'au lanceur.
//
// ⚠ ET IL PORTE UNE SOMME, PAS UN COUP. Une frappe par paquet noierait la
// liaison — le serveur le dit en clair : sous une zone sur un gros paquet de
// créatures, cela fait des centaines de petits paquets par seconde et fige le
// client. Il accumule donc par lanceur et n'envoie qu'un total par cycle
// d'unités. Le compteur y gagne le bon total, et y perd le détail des coups :
// c'est le marché du protocole, pas une approximation de notre part.
[[nodiscard]] FrameOutcome ApplyAreaDamageFrame(const Frame& frame, sim::World& world,
                                                std::uint32_t now_ms);

}  // namespace ro::net
