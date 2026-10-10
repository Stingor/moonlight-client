#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace ro::proto {

// LA DEMANDE DE NAVIGATION qu'un script adresse au joueur (`navigateto`).
//
// 🔴 LE SERVEUR L'ÉCRIT OCTET PAR OCTET, sans structure : le générateur ne peut
// pas la produire, et sa longueur vient de `RAW_WRITTEN_PACKETS` dans
// `tools/packetgen/length_extractors.py`. Sans cette entrée, le découpage du
// flux ne connaît pas l'opcode et ferme la session.
struct NavigationActive {
  static constexpr std::uint16_t kOpcode = 0x08E2;
  static constexpr std::size_t kWireSize = 27;
  // La largeur du nom de carte, extension comprise.
  static constexpr std::size_t kMapNameWidth = 16;

  // Ce que la navigation vise : 0 une cellule, 1 la carte seule, 2 un PNJ par
  // ses coordonnées, 3 un monstre par son identifiant.
  std::uint8_t type = 0;
  // Les services de transport permis, en un nombre écrit en DÉCIMAL : les
  // centaines pour les services de transport, les dizaines pour l'avion, les
  // unités pour les parchemins.
  std::uint8_t flag = 0;
  // Non nul : la fenêtre de navigation ne s'ouvre pas.
  std::uint8_t hide_window = 0;
  // Le nom de carte tel que le serveur l'écrit, sans son zéro terminal.
  std::string map;
  std::uint16_t x = 0;
  std::uint16_t y = 0;
  std::uint16_t mob_id = 0;
};

// Lit une trame de navigation. Faux si elle n'a ni le bon opcode ni la bonne
// taille.
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, NavigationActive& packet);

// L'envers du décodeur, pour les essais : le client n'émet pas ce paquet.
[[nodiscard]] std::vector<std::uint8_t> Encode(const NavigationActive& packet);

}  // namespace ro::proto
