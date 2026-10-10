#pragma once

#include <cstdint>
#include <span>

#include "ro/render/effect_frame.h"

namespace ro::render {

// COMBIEN DE TICS UN EFFET VIT — la table que le client garnit au démarrage.
//
// 🔴 CE N'EST PAS LA MÊME QUESTION QUE `effect_life_table.h`. Celle-là répond
// « quelle vie un SEMIS donne à son NŒUD », et ne porte donc que les bâtisseurs
// de l'espèce « copie » ; celle-ci porte la durée de l'EFFET lui-même, pour les
// 2 424 identifiants relevés. Les deux coïncident pour l'espèce « copie » et
// divergent partout ailleurs — confondre les deux donne un nombre plausible,
// c'est-à-dire le pire genre de faux.
//
// Le client ne la porte dans aucun fichier : il la garnit au démarrage. Voir
// l'ADR 0042.
//
// ⚠ ET C'EST CE NOMBRE QUI DÉCIDE DE LA RELANCE D'UN VISUEL D'UNITÉ AU SOL.
// À chaque tic, le compteur de l'unité est comparé à
// `durée(effet) − retrait` et repose l'effet quand il l'atteint — QUOI QUE
// L'EFFET CONTIENNE. Prendre la longueur du fichier `.str` à la place fait
// rejouer Safety Wall une fois par seconde là où le client le pose UNE FOIS.
struct EffectDurationEntry {
  std::uint32_t effect_id = 0;
  std::int32_t duration_ticks = 0;
};

// La table entière, triée par identifiant.
[[nodiscard]] std::span<const EffectDurationEntry> EffectDurationTable();

// LA DURÉE DE CET EFFET EST-ELLE RELEVÉE ?
//
// ⚠ À DEMANDER AVANT `EffectDurationTicks`, qui rend zéro pour ce qu'elle ne
// connaît pas. Un zéro n'est pas une durée courte : c'est l'absence de mesure,
// et le confondre avec une mesure fait relancer un effet à chaque image.
[[nodiscard]] bool EffectDurationIsKnown(std::uint32_t effect_id);

// La durée relevée, ou ZÉRO pour un identifiant que la table ne porte pas.
[[nodiscard]] std::int32_t EffectDurationTicks(std::uint32_t effect_id);

// AU BOUT DE COMBIEN DE MILLISECONDES UN VISUEL D'UNITÉ AU SOL SE REPOSE.
//
// C'est la règle du client, mot pour mot : `durée(effet) − retrait`, et le
// retrait est un RECOUVREMENT — il fait repartir l'effet quelques tics avant sa
// fin pour qu'aucune image ne montre le sol nu.
//
// 🔴 ZÉRO VEUT DIRE « ON NE RELANCE PAS », et c'est la réponse pour un effet
// dont la durée n'est pas relevée. Un plancher à un tic resèmerait à chaque
// image ; ne pas relancer du tout laisse l'unité montrer son effet une fois,
// ce qui se voit et se corrige.
[[nodiscard]] std::uint32_t SkillUnitRespawnMs(std::uint32_t effect_id,
                                               std::uint8_t withhold_ticks);

}  // namespace ro::render
