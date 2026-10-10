#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "ro/core/error.h"

namespace ro::proto {

// ── LES PAQUETS DE L'HOMONCULE QUE LE GÉNÉRATEUR N'A PAS ────────────────────
//
// Le changement d'état (`ZcChangestateMer`), les compétences
// (`ZcHoskillinfoList`, `ZcHoskillinfoUpdate`) et l'accusé de nourrissage
// (`ZcFeedMer`) sont engendrés ; la montée d'une compétence est
// `SkillUpRequest`, et l'auto-alimentation un `ConfigRequest`. Restent ici
// ceux que le serveur choisit selon sa version de protocole, ou qu'il lit à la
// main.

// LA FICHE COMPLÈTE (`ZC_PROPERTY_HOMUN`, quatrième forme) : 85 octets.
//
// ⚠ LES STATISTIQUES SONT DES ENTIERS DE SEIZE BITS SIGNÉS. Le serveur les
// déclare non signées, mais il les plafonne à 32 767 et le client les relit
// signées : c'est la lecture qui fait foi.
//
// ⚠ L'INTIMITÉ ARRIVE DÉJÀ DIVISÉE PAR CENT (0 à 1 000), et une expérience
// suivante nulle veut dire « niveau maximum ».
struct HomunculusProperties {
  static constexpr std::uint16_t kOpcode = 0x0BA4;
  static constexpr std::size_t kWireSize = 85;
  static constexpr std::size_t kNameBytes = 24;

  std::string name;
  std::uint8_t flags = 0;
  std::int16_t level = 0;
  std::int16_t hunger = 0;
  std::int16_t intimacy = 0;
  std::int16_t atk = 0;
  std::int16_t matk = 0;
  std::int16_t hit = 0;
  std::int16_t crit = 0;
  std::int16_t def = 0;
  std::int16_t mdef = 0;
  std::int16_t flee = 0;
  std::int16_t amotion_ms = 0;
  std::uint32_t hp = 0;
  std::uint32_t max_hp = 0;
  std::uint32_t sp = 0;
  std::uint32_t max_sp = 0;
  std::int64_t exp = 0;
  std::int64_t exp_next = 0;
  std::int16_t skill_points = 0;
  std::int16_t attack_range_cells = 0;
};

// Les trois bits de `flags`.
inline constexpr std::uint8_t kHomunculusRenamedFlag = 0x1;
inline constexpr std::uint8_t kHomunculusRestingFlag = 0x2;
// ⚠ VRAI QUAND IL A DES PV : le serveur le pose sur `hp > 0`, quoi qu'en dise
// le commentaire de sa structure.
inline constexpr std::uint8_t kHomunculusAliveFlag = 0x4;

// ⚠ REFUSE UNE TRAME QUI NE FAIT PAS SA LONGUEUR, ou qui porte un autre opcode.
[[nodiscard]] core::Result<HomunculusProperties> DecodeHomunculusProperties(
    std::span<const std::uint8_t> frame);

// L'envers du décodeur, pour les essais : le client n'émet pas la fiche.
[[nodiscard]] std::vector<std::uint8_t> EncodeHomunculusProperties(
    const HomunculusProperties& properties);

// UN CHAMP QUI CHANGE (`ZC_HO_PAR_CHANGE`, seconde forme) : 12 octets, valeur
// sur soixante-quatre bits.
struct HomunculusParameter {
  static constexpr std::uint16_t kOpcode = 0x0BA5;
  static constexpr std::size_t kWireSize = 12;

  std::uint16_t type = 0;
  std::uint64_t value = 0;
};

// Les trois champs que le serveur envoie ainsi, numérotés comme ses `_sp`.
inline constexpr std::uint16_t kHomunculusParameterExp = 1;
inline constexpr std::uint16_t kHomunculusParameterHp = 5;
inline constexpr std::uint16_t kHomunculusParameterSp = 7;

[[nodiscard]] core::Result<HomunculusParameter> DecodeHomunculusParameter(
    std::span<const std::uint8_t> frame);
[[nodiscard]] std::vector<std::uint8_t> EncodeHomunculusParameter(
    const HomunculusParameter& parameter);

// UN ORDRE DU MENU (`CZ_COMMAND_MER`) : 5 octets, un type toujours nul puis la
// commande.
//
// ⚠ LE SERVEUR LE JETTE SANS UN MOT quand l'homoncule n'est pas actif — au
// repos, ou sans PV.
enum class HomunculusCommand : std::uint8_t {
  // Le serveur ne fait rien de celle-ci.
  kInformation = 0,
  kFeed = 1,
  // Définitif : l'homoncule et son histoire sont effacés.
  kDelete = 2,
};

struct HomunculusCommandRequest {
  static constexpr std::uint16_t kOpcode = 0x022D;
  static constexpr std::size_t kWireSize = 5;

  HomunculusCommand command = HomunculusCommand::kInformation;
};

[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(
    const HomunculusCommandRequest& request);

// LE NOUVEAU NOM (`CZ_RENAME_MER`) : 26 octets, un nom sur vingt-quatre.
//
// ⚠ LES OCTETS PARTENT TELS QUELS : le transcodage est l'affaire de `net`.
struct HomunculusRenameRequest {
  static constexpr std::uint16_t kOpcode = 0x0231;
  static constexpr std::size_t kWireSize = 26;
  static constexpr std::size_t kNameBytes = 24;

  std::string name;
};

[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(
    const HomunculusRenameRequest& request);

}  // namespace ro::proto
