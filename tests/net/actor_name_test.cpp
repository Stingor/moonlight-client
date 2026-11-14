// LE NOM AU-DESSUS D'UN ACTEUR, décodé.
//
// 🔴 CE BANC PORTE QUATRE CHOSES QU'AUCUNE SESSION NE MONTRERAIT VITE, et qui se
// ressemblent toutes à l'écran — un nom plausible, ou rien :
//
//   · la carte REMPLACE le nom de l'apparition, parce qu'elle seule porte ce
//     que le serveur veut MONTRER — un pseudo d'emprunt ; mais l'étiquette
//     d'absence ne fait que se peindre, et le nom reste celui qu'on invite ;
//   · un acteur déguisé est nommé sous l'OPPOSÉ de son identifiant, et cela
//     retombe juste sans rien faire ;
//   · une carte VIDE se pose quand même : « le serveur a répondu, il n'y a
//     rien » n'est pas « on n'a pas encore demandé », et c'est cette
//     distinction qui empêche de redemander sans fin ;
//   · un PNJ passe par l'autre paquet, et ses trois autres chaînes sont vides
//     parce que le client natif les lui passe vides.

#include "ro/net/actor_name.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/text.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/actor.h"
#include "ro/sim/world.h"

using ro::net::ApplyActorNameFrame;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::sim::Actor;
using ro::sim::ActorKind;
using ro::sim::World;

namespace {

constexpr std::uint32_t kPlayer = 2000123;
constexpr std::uint32_t kStranger = 2000999;

[[nodiscard]] World WithActor(std::uint32_t id, ActorKind kind, std::string name) {
  World world;
  Actor actor;
  actor.id = id;
  actor.kind = kind;
  actor.name = std::move(name);
  world.Spawn(std::move(actor));
  return world;
}

[[nodiscard]] Frame NameAllFrame(std::int32_t gid, std::string name, std::string party,
                                 std::string guild, std::string position, std::int32_t title_id) {
  ro::proto::ZcAckReqnameall packet;
  packet.gid = gid;
  packet.name = std::move(name);
  packet.party_name = std::move(party);
  packet.guild_name = std::move(guild);
  packet.position_name = std::move(position);
  packet.title_id = title_id;
  return Frame{.opcode = ro::proto::ZcAckReqnameall::kOpcode, .bytes = ro::proto::Encode(packet)};
}

}  // namespace

TEST_CASE("Nom d'acteur : la carte REMPLACE le nom de l'apparition", "[net][name]") {
  // Un pseudo d'emprunt : le serveur ne montre plus que lui.
  World world = WithActor(kPlayer, ActorKind::kPlayer, "Stingor");
  const Frame frame = NameAllFrame(static_cast<std::int32_t>(kPlayer), "Masque", "", "", "", 0);
  CHECK(ApplyActorNameFrame(frame, world) == FrameOutcome::kApplied);

  const Actor* actor = world.Find(kPlayer);
  REQUIRE(actor != nullptr);
  CHECK(actor->name == "Masque");
  REQUIRE(actor->card.has_value());
  CHECK(actor->card->shown_name.empty());
}

