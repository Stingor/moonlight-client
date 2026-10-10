#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace ro::render {

// Une entrée de la table des bruits de classe : l'INDEX du vecteur du client,
// et le fichier qu'il y range.
//
// ⚠ L'index n'est pas la classe — voir `ClassHitSound`, qui fait la
// conversion.
struct ClassHitSoundEntry {
  std::uint16_t index = 0;
  std::string_view sound;
};

// LE BRUIT QUE LE CLIENT DONNE À LA QUASI-TOTALITÉ DES CLASSES, et qu'on entend
// donc à chaque coup porté sur un joueur. Le vecteur du client naît rempli de
// celui-ci, et seules quatre-vingt-quatre entrées en portent un autre.
inline constexpr std::string_view kDefaultClassHitSound = "player_clothes.wav";

// Les seules entrées qui s'écartent du défaut, triées par index.
[[nodiscard]] std::span<const ClassHitSoundEntry> ClassHitSoundTable();

}  // namespace ro::render
