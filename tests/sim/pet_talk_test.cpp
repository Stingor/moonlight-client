// LA PAROLE DES FAMILIERS : l'entier qui part, ce qu'un entier reçu dit, et
// les occasions où le client du maître fait parler le sien.

#include "ro/sim/pet_talk.h"

#include <cstdint>
#include <variant>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/pet.h"

using ro::sim::Pet;
using ro::sim::PetActHeard;
using ro::sim::PetChatter;
using ro::sim::PetEmotionHeard;
using ro::sim::PetLineHeard;
using ro::sim::PetStateChange;
using ro::sim::PetTalkOccasion;
using Codes = std::vector<std::int32_t>;

namespace {

constexpr std::uint32_t kPetActor = 110000123;
constexpr std::uint32_t kPoring = 1002;
constexpr int kLoyal = 950;
constexpr int kNeutral = 500;
// Un tirage gagnant pour toutes les occasions, et un perdant.
constexpr int kWinningRoll = 10;
constexpr int kLosingRoll = 60;

// Un Poring sorti, rassasié à moitié puis nourri jusqu'à 80.
Pet FedPoring(int intimacy) {
  Pet pet;
  CHECK(pet.ApplyStateChange(PetStateChange::kBirth, kPetActor, 0));
  pet.ApplyProperty(ro::sim::PetProperty{.name = "Popo",
                                         .renamed = false,
                                         .level = 1,
                                         .hunger = 50,
                                         .intimacy = intimacy,
                                         .accessory_item_id = 0,
                                         .pet_class = kPoring});
  CHECK(pet.ApplyStateChange(PetStateChange::kHunger, kPetActor, 80));
  return pet;
}

}  // namespace

TEST_CASE("PetTalk : l'entier d'une réplique et celui d'une émoticône", "[pettalk]") {
  CHECK(ro::sim::PetLineCode(kPoring, PetTalkOccasion::kFeeding, 3) == 1002030);
  CHECK(ro::sim::PetLineCode(kPoring, PetTalkOccasion::kPerformance3, 0) == 1002900);
  // Faim et intimité au plus haut, au repos : le baiser.
  CHECK(ro::sim::PetEmotionCode(4, 4, PetTalkOccasion::kStand) == 302);
  // La chasse d'un familier affamé et cordial ne montre rien.
  CHECK_FALSE(ro::sim::PetEmotionCode(0, 3, PetTalkOccasion::kHunting).has_value());
  // La case que le client ne remplit pas : la surprise.
  CHECK(ro::sim::PetEmotionCode(2, 0, PetTalkOccasion::kLevelUp) == 2);
  // Un spectacle n'a pas d'émoticône.
  CHECK_FALSE(ro::sim::PetEmotionCode(4, 4, PetTalkOccasion::kPerformance1).has_value());
  CHECK_FALSE(ro::sim::PetEmotionCode(5, 0, PetTalkOccasion::kFeeding).has_value());
}

TEST_CASE("PetTalk : une réplique ne part que d'un familier fidèle", "[pettalk]") {
  CHECK_FALSE(ro::sim::PetActLeaves(1002030, 899));
  CHECK(ro::sim::PetActLeaves(1002030, 900));
  // L'émoticône part toujours.
  CHECK(ro::sim::PetActLeaves(302, 0));
}

TEST_CASE("PetTalk : un entier reçu se lit chiffre par chiffre", "[pettalk]") {
  CHECK(ro::sim::ReadPetActCode(1002030) ==
        PetActHeard{PetLineHeard{.pet_class = kPoring, .hunger_digit = 3, .occasion_digit = 0}});
  CHECK(ro::sim::ReadPetActCode(302) == PetActHeard{PetEmotionHeard{.emotion = 30}});
  // Le salut du serveur : une réplique « repos » au reste trois, qui se lit
  // comme la connexion de l'espèce.
  CHECK(ro::sim::ReadPetActCode(1002 * 1000 + 4 * 100 + 3 * 10 + 3) ==
        PetActHeard{PetLineHeard{.pet_class = kPoring, .hunger_digit = 3, .occasion_digit = 10}});
  // Un reste de un donne une réplique ordinaire.
  CHECK(ro::sim::ReadPetActCode(1002431) ==
        PetActHeard{PetLineHeard{.pet_class = kPoring, .hunger_digit = 3, .occasion_digit = 4}});
}

TEST_CASE("PetTalk : la ligne montrée porte la marque, le nom et la réplique", "[pettalk]") {
  CHECK(ro::sim::PetLineShown("Popo", "Yum~") == "[Pet] Popo : Yum~");
  CHECK(ro::sim::PetLineShown("", "Hi") == "[Pet]  : Hi");
}

TEST_CASE("PetTalk : le repas parle de la faim d'avant", "[pettalk]") {
  const PetChatter chatter;
  // Faim d'avant 50 (rang 2), intimité fidèle (rang 4) : merci.
  CHECK(chatter.Fed(FedPoring(kLoyal)) == Codes{1002020, 152});
  // Un familier neutre ne dit rien, il montre son émoticône.
  CHECK(chatter.Fed(FedPoring(kNeutral)) == Codes{332});
}

