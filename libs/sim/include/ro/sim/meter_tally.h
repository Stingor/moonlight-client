#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>

namespace ro::sim {

// CE QUE LE COMPTEUR MESURE. Les quatre colonnes de Skada et de Recount : ce
// qu'on inflige, ce qu'on soigne, ce qu'on encaisse, ce qu'on reçoit.
enum class MeterMode : std::uint8_t {
  kDamageDone = 0,
  kHealingDone,
  kDamageTaken,
  kHealingTaken,
  kCount,
};

inline constexpr std::size_t kMeterModeCount = static_cast<std::size_t>(MeterMode::kCount);

[[nodiscard]] constexpr std::size_t Rank(MeterMode mode) noexcept {
  return static_cast<std::size_t>(mode);
}

// Les DÉGÂTS, infligés ou subis, ouvrent un combat ; les soins s'y rangent.
[[nodiscard]] constexpr bool IsDamage(MeterMode mode) noexcept {
  return mode == MeterMode::kDamageDone || mode == MeterMode::kDamageTaken;
}

// ── LES CLÉS DE VENTILATION QUI NE SONT PAS DES COMPÉTENCES ─────────────────
// Zéro est le coup ordinaire. Ces deux-là sont hors de la numérotation des
// compétences, qui ne monte pas si haut.
//
// La somme d'un battement de sort de zone, que le serveur envoie au lanceur
// seul, sans dire de quelle compétence.
inline constexpr std::uint16_t kAreaCycleKey = 0xFFFF;
// Ce que rend une récupération — régénération des compétences, vol de vie,
// Kaahi… : le serveur ne le dit qu'à nous, et sans nommer de compétence.
inline constexpr std::uint16_t kRecoveryKey = 0xFFFE;

// UN FAIT DE COMBAT TEL QUE LE COMPTEUR LE RETIENT.
struct MeterBlow {
  MeterMode mode = MeterMode::kDamageDone;
  // Celui qu'il crédite : l'auteur pour ce qu'on inflige ou soigne, la cible
  // pour ce qu'on encaisse ou reçoit.
  std::uint32_t who = 0;
  // ⚠ SUR SOIXANTE-QUATRE BITS : les deux mains additionnées dépassent un
  // entier 32 bits signé.
  std::int64_t amount = 0;
  // La compétence, zéro pour un coup ordinaire, ou l'une des clés ci-dessus.
  std::uint16_t skill_id = 0;
  std::uint16_t skill_level = 0;
  // Le nombre de coups que le total réunit — un Double Strafe en fait deux.
  std::uint16_t hits = 1;
  bool critical = false;
  // L'AUTRE BOUT : la cible de ce qu'on inflige ou soigne, la source de ce
  // qu'on encaisse ou reçoit. ⚠ SON NOM EST RELEVÉ AU COUP : l'acteur peut
  // disparaître, et son nom avec lui.
  std::uint32_t other_id = 0;
  std::string other_name;
};

// LE COMPTE D'UNE COMPÉTENCE, ou de tout un acteur.
struct MeterTally {
  std::int64_t amount = 0;
  // Les faits comptés — un Double Strafe en est un.
  std::uint32_t events = 0;
  std::uint32_t critical = 0;
  std::int64_t biggest = 0;

  void Add(const MeterBlow& blow);
};

// CE QU'UN ACTEUR A FAIT DANS UN MODE : son total, et sa ventilation.
struct MeterActorTally {
  std::string name;
  MeterTally total;
  std::map<std::uint16_t, MeterTally> skills;
};

// UN SEGMENT : un combat, ou toute la session.
struct MeterSegment {
  // Le temps ACTIF qu'il couvre, en millisecondes — ce qui divise ses totaux.
  // Pour un combat, du premier au dernier coup ; pour la session, la somme
  // des combats.
  std::uint32_t active_ms = 0;
  std::array<std::map<std::uint32_t, MeterActorTally>, kMeterModeCount> modes;

  void Add(const MeterBlow& blow, std::string_view who_name);
  [[nodiscard]] bool empty() const noexcept;
  [[nodiscard]] const std::map<std::uint32_t, MeterActorTally>& of(MeterMode mode) const {
    return modes[Rank(mode)];
  }
};

}  // namespace ro::sim
