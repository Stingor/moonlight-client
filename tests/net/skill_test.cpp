#include "ro/net/skill.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/proto/manual_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/proto/skill_text_packets.h"
#include "ro/sim/bank.h"
#include "ro/sim/skill_book.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/net_frames.h"
#include "support/wire_bytes.h"

// LES COMPÉTENCES : ce que le serveur dit qu'on sait faire, et les deux
// demandes qui servent à le faire.
using ro::net::ApplySkillFrame;
using ro::net::Connection;
using ro::net::FrameOutcome;
using ro::net::RequestSkillOnGround;
using ro::net::RequestSkillOnTarget;
using ro::proto::SkillOnGroundRequest;
using ro::proto::SkillOnTargetRequest;
using ro::test::Connect;
using ro::test::LoopbackServer;
using ro::test_support::FrameOf;
using ro::test_support::ReadU16;
using ro::test_support::ReadU32;

namespace {

// Une ligne de compétence telle que le serveur l'écrit : identifiant, visée,
// rang, coût, portée, nom sur vingt-quatre octets, puis le rang à monter.
ro::proto::Skilldata Line(std::uint16_t id, std::string name, std::int32_t targeting,
                          std::uint16_t level) {
  ro::proto::Skilldata data;
  data.id = id;
  data.inf = targeting;
  data.level = level;
  data.sp = 25;
  data.range2 = 9;
  data.name = std::move(name);
  data.upFlag = 1;
  return data;
}

}  // namespace

TEST_CASE("Compétences : lancer sur quelqu'un met le NIVEAU avant l'identifiant", "[net][skills]") {
  // L'ordre paraît inversé et il ne l'est pas : le serveur lit le niveau au
  // premier décalage et l'identifiant au second. Les intervertir lancerait la
  // compétence numéro « rang », au rang « numéro ».
  LoopbackServer server;
  REQUIRE(server.listening());
  Connection link = Connect(server);

  REQUIRE(RequestSkillOnTarget(link, 14, 5, 0x11223344).has_value());
  const std::vector<std::uint8_t> sent = server.Receive(SkillOnTargetRequest::kWireSize);
  REQUIRE(sent.size() == SkillOnTargetRequest::kWireSize);
  CHECK(ReadU16(sent, 0) == SkillOnTargetRequest::kOpcode);
  CHECK(ReadU16(sent, 2) == 5);
  CHECK(ReadU16(sent, 4) == 14);
  CHECK(ReadU32(sent, 6) == 0x11223344);
}

TEST_CASE("Compétences : lancer au sol porte la case, et un onzième octet nul", "[net][skills]") {
  // Le client d'origine écrit cet octet, mais le serveur ne le lit PAS : sa
  // ligne de lecture est commentée. Il ne compte donc que pour la taille — et
  // la taille, elle, doit être juste, sans quoi le serveur perd le flux.
  LoopbackServer server;
  REQUIRE(server.listening());
  Connection link = Connect(server);

  ro::sim::CellPosition where;
  where.x = 150;
  where.y = 88;
  REQUIRE(RequestSkillOnGround(link, 83, 10, where).has_value());
  const std::vector<std::uint8_t> sent = server.Receive(SkillOnGroundRequest::kWireSize);
  REQUIRE(sent.size() == SkillOnGroundRequest::kWireSize);
  CHECK(ReadU16(sent, 0) == SkillOnGroundRequest::kOpcode);
  CHECK(ReadU16(sent, 2) == 10);
  CHECK(ReadU16(sent, 4) == 83);
  CHECK(ReadU16(sent, 6) == 150);
  CHECK(ReadU16(sent, 8) == 88);
  CHECK(sent[10] == 0);
}

TEST_CASE("Compétences : Talkie Box part avec son texte, sur le paquet que le serveur lit",
          "[net][skills]") {
  // 🔴 SANS CE PAQUET, LA BOÎTE SE POSAIT MUETTE : la demande au sol ordinaire
  // ne porte aucun texte, et le serveur n'en copie que depuis celle-ci.
  using ro::proto::SkillOnGroundWithTextRequest;
  LoopbackServer server;
  Connection link = Connect(server);

  ro::sim::CellPosition where;
  where.x = 101;
  where.y = 57;
  REQUIRE(ro::net::RequestSkillOnGroundWithText(link, ro::sim::kTalkieBoxSkill, 1, where,
                                                "Bonjour à toi")
              .has_value());
  const std::vector<std::uint8_t> sent = server.Receive(SkillOnGroundWithTextRequest::kWireSize);
  REQUIRE(sent.size() == SkillOnGroundWithTextRequest::kWireSize);
  CHECK(ReadU16(sent, 0) == 0x0367);
  CHECK(ReadU16(sent, 2) == 1);
  CHECK(ReadU16(sent, 4) == ro::sim::kTalkieBoxSkill);
  CHECK(ReadU16(sent, 6) == 101);
  CHECK(ReadU16(sent, 8) == 57);
  // Le texte part dans la page du jeu : « à » tient sur UN octet, 0xE0.
  const std::string text(sent.begin() + 10, sent.begin() + 23);
  CHECK(text == "Bonjour \xE0 toi");
  CHECK(sent[23] == 0);
  CHECK(sent.back() == 0);
}

