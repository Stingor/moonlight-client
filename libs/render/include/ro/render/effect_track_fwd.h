#pragma once

#include <cstddef>
#include <cstdint>

namespace ro::render {

// LA PISTE NOMMÉE SANS ÊTRE DÉFINIE, et les dimensions qu'elle partage avec ceux
// qui la manipulent.
//
// Un en-tête qui ne fait que déclarer une fonction prenant une piste, ou les
// quatre pistes d'un nœud, s'en contente : la définition complète est dans
// `effect_track.h`, et seul le fichier qui lit ou écrit un champ l'inclut.

struct EffectTrack;

// Combien de cases une piste porte, en flottants comme en octets.
//
// Vingt et une, et ce compte a deux origines qui se répondent : la zone mesure
// quatre-vingt-quatre octets, et le mouvement de l'anneau balaie ses segments
// sur un DEMI-TOUR de sinus par pas de neuf degrés — zéro à cent quatre-vingts,
// bornes comprises. Changer le pas sans le compte déforme l'onde.
inline constexpr std::size_t kEffectTrackSlots = 21;

// Combien de cases une piste porte APRÈS son ancre, jusqu'à sa fin.
//
// Neuf, et ce compte se lit deux fois : cent quatre-vingts octets moins les cent
// quarante-quatre qui précèdent en font trente-six, soit neuf flottants ; et la
// mesure des cent onze dessinateurs en trouve exactement neuf lus, de cent
// quarante-quatre à cent soixante-seize, aucun au-delà. La piste est pleine.
inline constexpr std::size_t kEffectTrackTailSlots = 9;
inline constexpr std::int32_t kEffectTrackUndulationStepDegrees = 9;

// Combien de pistes un nœud porte.
inline constexpr std::size_t kEffectTracksPerNode = 4;

// LE PLAFOND DE L'ALPHA D'UNE PISTE, en entier.
//
// ⚠ DEUX CENT CINQUANTE-CINQ, ET NON DEUX CENT CINQUANTE-QUATRE : c'est
// l'alpha du NŒUD qui plafonne un cran plus bas, et les deux coexistent. Un
// mouvement qui monte l'alpha d'une piste s'arrête ici au lieu de reboucler.
inline constexpr std::int16_t kEffectTrackAlphaMax = 255;

// Le tour complet, en degrés — la piste range ses angles en entiers, comme le
// client, qui indexe une table de sinus par degré.
inline constexpr std::int32_t kEffectTrackFullTurnDegrees = 360;
// LE DEMI-TOUR, dont une figure au moins se sert pour prendre l'angle OPPOSÉ à
// son azimut : elle l'ajoute ou le retranche selon le côté où elle se trouve.
inline constexpr std::int32_t kEffectTrackHalfTurnDegrees = kEffectTrackFullTurnDegrees / 2;

}  // namespace ro::render
