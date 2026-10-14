#include "game/connected_session.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>

#include "ro/core/text.h"
#include "ro/net/handshake.h"
#include "ro/net/session.h"
#include "ro/sim/packed_position.h"

namespace moonclient {
namespace {

// Le sexe que le personnage laisse au COMPTE : la liste le marque ainsi
// lorsqu'il n'en porte pas lui-même (char_mmo_char_tobuf, dans
// src/char/char.cpp : 0 femme, 1 homme, 99 « celui de la connexion »).
constexpr std::uint8_t kSexFromAccount = 99;

// L'INSTANT DES ÉTATS QUE LE JOUEUR APPORTE EN ENTRANT, dans l'horloge du
// monde : avant toute image. Un corps qui entre gelé l'est déjà, il ne se fige
// pas sous les yeux.
constexpr std::uint32_t kEnteredAtMs = 0;

}  // namespace

ro::sim::Appearance AppearanceOf(const ro::proto::CharacterInfo& character,
                                 std::uint8_t account_sex) {
  ro::sim::Appearance look;
  look.job = static_cast<std::uint16_t>(character.job);
  look.sex = character.sex == kSexFromAccount ? account_sex : character.sex;
  look.head = static_cast<std::uint16_t>(character.head);
  look.body_style = static_cast<std::uint16_t>(character.body);
  look.weapon = static_cast<std::uint32_t>(character.weapon);
  look.shield = static_cast<std::uint32_t>(character.shield);
  look.lower_headgear = static_cast<std::uint16_t>(character.accessory);
  look.upper_headgear = static_cast<std::uint16_t>(character.accessory2);
  look.middle_headgear = static_cast<std::uint16_t>(character.accessory3);
  look.robe = static_cast<std::uint16_t>(character.robePalette);
  look.head_palette = character.headpalette;
  look.body_palette = character.bodypalette;
  return look;
}

MapEntry ComposeMapEntry(const ro::proto::CharacterInfo& chosen,
                         const ro::net::LoginPhase::Account& account, std::string map_name,
                         const ro::net::MapPhase::Spawn& spawn) {
  MapEntry entry;
  entry.map_name = std::move(map_name);
  entry.character_name = ro::core::Utf8FromCp1252(chosen.name);
  entry.map_id = account.account_id;
  entry.account = account;
  entry.look = AppearanceOf(chosen, account.sex);
  // La vitesse de la liste, le temps que le serveur dise la sienne par
  // SP_SPEED : à zéro le personnage se téléporterait.
  entry.speed_ms_per_cell = static_cast<std::uint16_t>(std::max<std::int16_t>(0, chosen.speed));
  entry.base_level = chosen.level;
  entry.job_level = chosen.joblevel;
  entry.zeny = chosen.money;
  entry.spawn = ro::sim::UnpackPosition(spawn.packed_position);
  entry.body_state = static_cast<std::uint16_t>(chosen.bodystate);
  entry.health_state = static_cast<std::uint16_t>(chosen.healthstate);
  entry.option = static_cast<std::uint32_t>(chosen.effectstate);
  return entry;
}

void SpawnPlayer(const MapEntry& entry, ro::sim::World& world) {
  // Le monde connaît QUI l'on est avant d'avoir vu personne : son identifiant
  // vient de la phase de connexion, et le paquet qui annonce le déplacement du
  // joueur ne nomme personne.
  world.set_player_id(entry.map_id);
  world.set_player_name(entry.character_name);

  // LES DEUX NIVEAUX ET LA BOURSE, POUR LA MÊME RAISON QUE LE NOM : le serveur
  // de zone ne les dit qu'au moment où ils CHANGENT. Ils passent par la MÊME
  // porte que le reste — c'est elle qui sait où va chaque valeur —, et un paquet
  // du serveur les écrasera sans cérémonie le jour où il en parlera.
  static_cast<void>(world.SetPlayerParameter(
      static_cast<std::uint16_t>(ro::sim::ServerParameter::kBaseLevel), entry.base_level));
  static_cast<void>(world.SetPlayerParameter(
      static_cast<std::uint16_t>(ro::sim::ServerParameter::kJobLevel), entry.job_level));
  static_cast<void>(world.SetPlayerParameter(
      static_cast<std::uint16_t>(ro::sim::ServerParameter::kZeny), entry.zeny));

  const std::uint32_t self_id = entry.map_id;
  ro::sim::Actor self;
  self.id = self_id;
  self.name = entry.character_name;
  self.kind = ro::sim::ActorKind::kPlayer;
  self.position = entry.spawn;
  self.look = entry.look;
  self.speed_ms_per_cell =
      entry.speed_ms_per_cell != 0 ? entry.speed_ms_per_cell : ro::sim::kDefaultWalkSpeedMsPerCell;
  world.Spawn(std::move(self));

  // SES ÉTATS PASSENT PAR LA PORTE D'UN CHANGEMENT D'ÉTAT, comme s'il venait
  // d'être annoncé : les bits allumés le sont en FRONT MONTANT, et ce qui naît
  // d'une option — la monture, Ruwach, Sight — naît comme en cours de partie.
  // Aucun mode PK : la liste n'en porte pas.
  static_cast<void>(
      world.SetStates(self_id, entry.body_state, entry.health_state, entry.option, kEnteredAtMs));
}

std::string DescribeLinkLoss(ro::net::LinkStatus status, const ro::core::Error& why) {
  switch (status) {
    case ro::net::LinkStatus::kClosedByPeer:
      // Le cas ordinaire, et celui qui n'a pas d'erreur à montrer. La cause la
      // plus fréquente est le silence : le serveur coupe une session dont il
      // n'a rien reçu depuis stall_time secondes, soixante par défaut.
      return "le serveur a fermé la connexion";
    case ro::net::LinkStatus::kProtocolError:
      return "protocole : " + why.detail;
    case ro::net::LinkStatus::kIoError:
      return "entrée/sortie : " + why.detail;
    case ro::net::LinkStatus::kOpen:
      break;
  }
  return "état inattendu";
}

}  // namespace moonclient
