// LE CHARIOT, LA MONTURE, LE FAUCON — décodés, et ce qu'on leur demande.
//
// 🔴 CE BANC PORTE TROIS CHOSES QUI NE SE DEVINENT PAS, et dont deux sont la
// RAISON D'ÊTRE de ce paquet :
//
//   · le chariot porte un TYPE, pas un drapeau : zéro est « aucun », un à N
//     sont autant d'apparences, et le réduire à un booléen perdrait laquelle ;
//   · un niveau à ZÉRO veut dire « compétence pas apprise », donc PAS DE
//     CASE — et non une case grisée sur un métier qui n'en aura jamais ;
//   · les identifiants de compétence VIENNENT DU FIL, pour qu'aucun ne soit
//     écrit en dur : c'est par eux qu'on retrouve l'icône.

#include "ro/net/companion.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/skill.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/companion.h"
#include "ro/sim/world.h"

#include "support/net_frames.h"

using ro::net::ApplyCompanionFrame;
using ro::net::ApplySkillFrame;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::sim::CompanionActive;
using ro::sim::CompanionKind;
using ro::sim::CompanionLevelOf;
using ro::sim::CompanionSkillOf;
using ro::sim::World;
using ro::test_support::FrameOf;

namespace {

// Les numéros AEGIS que ce serveur emploie : `MC_PUSHCART`, `KN_RIDING`,
// `HT_FALCON`. ⚠ ÉCRITS ICI ET NULLE PART DANS LE CLIENT : c'est justement ce
// que le paquet évite.
constexpr std::uint16_t kPushCart = 39;
constexpr std::uint16_t kRiding = 63;
constexpr std::uint16_t kFalcon = 128;

[[nodiscard]] Frame StateFrame(ro::proto::ZcMoonlightCompanionState packet) {
  return Frame{.opcode = ro::proto::ZcMoonlightCompanionState::kOpcode,
               .bytes = ro::proto::Encode(packet)};
}

// Une ligne du grimoire : seuls l'identifiant et le rang comptent ici.
[[nodiscard]] ro::proto::Skilldata Line(std::uint16_t skill_id, std::uint16_t level) {
  ro::proto::Skilldata data;
  data.id = skill_id;
  data.level = level;
  return data;
}

}  // namespace

TEST_CASE("Compagnon : le chariot porte un TYPE, pas un drapeau", "[net][companion]") {
  // 🔴 ET C'EST CE QUI LE SÉPARE DES DEUX AUTRES. Le chariot a plusieurs
  // apparences ; savoir qu'il est là ne dit pas laquelle il porte, et le
  // changer de décoration demande justement ce nombre.
  World world;
  ro::proto::ZcMoonlightCompanionState packet;
  packet.pushcart_lv = 10;
  packet.cart_active = 4;
  packet.cart_deco_max = 5;
  packet.pushcart_id = kPushCart;
  CHECK(ApplyCompanionFrame(StateFrame(packet), world) == FrameOutcome::kApplied);

  CHECK(world.companions().cart_type == 4);
  CHECK(world.companions().cart_decoration_most == 5);
  CHECK(CompanionActive(world.companions(), CompanionKind::kCart));
}

TEST_CASE("Compagnon : un niveau à ZÉRO veut dire PAS DE CASE", "[net][companion]") {
  // ⚠ ET NON UNE CASE GRISÉE : un métier qui n'aura jamais de faucon ne doit
  // pas en voir une. C'est le niveau, et lui seul, qui décide de l'existence de
  // la case ; l'état ne décide que de son aspect.
  World world;
  ro::proto::ZcMoonlightCompanionState packet;
  packet.riding_lv = 1;
  packet.riding_active = 1;
  packet.pushcart_id = kPushCart;
  packet.riding_id = kRiding;
  packet.falcon_id = kFalcon;
  CHECK(ApplyCompanionFrame(StateFrame(packet), world) == FrameOutcome::kApplied);
  // ⚠ LE CHARIOT EST DANS LE GRIMOIRE, MAIS AU NIVEAU ZÉRO : le serveur y range
  // ce qu'on pourrait apprendre, et ce n'est pas l'avoir appris.
  ro::proto::ZcSkillinfoList list;
  list.skills.push_back(Line(kRiding, 1));
  list.skills.push_back(Line(kPushCart, 0));
  CHECK(ApplySkillFrame(FrameOf(Encode(list)), world.skill_book(), 0) == FrameOutcome::kApplied);

  CHECK(CompanionLevelOf(world.companions(), world.skill_book(), CompanionKind::kMount) == 1);
  CHECK(CompanionActive(world.companions(), CompanionKind::kMount));
  CHECK(CompanionLevelOf(world.companions(), world.skill_book(), CompanionKind::kCart) == 0);
  CHECK(CompanionLevelOf(world.companions(), world.skill_book(), CompanionKind::kFalcon) == 0);
  CHECK_FALSE(CompanionActive(world.companions(), CompanionKind::kFalcon));
}

