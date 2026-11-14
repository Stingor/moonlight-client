#include "ro/render/shown_job.h"

#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/companion_falcon.h"
#include "ro/render/sprite_catalog.h"

using ro::render::ActorLook;
using ro::render::BodySpritePath;
using ro::render::CompanionShownJob;
using ro::render::IsCostumeJob;
using ro::render::IsMadogearJob;
using ro::render::kDressUpJob;
using ro::render::kHanbokJob;
using ro::render::kOktoberfestJob;
using ro::render::kOptionBlackDragon;
using ro::render::kOptionBlueDragon;
using ro::render::kOptionFlying;
using ro::render::kOptionGreenDragon;
using ro::render::kOptionHanbok;
using ro::render::kOptionMadogear;
using ro::render::kOptionOktoberfest;
using ro::render::kOptionPeco;
using ro::render::kOptionRedDragon;
using ro::render::kOptionSummer;
using ro::render::kOptionWedding;
using ro::render::kOptionWolfRider;
using ro::render::kOptionXmas;
using ro::render::kShownCostumeJob;
using ro::render::kSummerJob;
using ro::render::kWeddingJob;
using ro::render::kXmasJob;
using ro::render::ShadowScaleFor;
using ro::render::ShieldSpritePaths;
using ro::render::ShownJobFor;
using ro::render::SpriteCatalog;
using ro::render::WeaponSpritePaths;
using ro::render::WeaponTrailPaths;

namespace {

constexpr std::uint32_t kNovice = 0;
constexpr std::uint32_t kKnight = 7;
constexpr std::uint32_t kCrusader = 14;
constexpr std::uint32_t kLordKnight = 4008;
constexpr std::uint32_t kStarGladiator = 4047;
constexpr std::uint32_t kRuneKnight = 4054;
constexpr std::uint32_t kRanger = 4056;
constexpr std::uint32_t kMechanic = 4058;
constexpr std::uint32_t kRuneKnightTrans = 4060;
constexpr std::uint32_t kRoyalGuard = 4066;
constexpr std::uint32_t kBabyMechanic = 4100;
constexpr std::uint32_t kDragonKnight = 4252;
constexpr std::uint32_t kMeister = 4253;
constexpr std::uint32_t kWindHawk = 4257;
constexpr std::uint32_t kImperialGuard = 4258;

constexpr std::uint32_t kMadogearBody = 4086;
constexpr std::uint32_t kBabyMadogearBody = 4112;
constexpr std::uint32_t kMeisterMadogearBody = 4279;

constexpr std::uint8_t kMale = 1;

ActorLook Player(std::uint32_t job) {
  ActorLook look;
  look.player = true;
  look.job = job;
  look.sex = kMale;
  look.weapon = 1201;
  look.shield = 2101;
  return look;
}

}  // namespace

TEST_CASE("Métier montré : le Peco, le griffon et les dragons choisissent le corps",
          "[render][shown_job]") {
  CHECK(ShownJobFor(kKnight, kOptionPeco) == 13);
  CHECK(ShownJobFor(kCrusader, kOptionPeco) == 21);
  CHECK(ShownJobFor(kLordKnight, kOptionPeco) == 4014);
  // Le griffon de la Royal Guard porte le bit du Peco.
  CHECK(ShownJobFor(kRoyalGuard, kOptionPeco) == 4082);
  CHECK(ShownJobFor(kImperialGuard, kOptionPeco) == 4281);
  // Une couleur par bit ; le premier levé choisit seul.
  CHECK(ShownJobFor(kRuneKnight, kOptionGreenDragon) == 4080);
  CHECK(ShownJobFor(kRuneKnight, kOptionBlackDragon) == 4088);
  CHECK(ShownJobFor(kRuneKnight, kOptionRedDragon) == 4094);
  CHECK(ShownJobFor(kRuneKnightTrans, kOptionBlueDragon) == 4093);
  CHECK(ShownJobFor(kRuneKnight, kOptionGreenDragon | kOptionBlackDragon) == 4080);
  CHECK(ShownJobFor(kDragonKnight, kOptionGreenDragon) == 4280);
  // Le Dragon Knight n'a de corps que pour le vert.
  CHECK(ShownJobFor(kDragonKnight, kOptionBlackDragon) == kDragonKnight);
  // Sans bit, ou sans corps pour ce métier, le métier se montre tel quel.
  CHECK(ShownJobFor(kKnight, 0) == kKnight);
  CHECK(ShownJobFor(kNovice, kOptionPeco) == kNovice);
}

TEST_CASE("Métier montré : un bit sans corps pour ce métier laisse jouer la règle suivante",
          "[render][shown_job]") {
  // Le Peco n'a rien pour le Rune Knight : c'est son dragon qui décide.
  CHECK(ShownJobFor(kRuneKnight, kOptionPeco | kOptionGreenDragon) == 4080);
  CHECK(ShownJobFor(kRanger, kOptionGreenDragon | kOptionWolfRider) == 4084);
  CHECK(ShownJobFor(kWindHawk, kOptionWolfRider) == 4278);
}