TEST_CASE("Nom d'acteur : l'absence se PEINT, elle ne renomme pas", "[net][name]") {
  // 🔴 LE SCÉNARIO DE LA PREMIÈRE INVITATION : un joueur inactif depuis une
  // minute porte « [AFK] » devant son pseudo, et le serveur ne l'écrit QUE
  // dans la carte, demandée au premier survol — donc juste avant le clic droit.
  // Si elle renommait l'acteur, l'invitation partirait sous « [AFK] Stingor »,
  // que le serveur ne trouve pas : « pas connecté ou n'existe pas ».
  World world = WithActor(kPlayer, ActorKind::kPlayer, "Stingor");
  const Frame frame = NameAllFrame(static_cast<std::int32_t>(kPlayer), "[AFK] Stingor", "",
                                   "Moonlight-Destiny", "Handyman", 1003);
  CHECK(ApplyActorNameFrame(frame, world) == FrameOutcome::kApplied);

  const Actor* actor = world.Find(kPlayer);
  REQUIRE(actor != nullptr);
  CHECK(actor->name == "Stingor");
  REQUIRE(actor->card.has_value());
  CHECK(actor->card->shown_name == "[AFK] Stingor");

  // Ce que l'invitation par le nom écrit sur le fil.
  ro::proto::CzPartyJoinReq invite;
  invite.name = ro::core::Cp1252FromUtf8(actor->name);
  ro::proto::CzPartyJoinReq sent;
  REQUIRE(ro::proto::Decode(ro::proto::Encode(invite), sent));
  CHECK(sent.name == "Stingor");

  // Au retour, le serveur renvoie la carte sans l'étiquette.
  const Frame back = NameAllFrame(static_cast<std::int32_t>(kPlayer), "Stingor", "",
                                  "Moonlight-Destiny", "Handyman", 1003);
  CHECK(ApplyActorNameFrame(back, world) == FrameOutcome::kApplied);
  actor = world.Find(kPlayer);
  REQUIRE(actor != nullptr);
  CHECK(actor->name == "Stingor");
  REQUIRE(actor->card.has_value());
  CHECK(actor->card->shown_name.empty());
}

TEST_CASE("Nom d'acteur : l'étiquette d'absence ampute un pseudo long", "[net][name]") {
  // Vingt-trois octets de champ : six pour l'étiquette, dix-sept pour le pseudo.
  World world = WithActor(kPlayer, ActorKind::kPlayer, "Abcdefghijklmnopqrstu");
  const Frame frame =
      NameAllFrame(static_cast<std::int32_t>(kPlayer), "[AFK] Abcdefghijklmnopq", "", "", "", 0);
  CHECK(ApplyActorNameFrame(frame, world) == FrameOutcome::kApplied);

  const Actor* actor = world.Find(kPlayer);
  REQUIRE(actor != nullptr);
  CHECK(actor->name == "Abcdefghijklmnopqrstu");
  REQUIRE(actor->card.has_value());
  CHECK(actor->card->shown_name == "[AFK] Abcdefghijklmnopq");
}

TEST_CASE("Nom d'acteur : un pseudo qui COMMENCE par « [AFK] » reste un nom", "[net][name]") {
  // Présent, sa carte porte son vrai nom ; absent, elle porterait
  // « [AFK] [AFK] Bob ». Seul le nom connu derrière l'étiquette les distingue.
  World world = WithActor(kPlayer, ActorKind::kPlayer, "[AFK] Bob");
  const Frame frame = NameAllFrame(static_cast<std::int32_t>(kPlayer), "[AFK] Bob", "", "", "", 0);
  CHECK(ApplyActorNameFrame(frame, world) == FrameOutcome::kApplied);

  const Actor* actor = world.Find(kPlayer);
  REQUIRE(actor != nullptr);
  CHECK(actor->name == "[AFK] Bob");
  REQUIRE(actor->card.has_value());
  CHECK(actor->card->shown_name.empty());
}

TEST_CASE("Nom d'acteur : la carte porte groupe, guilde, rang et titre", "[net][name]") {
  World world = WithActor(kPlayer, ActorKind::kPlayer, "Stingor");
  const Frame frame = NameAllFrame(static_cast<std::int32_t>(kPlayer), "Stingor", "",
                                   "Moonlight-Destiny", "Handyman", 1003);
  CHECK(ApplyActorNameFrame(frame, world) == FrameOutcome::kApplied);

  const Actor* actor = world.Find(kPlayer);
  REQUIRE(actor != nullptr);
  REQUIRE(actor->card.has_value());
  CHECK(actor->card->banner == "Moonlight-Destiny");
  CHECK(actor->card->badge == "Handyman");
  CHECK(actor->card->title_id == 1003);
  // ⚠ LE TITRE N'EST QU'UN NUMÉRO SUR CE PAQUET : le libellé vit dans une table
  // de l'installation, que cette couche ne lit pas.
  CHECK(actor->card->title.empty());
}

