#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "ro/core/error.h"

namespace ro::proto {

// ── LES TITRES, ET LA LISTE DES SUCCÈS D'OÙ ILS SORTENT ─────────────────────
//
// 🔴 AUCUN DE CES TROIS PAQUETS N'A DE STRUCTURE CÔTÉ SERVEUR que le générateur
// puisse lire. Les deux du titre s'écrivent champ par champ (`WFIFOL`,
// `RFIFOL`) ; la liste des succès a bien une structure, mais sa longueur y est
// un plafond de base de données et non le compte réel — `refus.csv` l'écarte
// pour cette raison. Ils sont donc écrits ici, et recoupés avec ce que le
// serveur déclare.

// « Je porte ce titre. » Zéro le retire.
//
// ⚠ LE SERVEUR REVALIDE : un titre que le personnage ne possède pas est refusé
// par l'accusé, et le même titre redemandé ne vaut aucune réponse du tout.
struct ChangeTitleRequest {
  static constexpr std::uint16_t kOpcode = 0x0A2E;
  static constexpr std::size_t kWireSize = 6;

  std::int32_t title_id = 0;
};

// L'accusé de la demande. ⚠ LE REFUS N'EST DIT QUE PAR `result` : le numéro
// renvoyé est celui qu'on a demandé, porté ou non.
struct ChangeTitleAck {
  static constexpr std::uint16_t kOpcode = 0x0A2F;
  static constexpr std::size_t kWireSize = 7;

  // Zéro : porté. Un : le personnage ne possède pas ce titre.
  static constexpr std::uint8_t kAccepted = 0;

  std::uint8_t result = kAccepted;
  std::int32_t title_id = 0;
};

// UN SUCCÈS DE LA LISTE, tel que le serveur le décrit à ce personnage.
struct AchievementEntry {
  static constexpr std::size_t kWireSize = 50;
  static constexpr std::size_t kObjectiveCount = 10;

  std::uint32_t achievement_id = 0;
  bool completed = false;
  std::array<std::uint32_t, kObjectiveCount> objectives = {};
  // Un instant Unix, en secondes. Zéro tant que le succès n'est pas obtenu.
  std::uint32_t completed_at = 0;
  // 🔴 LA RÉCOMPENSE RÉCLAMÉE, et c'est ELLE qui donne le titre — pas
  // l'achèvement. Un succès obtenu dont on n'a pas réclamé le prix ne donne
  // aucun titre au client, même si le serveur l'accepterait.
  bool rewarded = false;
};

// LA LISTE COMPLÈTE DES SUCCÈS, poussée au login et après chaque récompense
// réclamée.
struct AchievementList {
  static constexpr std::uint16_t kOpcode = 0x0A23;
  // L'opcode, la longueur, le compte, les points, le rang et ses deux bornes.
  static constexpr std::size_t kHeaderSize = 22;

  std::uint32_t total_points = 0;
  std::uint16_t rank = 0;
  std::uint32_t current_rank_points = 0;
  std::uint32_t next_rank_points = 0;
  std::vector<AchievementEntry> entries;
};

// « Je réclame la récompense de ce succès. »
//
// ⚠ LE SERVEUR REVALIDE : un succès inconnu, inachevé ou déjà réclamé ne vaut
// qu'un accusé d'échec. Et la réponse d'une réussite dépend de la récompense :
// la liste complète quand elle porte un titre, l'accusé puis la mise à jour du
// succès sinon.
struct AchievementRewardRequest {
  static constexpr std::uint16_t kOpcode = 0x0A25;
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t achievement_id = 0;
};

[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const ChangeTitleRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(
    const AchievementRewardRequest& request);

// ⚠ LES DEUX DÉCODEURS REFUSENT UNE TRAME QUI NE TOMBE PAS JUSTE : un compte
// annoncé qui ne correspond pas à la longueur, un reste qui ne fait pas une
// entrée entière. Lire à moitié donnerait des titres tirés du bourrage.
[[nodiscard]] core::Result<ChangeTitleAck> DecodeChangeTitleAck(
    std::span<const std::uint8_t> frame);
[[nodiscard]] core::Result<AchievementList> DecodeAchievementList(
    std::span<const std::uint8_t> frame);

// L'ENVERS DES DEUX DÉCODEURS, pour les essais : le client n'émet ni l'un ni
// l'autre.
[[nodiscard]] std::vector<std::uint8_t> EncodeChangeTitleAck(const ChangeTitleAck& ack);
[[nodiscard]] std::vector<std::uint8_t> EncodeAchievementList(const AchievementList& list);

}  // namespace ro::proto
