#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace ro::proto {

// Ce que le découpage a pu conclure sur les octets présents.
enum class FrameStatus : std::uint8_t {
  // Une trame entière est là.
  kComplete,
  // Pas encore assez d'octets. Ce n'est PAS une erreur : un flux TCP arrive par
  // morceaux qui n'ont aucun rapport avec les limites des paquets.
  kIncomplete,
  // L'opcode n'est pas dans la table. Impossible de savoir combien d'octets
  // consommer, donc impossible de reprendre le flux : la session est perdue.
  kUnknownOpcode,
  // La trame annonce une longueur impossible. Même conséquence.
  kCorrupt,
};

// Le résultat d'une tentative de découpage.
struct FrameResult {
  FrameStatus status = FrameStatus::kIncomplete;
  // L'opcode lu, dès qu'il a pu l'être. Renseigné même quand la trame est
  // incomplète : c'est ce qui permet de dire QUEL paquet on attend.
  std::uint16_t opcode = 0;
  // Taille TOTALE de la trame, opcode compris. Nulle tant qu'elle est
  // inconnue.
  std::size_t size = 0;
};

// Nombre d'octets de l'opcode, en tête de chaque trame.
inline constexpr std::size_t kOpcodeBytes = 2;

// Taille minimale d'une trame à longueur variable : l'opcode, puis les deux
// octets qui portent la longueur totale.
inline constexpr std::size_t kVariableHeaderBytes = 4;

// Découpe la PREMIÈRE trame d'un tampon reçu.
//
// C'est tout ce que le fil réseau a le droit de faire : lire l'opcode, en
// déduire la longueur, et dire si les octets sont là. Il ne décode rien — le
// décodage a lieu sur le fil principal, une fois par tour de boucle. Il n'y a
// donc aucune règle à retenir sur ce qui est sûr d'être touché depuis quel
// fil : le fil réseau ne touche à rien.
//
// ⚠ Un opcode inconnu N'EST PAS une trame à ignorer. Sans sa longueur, on ne
// sait pas de combien avancer ; sauter « au suivant » reviendrait à chercher un
// opcode au milieu d'un corps de paquet, et à inventer des trames à partir de
// données. La seule issue est de fermer la session.
[[nodiscard]] FrameResult NextFrame(std::span<const std::uint8_t> buffer);

}  // namespace ro::proto