TEST_CASE("Compétences : le texte de Talkie Box est coupé à vingt octets, zéro compris après",
          "[net][skills]") {
  using ro::proto::SkillOnGroundWithTextRequest;
  LoopbackServer server;
  Connection link = Connect(server);
  REQUIRE(ro::net::RequestSkillOnGroundWithText(link, ro::sim::kTalkieBoxSkill, 1,
                                                ro::sim::CellPosition{},
                                                "abcdefghijklmnopqrstuvwxyz")
              .has_value());
  const std::vector<std::uint8_t> sent = server.Receive(SkillOnGroundWithTextRequest::kWireSize);
  REQUIRE(sent.size() == SkillOnGroundWithTextRequest::kWireSize);
  const std::string text(sent.begin() + 10, sent.begin() + 30);
  CHECK(text == "abcdefghijklmnopqrst");
  CHECK(sent[30] == 0);
}

TEST_CASE("Compétences : la liste remplace le grimoire entier", "[net][skills]") {
  // Le serveur l'envoie entière à chaque entrée sur une carte. Fusionner
  // laisserait une compétence oubliée — désapprise, ou perdue avec un métier —
  // dans un grimoire qui ne la porte plus.
  ro::sim::SkillBook book;
  book.Learn(ro::sim::KnownSkill{999, 1, 0, 0, ro::sim::kSkillOnEnemy, "OUBLIEE", false});

  ro::proto::ZcSkillinfoList list;
  list.skills.push_back(Line(28, "AL_HEAL", ro::sim::kSkillOnFriend, 10));
  list.skills.push_back(Line(14, "MG_COLDBOLT", ro::sim::kSkillOnEnemy, 4));
  CHECK(ApplySkillFrame(FrameOf(Encode(list)), book, 0) == FrameOutcome::kApplied);

  REQUIRE(book.size() == 2);
  CHECK(book.Find(999) == nullptr);
  const ro::sim::KnownSkill* bolt = book.Find(14);
  REQUIRE(bolt != nullptr);
  CHECK(bolt->name == "MG_COLDBOLT");
  CHECK(bolt->level == 4);
  CHECK(bolt->sp_cost == 25);
  CHECK(bolt->range_cells == 9);
  CHECK(bolt->targeting == ro::sim::kSkillOnEnemy);
  CHECK(bolt->upgradable);
}

TEST_CASE("Compétences : une compétence s'ajoute, se met à jour, et s'oublie", "[net][skills]") {
  ro::sim::SkillBook book;

  ro::proto::ZcAddSkill added;
  added.skill = Line(14, "MG_COLDBOLT", ro::sim::kSkillOnEnemy, 3);
  CHECK(ApplySkillFrame(FrameOf(Encode(added)), book, 0) == FrameOutcome::kApplied);
  REQUIRE(book.size() == 1);

  // ⚠ Le serveur envoie aussi ce paquet AVEC UN IDENTIFIANT NUL, pour dire
  // qu'il n'y a rien à ajouter : l'accepter mettrait une ligne vide en tête du
  // grimoire, qu'aucun clic ne pourrait lancer.
  ro::proto::ZcAddSkill nothing;
  CHECK(ApplySkillFrame(FrameOf(Encode(nothing)), book, 0) == FrameOutcome::kIgnored);
  CHECK(book.size() == 1);

  ro::proto::ZcSkillinfoUpdate2 raised;
  raised.id = 14;
  raised.level = 6;
  raised.sp = 40;
  raised.range2 = 9;
  raised.inf = ro::sim::kSkillOnEnemy;
  CHECK(ApplySkillFrame(FrameOf(Encode(raised)), book, 0) == FrameOutcome::kApplied);
  REQUIRE(book.size() == 1);
  CHECK(book.Find(14)->level == 6);
  // La mise à jour ne porte pas le nom : il ne doit pas disparaître avec elle.
  CHECK(book.Find(14)->name == "MG_COLDBOLT");

  ro::proto::ZcSkillinfoDelete removed;
  removed.skillID = 14;
  CHECK(ApplySkillFrame(FrameOf(Encode(removed)), book, 0) == FrameOutcome::kApplied);
  CHECK(book.size() == 0);
  // Un retrait de ce qu'on ne portait pas n'est pas une erreur : le serveur
  // l'envoie aussi pour ce qu'on n'a jamais su.
  CHECK(ApplySkillFrame(FrameOf(Encode(removed)), book, 0) == FrameOutcome::kIgnored);
}