TEST_CASE("Compagnon : un changement de métier fait paraître la case sans que le paquet reparte",
          "[net][companion]") {
  // 🔴 LE SERVEUR NE RENVOIE PAS L'ÉTAT DES COMPAGNONS QUAND LE MÉTIER CHANGE :
  // il ne le fait qu'à la connexion et quand un compagnon bascule. Seul le
  // grimoire suit. Le niveau lu dans le paquet laisserait un marchand sans
  // chariot jusqu'à la reconnexion — et un chevalier redevenu novice avec le
  // sien.
  World world;
  ro::proto::ZcMoonlightCompanionState packet;
  packet.pushcart_id = kPushCart;
  packet.riding_id = kRiding;
  packet.falcon_id = kFalcon;
  CHECK(ApplyCompanionFrame(StateFrame(packet), world) == FrameOutcome::kApplied);
  REQUIRE(CompanionLevelOf(world.companions(), world.skill_book(), CompanionKind::kCart) == 0);

  ro::proto::ZcSkillinfoList merchant;
  merchant.skills.push_back(Line(kPushCart, 1));
  CHECK(ApplySkillFrame(FrameOf(Encode(merchant)), world.skill_book(), 0) ==
        FrameOutcome::kApplied);
  CHECK(CompanionLevelOf(world.companions(), world.skill_book(), CompanionKind::kCart) == 1);

  // Le sens inverse : la liste d'un nouveau métier la retire.
  CHECK(ApplySkillFrame(FrameOf(Encode(ro::proto::ZcSkillinfoList{})), world.skill_book(), 0) ==
        FrameOutcome::kApplied);
  CHECK(CompanionLevelOf(world.companions(), world.skill_book(), CompanionKind::kCart) == 0);
}

TEST_CASE("Compagnon : sans identifiant du serveur, aucune case", "[net][companion]") {
  // Tant que le paquet des compagnons n'est pas arrivé, rien ne dit quelle
  // compétence les appelle : le grimoire ne suffit pas, et aucun numéro n'est
  // écrit dans le client pour le suppléer.
  World world;
  ro::proto::ZcSkillinfoList list;
  list.skills.push_back(Line(kPushCart, 10));
  CHECK(ApplySkillFrame(FrameOf(Encode(list)), world.skill_book(), 0) == FrameOutcome::kApplied);

  CHECK(CompanionLevelOf(world.companions(), world.skill_book(), CompanionKind::kCart) == 0);
}

TEST_CASE("Compagnon : les identifiants de compétence viennent du FIL", "[net][companion]") {
  // 🔴 C'EST PAR EUX QU'ON RETROUVE L'ICÔNE, et les graver dans le client
  // ferait mentir la case le jour où le serveur renumérote. Trois nombres sur
  // le fil valent mieux que trois constantes dans deux dépôts.
  World world;
  ro::proto::ZcMoonlightCompanionState packet;
  packet.pushcart_lv = 10;
  packet.riding_lv = 1;
  packet.falcon_lv = 1;
  packet.pushcart_id = kPushCart;
  packet.riding_id = kRiding;
  packet.falcon_id = kFalcon;
  CHECK(ApplyCompanionFrame(StateFrame(packet), world) == FrameOutcome::kApplied);

  CHECK(CompanionSkillOf(world.companions(), CompanionKind::kCart) == kPushCart);
  CHECK(CompanionSkillOf(world.companions(), CompanionKind::kMount) == kRiding);
  CHECK(CompanionSkillOf(world.companions(), CompanionKind::kFalcon) == kFalcon);
}

TEST_CASE("Compagnon : la décoration demande SA compétence à elle", "[net][companion]") {
  // `MC_CHANGECART` est distincte de `MC_PUSHCART` : on peut pousser un chariot
  // sans avoir le droit d'en changer l'apparence, et le serveur revérifie les
  // deux. Sans elle, il n'y a pas de menu de décoration à proposer.
  World world;
  ro::proto::ZcMoonlightCompanionState packet;
  packet.pushcart_lv = 10;
  packet.changecart_lv = 0;
  packet.pushcart_id = kPushCart;
  CHECK(ApplyCompanionFrame(StateFrame(packet), world) == FrameOutcome::kApplied);
  CHECK(world.companions().change_cart_level == 0);

  packet.changecart_lv = 1;
  CHECK(ApplyCompanionFrame(StateFrame(packet), world) == FrameOutcome::kApplied);
  CHECK(world.companions().change_cart_level == 1);
}

TEST_CASE("Compagnon : l'état REMPLACE, il ne se cumule pas", "[net][companion]") {
  // Le serveur pousse l'état ENTIER à chaque changement. Fusionner laisserait
  // une monture montée après qu'on en est descendu.
  World world;
  ro::proto::ZcMoonlightCompanionState mounted;
  mounted.riding_lv = 1;
  mounted.riding_active = 1;
  mounted.riding_id = kRiding;
  CHECK(ApplyCompanionFrame(StateFrame(mounted), world) == FrameOutcome::kApplied);
  REQUIRE(CompanionActive(world.companions(), CompanionKind::kMount));

  ro::proto::ZcMoonlightCompanionState pied;
  pied.riding_lv = 1;
  pied.riding_active = 0;
  pied.riding_id = kRiding;
  CHECK(ApplyCompanionFrame(StateFrame(pied), world) == FrameOutcome::kApplied);
  CHECK_FALSE(CompanionActive(world.companions(), CompanionKind::kMount));
}

TEST_CASE("Compagnon : un autre opcode ne lui appartient pas", "[net][companion]") {
  World world;
  const Frame frame{.opcode = 0x0000, .bytes = {}};
  CHECK(ApplyCompanionFrame(frame, world) == FrameOutcome::kUnhandled);
}
