// LE FIL DE CONVERSATION DU MONDE : ce qui est dit, sur qui, et combien on en
// garde par genre.

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/world.h"

using ro::sim::Actor;
using ro::sim::ActorKind;
using ro::sim::CellPosition;
using ro::sim::World;

namespace {

Actor Someone(std::uint32_t id, CellPosition where) {
  Actor actor;
  actor.id = id;
  actor.kind = ActorKind::kPlayer;
  actor.position = where;
  actor.name = "quelqu'un";
  return actor;
}

}  // namespace

TEST_CASE("Monde : ce qui est dit se garde, et se pose sur qui l'a dit", "[sim][world]") {
  // La conversation traverse les changements de carte : ce qui a été dit reste
  // dit. La bulle, elle, s'accroche à son locuteur — quand on le connaît.
  World world;
  world.Spawn(Someone(150'001, CellPosition{10, 10, 0}));

  world.RecordChat(ro::sim::ChatKind::kServer, 150'001, "Alice : bonjour", 1000);
  REQUIRE(world.chat_lines().size() == 1);
  CHECK(world.chat_lines()[0].speaker_id == 150'001);
  CHECK(world.Find(150'001)->chat_text == "Alice : bonjour");
  CHECK(world.Find(150'001)->chat_since_ms == 1000);

  // Ce qui ne vient de personne entre dans la conversation sans se poser nulle
  // part : c'est ainsi qu'arrivent les réponses du serveur.
  world.RecordChat(ro::sim::ChatKind::kServer, 0, "vitesse changee", 2000);
  REQUIRE(world.chat_lines().size() == 2);
  CHECK(world.chat_lines()[1].speaker_id == 0);

  // Une réannonce d'unité n'efface pas la bulle.
  world.Spawn(Someone(150'001, CellPosition{11, 10, 0}));
  CHECK(world.Find(150'001)->chat_text == "Alice : bonjour");

  // La plus ancienne s'en va quand le genre déborde.
  world.SetChatLinesKeptPerKind(ro::sim::kFewestChatLinesKept);
  for (int line = 0; line < ro::sim::kFewestChatLinesKept * 2; ++line) {
    world.RecordChat(ro::sim::ChatKind::kServer, 0, "ligne", 3000);
  }
  int served = 0;
  for (const ro::sim::ChatLine& line : world.chat_lines()) {
    served += line.kind == ro::sim::ChatKind::kServer ? 1 : 0;
  }
  CHECK(served == ro::sim::kFewestChatLinesKept);
  CHECK(world.chat_lines().back().text == "ligne");
}

TEST_CASE("Monde : ce qui est bavard n'efface pas ce qui est rare", "[sim][world]") {
  // 🔴 C'EST TOUTE LA RAISON DE COMPTER PAR GENRE. Sous un tampon commun,
  // un mot de guilde s'efface parce que la carte parle beaucoup — et c'est
  // justement celui-là qu'on remonte chercher une heure plus tard.
  World world;
  world.SetChatLinesKeptPerKind(ro::sim::kFewestChatLinesKept);

  world.RecordChat(ro::sim::ChatKind::kGuild, 0, "rendez-vous a 21h", 1000);
  // La carte, elle, parle bien au-delà de la borne.
  for (int line = 0; line < ro::sim::kFewestChatLinesKept * 3; ++line) {
    world.RecordChat(ro::sim::ChatKind::kPublic, 0, "bavardage", 2000);
  }

  int guild = 0;
  int said = 0;
  bool kept = false;
  for (const ro::sim::ChatLine& line : world.chat_lines()) {
    guild += line.kind == ro::sim::ChatKind::kGuild ? 1 : 0;
    said += line.kind == ro::sim::ChatKind::kPublic ? 1 : 0;
    kept = kept || line.text == "rendez-vous a 21h";
  }
  CHECK(guild == 1);
  CHECK(kept);
  // Et le genre bavard, lui, s'est bien fait élaguer à sa propre borne.
  CHECK(said == ro::sim::kFewestChatLinesKept);
}

TEST_CASE("Monde : abaisser la borne n'élague pas ce qui est déjà là", "[sim][world]") {
  // ⚠ CE QU'ON EST EN TRAIN DE LIRE NE DISPARAÎT PAS SOUS LES YEUX. Le
  // trop-plein part au fil des lignes qui arrivent, comme d'habitude — un
  // réglage qui vide la fenêtre au moment où on le bouge se lit comme une perte.
  World world;
  for (int line = 0; line < ro::sim::kFewestChatLinesKept + 10; ++line) {
    world.RecordChat(ro::sim::ChatKind::kServer, 0, "ligne", 1000);
  }
  const std::size_t before = world.chat_lines().size();
  world.SetChatLinesKeptPerKind(ro::sim::kFewestChatLinesKept);
  CHECK(world.chat_lines().size() == before);
}

TEST_CASE("Monde : une borne abaissée finit par être tenue", "[sim][world]") {
  // 🔴 UNE ANCIENNE RETIRÉE POUR UNE NOUVELLE AJOUTÉE LAISSE LE COMPTE OÙ IL
  // EST : la borne abaissée ne prenait jamais effet, et la mémoire n'était
  // pas rendue avant le redémarrage.
  World world;
  constexpr int kExcess = 10;
  for (int line = 0; line < ro::sim::kFewestChatLinesKept + kExcess; ++line) {
    world.RecordChat(ro::sim::ChatKind::kServer, 0, "ancienne", 1000);
  }
  world.SetChatLinesKeptPerKind(ro::sim::kFewestChatLinesKept);
  for (int line = 0; line < kExcess + 1; ++line) {
    world.RecordChat(ro::sim::ChatKind::kServer, 0, "nouvelle", 2000);
  }
  CHECK(world.chat_lines().size() == static_cast<std::size_t>(ro::sim::kFewestChatLinesKept));
  CHECK(world.chat_lines().back().text == "nouvelle");
}

TEST_CASE("Monde : le fil prêté ne bouge pas, et ce qui s'y dit attend son tour", "[sim][world]") {
  constexpr int kLinesOfKind = ro::sim::kFewestChatLinesKept;
  World world;
  world.SetChatLinesKeptPerKind(kLinesOfKind);
  for (int rank = 0; rank < kLinesOfKind; ++rank) {
    world.RecordChat(ro::sim::ChatKind::kBattle, 0, "une ligne assez longue pour le tas", 1000);
  }
  const std::uint64_t last_before = world.last_chat_seq();
  const ro::sim::ChatLine* const lines_before = world.chat_lines().data();
  const char* const oldest_text = world.chat_lines().front().text.data();
  world.Spawn(Someone(150'001, CellPosition{10, 10, 0}));
  {
    const ro::sim::LentChatLines lent(world);
    // Cette ligne-ci aurait évincé la plus ancienne, sous les vues qui la lisent.
    world.RecordChat(ro::sim::ChatKind::kBattle, 150'001, "cumul de zone", 2000);
    ro::sim::WhisperFacts facts;
    facts.peer_name = "Alice";
    world.RecordWhisper(std::move(facts), "psst", 3000);

    CHECK(world.chat_lines().data() == lines_before);
    CHECK(world.chat_lines().size() == static_cast<std::size_t>(kLinesOfKind));
    CHECK(world.chat_lines().front().text.data() == oldest_text);
    CHECK(world.last_chat_seq() == last_before);
    // LA BULLE N'ATTEND PAS : elle ne vit pas dans le fil.
    CHECK(world.Find(150'001)->chat_text == "cumul de zone");
  }
  // Tout entre, dans l'ordre où cela a été dit, et l'éviction reprend.
  const std::span<const ro::sim::ChatLine> after = world.chat_lines();
  // Le cumul a évincé une ligne de son genre ; le chuchotement n'en évince aucune.
  REQUIRE(after.size() == static_cast<std::size_t>(kLinesOfKind) + 1);
  CHECK(world.last_chat_seq() == last_before + 2);
  CHECK(after[after.size() - 2].text == "cumul de zone");
  CHECK(after[after.size() - 2].seq == last_before + 1);
  CHECK(after.back().text == "psst");
  CHECK(after.back().seq == last_before + 2);
  CHECK(after.back().whisper.peer_name == "Alice");
}

TEST_CASE("Monde : hors prêt, une ligne entre dans le fil sur-le-champ", "[sim][world]") {
  World world;
  ro::sim::WhisperFacts facts;
  facts.peer_name = "Alice";
  world.RecordWhisper(std::move(facts), "psst", 1000);
  REQUIRE(world.chat_lines().size() == 1);
  CHECK(world.chat_lines().front().whisper.peer_name == "Alice");
}
