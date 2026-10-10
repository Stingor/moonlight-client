#pragma once

#include <cstdint>

#include "ro/net/connection.h"
#include "ro/sim/actor.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// Ce qu'une trame a fait au monde.
enum class FrameOutcome : std::uint8_t {
  // Le monde a changé.
  kApplied,
  // Trame comprise, mais qui ne décrit rien du monde — un message, un état
  // d'inventaire. Ce n'est PAS une erreur.
  kIgnored,
  // Opcode que ce traducteur ne traite pas encore. La distinction avec le
  // précédent est ce qui permet de savoir ce qu'il reste à écrire.
  kUnhandled,
  // Trame dont le décodeur a refusé la disposition.
  kUndecodable,
};

// Applique une trame reçue au monde.
//
// C'EST ICI QUE LE PROTOCOLE S'ARRÊTE. ro_sim ne connaît aucun paquet, et cette
// fonction est la seule chose qui sache traduire les uns dans l'autre. La
// frontière n'est pas décorative : elle permet de rejouer une capture dans un
// monde sans socket, et de bâtir un monde à la main dans un essai.
//
// ⚠ Une trame qui parle d'un acteur inconnu n'en crée PAS un. Elle peut arriver
// parce que son apparition s'est perdue ou n'a jamais été envoyée ; l'inventer
// donnerait un acteur sans apparence ni nom, qu'aucun sprite ne saurait
// dessiner. Le verdict est alors kIgnored.
//
// `now_ms` est l'horloge de l'APPELANT. La couche réseau n'en lit aucune : un
// rejeu fournit les instants de sa capture et obtient les mêmes positions.
[[nodiscard]] FrameOutcome ApplyFrame(const Frame& frame, sim::World& world, std::uint32_t now_ms);

// Le genre d'acteur que le serveur annonce dans l'octet de type de ses paquets
// d'apparition.
//
// Les valeurs sont celles de clif_bl_type, dans src/map/clif.cpp. Un genre
// inconnu n'est pas une erreur : le serveur en ajoute au fil des versions, et
// un acteur au genre inconnu reste un acteur qu'il faut placer.
[[nodiscard]] sim::ActorKind ActorKindOf(std::uint8_t object_type);
// Et, pour un non-joueur, s'il est un familier : le genre les confond avec les
// PNJ.
[[nodiscard]] sim::Familiar FamiliarOf(std::uint8_t object_type);

}  // namespace ro::net