TEST_CASE("Métier montré : les costumes passent avant la monture, et le vol les suspend",
          "[render][shown_job]") {
  CHECK(ShownJobFor(kKnight, kOptionPeco | kOptionWedding) == kWeddingJob);
  CHECK(ShownJobFor(kKnight, kOptionXmas) == kXmasJob);
  CHECK(ShownJobFor(kKnight, kOptionSummer) == kSummerJob);
  CHECK(ShownJobFor(kKnight, kOptionHanbok) == kHanbokJob);
  CHECK(ShownJobFor(kKnight, kOptionOktoberfest) == kOktoberfestJob);
  CHECK(ShownJobFor(kKnight, 0, {.dressed_up = true}) == kDressUpJob);
  // En vol, l'union l'emporte sur les costumes — sauf sur le mariage.
  CHECK(ShownJobFor(kStarGladiator, kOptionFlying | kOptionXmas) == 4048);
  CHECK(ShownJobFor(kStarGladiator, kOptionFlying, {.dressed_up = true}) == 4048);
  CHECK(ShownJobFor(kStarGladiator, kOptionFlying | kOptionWedding) == kWeddingJob);
  CHECK(IsCostumeJob(kWeddingJob));
  CHECK(IsCostumeJob(kDressUpJob));
  CHECK_FALSE(IsCostumeJob(23));
  CHECK_FALSE(IsCostumeJob(13));
}

TEST_CASE("Métier montré : le Madogear, par son bit d'option comme par son état",
          "[render][shown_job]") {
  CHECK(ShownJobFor(kMechanic, kOptionMadogear) == kMadogearBody);
  CHECK(ShownJobFor(kMechanic, 0, {.madogear = true}) == kMadogearBody);
  CHECK(ShownJobFor(kBabyMechanic, kOptionMadogear) == kBabyMadogearBody);
  CHECK(ShownJobFor(kMeister, 0, {.madogear = true}) == kMeisterMadogearBody);
  CHECK(IsMadogearJob(kMadogearBody));
  CHECK(IsMadogearJob(kMeisterMadogearBody));
  CHECK_FALSE(IsMadogearJob(kMechanic));
  // Le faucon et la charrette lisent la même règle, réduite : un costume ne
  // dit plus rien du métier.
  CHECK(CompanionShownJob(kMechanic, kOptionMadogear) == kMadogearBody);
  CHECK(CompanionShownJob(kKnight, kOptionWedding) == kShownCostumeJob);
}

TEST_CASE("Catalogue : le Madogear ne tient rien, et porte la lueur de sa machine",
          "[render][shown_job][catalog]") {
  const SpriteCatalog catalog;
  const ActorLook machine = Player(kMadogearBody);
  CHECK(BodySpritePath(catalog, machine) == "data\\sprite\\인간족\\몸통\\남\\마도기어_남");
  CHECK(WeaponSpritePaths(catalog, machine).empty());
  CHECK(ShieldSpritePaths(catalog, machine).empty());
  CHECK(WeaponTrailPaths(catalog, machine) ==
        std::vector<std::string>{"data\\sprite\\인간족\\마도기어\\마도기어_남_검광"});

  // ⚠ LA COMBINAISON CHANGE LE FICHIER, PAS LE DOSSIER.
  ActorLook suit = machine;
  suit.madogear_suit = true;
  CHECK(BodySpritePath(catalog, suit) == "data\\sprite\\인간족\\몸통\\남\\마도아머_남");
  CHECK(WeaponTrailPaths(catalog, suit) ==
        std::vector<std::string>{"data\\sprite\\인간족\\마도기어\\마도아머_남_검광"});

  ActorLook meister = Player(kMeisterMadogearBody);
  meister.madogear_suit = true;
  CHECK(BodySpritePath(catalog, meister) == "data\\sprite\\인간족\\몸통\\남\\MEISTER_MADOGEAR2_남");
  CHECK(WeaponTrailPaths(catalog, meister) ==
        std::vector<std::string>{
            "data\\sprite\\인간족\\MEISTER_MADOGEAR\\MEISTER_MADOGEAR2_남_검광"});

  // La combinaison ne touche à rien d'autre qu'à un corps de Madogear.
  ActorLook mechanic = Player(kMechanic);
  mechanic.madogear_suit = true;
  CHECK(BodySpritePath(catalog, mechanic) == BodySpritePath(catalog, Player(kMechanic)));
}

TEST_CASE("Catalogue : la combinaison de Madogear élargit l'ombre",
          "[render][shown_job][catalog]") {
  const SpriteCatalog catalog;
  ActorLook suit = Player(kMadogearBody);
  CHECK(ShadowScaleFor(catalog, suit) == 1.0F);
  suit.madogear_suit = true;
  CHECK(ShadowScaleFor(catalog, suit) == 1.5F);
  ActorLook baby = Player(kBabyMadogearBody);
  baby.madogear_suit = true;
  CHECK(ShadowScaleFor(catalog, baby) == 1.12F);
}