TEST_CASE("Nom d'acteur : le champ du GROUPE porte la vie d'un monstre", "[net][name]") {
  // 🔴 LE SERVEUR DÉTOURNE SES PROPRES CHAMPS, et c'est délibéré de sa part :
  // sous `show_mob_info`, le niveau et la vie partent dans le champ du groupe,
  // la race dans celui de la guilde, l'élément dans celui du rang. Son
  // commentaire le dit — « we send it as one so the client can parse it ».
  // Nommer ces champs d'après le joueur ferait chercher un groupe là où il y a
  // une barre de vie.
  World world = WithActor(kPlayer, ActorKind::kMonster, "Poring");
  const Frame frame = NameAllFrame(static_cast<std::int32_t>(kPlayer), "Poring", "Lv. 12 | HP: 63%",
                                   "Plant", "Water", 0);
  CHECK(ApplyActorNameFrame(frame, world) == FrameOutcome::kApplied);

  const Actor* actor = world.Find(kPlayer);
  REQUIRE(actor != nullptr);
  REQUIRE(actor->card.has_value());
  CHECK(actor->card->aside == "Lv. 12 | HP: 63%");
  CHECK(actor->card->banner == "Plant");
  CHECK(actor->card->badge == "Water");
}

TEST_CASE("Nom d'acteur : un acteur DÉGUISÉ est nommé sous l'opposé de son numéro", "[net][name]") {
  // `disguised_bl_id` rend `-(int32)id`, et le paquet d'APPARITION emploie
  // exactement le même calcul : les deux se retrouvent donc sous la même valeur
  // non signée, sans qu'on ait rien à rattraper. Prendre la valeur absolue
  // poserait au contraire le nom du déguisement sur le joueur nu.
  const auto disguised = static_cast<std::uint32_t>(-static_cast<std::int32_t>(kPlayer));
  World world = WithActor(disguised, ActorKind::kPlayer, "Poring");
  const Frame frame = NameAllFrame(-static_cast<std::int32_t>(kPlayer), "Stingor", "", "", "", 0);
  CHECK(ApplyActorNameFrame(frame, world) == FrameOutcome::kApplied);

  const Actor* actor = world.Find(disguised);
  REQUIRE(actor != nullptr);
  CHECK(actor->name == "Stingor");
}

TEST_CASE("Nom d'acteur : une carte VIDE se pose quand même", "[net][name]") {
  // ⚠ « LE SERVEUR A RÉPONDU, IL N'Y A RIEN » N'EST PAS « ON N'A PAS ENCORE
  // DEMANDÉ ». Un joueur sans guilde ni groupe reçoit une carte entièrement
  // vide ; ne pas la poser ferait redemander son nom à chaque survol, pour
  // toujours.
  World world = WithActor(kPlayer, ActorKind::kPlayer, "Stingor");
  const Frame frame = NameAllFrame(static_cast<std::int32_t>(kPlayer), "Stingor", "", "", "", 0);
  CHECK(ApplyActorNameFrame(frame, world) == FrameOutcome::kApplied);

  const Actor* actor = world.Find(kPlayer);
  REQUIRE(actor != nullptr);
  REQUIRE(actor->card.has_value());
  CHECK(actor->card->aside.empty());
  CHECK(actor->card->banner.empty());
}

