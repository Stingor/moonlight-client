#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "ro/render/sprite_catalog.h"
#include "ro/render/sprite_frame.h"

#include "sprite_catalog_internal.h"

namespace ro::render {
namespace {

using catalog_internal::DualWieldClass;
using catalog_internal::IsDualWielder;
using catalog_internal::kAxeClass;
using catalog_internal::kBookClass;
using catalog_internal::kBowClass;
using catalog_internal::kDaggerClass;
using catalog_internal::kFirstWeaponPairClass;
using catalog_internal::kFistsClass;
using catalog_internal::kGrenadeLauncherClass;
using catalog_internal::kKatarClass;
using catalog_internal::kKnuckleClass;
using catalog_internal::kLastWeaponPairClass;
using catalog_internal::kMaceClass;
using catalog_internal::kRifleClass;
using catalog_internal::kRodClass;
using catalog_internal::kShurikenClass;
using catalog_internal::kSpearClass;
using catalog_internal::kSwordClass;
using catalog_internal::kTwoHandAxeClass;
using catalog_internal::kTwoHandRodClass;
using catalog_internal::kTwoHandSpearClass;
using catalog_internal::WeaponClassOf;
using catalog_internal::WeaponFamilyClass;

template <std::size_t N>
bool Listed(const std::array<std::uint32_t, N>& values, std::uint32_t value) {
  return std::ranges::find(values, value) != values.end();
}

constexpr std::uint8_t kFemale = 0;

// ── L'animation d'attaque ───────────────────────────────────────────────────
//
// Un joueur attaque au rang dix ou au rang onze. Le client décide par FAMILLE
// de métiers et par famille d'arme — la classe de l'arme tenue, ramenée à sa
// famille par GetRealWeaponId ; un couple d'armes compte pour sa classe de
// couple. Les familles sont celles du client, telles qu'il les groupe.

// Une valeur au-delà de 105 est un identifiant d'objet, à traduire en classe ;
// en deçà, c'est déjà une classe.
constexpr std::uint32_t kLastBareClassValue = 105;

// Épéistes, chevaliers, croisés et leurs descendants : la lance.
constexpr std::array<std::uint32_t, 39> kSpearFamily = {
    1,    7,    13,   14,   21,   4002, 4008, 4014, 4015, 4022, 4024, 4030, 4036,
    4037, 4044, 4054, 4060, 4066, 4073, 4080, 4081, 4082, 4083, 4088, 4089, 4090,
    4091, 4092, 4093, 4094, 4095, 4096, 4102, 4109, 4110, 4252, 4258, 4280, 4281};
// Mages et marchands : la dague.
constexpr std::array<std::uint32_t, 6> kDaggerFamily = {2, 5, 4003, 4006, 4025, 4028};
// Novices : la dague, pour une femme.
constexpr std::array<std::uint32_t, 8> kNoviceFamily = {0, 23, 4001, 4023, 4045, 4190, 4191, 4307};
// Archers : tout sauf l'arc.
constexpr std::array<std::uint32_t, 3> kArcherFamily = {3, 4004, 4026};
// Voleurs, chasseurs, bardes, danseuses et leurs descendants : l'arc.
constexpr std::array<std::uint32_t, 35> kBowFamily = {
    6,    11,   17,   19,   20,   4007, 4012, 4018, 4020, 4021, 4029, 4034,
    4040, 4042, 4043, 4056, 4062, 4068, 4069, 4072, 4075, 4076, 4079, 4084,
    4085, 4098, 4104, 4105, 4108, 4111, 4257, 4260, 4263, 4264, 4278};
// Prêtres : le livre.
constexpr std::array<std::uint32_t, 7> kBookFamily = {8, 4009, 4031, 4057, 4063, 4099, 4256};
// Sorciers, lieurs et faucheurs : la dague pour un homme, le bâton pour une femme.
constexpr std::array<std::uint32_t, 12> kRodFamily = {9,    4010, 4032, 4049, 4055, 4061,
                                                      4097, 4227, 4240, 4242, 4255, 4303};
// Forgerons et alchimistes : l'épée, la hache, la hache à deux mains, la masse.
constexpr std::array<std::uint32_t, 11> kSmithFamily = {10,   4011, 4033, 4058, 4064, 4086,
                                                        4087, 4100, 4112, 4253, 4279};
constexpr std::array<std::uint32_t, 7> kAlchemistFamily = {18, 4019, 4041, 4071, 4078, 4107, 4259};
// Moines : les poings nus, ou le poing.
constexpr std::array<std::uint32_t, 7> kKnuckleFamily = {15, 4016, 4038, 4070, 4077, 4106, 4262};
// Sages : la lance à deux mains, le bâton, le livre, le bâton à deux mains.
constexpr std::array<std::uint32_t, 7> kSageFamily = {16, 4017, 4039, 4067, 4074, 4103, 4261};
// Ninjas : le shuriken.
constexpr std::array<std::uint32_t, 8> kShurikenFamily = {25,   4211, 4212, 4222,
                                                          4223, 4224, 4304, 4305};
// Tireurs : du fusil au lance-grenades.
constexpr std::array<std::uint32_t, 5> kGunFamily = {24, 4215, 4228, 4229, 4306};
// Taekwons et leurs astres : le client tire au sort à chaque coup, sept fois
// sur dix le rang onze.
constexpr std::array<std::uint32_t, 12> kTaekwonFamily = {4046, 4047, 4048, 4225, 4226, 4238,
                                                          4239, 4241, 4243, 4244, 4302, 4316};
constexpr std::uint8_t kTaekwonSecondVariantTenths = 7;

// La classe que le client juge : le couple pour qui tient deux armes, l'arme
// droite sinon ; rien tenu vaut les poings nus.
std::uint32_t JudgedWeaponClass(const SpriteCatalog& catalog, const ActorLook& look) {
  if (look.weapon <= kLastBareClassValue) {
    return look.weapon;
  }
  if (IsDualWielder(look.job) && WeaponClassOf(catalog, look.shield) > 0) {
    return DualWieldClass(look.weapon, look.shield);
  }
  return static_cast<std::uint32_t>(std::max(0, WeaponClassOf(catalog, look.weapon)));
}

bool SecondAttackVariant(const SpriteCatalog& catalog, const ActorLook& look) {
  const std::uint32_t family = WeaponFamilyClass(catalog, JudgedWeaponClass(catalog, look));
  const bool female = look.sex == kFemale;
  const std::uint32_t job = look.job;
  if (Listed(kSpearFamily, job)) {
    return family == kSpearClass || family == kTwoHandSpearClass;
  }
  if (Listed(kDaggerFamily, job)) {
    return family == kDaggerClass;
  }
  if (Listed(kNoviceFamily, job)) {
    return female && family == kDaggerClass;
  }
  if (Listed(kArcherFamily, job)) {
    return family != kBowClass;
  }
  if (Listed(kBowFamily, job)) {
    return family == kBowClass;
  }
  if (Listed(kBookFamily, job)) {
    return family == kBookClass;
  }
  if (Listed(kRodFamily, job)) {
    if (family == kDaggerClass) {
      return !female;
    }
    return female && (family == kRodClass || family == kTwoHandRodClass);
  }
  if (Listed(kSmithFamily, job) || Listed(kAlchemistFamily, job)) {
    return family == kSwordClass || family == kAxeClass || family == kTwoHandAxeClass ||
           family == kMaceClass;
  }
  if (IsDualWielder(job)) {
    return family == kKatarClass ||
           (family >= kFirstWeaponPairClass && family <= kLastWeaponPairClass);
  }
  if (Listed(kKnuckleFamily, job)) {
    return family == kFistsClass || family == kKnuckleClass;
  }
  if (Listed(kSageFamily, job)) {
    return family == kTwoHandSpearClass || family == kRodClass || family == kBookClass ||
           family == kTwoHandRodClass;
  }
  if (Listed(kShurikenFamily, job)) {
    return family == kShurikenClass;
  }
  if (Listed(kGunFamily, job)) {
    return family >= kRifleClass && family <= kGrenadeLauncherClass;
  }
  return false;
}

// ── L'image où le coup porte ────────────────────────────────────────────────
//
// La sixième, sauf pour quelques métiers ; et pour un katar ou un couple
// d'armes, la troisième — mais le client juge alors la valeur brute que le
// serveur envoie, un identifiant d'objet sur ce serveur, que ces classes ne
// peuvent égaler.
constexpr float kDefaultDamageFrame = 6.0F;
constexpr float kMerchantDamageFrame = 5.85F;
constexpr float kThiefDamageFrame = 5.75F;
constexpr float kMaleSecondVariantDamageFrame = 5.85F;
constexpr float kKatarDamageFrame = 3.0F;
constexpr std::uint32_t kMerchant = 5;
constexpr std::uint32_t kThief = 6;
constexpr std::array<std::uint32_t, 5> kNoviceDamageJobs = {0, 23, 4045, 4190, 4191};

// ── Les deux cadences que le serveur donne à un coup ────────────────────────
//
// Le paquet en porte DEUX durées : celle de qui frappe et celle de qui encaisse.
// Ni l'une ni l'autre n'est une durée à tenir — chacune est un DIVISEUR, et
// chacune a son unité, qui n'est pas la même :
//
//   l'attaque   / 432 ms
//   la blessure / 288 ms
//
// Un facteur d'un laisse l'animation à la cadence que son .act déclare ; deux
// la joue deux fois plus lentement. L'attaque est plafonnée à un — sauf pour un
// arc, dont le tir prend son temps — et la blessure ne l'est pas.
constexpr float kUnitAttackMotionMs = 432.0F;
constexpr float kUnitHurtMotionMs = 288.0F;
// Le pas de l'animation : une cadence se compte en pas de vingt-quatre
// millisecondes par image.
constexpr float kActionStepMs = 24.0F;

// Ce que le client met à la place d'une durée NULLE.
//
// Pour l'attaque, toujours. Pour la blessure, seulement quand le coup n'est ni
// ordinaire ni encaissé : ces deux sortes-là gardent le zéro, et zéro veut
// dire « la cadence du fichier » — le client refuse d'écrire un facteur qui
// n'est pas positif.
constexpr std::uint32_t kDefaultMotionMs = 1440;
constexpr std::uint8_t kPlainBlow = 0;
constexpr std::uint8_t kEnduredBlow = 4;

// ── Le retard d'un projectile ───────────────────────────────────────────────
//
// La cible bronche quand l'image du coup s'affiche ; un archer y ajoute le vol
// de sa flèche — `kArrowFlightMs`, huit pas de cadence —, un joueur à baguette
// le vol de son trait, trois fois plus long — voir `ShootsWandBolts` —, et
// quelques monstres, dont le projectile est plus lent que la flèche, le leur.
struct SlowProjectile {
  std::uint32_t job;
  float delay_ms;
};
constexpr std::array<SlowProjectile, 7> kSlowProjectiles = {{{1016, 192.0F},
                                                             {1420, 192.0F},
                                                             {1285, 912.0F},
                                                             {1830, 912.0F},
                                                             {1286, 408.0F},
                                                             {1829, 408.0F},
                                                             {1287, 408.0F}}};
// Le coup d'une COMPÉTENCE ne rattrape que deux espèces, du vol d'une flèche.
constexpr std::array<std::uint32_t, 2> kSkillHitSlowProjectileJobs = {1016, 1420};

// L'homoncule tire son rang d'attaque à pile ou face : un dixième pair donne
// le second rang.
constexpr std::uint8_t kEvenTenthDivisor = 2;

}  // namespace

std::size_t AttackAnimation(const SpriteCatalog& catalog, const ActorLook& look,
                            std::uint8_t chance_tenth) {
  if (!look.player) {
    if (IsHomunculusJob(look.job) && look.job != kSingleAttackHomunculusJob &&
        chance_tenth % kEvenTenthDivisor == 0) {
      return kHomunculusAnimationAttack2;
    }
    return kCreatureAnimationAttack;
  }
  if (Listed(kTaekwonFamily, look.job)) {
    return chance_tenth < kTaekwonSecondVariantTenths ? kAnimationAttack3 : kAnimationAttack2;
  }
  return SecondAttackVariant(catalog, look) ? kAnimationAttack3 : kAnimationAttack2;
}

std::size_t AttackVariantAnimation(const SpriteCatalog& catalog, const ActorLook& look) {
  if (!look.player) {
    return kCreatureAnimationAttack;
  }
  return SecondAttackVariant(catalog, look) ? kAnimationAttack3 : kAnimationAttack2;
}

float DamageFrame(const ActorLook& look, std::size_t attack_animation) {
  if (attack_animation != kAnimationAttack3) {
    if (look.job == kMerchant) {
      return kMerchantDamageFrame;
    }
    return look.job == kThief ? kThiefDamageFrame : kDefaultDamageFrame;
  }
  if (IsDualWielder(look.job)) {
    const bool paired = look.weapon == kKatarClass || (look.weapon >= kFirstWeaponPairClass &&
                                                       look.weapon <= kLastWeaponPairClass);
    return paired ? kKatarDamageFrame : kDefaultDamageFrame;
  }
  if (Listed(kNoviceDamageJobs, look.job)) {
    return look.sex != kFemale ? kMaleSecondVariantDamageFrame : kDefaultDamageFrame;
  }
  return kDefaultDamageFrame;
}

float AttackMotionFactor(const SpriteCatalog& catalog, const ActorLook& look,
                         std::uint32_t attack_motion_ms) {
  const std::uint32_t given = attack_motion_ms == 0 ? kDefaultMotionMs : attack_motion_ms;
  const bool bow = WeaponClassOf(catalog, look.weapon) == static_cast<int>(kBowClass);
  const auto capped =
      bow ? static_cast<float>(given) : std::min(static_cast<float>(given), kUnitAttackMotionMs);
  return capped / kUnitAttackMotionMs;
}

bool ImpactSuppressesNextArrow(std::uint16_t skill_id) {
  constexpr std::array<std::uint16_t, 6> kSkills = {2288, 5316, 5322, 5334, 6513, 6514};
  return std::ranges::find(kSkills, skill_id) != kSkills.end();
}

std::optional<float> ImpactFramePeriodSteps(std::uint16_t skill_id, std::uint8_t damage_type,
                                            std::uint32_t attack_motion_ms) {
  constexpr std::array<std::uint16_t, 2> kSlowingSkills = {2284, 732};
  constexpr std::uint8_t kSplashBlow = 5;
  constexpr float kSlowingMultiple = 4.0F;
  if (damage_type != kSplashBlow || attack_motion_ms == 0 ||
      std::ranges::find(kSlowingSkills, skill_id) == kSlowingSkills.end()) {
    return std::nullopt;
  }
  return static_cast<float>(attack_motion_ms) / kUnitAttackMotionMs * kSlowingMultiple;
}

float HurtMotionFactor(std::uint32_t hurt_motion_ms, std::uint8_t damage_type) {
  std::uint32_t given = hurt_motion_ms;
  if (given == 0 && damage_type != kPlainBlow && damage_type != kEnduredBlow) {
    given = kDefaultMotionMs;
  }
  if (given == 0) {
    return 1.0F;
  }
  return static_cast<float>(given) / kUnitHurtMotionMs;
}

float FileCadenceSteps(float frame_interval_ms) {
  return std::max(frame_interval_ms / kActionStepMs, 1.0F);
}

HurtPlay HurtPlayFor(float file_steps, std::uint32_t hurt_motion_ms, std::uint8_t damage_type,
                     bool player_class) {
  std::uint32_t given = hurt_motion_ms;
  if (given == 0 && damage_type != kPlainBlow && damage_type != kEnduredBlow) {
    given = kDefaultMotionMs;
  }
  const float quotient = static_cast<float>(given) / kUnitHurtMotionMs;
  HurtPlay play;
  play.cadence_steps = file_steps;
  // Un quotient nul ne pose rien : cadence du fichier, un seul passage.
  if (!(quotient > 0.0F)) {
    return play;
  }
  if (quotient > 1.0F) {
    play.passes = quotient;
  } else {
    play.cadence_steps = player_class ? quotient * file_steps : quotient;
  }
  return play;
}

float HurtCadenceSteps(float file_steps, std::uint32_t hurt_motion_ms, std::uint8_t damage_type,
                       bool player_class) {
  return HurtPlayFor(file_steps, hurt_motion_ms, damage_type, player_class).cadence_steps;
}

std::optional<std::size_t> HurtFrameAt(const HurtPlay& play, std::size_t frame_count,
                                       float since_ms) {
  if (since_ms < 0.0F || frame_count == 0 || !(play.cadence_steps > 0.0F)) {
    return std::nullopt;
  }
  const float position = since_ms / kActionStepMs / play.cadence_steps;
  const auto images = static_cast<float>(frame_count);
  // ⚠ LE PASSAGE COMPTE AU MOINS POUR UN : le client ne teste la fin qu'une
  // fois la dernière image atteinte.
  if (position / images >= std::max(play.passes, 1.0F)) {
    return std::nullopt;
  }
  if (position >= images) {
    return frame_count - 1;
  }
  return static_cast<std::size_t>(position);
}

float ArrowDelayMs(const SpriteCatalog& catalog, const ActorLook& look, bool shoots_souls) {
  if (!look.player) {
    return 0.0F;
  }
  float delay_ms = 0.0F;
  if (ShootsArrows(catalog, look)) {
    delay_ms += kArrowFlightMs;
  } else if (ShootsWandBolts(catalog, look)) {
    delay_ms += kWandBoltFlightMs;
  }
  if (shoots_souls && IsDoramJob(look.job)) {
    delay_ms += kArrowFlightMs;
  }
  return delay_ms;
}

float ProjectileDelayMs(const SpriteCatalog& catalog, const ActorLook& look, bool shoots_souls) {
  if (!look.player) {
    for (const SlowProjectile& slow : kSlowProjectiles) {
      if (slow.job == look.job) {
        return slow.delay_ms;
      }
    }
    return 0.0F;
  }
  return ArrowDelayMs(catalog, look, shoots_souls);
}

float SkillHitProjectileDelayMs(const ActorLook& look) {
  return !look.player && Listed(kSkillHitSlowProjectileJobs, look.job) ? kArrowFlightMs : 0.0F;
}

bool ShootsArrows(const SpriteCatalog& catalog, const ActorLook& look) {
  // ⚠ LE MÊME CRIBLE QUE LE RETARD DU PROJECTILE, et il le faut : la flèche
  // qu'on voit partir est celle dont on attend l'impact.
  return look.player && WeaponFamilyClass(catalog, JudgedWeaponClass(catalog, look)) == kBowClass;
}

bool ShootsWandBolts(const SpriteCatalog& catalog, const ActorLook& look) {
  // ⚠ LE MÊME CRIBLE QUE SON RETARD, pour la même raison que la flèche.
  if (!look.player) {
    return false;
  }
  const std::uint32_t family = WeaponFamilyClass(catalog, JudgedWeaponClass(catalog, look));
  return family == kRodClass || family == kTwoHandRodClass;
}

}  // namespace ro::render