TEST_CASE("Métier à pied : chaque corps monté rend celui du client, et rien d'autre ne bouge",
          "[render][shown_job]") {
  // LA TABLE DU CLIENT, entière : corps monté → métier à pied. Où deux métiers
  // partagent une monture, c'est le plus petit qu'il rend.
  struct FootPair {
    std::uint32_t mounted;
    std::uint32_t foot;
  };
  const std::vector<FootPair> client = {
      {13, 7},      {21, 14},     {4014, 4008}, {4022, 4015}, {4036, 4030}, {4044, 4037},
      {4080, 4054}, {4081, 4060}, {4082, 4066}, {4083, 4073}, {4084, 4056}, {4085, 4062},
      {4086, 4058}, {4087, 4064}, {4088, 4054}, {4089, 4060}, {4090, 4054}, {4091, 4060},
      {4092, 4054}, {4093, 4060}, {4094, 4054}, {4095, 4060}, {4109, 4096}, {4110, 4102},
      {4111, 4098}, {4112, 4100}, {4114, 25},   {4115, 24},   {4116, 1},    {4117, 4049},
      {4118, 4011}, {4119, 5},    {4120, 4071}, {4121, 4019}, {4122, 3},    {4123, 4047},
      {4124, 0},    {4125, 15},   {4126, 4},    {4127, 4070}, {4128, 23},   {4129, 4057},
      {4130, 2},    {4131, 16},   {4132, 4067}, {4133, 4055}, {4134, 9},    {4135, 4017},
      {4136, 4010}, {4137, 18},   {4138, 10},   {4139, 4016}, {4140, 4059}, {4141, 6},
      {4142, 17},   {4143, 4072}, {4144, 4018}, {4145, 12},   {4146, 4013}, {4147, 20},
      {4148, 4068}, {4149, 19},   {4150, 4012}, {4151, 4069}, {4152, 4021}, {4153, 4020},
      {4154, 11},   {4155, 4046}, {4156, 8},    {4157, 4009}, {4158, 4023}, {4159, 4024},
      {4160, 4025}, {4161, 4026}, {4162, 4027}, {4163, 4028}, {4164, 4034}, {4165, 4035},
      {4166, 4038}, {4167, 4039}, {4168, 4040}, {4169, 4041}, {4170, 4042}, {4171, 4043},
      {4172, 4045}, {4173, 4097}, {4174, 4099}, {4175, 4101}, {4176, 4103}, {4177, 4104},
      {4178, 4105}, {4179, 4106}, {4180, 4107}, {4181, 4029}, {4182, 4108}, {4183, 4001},
      {4184, 4002}, {4185, 4003}, {4186, 4004}, {4187, 4005}, {4188, 4006}, {4189, 4007},
      {4192, 4190}, {4193, 4191}, {4194, 4031}, {4195, 4032}, {4196, 4033}, {4197, 4058},
      {4198, 4056}, {4199, 7},    {4200, 4008}, {4201, 4066}, {4202, 4054}, {4203, 14},
      {4204, 4015}, {4205, 4100}, {4206, 4098}, {4207, 4030}, {4208, 4102}, {4209, 4096},
      {4210, 4037}, {4213, 4211}, {4214, 4212}, {4216, 4215}, {4219, 4218}, {4221, 4220},
      {4230, 4222}, {4231, 4223}, {4232, 4224}, {4233, 4225}, {4234, 4226}, {4235, 4227},
      {4236, 4228}, {4237, 4229}, {4245, 4239}, {4246, 4240}, {4247, 4241}, {4248, 4242},
      {4265, 4252}, {4266, 4253}, {4267, 4254}, {4268, 4255}, {4269, 4256}, {4270, 4257},
      {4271, 4258}, {4272, 4259}, {4273, 4260}, {4274, 4261}, {4275, 4262}, {4276, 4263},
      {4277, 4264}, {4278, 4257}, {4279, 4253}, {4280, 4252}, {4281, 4258}, {4309, 4302},
      {4310, 4303}, {4311, 4304}, {4312, 4305}, {4313, 4306}, {4314, 4307}, {4315, 4308}};
  constexpr std::uint32_t kPastLastJob = 4400;
  std::vector<std::uint32_t> foot_of(kPastLastJob);
  for (std::uint32_t job = 0; job < kPastLastJob; ++job) {
    foot_of[job] = job;
  }
  for (const FootPair& pair : client) {
    foot_of[pair.mounted] = pair.foot;
  }
  for (std::uint32_t job = 0; job < kPastLastJob; ++job) {
    INFO("corps " << job);
    CHECK(ro::render::FootJobOf(job) == foot_of[job]);
  }
}
