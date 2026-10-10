#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace ro::render {

// Où le client range ses sons. Les chemins des tables et ceux que portent les
// animations sont relatifs à ce dossier.
inline constexpr std::string_view kSoundFolder = "data\\wav\\";

// Une entrée de la table des sons de compétence.
struct SkillSoundEntry {
  std::uint16_t skill_id = 0;
  // Le chemin du fichier, relatif au dossier des sons du client, écrit comme
  // dans l'archive.
  std::string_view sound;
};

// La table entière, triée par identifiant de compétence.
[[nodiscard]] std::span<const SkillSoundEntry> SkillSoundTable();

// Le son d'une compétence, ou une chaîne VIDE pour une compétence qui n'en a
// pas dans cette table.
//
// ⚠ Le silence n'y veut pas dire que la compétence est muette, et la raison
// n'est PAS l'animation. Le client range le bruit des compétences classiques
// sous leur EFFET, pas sous elles : voir EffectSound, et demander celui-ci
// quand cette table ne répond rien.
[[nodiscard]] std::string_view SkillSound(std::uint32_t skill_id);

}  // namespace ro::render