TEST_CASE("PetTalk : la mort, le niveau et le spectacle", "[pettalk]") {
  const Pet pet = FedPoring(kLoyal);
  const PetChatter chatter;
  // Faim 80 (rang 3), fidèle : les larmes, puis le sourire.
  CHECK(chatter.OwnerDied(pet) == Codes{1002330, 282});
  CHECK(chatter.OwnerLevelledUp(pet) == Codes{1002630, 182});
  CHECK(ro::sim::PerformanceOccasion(4) == PetTalkOccasion::kSpecialPerformance);
  CHECK(chatter.Performed(pet, 2) == Codes{1002830});
  CHECK(chatter.Performed(pet, 4) == Codes{1002530});
  CHECK(chatter.Performed(pet, 0).empty());
  // Un numéro inconnu fait partir l'entier nul.
  CHECK(chatter.Performed(pet, 7) == Codes{0});
  CHECK(chatter.Performed(FedPoring(kNeutral), 2).empty());
}

TEST_CASE("PetTalk : la chasse et le danger partagent leur silence", "[pettalk]") {
  const Pet pet = FedPoring(kLoyal);
  PetChatter chatter;
  // Un tirage perdu ne remet pas le silence à zéro.
  CHECK(chatter.OwnerEngaged(pet, 1000, kLosingRoll).empty());
  CHECK(chatter.OwnerEngaged(pet, 1000, kWinningRoll) == Codes{1002130, 212});
  CHECK(chatter.OwnerEngaged(pet, 30000, kWinningRoll).empty());
  // Le danger n'exige que cinq secondes, depuis la chasse.
  CHECK(chatter.OwnerHurt(pet, 20, 100, 30000, kWinningRoll) == Codes{1002230, 262});
  // Et la chasse compte depuis le danger.
  CHECK(chatter.OwnerEngaged(pet, 61000, kWinningRoll).empty());
  CHECK(chatter.OwnerEngaged(pet, 90000, kWinningRoll) == Codes{1002130, 212});
}

TEST_CASE("PetTalk : le danger, sous le quart de la vie seulement", "[pettalk]") {
  const Pet pet = FedPoring(kLoyal);
  PetChatter chatter;
  CHECK(chatter.OwnerHurt(pet, 25, 100, 1000, kWinningRoll).empty());
  CHECK(chatter.OwnerHurt(pet, 24, 100, 1000, 49).size() == 2);
  CHECK(chatter.OwnerHurt(pet, 1, 100, 9000, 50).empty());
  CHECK(chatter.OwnerHurt(pet, 1, 0, 9000, kWinningRoll).empty());
}

TEST_CASE("PetTalk : le danger se juge à chaque redite, même inchangée", "[pettalk]") {
  const Pet pet = FedPoring(kLoyal);
  PetChatter chatter;
  CHECK(chatter.OwnerHurt(pet, 10, 100, 1000, kWinningRoll).size() == 2);
  // La même vie redite, le silence écoulé : le familier reparle.
  CHECK(chatter.OwnerHurt(pet, 10, 100, 6000, kWinningRoll).size() == 2);
}

TEST_CASE("PetTalk : la chasse exige le repos et une cible neuve", "[pettalk]") {
  constexpr std::uint32_t kPoringMob = 5001;
  constexpr std::uint32_t kLunaticMob = 5002;
  constexpr std::uint8_t kHit = 0;
  ro::sim::PetHuntWatch hunt;
  // En plein geste : pas de chasse, mais la cible se mémorise.
  CHECK_FALSE(hunt.Strike(false, kPoringMob, kHit));
  // Revenu au repos, la même cible ne dit rien.
  CHECK_FALSE(hunt.Strike(true, kPoringMob, kHit));
  // Une cible neuve, au repos : la chasse.
  CHECK(hunt.Strike(true, kLunaticMob, kHit));
  // S'asseoir n'en est pas une, et mémorise sa cible nulle.
  CHECK_FALSE(hunt.Strike(true, 0, ro::sim::kOwnerSitDownAction));
  CHECK(hunt.Strike(true, kLunaticMob, kHit));
  // Un coup de compétence ou une incantation mémorisent aussi.
  hunt.Remember(kPoringMob);
  CHECK_FALSE(hunt.Strike(true, kPoringMob, kHit));
  // L'entrée sur une carte oublie.
  hunt.Forget();
  CHECK(hunt.Strike(true, kPoringMob, kHit));
}

TEST_CASE("PetTalk : le familier arrêté parle une fois, après trois secondes", "[pettalk]") {
  const Pet pet = FedPoring(kLoyal);
  PetChatter chatter;
  CHECK(chatter.ObserveRest(pet, false, 0, kWinningRoll).empty());
  CHECK(chatter.ObserveRest(pet, true, 50, kWinningRoll).empty());
  CHECK(chatter.ObserveRest(pet, false, 100, kWinningRoll).empty());
  CHECK(chatter.ObserveRest(pet, false, 3100, kWinningRoll).empty());
  // Faim rang 3, fidèle, au repos : le baiser.
  CHECK(chatter.ObserveRest(pet, false, 3101, kWinningRoll) == Codes{1002430, 302});
  CHECK(chatter.ObserveRest(pet, false, 9000, kWinningRoll).empty());
  // Le tirage perdu consomme l'arrêt lui aussi.
  CHECK(chatter.ObserveRest(pet, true, 9100, kWinningRoll).empty());
  CHECK(chatter.ObserveRest(pet, false, 9200, kWinningRoll).empty());
  CHECK(chatter.ObserveRest(pet, false, 12201, kLosingRoll).empty());
  CHECK(chatter.ObserveRest(pet, false, 20000, kWinningRoll).empty());
}
