// CE QUE DISENT LES MONSTRES QUAND ILS FRAPPENT : l'espèce, la ligne, et la
// cible que chaque acteur mémorise.

#include "ro/sim/monster_talk.h"

#include <cstdint>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/pet_talk.h"

using ro::sim::StrikeMemories;

namespace {

constexpr std::uint32_t kOrc = 110000001;
constexpr std::uint32_t kPoring = 110000002;
constexpr std::uint32_t kHero = 2000001;
constexpr std::uint32_t kSidekick = 2000002;
constexpr std::uint8_t kSingleHit = 0;

}  // namespace

TEST_CASE("MonsterTalk : l'espèce est le nom de ressource en minuscules ASCII", "[monstertalk]") {
  CHECK(ro::sim::MonsterTalkSpecies("ORK_WARRIOR") == "ork_warrior");
  CHECK(ro::sim::MonsterTalkSpecies("Poring") == "poring");
  // Un octet hors de l'ASCII reste tel quel.
  const std::string korean = "\xB8\xF3";
  CHECK(ro::sim::MonsterTalkSpecies(korean + "Ab") == korean + "ab");
}

TEST_CASE("MonsterTalk : la ligne est le nom, deux points, la réplique", "[monstertalk]") {
  CHECK(ro::sim::MonsterLineShown("Orc Warrior", "Grr !") == "Orc Warrior : Grr !");
}

TEST_CASE("MonsterTalk : seul un changement de cible fait parler, au repos", "[monstertalk]") {
  StrikeMemories memories;
  // Une cible neuve, au repos : il parle.
  CHECK(memories.Strike(kOrc, true, kHero, kSingleHit));
  // La même cible : rien.
  CHECK_FALSE(memories.Strike(kOrc, true, kHero, kSingleHit));
  // En plein geste, la cible neuve se mémorise sans rien dire.
  CHECK_FALSE(memories.Strike(kOrc, false, kSidekick, kSingleHit));
  CHECK_FALSE(memories.Strike(kOrc, true, kSidekick, kSingleHit));
  // Ramasser n'est pas un coup, mais mémorise sa cible.
  CHECK_FALSE(memories.Strike(kOrc, true, kHero, ro::sim::kOwnerPickUpAction));
  CHECK_FALSE(memories.Strike(kOrc, true, kHero, kSingleHit));
}

TEST_CASE("MonsterTalk : chaque acteur a sa mémoire, que la vue et la carte effacent",
          "[monstertalk]") {
  StrikeMemories memories;
  CHECK(memories.Strike(kOrc, true, kHero, kSingleHit));
  // Un autre acteur ne partage pas la mémoire du premier.
  CHECK(memories.Strike(kPoring, true, kHero, kSingleHit));
  // Un coup de compétence ou une incantation mémorise sa cible.
  memories.Remember(kPoring, kSidekick);
  CHECK_FALSE(memories.Strike(kPoring, true, kSidekick, kSingleHit));
  // Sorti de la vue, l'acteur revient neuf.
  memories.Forget(kOrc);
  CHECK(memories.Strike(kOrc, true, kHero, kSingleHit));
  memories.Clear();
  CHECK(memories.Strike(kPoring, true, kSidekick, kSingleHit));
}