TEST_CASE("Compétences : une trame qui n'est pas la sienne ne la trompe pas", "[net][skills]") {
  ro::sim::SkillBook book;
  // Un opcode qu'elle ne traite pas : c'est ainsi qu'ApplyFrame sait qu'il lui
  // reste à chercher ailleurs.
  CHECK(ApplySkillFrame(ro::net::Frame{0x0064, {0x64, 0x00}}, book, 0) == FrameOutcome::kUnhandled);

  // Le bon opcode, mais une trame tronquée : refusée plutôt que lue de travers.
  std::vector<std::uint8_t> truncated = Encode(ro::proto::ZcAddSkill{});
  truncated.pop_back();
  CHECK(ApplySkillFrame(FrameOf(truncated), book, 0) == FrameOutcome::kUndecodable);
}

TEST_CASE("Grimoire : le serveur pose un temps d'attente, et il s'écoule", "[net][skill]") {
  // Le paquet 0x043D suit un lancement réussi et dit combien de temps le
  // serveur refusera de relancer. Le client natif, lui, le PERD quand la barre
  // de raccourcis ne dessine pas l'icône : suivre le paquet est ce qu'il faut.
  ro::sim::SkillBook book;

  ro::proto::ZcSkillPostdelay postdelay;
  postdelay.SKID = 28;
  postdelay.DelayTM = 3000;
  CHECK(ApplySkillFrame(FrameOf(Encode(postdelay)), book, 10'000) == FrameOutcome::kApplied);
  CHECK(book.CooldownRemainingMs(28, 10'000) == 3000);
  CHECK(book.CooldownRemainingMs(28, 11'500) == 1500);

  // Échéance passée : zéro, et non quarante-neuf jours.
  CHECK(book.CooldownRemainingMs(28, 13'000) == 0);
  CHECK(book.CooldownRemainingMs(28, 90'000) == 0);

  // Une compétence dont rien n'a été dit est prête.
  CHECK(book.CooldownRemainingMs(14, 10'000) == 0);

  // Une SECONDE annonce remplace la première plutôt que de s'y ajouter.
  postdelay.DelayTM = 500;
  CHECK(ApplySkillFrame(FrameOf(Encode(postdelay)), book, 12'000) == FrameOutcome::kApplied);
  CHECK(book.CooldownRemainingMs(28, 12'000) == 500);

  // Le temps d'attente SURVIT au remplacement de la liste : le serveur la
  // renvoie entière à chaque entrée sur une carte, et lui garde ses échéances.
  book.Clear();
  CHECK(book.CooldownRemainingMs(28, 12'200) == 300);
}

TEST_CASE("Compétences : un objet prête sa compétence le temps d'une visée", "[net][skills]") {
  // 🔴 LA FEUILLE D'YGGDRASIL : le serveur décrit Resurrection au niveau un,
  // et le client doit passer en visée avec elle. Ignorer le paquet consumait
  // l'objet sans que rien ne se passe.
  ro::sim::SkillBook book;
  ro::proto::ZcAutorunSkill offered;
  offered.skill_id = 54;
  offered.skill_type = 16;
  offered.skill_lv = 1;
  offered.skill_sp = 1;
  offered.skill_range = 9;
  offered.skill_name = "ALL_RESURRECTION";
  CHECK(ApplySkillFrame(FrameOf(Encode(offered)), book, 1000) == FrameOutcome::kApplied);

  // Elle n'entre pas au grimoire...
  CHECK(book.Find(54) == nullptr);
  // ... et se reprend une fois, à son niveau et avec sa portée.
  const std::optional<ro::sim::KnownSkill> taken = book.TakeOfferedSkill();
  REQUIRE(taken.has_value());
  CHECK(taken->id == 54);
  CHECK(taken->level == 1);
  CHECK(taken->range_cells == 9);
  CHECK(taken->targeting == 16);
  CHECK_FALSE(book.TakeOfferedSkill().has_value());
}
