#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMEUR QUI MARCHE, sous-type 120 : le mouvement et ce qu'il fait naître.
//
// Trois effets le posent, 551, 555 et 1204. Il n'a AUCUNE image : c'est un
// porteur invisible dont la première piste marche en ligne droite vers la
// cible de l'instance, et dont la peinture fait naître un autre effet là où
// elle se trouve — 552 tous les huit tics pour l'octet zéro, 556 à CHAQUE
// peinture pour l'octet un, 1205 tous les huit tics pour l'octet quatre-vingt-dix-neuf.
//
// ⚠ SEULE LA PREMIÈRE PISTE COMPTE. Le mouvement fait marcher les quatre, et
// l'émission ne lit que la première.

// LES CASES DU SEMIS ET DU MOUVEMENT. Le CAP, en degrés entiers du client, va
// dans `EffectTrack::degrees` ; la place de la piste dans `EffectTrack::anchor`,
// dans NOTRE repère et en coordonnées du monde.
inline constexpr std::size_t kWalkingSowerSlotOfSpeed = 1;
inline constexpr std::size_t kWalkingSowerMarkOfByte = 0;

// LE PAS de la marche, en unités par tic.
inline constexpr float kWalkingSowerSpeed = 0.8F;
// LE TIC OÙ LE CAP SE PREND, vers la cible, une fois pour toutes.
inline constexpr std::int32_t kWalkingSowerHeadingTick = 1;

// LES TROIS OCTETS QUE LES SITES PASSENT, et l'effet que chacun fait naître.
inline constexpr std::uint8_t kWalkingSowerByteOfRings = 0;
inline constexpr std::uint8_t kWalkingSowerByteOfSparks = 1;
inline constexpr std::uint8_t kWalkingSowerByteOfBlueRings = 99;
inline constexpr std::uint32_t kWalkingSowerChildOfRings = 552;
inline constexpr std::uint32_t kWalkingSowerChildOfSparks = 556;
inline constexpr std::uint32_t kWalkingSowerChildOfBlueRings = 1205;
// La période des deux premiers : le compteur de la piste multiple de huit.
inline constexpr std::int32_t kWalkingSowerRingPeriodTicks = 8;

// LE MOUVEMENT DU SOUS-TYPE 120, d'un tic.
//
// Pour chaque piste allumée : le compteur avance, et tant qu'il n'est pas
// positif rien d'autre. Au PREMIER tic positif, le cap se prend du nœud vers la
// cible — `client_heading_to_target` —, et à chaque tic la piste avance d'un
// pas le long de ce cap.
void AdvanceWalkingSowerTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                               std::int16_t client_heading_to_target);

// L'EFFET QU'UNE PEINTURE DE CE NŒUD FAIT NAÎTRE, ou rien. Seule la première
// piste est lue.
//
// ⚠ UN COMPTEUR NÉGATIF N'EST PAS MULTIPLE DE HUIT, sauf zéro : la piste
// retardée ne sème pas avant son tic zéro.
[[nodiscard]] std::optional<std::uint32_t> WalkingSowerChildOf(
    const std::array<EffectTrack, kEffectTracksPerNode>& tracks);

}  // namespace ro::render