TEST_CASE("Nom d'acteur : un nom VIDE n'efface pas celui qu'on a", "[net][name]") {
  World world = WithActor(kPlayer, ActorKind::kPlayer, "Stingor");
  const Frame frame = NameAllFrame(static_cast<std::int32_t>(kPlayer), "", "", "", "", 0);
  CHECK(ApplyActorNameFrame(frame, world) == FrameOutcome::kApplied);

  const Actor* actor = world.Find(kPlayer);
  REQUIRE(actor != nullptr);
  CHECK(actor->name == "Stingor");
}

TEST_CASE("Nom d'acteur : un PNJ passe par l'autre paquet", "[net][name]") {
  World world = WithActor(kPlayer, ActorKind::kNonPlayer, "Kafra");
  ro::proto::ZcAckReqnameallNpc packet;
  packet.gid = static_cast<std::int32_t>(kPlayer);
  packet.groupId = 7;
  packet.name = "Kafra Employee";
  packet.title = "Prontera";
  const Frame frame{.opcode = ro::proto::ZcAckReqnameallNpc::kOpcode,
                    .bytes = ro::proto::Encode(packet)};
  CHECK(ApplyActorNameFrame(frame, world) == FrameOutcome::kApplied);

  const Actor* actor = world.Find(kPlayer);
  REQUIRE(actor != nullptr);
  CHECK(actor->name == "Kafra Employee");
  REQUIRE(actor->card.has_value());
  // ⚠ LE TITRE ARRIVE EN TOUTES LETTRES ICI, et les trois autres chaînes
  // restent vides : le client natif appelle le même poseur que pour 0x0A30 en
  // lui passant quatre chaînes vides.
  CHECK(actor->card->title == "Prontera");
  CHECK(actor->card->title_id == 0);
  CHECK(actor->card->aside.empty());
  CHECK(actor->card->banner.empty());
  CHECK(actor->card->badge.empty());
}

TEST_CASE("Nom d'acteur : un inconnu n'en crée pas un", "[net][name]") {
  // La carte arrive parfois juste après qu'un monstre est sorti de vue.
  // Inventer un acteur sans apparence ni case ne donnerait rien à dessiner —
  // c'est la règle de toute cette couche.
  World world = WithActor(kPlayer, ActorKind::kPlayer, "Stingor");
  const Frame frame = NameAllFrame(static_cast<std::int32_t>(kStranger), "Passant", "", "", "", 0);
  CHECK(ApplyActorNameFrame(frame, world) == FrameOutcome::kIgnored);
  CHECK(world.Find(kStranger) == nullptr);
}

TEST_CASE("Nom d'acteur : la carte SURVIT à une réannonce", "[net][name]") {
  // 🔴 LE SERVEUR RÉANNONCE UNE UNITÉ À CHAQUE FOIS QU'ELLE SE MET EN MARCHE, et
  // sa réannonce ne porte pas la carte. La perdre là serait définitif : rien ne
  // la redirait tant que le joueur reste en vue, et sa guilde disparaîtrait au
  // premier pas.
  World world = WithActor(kPlayer, ActorKind::kPlayer, "Stingor");
  const Frame frame = NameAllFrame(static_cast<std::int32_t>(kPlayer), "Stingor", "",
                                   "Moonlight-Destiny", "Handyman", 0);
  CHECK(ApplyActorNameFrame(frame, world) == FrameOutcome::kApplied);

  Actor again;
  again.id = kPlayer;
  again.kind = ActorKind::kPlayer;
  again.name = "Stingor";
  world.Spawn(std::move(again));

  const Actor* actor = world.Find(kPlayer);
  REQUIRE(actor != nullptr);
  REQUIRE(actor->card.has_value());
  CHECK(actor->card->banner == "Moonlight-Destiny");
}

TEST_CASE("Nom d'acteur : un autre opcode ne lui appartient pas", "[net][name]") {
  World world = WithActor(kPlayer, ActorKind::kPlayer, "Stingor");
  const Frame frame{.opcode = 0x0000, .bytes = {}};
  CHECK(ApplyActorNameFrame(frame, world) == FrameOutcome::kUnhandled);
}
