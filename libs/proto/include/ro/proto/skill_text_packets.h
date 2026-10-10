#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "ro/core/error.h"

namespace ro::proto {

// « Je pose cette compétence SUR CETTE CASE, avec ce texte. » — Talkie Box.
//
// Les quatre champs de la demande au sol, aux mêmes places, puis le texte sur
// vingt et un octets, zéro compris : le client le TRONQUE à cette taille, et
// le serveur ne lit la chaîne que jusqu'à son zéro. Sans ce paquet, le serveur
// pose la boîte avec un texte vide.
//
// ⚠ L'ENCODEUR VIT AVEC LES AUTRES DEMANDES ÉCRITES À LA MAIN, dans
// `manual_packets.cpp` : il recoupe la disposition du serveur de la même façon.
struct SkillOnGroundWithTextRequest {
  static constexpr std::uint16_t kOpcode = 0x0367;
  static constexpr std::size_t kWireSize = 31;
  static constexpr std::size_t kTextSize = 21;

  std::uint16_t skill_level = 0;
  std::uint16_t skill_id = 0;
  std::uint16_t cell_x = 0;
  std::uint16_t cell_y = 0;
  // En CP-1252, la page du jeu ; au-delà de vingt octets, coupé.
  std::string text;
};

[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(
    const SkillOnGroundWithTextRequest& request);

}  // namespace ro::proto
