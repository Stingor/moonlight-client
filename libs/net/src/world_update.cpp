#include "ro/net/world_update.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

#include "ro/core/text.h"
#include "ro/net/character.h"
#include "ro/net/companion.h"
#include "ro/net/map_name.h"
#include "ro/net/npc_dialog.h"
#include "ro/net/player_bonuses.h"
#include "ro/net/skill.h"
#include "ro/net/world_notice.h"
#include "ro/net/world_skill.h"
#include "ro/proto/manual_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/movement.h"
#include "ro/sim/packed_position.h"
#include "ro/sim/world.h"

#include "frame_apply.h"
#include "world_arrivals.h"
#include "world_relay.h"

namespace ro::net {
namespace {

using sim::Actor;
using sim::ActorKind;
using sim::CellMovement;
using sim::CellPosition;
using sim::GroundItem;
// Les valeurs de clif_bl_type, dans src/map/clif.cpp.
constexpr std::uint8_t kObjectPlayer = 0x0;
constexpr std::uint8_t kObjectDisguisedPlayer = 0x1;
constexpr std::uint8_t kObjectItem = 0x2;
constexpr std::uint8_t kObjectSkill = 0x3;
constexpr std::uint8_t kObjectMonster = 0x5;
constexpr std::uint8_t kObjectNonPlayer = 0x6;
constexpr std::uint8_t kObjectPet = 0x7;
constexpr std::uint8_t kObjectHomunculus = 0x8;
constexpr std::uint8_t kObjectMercenary = 0x9;
constexpr std::uint8_t kObjectElemental = 0xA;
// Un PNJ dont l'apparence est une classe de MONSTRE : il se dessine et se parle
// comme tout PNJ, seul l'octet de type le distingue.
constexpr std::uint8_t kObjectMonsterLookingNonPlayer = 0xC;
// Les monstres invoqués par un Meister (ABR) ou un Biolo (Bionic) : des
// monstres, que le serveur annonce à part.
constexpr std::uint8_t kObjectAbrMonster = 0xD;
constexpr std::uint8_t kObjectBionicMonster = 0xE;

// Remplit ce qu'un paquet d'unité dit d'un acteur, hors position.
//
// Les trois paquets d'unité — au repos, qui apparaît, qui marche — portent les
// mêmes champs d'apparence à la même place ; seule la fin diffère. Écrire trois
// fois la même recopie inviterait les trois à diverger.
template <typename Packet>
Actor ActorFrom(const Packet& packet) {
  Actor actor;
  // ⚠ C'est AID qui nomme l'acteur, pas GID. Depuis PACKETVER 20131223, le
  // serveur met l'identifiant du bloc dans AID et celui du PERSONNAGE dans
  // GID — NUL pour tout ce qui n'est pas un joueur (clif_set_unit_idle, src/map/clif.cpp).
  // Le prendre rangerait PNJ et monstres sous le même identifiant, et ils
  // s'effaceraient les uns les autres. Les autres paquets nomment eux aussi le bloc.
  actor.id = packet.AID;
  actor.kind = ActorKindOf(packet.objecttype);
  actor.familiar = FamiliarOf(packet.objecttype);
  actor.object_type = packet.objecttype;
  actor.player_sprite = packet.objecttype == kObjectPlayer;
  actor.speed_ms_per_cell = static_cast<std::uint16_t>(packet.speed);
  // Le nom vient du JEU, donc de sa page de codes : voir ro/core/text.h.
  actor.name = core::Utf8FromCp1252(packet.name);
  actor.head_direction = static_cast<std::uint8_t>(packet.headDir);
  actor.look.job = static_cast<std::uint16_t>(packet.job);
  actor.look.head = packet.head;
  actor.look.weapon = packet.weapon;
  actor.look.shield = packet.shield;
  // Les trois accessoires ne suivent pas l'ordre bas, milieu, haut : le
  // serveur écrit le BAS, puis le HAUT, puis le MILIEU (clif_set_unit_idle
  // recopie LOOK_HEAD_BOTTOM, LOOK_HEAD_TOP, LOOK_HEAD_MID dans cet ordre).
  actor.look.lower_headgear = packet.accessory;
  actor.look.upper_headgear = packet.accessory2;
  actor.look.middle_headgear = packet.accessory3;
  actor.look.robe = packet.robe;
  actor.look.body_style = static_cast<std::uint16_t>(packet.body);
  actor.look.head_palette = packet.headpalette;
  actor.look.body_palette = packet.bodypalette;
  actor.look.sex = packet.sex;
  actor.body_state = static_cast<std::uint16_t>(packet.bodyState);
  actor.health_state = static_cast<std::uint16_t>(packet.healthState);
  actor.option = static_cast<std::uint32_t>(packet.effectState);
  actor.pk_mode = packet.isPKModeON != 0;
  actor.boss = sim::BossRankOf(packet.isBoss);
  // ⚠ LE NIVEAU EST CELUI DE QUI APPARAÎT, pas le nôtre : c'est lui qui décide
  // de la couleur d'un nom de monstre trop fort pour nous.
  actor.level = packet.clevel;
  // La guilde de qui apparaît, que le serveur écrit pour toute unité.
  actor.guild_id = packet.GUID;
  // 🔴 L'APPARITION PORTE LA VIE, et nous la jetions. Depuis PACKETVER
  // 20120221, `clif_set_unit_idle` y écrit `HP`/`maxHP` — pour un MONSTRE
  // entamé, sous `monster_hp_bars_info` et hors des cartes marquées
  // `HIDEMOBHPBAR` — et MOINS UN partout ailleurs, ce qui est exactement notre
  // `kUnknownHealth`. Sans cette lecture, un monstre déjà blessé qui entre dans
  // le champ de vision n'avait de jauge qu'au coup suivant, et pas du tout si
  // c'est un autre qui le frappait.
  //
  // ⚠ UN MAXIMUM NUL OU NÉGATIF RESTE INCONNU : voir `World::Spawn`, qui ne
  // laisse pas une réannonce muette effacer ce que les paquets de vie ont dit.
  if (packet.maxHP > 0 && packet.HP >= 0) {
    actor.health = packet.HP;
    actor.max_health = packet.maxHP;
  }
  return actor;
}

// L'état d'une unité au repos (dead_sit, dans src/map/status.hpp) : debout,
// morte, assise. Seul ce paquet le porte ; qui apparaît ou marche est debout.
constexpr std::uint8_t kUnitStateDead = 1;
constexpr std::uint8_t kUnitStateSitting = 2;

// Les actions d'un ZC_NOTIFY_ACT : les valeurs de e_damage_type, dans
// src/map/clif.hpp. S'asseoir et se relever ne sont pas des coups ; parmi
// les coups, le client n'en joue que sept, et deux ne font pas broncher : ceux
// qu'on encaisse.
constexpr std::uint8_t kActionPickUp = 1;
constexpr std::uint8_t kActionSitDown = 2;
constexpr std::uint8_t kActionStandUp = 3;
constexpr std::array<std::uint8_t, 7> kBlowActions = {0, 4, 8, 9, 10, 11, 13};

// Les aspects qu'un changement d'apparence sait changer un à un : les valeurs
// de enum _look, dans src/map/map.hpp.
constexpr std::uint8_t kLookBase = 0;
constexpr std::uint8_t kLookHair = 1;
constexpr std::uint8_t kLookWeapon = 2;
constexpr std::uint8_t kLookHeadBottom = 3;
constexpr std::uint8_t kLookHeadTop = 4;
constexpr std::uint8_t kLookHeadMid = 5;
constexpr std::uint8_t kLookHairColor = 6;
constexpr std::uint8_t kLookClothesColor = 7;
constexpr std::uint8_t kLookShield = 8;
constexpr std::uint8_t kLookResetCostumes = 11;
constexpr std::uint8_t kLookRobe = 12;
constexpr std::uint8_t kLookBody2 = 13;

// Applique un changement d'aspect à une apparence. Rend faux pour un aspect
// que le monde ne porte pas — les chaussures, ou un aspect inconnu.
bool Restyled(sim::Appearance& look, std::uint8_t aspect, std::uint32_t value,
              std::uint32_t second_value) {
  switch (aspect) {
    case kLookBase:
      look.job = static_cast<std::uint16_t>(value);
      return true;
    case kLookHair:
      look.head = static_cast<std::uint16_t>(value);
      return true;
    case kLookWeapon:
      // Le serveur dit les DEUX mains d'un coup, quelle que soit celle qui
      // change (clif_changelook, dans src/map/clif.cpp).
      look.weapon = value;
      look.shield = second_value;
      return true;
    case kLookHeadBottom:
      look.lower_headgear = static_cast<std::uint16_t>(value);
      return true;
    case kLookHeadTop:
      look.upper_headgear = static_cast<std::uint16_t>(value);
      return true;
    case kLookHeadMid:
      look.middle_headgear = static_cast<std::uint16_t>(value);
      return true;
    case kLookHairColor:
      look.head_palette = static_cast<std::int16_t>(value);
      return true;
    case kLookClothesColor:
      look.body_palette = static_cast<std::int16_t>(value);
      return true;
    case kLookShield:
      look.shield = value;
      return true;
    case kLookResetCostumes:
      // Les trois coiffes tombent d'un coup : c'est ce que le serveur en
      // dit (enum _look).
      look.lower_headgear = 0;
      look.middle_headgear = 0;
      look.upper_headgear = 0;
      return true;
    case kLookRobe:
      look.robe = static_cast<std::uint16_t>(value);
      return true;
    case kLookBody2:
      look.body_style = static_cast<std::uint16_t>(value);
      return true;
    default:
      return false;
  }
}

// Ce que le paquet dit de la DURÉE, quand il en dit quelque chose.
//
// ⚠ TROIS PAQUETS, TROIS RICHESSES, ET UN SEUL SENS. Le plus récent porte le
// total ET le restant — que le serveur remplit de la même valeur au début
// (« at this stage remain and total are the same value ») —, le précédent le
// restant seul, le plus ancien rien du tout. On prend le plus grand des deux
// quand les deux sont là : un état capté en cours de route ne doit pas voir sa
// jauge partir du milieu.
template <typename Packet>
[[nodiscard]] std::uint32_t AnnouncedDurationMs(const Packet& packet) {
  if constexpr (requires { packet.Total; }) {
    return std::max(static_cast<std::uint32_t>(packet.Total),
                    static_cast<std::uint32_t>(packet.Left));
  } else if constexpr (requires { packet.Left; }) {
    return static_cast<std::uint32_t>(packet.Left);
  } else {
    return 0;
  }
}

// Un état posé ou retiré. Les trois paquets d'état — avec durées, avec
// valeurs, sans rien — portent le même trio : l'acteur, l'état, posé ou non.
//
// 🔴 ET LA DURÉE NE SE GARDE QUE POUR LE JOUEUR. Elle ne sert qu'à une barre
// d'états, qui est la sienne ; la tenir pour chaque monstre à l'écran ferait
// vivre des centaines d'échéances que personne ne lit. Les autres acteurs
// gardent la seule présence, qui décide de ce qu'on dessine autour d'eux.
template <typename Packet>
std::optional<std::int32_t> AnnouncedValue(const Packet& packet) {
  // 🔴 LA VALEUR ÉTAIT SUR LE FIL DEPUIS TOUJOURS, ET NOUS LA JETIONS. Six
  // effets se calculent sur elle et non sur l'état : Rolling Cutter montre un
  // anneau par tour — `valeur + 1299`, de un à dix —, et la malédiction du
  // Devil assombrit par paliers. Sans elle, ces états-là restaient muets.
  if constexpr (requires { packet.val1; }) {
    return packet.val1;
  } else {
    return std::nullopt;
  }
}

template <typename Packet>
FrameOutcome StatusApplied(sim::World& world, const Packet& packet, std::uint32_t now_ms) {
  const auto effect = static_cast<sim::StatusEffectId>(packet.index);
  const bool active = packet.state != 0;
  // Et la SECONDE valeur, par la même règle : un état y désigne un autre acteur.
  std::optional<std::int32_t> second;
  if constexpr (requires { packet.val2; }) {
    second = packet.val2;
  }
  const bool known =
      world.SetStatusEffect(packet.AID, effect, active, AnnouncedValue(packet), second, now_ms);
  if (packet.AID == world.player_id()) {
    world.NotePlayerStatus(effect, active, AnnouncedDurationMs(packet), now_ms);
  }
  return known ? FrameOutcome::kApplied : FrameOutcome::kIgnored;
}

}  // namespace

ActorKind ActorKindOf(std::uint8_t object_type) {
  switch (object_type) {
    case kObjectPlayer:
    case kObjectDisguisedPlayer:
      return ActorKind::kPlayer;
    case kObjectItem:
      return ActorKind::kItem;
    case kObjectSkill:
      return ActorKind::kSkillUnit;
    case kObjectMonster:
    case kObjectAbrMonster:
    case kObjectBionicMonster:
      return ActorKind::kMonster;
    case kObjectNonPlayer:
    case kObjectMonsterLookingNonPlayer:
    case kObjectPet:
    case kObjectHomunculus:
    case kObjectMercenary:
    case kObjectElemental:
      return ActorKind::kNonPlayer;
    default:
      return ActorKind::kUnknown;
  }
}

sim::Familiar FamiliarOf(std::uint8_t object_type) {
  switch (object_type) {
    case kObjectPet:
      return sim::Familiar::kPet;
    case kObjectHomunculus:
      return sim::Familiar::kHomunculus;
    case kObjectMercenary:
      return sim::Familiar::kMercenary;
    case kObjectElemental:
      return sim::Familiar::kElemental;
    default:
      return sim::Familiar::kNone;
  }
}

FrameOutcome ApplyFrame(const Frame& frame, sim::World& world, std::uint32_t now_ms) {
  switch (frame.opcode) {
    case proto::PacketIdleUnit::kOpcode:
      return Decoded<proto::PacketIdleUnit>(frame, [&world, now_ms](const auto& packet) {
        Actor actor = ActorFrom(packet);
        actor.position = sim::UnpackPosition(packet.PosDir);
        // Une unité au repos peut être assise, ou morte à terre.
        if (packet.state == kUnitStateDead) {
          actor.posture = sim::Posture::kDead;
        } else if (packet.state == kUnitStateSitting) {
          actor.posture = sim::Posture::kSitting;
        }
        actor.posture_since_ms = now_ms;
        actor.body_state_since_ms = now_ms;
        world.Spawn(std::move(actor));
        return FrameOutcome::kApplied;
      });

    case proto::PacketSpawnUnit::kOpcode:
      return Decoded<proto::PacketSpawnUnit>(frame, [&world, now_ms](const auto& packet) {
        Actor actor = ActorFrom(packet);
        actor.position = sim::UnpackPosition(packet.PosDir);
        actor.body_state_since_ms = now_ms;
        SpawnAppearing(world, std::move(actor), packet.objecttype == kObjectPlayer, now_ms);
        return FrameOutcome::kApplied;
      });

    case proto::PacketUnitWalking::kOpcode:
      return Decoded<proto::PacketUnitWalking>(frame, [&world, now_ms](const auto& packet) {
        // Un acteur qui marche est annoncé ENTIER : c'est une apparition et un
        // déplacement à la fois. Ne traiter que le déplacement laisserait sans
        // apparence quelqu'un qui entre dans le champ de vision en marchant.
        const CellMovement path = sim::UnpackMovement(packet.MoveData);
        Actor actor = ActorFrom(packet);
        actor.position = path.from;
        actor.movement = path;
        // La marche a commencé quand le SERVEUR le dit, pas quand la trame nous
        // parvient : dater du second ajoute la latence à chaque départ, et
        // l'acteur saute dès qu'un arrêt le remet à sa vraie place.
        // ⚠ UN CALAGE, PAS UNE RÉPONSE : cette trame ne ferme pas l'aller-retour
        // du battement.
        world.server_clock().NoteServerTimestamp(packet.moveStartTime, now_ms);
        actor.movement_started_ms = world.server_clock().LocalTimeOf(packet.moveStartTime, now_ms);
        actor.body_state_since_ms = now_ms;
        world.Spawn(std::move(actor));
        return FrameOutcome::kApplied;
      });

    case proto::ZcItemEntry::kOpcode:
      return Decoded<proto::ZcItemEntry>(frame, [&world, now_ms](const auto& packet) {
        // Un objet déjà au sol quand on entre en vue. Il n'a ni nom ni
        // apparence : son identifiant d'objet va dans `GroundItem::item_id`,
        // et son nombre ne change rien à ce qu'on dessine.
        //
        // ⚠ IL NE TOMBE PAS : il est à terre depuis avant qu'on arrive, et le
        // client ne lui arme aucune chute. Seul son scintillement court, depuis
        // l'instant où on le découvre.
        Actor item;
        item.id = packet.AID;
        item.kind = ActorKind::kItem;
        item.position = CellPosition{packet.x, packet.y, 0};
        item.sub_x = packet.subX;
        item.sub_y = packet.subY;
        item.ground_item = GroundItem{.since_ms = now_ms,
                                      .falling = false,
                                      .identified = packet.identify != 0,
                                      .amount = packet.amount,
                                      .item_id = packet.itemId};
        world.Spawn(std::move(item));
        return FrameOutcome::kApplied;
      });

    case proto::PacketDropflooritem::kOpcode:
      return Decoded<proto::PacketDropflooritem>(frame, [&world, now_ms](const auto& packet) {
        // Un objet qui TOMBE — un butin qu'une créature vient de lâcher, ou ce
        // qu'un joueur jette à ses pieds. Le client le fait choir du ciel : il
        // naît quinze unités en l'air et se pose en quelque quatre cents
        // millisecondes.
        Actor item;
        item.id = packet.ITAID;
        item.kind = ActorKind::kItem;
        item.position = CellPosition{static_cast<std::uint16_t>(packet.xPos),
                                     static_cast<std::uint16_t>(packet.yPos), 0};
        item.sub_x = packet.subX;
        item.sub_y = packet.subY;
        item.ground_item =
            GroundItem{.since_ms = now_ms,
                       .falling = true,
                       .show_drop_effect = packet.showdropeffect == 1,
                       .drop_effect_mode = static_cast<std::uint16_t>(packet.dropeffectmode),
                       .identified = packet.IsIdentified != 0,
                       .type = packet.type,
                       .amount = static_cast<std::uint16_t>(packet.count),
                       .item_id = packet.ITID};
        world.Spawn(std::move(item));
        return FrameOutcome::kApplied;
      });

    case proto::ZcItemDisappear::kOpcode:
      return Decoded<proto::ZcItemDisappear>(frame, [&world](const auto& packet) {
        world.Remove(packet.itemAid);
        return FrameOutcome::kApplied;
      });

    case proto::PacketMonsterHp::kOpcode:
      return Decoded<proto::PacketMonsterHp>(frame, [&world](const auto& packet) {
        // Le serveur ne l'envoie que sous un réglage, et seulement d'un monstre
        // déjà entamé : ne pas le recevoir est le cas ordinaire.
        return world.SetHealth(packet.GID, packet.HP, packet.MaxHP) ? FrameOutcome::kApplied
                                                                    : FrameOutcome::kIgnored;
      });

    case proto::ZcNotifyHpToGroupm::kOpcode:
      return Decoded<proto::ZcNotifyHpToGroupm>(frame, [&world](const auto& packet) {
        // LA VIE D'UN COMPAGNON DE GROUPE, et c'est la seule source qu'on en
        // ait : ZC_NOTIFY_HP ne parle que des monstres, et aucun paquet d'unité
        // ne porte la vie. Sans celui-ci, la jauge d'un allié reste inconnue
        // alors qu'il est sous les yeux.
        //
        // Un membre HORS DE VUE n'est pas dans le monde, et le serveur l'annonce
        // quand même — la fenêtre de groupe le montre où qu'il soit. Ce client
        // n'a pas encore cette fenêtre : la trame est alors comprise et sans
        // effet, ce qui n'est pas la même chose qu'un opcode à porter.
        return world.SetHealth(packet.AID, packet.hp, packet.maxhp) ? FrameOutcome::kApplied
                                                                    : FrameOutcome::kIgnored;
      });

    case proto::ZcStoreEntry::kOpcode:
      return Decoded<proto::ZcStoreEntry>(frame, [&world](const auto& packet) {
        // L'ENSEIGNE D'UNE ÉCHOPPE, au-dessus du joueur qui vend. Son `makerAID`
        // est l'identifiant de compte du vendeur, lequel EST son identifiant de
        // bloc : le serveur y écrit `sd.status.account_id`, et c'est la même
        // valeur que le paquet d'unité a déjà donnée.
        sim::Signboard board;
        board.kind = sim::SignboardKind::kVendingShop;
        board.title = core::Utf8FromCp1252(packet.storeName);
        return world.SetSignboard(packet.makerAID, std::move(board)) ? FrameOutcome::kApplied
                                                                     : FrameOutcome::kIgnored;
      });

    case proto::ZcRoomNewentry::kOpcode:
      return Decoded<proto::ZcRoomNewentry>(frame, [&world](const auto& packet) {
        // UN SALON OUVERT, au-dessus de qui l'a ouvert — un joueur, ou un PNJ
        // qui tient une arche de guilde ou une arène.
        //
        // ⚠ `owner` EST L'ACTEUR, `id` EST LE SALON. Les deux sont des entiers de
        // même largeur côte à côte dans le paquet, et seul le second permettra de
        // le fermer.
        sim::Signboard board;
        board.kind = sim::SignboardKind::kChatRoom;
        board.title = core::Utf8FromCp1252(packet.title);
        board.room_id = static_cast<std::uint32_t>(packet.id);
        board.users = packet.users;
        board.limit = packet.limit;
        board.status = packet.type;
        return world.SetSignboard(static_cast<std::uint32_t>(packet.owner), std::move(board))
                   ? FrameOutcome::kApplied
                   : FrameOutcome::kIgnored;
      });

    case proto::ZcDisappearEntry::kOpcode:
      return Decoded<proto::ZcDisappearEntry>(frame, [&world](const auto& packet) {
        // L'échoppe ferme. Le serveur nomme son VENDEUR (`bl.id`), pas une
        // enseigne : voir Signboard.
        return world.ClearSignboard(static_cast<std::uint32_t>(packet.GID))
                   ? FrameOutcome::kApplied
                   : FrameOutcome::kIgnored;
      });

    case proto::ZcDestroyRoom::kOpcode:
      return Decoded<proto::ZcDestroyRoom>(frame, [&world](const auto& packet) {
        // Le salon ferme, et il se nomme LUI-MÊME. C'est la seule trame d'enseigne
        // qui ne désigne pas un acteur.
        return world.ClearRoom(static_cast<std::uint32_t>(packet.chatId)) ? FrameOutcome::kApplied
                                                                          : FrameOutcome::kIgnored;
      });

    case proto::ZcNotifyVanish::kOpcode:
      return Decoded<proto::ZcNotifyVanish>(frame, [&world, now_ms](const auto& packet) {
        return ApplyVanish(world, packet.gid, packet.type, now_ms) ? FrameOutcome::kApplied
                                                                   : FrameOutcome::kIgnored;
      });

    case proto::ZcNotifyAct::kOpcode:
      return Decoded<proto::ZcNotifyAct>(frame, [&world, now_ms](const auto& packet) {
        // S'asseoir et se relever passent par le paquet des coups.
        if (packet.type == kActionSitDown || packet.type == kActionStandUp) {
          const sim::Posture posture =
              packet.type == kActionSitDown ? sim::Posture::kSitting : sim::Posture::kStanding;
          return world.SetPosture(static_cast<std::uint32_t>(packet.srcID), posture, now_ms)
                     ? FrameOutcome::kApplied
                     : FrameOutcome::kIgnored;
        }
        // Ramasser : on se baisse ; l'objet, lui, s'en va par son propre paquet.
        if (packet.type == kActionPickUp) {
          return world.RecordPickup(static_cast<std::uint32_t>(packet.srcID), now_ms)
                     ? FrameOutcome::kApplied
                     : FrameOutcome::kIgnored;
        }
        // Les coups que le client ne joue pas attendent.
        if (!Listed(kBlowActions, packet.type)) {
          return FrameOutcome::kUnhandled;
        }
        sim::Strike strike;
        strike.attacker = static_cast<std::uint32_t>(packet.srcID);
        strike.target = static_cast<std::uint32_t>(packet.targetID);
        strike.started_ms = now_ms;
        strike.attack_motion_ms = static_cast<std::uint32_t>(std::max(packet.srcSpeed, 0));
        strike.hurt_motion_ms = static_cast<std::uint32_t>(std::max(packet.dmgSpeed, 0));
        strike.flinches = packet.damage > 0 && !Listed(kEnduredBlowActions, packet.type);
        strike.damage = packet.damage;
        strike.second_damage = packet.damage2;
        strike.hits = packet.div;
        strike.type = packet.type;
        strike.sp_damage = packet.isSPDamage != 0;
        return world.RecordStrike(strike) ? FrameOutcome::kApplied : FrameOutcome::kIgnored;
      });

    case proto::ZcResurrection::kOpcode:
      return Decoded<proto::ZcResurrection>(frame, [&world, now_ms](const auto& packet) {
        return world.SetPosture(packet.gid, sim::Posture::kStanding, now_ms)
                   ? FrameOutcome::kApplied
                   : FrameOutcome::kIgnored;
      });

    case proto::ZcAttackRange::kOpcode:
      return Decoded<proto::ZcAttackRange>(frame, [&world](const auto& packet) {
        world.SetPlayerAttackRange(packet.currentAttRange);
        return FrameOutcome::kApplied;
      });

    case proto::ZcHighjump::kOpcode:
      return Decoded<proto::ZcHighjump>(frame, [&world, now_ms](const auto& packet) {
        // LE RECUL — et le saut du taekwon, que le serveur annonce pareil.
        //
        // Le serveur l'émet EN PAIRE avec l'arrêt (clif_blown appelle
        // clif_slide puis clif_fixpos), et les deux nomment la même case : le
        // premier y jette l'acteur, le second confirme.
        //
        // ⚠ Ce paquet-ci ne construit AUCUN chemin. Le client écrit la position
        // dessinée de l'acteur telle quelle : il ne regarde ni la
        // grille de franchissement, ni la posture. Une case infranchissable se
        // prend donc, et qui était assis reste assis. L'arrêt, lui, fait GLISSER
        // l'acteur — son message 19 bâtit un chemin de marche — mais quand
        // la paire arrive, il glisse depuis la case où l'on est déjà, et ce
        // glissement-là ne se voit pas.
        const sim::Actor* actor = world.Find(packet.srcId);
        if (actor == nullptr) {
          return FrameOutcome::kIgnored;
        }
        // La direction n'est pas dite : celle qu'on MONTRE est gardée, comme
        // pour l'arrêt. ⚠ Et c'est bien la montrée, pas `position.direction` :
        // en pleine marche, celui-ci porte encore le regard d'AVANT le départ.
        world.PlaceAt(packet.srcId,
                      CellPosition{packet.x, packet.y, sim::FacingAt(*actor, now_ms)});
        return FrameOutcome::kApplied;
      });

    case proto::ZcStopmove::kOpcode:
      return Decoded<proto::ZcStopmove>(frame, [&world, now_ms](const auto& packet) {
        const sim::Actor* actor = world.Find(packet.AID);
        if (actor == nullptr) {
          return FrameOutcome::kIgnored;
        }
        // L'arrêt ne dit pas la direction : celle qu'on MONTRE est conservée.
        // 🔴 ET NON `position.direction`, qui ne vaut PAS ce que l'acteur
        // montre pendant une marche : le déplacement n'en porte aucune, si
        // bien que ce champ garde le regard d'AVANT le départ. Le relire ici
        // faisait pivoter l'acteur à l'arrêt, souvent vers le nord. Et l'arrêt
        // est une ARRIVÉE : le client y bâtit un chemin, qui s'achève.
        world.PlaceAt(packet.AID,
                      CellPosition{packet.xPos, packet.yPos, sim::FacingAt(*actor, now_ms)},
                      now_ms);
        return FrameOutcome::kApplied;
      });

    case proto::ZcChangeDirection::kOpcode:
      return Decoded<proto::ZcChangeDirection>(frame, [&world](const auto& packet) {
        // Le paquet porte les DEUX directions : celle du corps et celle de la
        // tête. C'est par la seconde que passe /doridori.
        //
        // ⚠ ET LE CORPS COMPTE AUTANT QUE LA TÊTE. Le déplacement annoncé
        // survivant à l'arrivée, la direction posée ici n'était visible que
        // sur la tête tant que le monde ne notait pas le demi-tour : voir
        // World::Turn et Actor::turned_since_walk_began.
        return world.Turn(packet.srcId, packet.dir, static_cast<std::uint8_t>(packet.headDir))
                   ? FrameOutcome::kApplied
                   : FrameOutcome::kIgnored;
      });

    case proto::ZcNotifyPlayermove::kOpcode:
      return Decoded<proto::ZcNotifyPlayermove>(frame, [&world, now_ms](const auto& packet) {
        // Ce paquet ne nomme personne : il ne parle que du joueur, et son
        // identifiant vient de la phase de connexion.
        if (world.player_id() == 0) {
          return FrameOutcome::kIgnored;
        }
        world.StartMoving(world.player_id(), sim::UnpackMovement(packet.moveData), now_ms);
        return FrameOutcome::kApplied;
      });

    case proto::ZcNpcackMapmove::kOpcode:
      return Decoded<proto::ZcNpcackMapmove>(frame, [&world](const auto& packet) {
        // Changer de carte remplace TOUT sauf le joueur : les acteurs de
        // l'ancienne n'ont rien à faire sur la nouvelle, et le serveur les
        // réannonce. Le paquet ne dit pas la direction du regard ; elle se
        // garde, faute de mieux et comme chez le client.
        world.EnterMap(MapBaseName(packet.mapName), packet.xPos, packet.yPos);
        return FrameOutcome::kApplied;
      });

    case proto::ZcNpcackServermove::kOpcode:
      return Decoded<proto::ZcNpcackServermove>(frame, [&world](const auto& packet) {
        // LA MÊME ENTRÉE, SERVIE PAR UN AUTRE PROCESSUS : le monde change de
        // carte ici, et la liaison se refait chez l'appelant — voir
        // `ServerMoveOf`.
        world.EnterMap(MapBaseName(packet.mapName), packet.xPos, packet.yPos);
        return FrameOutcome::kApplied;
      });

    case proto::ZcSpriteChange::kOpcode:
      return Decoded<proto::ZcSpriteChange>(frame, [&world, now_ms](const auto& packet) {
        const sim::Actor* actor = world.Find(packet.AID);
        if (actor == nullptr) {
          return ApplySkillUnitLook(world, packet.AID, packet.type, packet.val, now_ms);
        }
        sim::Appearance look = actor->look;
        if (!Restyled(look, packet.type, packet.val, packet.val2)) {
          return FrameOutcome::kIgnored;
        }
        world.SetLook(packet.AID, look);
        return FrameOutcome::kApplied;
      });

    // LES EFFETS DE CHAPEAU : ce qu'un costume, une monture ou une aura
    // d'événement fait paraître autour de celui qui le porte.
    //
    // ⚠ LE SERVEUR ENVOIE CE PAQUET DE DEUX FAÇONS, et la seconde est la raison
    // de ne pas le lire comme un REMPLACEMENT : `clif_hat_effects` envoie la
    // liste entière avec `status` à un, à l'entrée en vue ; `clif_hat_effect_
    // single` en envoie UN seul, posé ou retiré. Traduire la liste comme « voici
    // tout ce qu'il porte » marcherait pour la première et effacerait tout à la
    // seconde.
    case proto::ZcEquipmentEffect::kOpcode:
      return Decoded<proto::ZcEquipmentEffect>(frame, [&world](const auto& packet) {
        bool known = false;
        for (const std::uint16_t effect : packet.effects) {
          known = world.SetHatEffect(packet.aid, static_cast<sim::HatEffectId>(effect),
                                     packet.status != 0) ||
                  known;
        }
        return known ? FrameOutcome::kApplied : FrameOutcome::kIgnored;
      });

    case proto::ZcStateChange::kOpcode:
      return Decoded<proto::ZcStateChange>(frame, [&world, now_ms](const auto& packet) {
        const bool known = world.SetStates(packet.AID, static_cast<std::uint16_t>(packet.bodyState),
                                           static_cast<std::uint16_t>(packet.healthState),
                                           static_cast<std::uint32_t>(packet.effectState), now_ms,
                                           packet.isPKModeON != 0);
        return known ? FrameOutcome::kApplied : FrameOutcome::kIgnored;
      });
    // CE QU'UNE COMPÉTENCE MONTRE, sphères et Devotion compris : voir world_skill.h.
    case proto::ZcBladestop::kOpcode:
    case proto::ZcUseskillAck::kOpcode:
    case proto::ZcDispel::kOpcode:
    case proto::ZcNotifySkill::kOpcode:
    case proto::ZcNotifySkillPosition::kOpcode:
    case proto::ZcUseSkill::kOpcode:
    case proto::ZcNotifyGroundskill::kOpcode:
    case proto::SkillUnitNotice::kOpcode:
    case proto::PacketGraffitiEntry::kOpcode:
    case proto::ZcSkillDisappear::kOpcode:
    case proto::ZcNotifyEffect::kOpcode:
    case proto::SpecialEffectNotice::kOpcode:
    case proto::ValuedEffectNotice::kOpcode:
    case proto::ZcRemoveEffect::kOpcode:
    case proto::ZcSkillScale::kOpcode:
    case proto::ZcAckTouseskill::kOpcode:
    case proto::ZcSpirits::kOpcode:
    case proto::ZcSpiritsAttribute::kOpcode:
    case proto::ZcCombodelay::kOpcode:
    case proto::DevotionListNotice::kOpcode:
      return ApplyWorldSkillFrame(frame, world, now_ms);

    // LA BARRE DE RACCOURCIS DU SERVEUR : REGARDÉE, PUIS LAISSÉE.
    // ⚠ IGNORÉE, ET NON « NON TRAITÉE ». Le jeu d'origine garde ces cases sur
    // son serveur web ; nous gardons les nôtres chez le joueur, et
    // `ro/sim/shortcut_bar.h` dit pourquoi. La décision est prise et se relit :
    // la ranger parmi le travail restant ferait chercher, un jour, ce qu'il n'y
    // a pas à faire.
    case proto::ZcShortcutKeyList::kOpcode:
      return FrameOutcome::kIgnored;

    // LES NOMBRES DU PERSONNAGE : paramètres, paire, feuille, accusé d'une
    // montée, prix du point suivant — voir ro/net/character.h.
    //
    // ⚠ CE FICHIER-CI TRADUIT CE QUI ARRIVE À UN ACTEUR, et ces sept-là parlent
    // du JOUEUR. C'est la phrase d'en-tête qui a décidé de la coupure, pas le
    // plafond de lignes — même si le plafond l'a rendue urgente.
    case proto::ZcParChange::kOpcode:
    case proto::ZcLongparChange::kOpcode:
    case proto::ZcLonglongparChange::kOpcode:
    case proto::ZcCouplestatus::kOpcode:
    case proto::ZcStatus::kOpcode:
    case proto::ZcStatusChangeAck::kOpcode:
    case proto::ZcStatusChange::kOpcode:
      return ApplyCharacterFrame(frame, world);

    // Le GRIMOIRE : ce que le personnage sait lancer, à part du monde.
    case proto::ZcSkillinfoList::kOpcode:
    case proto::ZcAddSkill::kOpcode:
    case proto::ZcSkillinfoUpdate2::kOpcode:
    case proto::ZcSkillinfoDelete::kOpcode:
    case proto::ZcSkillPostdelay::kOpcode:
    case proto::ZcAutorunSkill::kOpcode:
      return ApplySkillFrame(frame, world.skill_book(), now_ms);

    // LE DIALOGUE D'UN PNJ : ce qu'il dit, ce qu'il demande, et ce qu'on lui
    // répond — voir ro/net/npc_dialog.h. C'est la seule famille où le client
    // PARLE en retour.
    case proto::ZcSayDialog::kOpcode:
    case proto::ZcSayDialog2::kOpcode:
    case proto::ZcWaitDialog::kOpcode:
    case proto::ZcWaitDialog2::kOpcode:
    case proto::ZcCloseDialog::kOpcode:
    case proto::ZcMenuList::kOpcode:
    case proto::ZcClearDialog::kOpcode:
    case proto::ZcOpenEditdlg::kOpcode:
    case proto::ZcOpenEditdlgstr::kOpcode:
    case proto::ZcDialogWindowSize::kOpcode:
    case proto::ZcDialogWindowPos::kOpcode:
      return ApplyNpcDialogFrame(frame, world, now_ms);

    case proto::PacketStatusChange::kOpcode:
      return Decoded<proto::PacketStatusChange>(frame, [&world, now_ms](const auto& packet) {
        return StatusApplied(world, packet, now_ms);
      });

    case proto::PacketStatusChange2::kOpcode:
      return Decoded<proto::PacketStatusChange2>(frame, [&world, now_ms](const auto& packet) {
        return StatusApplied(world, packet, now_ms);
      });

    case proto::ZcEfstSetEnter::kOpcode:
      return Decoded<proto::ZcEfstSetEnter>(frame, [&world, now_ms](const auto& packet) {
        // LES ÉTATS D'UN ACTEUR QUI ENTRE EN VUE. Le serveur en envoie un par
        // trame, ceux qui sont ENCORE POSÉS, juste APRÈS le paquet d'unité :
        // l'acteur est donc déjà là quand ceci arrive (clif_getareachar_unit).
        // Il n'y a pas de « retiré » dans ce paquet, et il n'en manque pas un :
        // la levée d'un état passe toujours par ZC_MSG_STATE_CHANGE.
        // ⚠ `type` SE NUMÉROTE COMME L'`index` DE ZC_EFST_SET. Le serveur y écrit
        // `status_db.getIcon()`, la même table d'icônes que le paquet de
        // transition, et ses VALEURS avec : le type d'une charrette n'arrive que là.
        // Seul le sommeil profond s'y date : il endort à l'entrée en vue.
        const auto type = static_cast<sim::StatusEffectId>(packet.type);
        const auto since_ms = type == sim::kEfstDeepSleep ? std::optional(now_ms) : std::nullopt;
        return world.SetStatusEffect(packet.targetID, type, true, packet.val1, packet.val2,
                                     since_ms)
                   ? FrameOutcome::kApplied
                   : FrameOutcome::kIgnored;
      });

    case proto::PacketStatusChangeEnd::kOpcode:
      return Decoded<proto::PacketStatusChangeEnd>(frame, [&world, now_ms](const auto& packet) {
        return StatusApplied(world, packet, now_ms);
      });

    // ⚠ CES QUATRE-LÀ SE NOMMENT EXPLICITEMENT parce qu'ils ne se distinguent pas
    // d'un paquet de monde par leur seul opcode, et que l'aiguillage par défaut
    // ne doit pas décider pour eux.
    case proto::WhisperNotice::kOpcode:
    case proto::ZcNotifyChatParty::kOpcode:
    case proto::ZcGuildChat::kOpcode:
    case proto::ColoredNotice::kOpcode:
      return ApplyNoticeFrame(frame, world, now_ms);

    default:
      break;
  }

  // CE QUI N'EST PAS UN FAIT D'ACTEUR part vers sa famille — voir
  // `world_relay.h`. Le plafond de lignes a sonné sur ce fichier, et la
  // couture était déjà écrite dedans : d'un côté un aiguillage sur les paquets
  // du MONDE, de l'autre une chaîne de relais qui ne décode rien.
  return RelayFrame(frame, world, now_ms);
}

}  // namespace ro::net
