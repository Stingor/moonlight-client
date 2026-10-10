#include "world/hit_popup_rules.h"

#include <algorithm>
#include <cmath>

#include "ro/render/second_hand.h"
#include "ro/sim/movement.h"

namespace moonclient {
namespace {

// Pleine opacité d'un canal, sur laquelle la transparence d'un nuancier se lit.
constexpr float kOpaque = 255.0F;

// Les deux états qui changent le « Miss » en bouclier, numérotés comme le
// serveur les numérote (EFST).
constexpr ro::sim::StatusEffectId kEfstKyrie = 19;
constexpr ro::sim::StatusEffectId kEfstParrying = 104;

// Les classes que le « Miss » tient pour une créature : les PNJ de 45 à 999,
// les monstres de 1001 à 3998, et les classes étendues de 20021 à 31999 — ni
// un homoncule ni un mercenaire.
constexpr std::uint32_t kFirstNpcClass = 45;
constexpr std::uint32_t kPastNpcClass = 1000;
constexpr std::uint32_t kPastMonsterClass = 3999;
constexpr std::uint32_t kFirstExtendedCreatureClass = 20021;
constexpr std::uint32_t kLastExtendedCreatureClass = 31999;

[[nodiscard]] bool MissTestsAttackerAsCreature(std::uint32_t job) noexcept {
  return (job >= kFirstNpcClass && job < kPastNpcClass) ||
         (job > kPastNpcClass && job < kPastMonsterClass) ||
         (job >= kFirstExtendedCreatureClass && job <= kLastExtendedCreatureClass);
}

// Vrai si le coup est CRITIQUE, d'une manière ou de l'autre : e_damage_type,
// dans src/map/clif.hpp.
[[nodiscard]] bool IsCritical(const ro::sim::Strike& strike) noexcept {
  return strike.type == ro::render::kCriticalBlow ||
         strike.type == ro::render::kCriticalMultipleBlows;
}

// Ce que le coup écrit, ou rien s'il a un nombre à montrer.
//
// L'ESQUIVE CHANCEUSE PASSE AVANT LE ZÉRO, et l'exclut : le répartiteur des
// messages de dégâts la reconnaît en tête et renvoie aussitôt, si bien qu'un
// coup esquivé ne montre ni nombre ni « Miss ». L'ordre de ces deux tests est
// donc la règle, et non un détail d'écriture.
[[nodiscard]] std::optional<ro::render::DamageMark> MarkOf(const ro::sim::Strike& strike) {
  if (strike.type == ro::render::kLuckyDodgeBlow) {
    return ro::render::DamageMark::kLucky;
  }
  // ZÉRO dégât veut dire MANQUÉ : c'est la seule condition que le client pose,
  // et il pose alors sa marque au lieu d'un nombre. Un montant NÉGATIF n'est
  // pas un raté : voir `IsSilentAmount`.
  if (strike.damage == 0) {
    return ro::render::DamageMark::kMiss;
  }
  return std::nullopt;
}

// Vrai quand le montant du coup est NÉGATIF : le serveur annonce ainsi un coup
// FACTICE, qui ne fait que jouer l'animation — Cart Revolution en envoie un
// avant ses vrais coups, à dégâts −30000, pour la première cible de sa zone.
//
// Le client ne le prend ni pour un raté ni pour des dégâts : il en retourne le
// signe, écrit le nombre dans une couleur TRANSPARENTE, et n'en fait pas
// broncher la cible. Rien ne paraît ; le bruit et l'impact, eux, restent.
[[nodiscard]] bool IsSilentAmount(const ro::sim::Strike& strike) noexcept {
  return strike.damage < 0;
}

// Un nombre de cette couleur, à cette allure, sans valeur encore.
[[nodiscard]] ro::render::FloatingNumber Tinted(std::uint32_t abgr,
                                                ro::render::NumberStyle style) noexcept {
  ro::render::FloatingNumber number;
  number.color = RgbOf(abgr);
  number.opacity = OpacityOf(abgr);
  number.style = style;
  return number;
}

// Un nombre de cette couleur, à l'allure du client pour ce type : elle ne
// donne plus que la taille où il s'arrête et le point où il naît.
[[nodiscard]] ro::render::FloatingNumber Moving(std::uint32_t abgr, DamagePath path) noexcept {
  return Tinted(abgr, StyleOfPath(path));
}

// La part d'un coup : le montant du fil est le TOTAL, et le client le divise
// — par le nombre de coups qu'il montre, ou par le compte annoncé quand le coup
// passe par l'impact d'une compétence. Voir `StrikePlan::divisor`.
[[nodiscard]] std::uint32_t PartOf(const ro::sim::Strike& strike, const StrikePlan& plan) noexcept {
  const auto total = static_cast<std::uint32_t>(std::max(strike.damage, 0));
  return plan.divisor > 1 ? total / plan.divisor : total;
}

// ── LE HACHAGE DE LA DISPERSION ─────────────────────────────────────────────
//
// Le mélangeur de SplitMix64 : un incrément puis deux multiplications-décalages
// qui étalent un germe sur soixante-quatre bits. Ce n'est pas un générateur —
// chaque germe se mélange seul, sans état —, et c'est exactement ce qu'on veut
// d'un écart qui doit être le même à chaque image pour le même nombre.
constexpr std::uint64_t kSplitMixIncrement = 0x9E3779B97F4A7C15ULL;
constexpr std::uint64_t kSplitMixFirstMultiplier = 0xBF58476D1CE4E5B9ULL;
constexpr std::uint64_t kSplitMixSecondMultiplier = 0x94D049BB133111EBULL;
constexpr unsigned kSplitMixFirstShift = 30U;
constexpr unsigned kSplitMixSecondShift = 27U;
constexpr unsigned kSplitMixLastShift = 31U;

[[nodiscard]] std::uint64_t Mixed(std::uint64_t seed) noexcept {
  std::uint64_t bits = seed + kSplitMixIncrement;
  bits = (bits ^ (bits >> kSplitMixFirstShift)) * kSplitMixFirstMultiplier;
  bits = (bits ^ (bits >> kSplitMixSecondShift)) * kSplitMixSecondMultiplier;
  return bits ^ (bits >> kSplitMixLastShift);
}

// Un nombre de moins un à un, depuis les vingt-quatre bits de poids fort d'un
// mot de trente-deux : c'est plus fin qu'un pixel, et c'est tout ce qu'il faut.
constexpr unsigned kWordBits = 32U;
constexpr unsigned kFractionBits = 24U;

[[nodiscard]] float SignedFraction(std::uint32_t word) noexcept {
  const auto fraction = static_cast<float>(word >> (kWordBits - kFractionBits)) /
                        static_cast<float>(1U << kFractionBits);
  return fraction * 2.0F - 1.0F;
}

// L'angle du client pour chaque direction de boussole, dans l'ordre du serveur :
// nord, nord-ouest, ouest, sud-ouest, sud, sud-est, est, nord-est. Le sud vaut
// zéro, et l'angle tourne dans le sens des aiguilles d'une montre vu du ciel.
constexpr float kDegreesPerEighth = 45.0F;
// Ce que le client ajoute à une direction du serveur avant d'en faire un angle :
// quatre huitièmes, un demi-tour, sans jamais retrancher le tour.
constexpr unsigned kServerFacingEighthsAdded = 4U;
constexpr std::array<float, ro::sim::kDirectionCount> kClientFacingDegrees = {
    4.0F * kDegreesPerEighth,  // nord
    5.0F * kDegreesPerEighth,  // nord-ouest
    6.0F * kDegreesPerEighth,  // ouest
    7.0F * kDegreesPerEighth,  // sud-ouest
    0.0F,                      // sud
    1.0F * kDegreesPerEighth,  // sud-est
    2.0F * kDegreesPerEighth,  // est
    3.0F * kDegreesPerEighth,  // nord-est
};

// ── LES DEUX LISTES DE L'IMPACT ─────────────────────────────────────────────
//
// ⚠ `scripts/etat_des_competences.py` LES LIT dans ce fichier pour ne plus
// compter comme manquant ce qu'elles portent : leur nom et leur forme — une
// liste de nombres entre accolades — sont un contrat.
constexpr std::array<std::uint16_t, 12> kRisingSkills = {
    413, 415, 417, 419, 421, 2332, 2336, 2342, 2576, 2580, 2592, 2593,
};
constexpr std::array<std::uint16_t, 9> kSkillsCarryingTotalOnEveryHit = {
    273, 371, 2218, 2219, 2220, 2221, 2328, 2329, 2330,
};

// Les sortes de coup que l'impact d'une compétence traite par la voie de ces
// deux listes : e_damage_type, dans src/map/clif.hpp.
constexpr std::uint8_t kEnduredBlow = 4;
constexpr std::uint8_t kSkillBlow = 6;
// Les deux sortes qui éclaboussent — la seconde contre une cible qui encaisse.
// L'impact les traite par une voie courte qui ne lit pas les deux listes —
// sauf pour les compétences de `kSplashTakingLongPath`.
constexpr std::uint8_t kSplashBlow = 5;
constexpr std::uint8_t kSplashBlowEndured = 14;

// LES COMPÉTENCES QUE L'IMPACT NE LAISSE PAS À LA VOIE COURTE des sortes qui
// éclaboussent : elles y prennent la voie longue, celle des deux listes, comme
// aux autres sortes. Une liste en dur, plus une compétence que sa fiche d'action
// range avec elles (5211).
constexpr std::array<std::uint16_t, 31> kSplashTakingLongPath = {
    11,   47,   273,  413,  732,  2258, 2284, 2307, 2317, 2328, 2329, 2332, 2337, 2476, 2519, 2576,
    3006, 3007, 5026, 5046, 5211, 5230, 5235, 5243, 5244, 5245, 5248, 5252, 5283, 5284, 5316,
};

[[nodiscard]] bool IsSplashBlow(std::uint8_t sort) noexcept {
  return sort == kSplashBlow || sort == kSplashBlowEndured;
}

[[nodiscard]] bool TakesSkillImpactPath(const ro::sim::Strike& strike,
                                        const StrikeSides& sides) noexcept {
  const std::uint8_t sort = strike.type;
  const bool sort_read =
      sort == kEnduredBlow || sort == kSkillBlow || sort == ro::render::kMultipleBlows ||
      sort == ro::render::kMultipleBlowsEndured || sort == ro::render::kCriticalMultipleBlows ||
      (IsSplashBlow(sort) &&
       std::ranges::find(kSplashTakingLongPath, strike.skill_id) != kSplashTakingLongPath.end());
  return sort_read && !sides.attacker_is_ground_unit;
}

// Le compte que l'impact divise : celui du paquet, ramené à un quand il sort
// d'un à trente-six — le client ne compte pas au-delà.
[[nodiscard]] std::uint16_t AnnouncedDivisor(std::uint16_t announced_hits) noexcept {
  return announced_hits >= 1 && announced_hits <= ro::render::kLargestHitCount ? announced_hits
                                                                               : std::uint16_t{1};
}

// ── LES COUPS FACTICES DE L'APERÇU ──────────────────────────────────────────
//
// Des montants qu'on reconnaît d'un coup d'œil — quatre chiffres différents
// pour les familles qu'on compare —, et une compétence quelconque pour la
// famille des compétences : le numéro ne sert qu'à n'être pas zéro.
constexpr std::int32_t kRehearsedBlow = 1234;
constexpr std::int32_t kRehearsedSkillBlow = 5678;
constexpr std::int32_t kRehearsedMultipleBlow = 3000;
constexpr std::uint16_t kRehearsedMultipleHits = 3;
constexpr std::int32_t kRehearsedCritical = 2468;
constexpr std::int32_t kRehearsedTakenBlow = 321;
constexpr std::int32_t kRehearsedOthersBlow = 777;
constexpr std::int32_t kRehearsedSpBlow = 99;
constexpr std::int32_t kRehearsedHeal = 250;
constexpr std::int32_t kRehearsedSpRecovery = 40;
constexpr std::uint16_t kRehearsedSkillId = 5;

// ⚠ L'APERÇU SE JOUE SUR LE PERSONNAGE DES DEUX CÔTÉS : un coup qu'il porte
// part de lui, et le « Miss » factice y naît donc — c'est bien là qu'il naît
// en jeu. `Rehearse` pose l'identifiant, ces faits-ci disent qui c'est.
constexpr StrikeSides kRehearsedByMe{
    .attacker_is_self = true, .attacker_is_known = true, .attacker_is_player = true};
constexpr StrikeSides kRehearsedOnMe{.target_is_player = true, .target_is_self = true};
constexpr StrikeSides kRehearsedByOther{};

// LE RANG DE LA BOUCLE QUI MONTRE CHAQUE TYPE, au rang de `DamagePath` : le coup
// ordinaire, le skill, le coup reçu, le critique, le coup multiple, le soin et
// le « Miss ».
constexpr std::array<std::size_t, kDamagePathCount> kRehearsalOfPath = {0, 1, 4, 3, 2, 9, 6};

}  // namespace

std::array<std::uint8_t, 3> RgbOf(std::uint32_t abgr) noexcept {
  return {static_cast<std::uint8_t>(abgr & 0xFFU), static_cast<std::uint8_t>((abgr >> 8U) & 0xFFU),
          static_cast<std::uint8_t>((abgr >> 16U) & 0xFFU)};
}

float OpacityOf(std::uint32_t abgr) noexcept { return static_cast<float>(abgr >> 24U) / kOpaque; }

bool IsRisingSkill(std::uint16_t skill_id) noexcept {
  return std::ranges::find(kRisingSkills, skill_id) != kRisingSkills.end();
}

bool CarriesTotalOnEveryHit(std::uint16_t skill_id) noexcept {
  return std::ranges::find(kSkillsCarryingTotalOnEveryHit, skill_id) !=
         kSkillsCarryingTotalOnEveryHit.end();
}

bool SkillImpactDivides(const ro::sim::Strike& strike, const StrikeSides& sides) noexcept {
  return strike.skill_id != 0 && (TakesSkillImpactPath(strike, sides) ||
                                  (IsSplashBlow(strike.type) && !sides.attacker_is_ground_unit));
}

bool StrikeReachesDamageMessages(const ro::sim::Strike& strike, const StrikeSides& sides) noexcept {
  if (strike.skill_id == 0 || sides.attacker_is_ground_unit) {
    return true;
  }
  return sides.attacker_is_known && SkillImpactDivides(strike, sides);
}

bool StrikeShown(const HitPopupLook& look, const StrikeSides& sides) noexcept {
  if (!look.shown) {
    return false;
  }
  if (sides.target_is_self) {
    return look.own_taken;
  }
  if (sides.attacker_is_self) {
    return look.own_dealt;
  }
  return look.others;
}

std::uint32_t TargetColor(const StrikeSides& sides, const HitPopupLook& look) noexcept {
  if (sides.target_is_self) {
    return look.color_on_self;
  }
  if (sides.target_is_player) {
    return look.color_on_player;
  }
  if (sides.attacker_is_self) {
    return look.color_own_dealt;
  }
  return look.color_on_monster;
}

std::uint32_t MissColor(const StrikeSides& sides, const HitPopupLook& look) noexcept {
  // ⚠ « MES COUPS » N'A PAS DE SENS ICI : le mot est sur l'attaquant, et ce
  // nuancier-là teint un nombre posé sur la CIBLE d'un coup que je porte.
  if (sides.attacker_is_self) {
    return look.color_on_self;
  }
  if (sides.attacker_is_player) {
    return look.color_on_player;
  }
  return look.color_on_monster;
}

bool CarriesMissGuard(const ro::sim::Actor& actor) noexcept {
  return actor.status_effects.contains(kEfstKyrie) || actor.status_effects.contains(kEfstParrying);
}

bool MissShowsGuardShield(const ro::sim::Strike& strike, const StrikeSides& sides) noexcept {
  if (!sides.target_is_self || !sides.target_carries_miss_guard || !sides.attacker_is_known ||
      !MissTestsAttackerAsCreature(sides.attacker_job)) {
    return false;
  }
  const std::optional<ro::render::DamageMark> mark = MarkOf(strike);
  return mark == ro::render::DamageMark::kMiss && StrikeReachesDamageMessages(strike, sides);
}

StrikePlan PlanStrike(const ro::sim::Strike& strike, const StrikeSides& sides,
                      const HitPopupLook& look) {
  StrikePlan plan;
  // LE BOUCLIER PREND LA PLACE DU MOT : voir `MissShowsGuardShield`.
  if (MissShowsGuardShield(strike, sides)) {
    return plan;
  }
  if (!StrikeShown(look, sides) || !StrikeReachesDamageMessages(strike, sides)) {
    return plan;
  }
  if (const std::optional<ro::render::DamageMark> mark = MarkOf(strike); mark.has_value()) {
    const bool lucky = *mark == ro::render::DamageMark::kLucky;
    if (lucky ? !look.lucky : !look.miss) {
      return plan;
    }
    // LE « MISS » NAÎT SUR CELUI QUI A RATÉ, et il n'a personne sur qui naître
    // quand le monde n'a pas l'acteur de l'attaquant : le client renonce alors,
    // au lieu de le poser ailleurs.
    if (!lucky && !sides.attacker_is_known) {
      return plan;
    }
    plan.shown = true;
    plan.mark = mark;
    plan.mark_on_attacker = !lucky;
    plan.path = DamagePath::kWords;
    // La bulle de l'esquive porte DÉJÀ ses couleurs — le client ne lui en passe
    // aucune —, et le blanc la laisse telle quelle. Le « Miss », lui, prend la
    // couleur de son PORTEUR, comme un nombre prend celle de sa cible.
    plan.number = Tinted(lucky ? kClientNumberWhite : MissColor(sides, look),
                         ro::render::NumberStyle::kRising);
    return plan;
  }
  if (IsSilentAmount(strike)) {
    return plan;
  }

  plan.shown = true;
  const bool critical = IsCritical(strike);
  const bool on_player = sides.target_is_player || sides.target_is_self;
  std::uint32_t color = TargetColor(sides, look);
  // La famille d'un coup sur un monstre se lit à sa COMPÉTENCE : un coup
  // ordinaire n'en porte aucune.
  if (on_player) {
    plan.path = DamagePath::kOnPlayer;
  } else if (strike.skill_id != 0) {
    plan.path = DamagePath::kSkillOnMonster;
  }
  // Le CRITIQUE l'emporte sur la cible : le client choisit son allure et sa
  // couleur sur la sorte du coup, si bien qu'un critique jaillit même sur un
  // monstre. Et la MAGIE l'emporte sur le critique — dans cet ordre-là.
  if (critical) {
    color = look.color_critical;
    plan.path = DamagePath::kCritical;
  }
  // ⚠ LA MAGIE NE SE LIT QUE HORS DE L'IMPACT D'UNE COMPÉTENCE : voir
  // `SkillImpactDivides`.
  const bool impact_divides = SkillImpactDivides(strike, sides);
  const bool sp_shown = strike.sp_damage && !impact_divides;
  if (sp_shown) {
    color = look.color_sp;
  }
  plan.number = Moving(color, plan.path);
  plan.star = critical && look.critical_star;
  plan.split = HitSplitOf(look.split);
  // LES COUPS DE PIED SUR UN MONSTRE, sous le partage du client : un nombre
  // par frappe, qui écrit la somme des frappes déjà portées et MONTE — le
  // genre du total, d'où son allure —, à la couleur de la cible et non à
  // celle du critique ; une étoile sous le premier. Un autre partage est un
  // choix du joueur, et vaut alors pour ces compétences comme pour les autres.
  const bool skill_impact = strike.skill_id != 0 && TakesSkillImpactPath(strike, sides);
  if (plan.split == HitSplit::kPartsAndTotal && skill_impact && !on_player &&
      IsRisingSkill(strike.skill_id)) {
    plan.number = Moving(TargetColor(sides, look), DamagePath::kTotal);
    plan.path = DamagePath::kTotal;
    plan.star = false;
    plan.rising_star = true;
    plan.split = HitSplit::kCumulative;
    plan.count = ro::render::ShownHitCount(strike.type, strike.hits);
    plan.divisor = AnnouncedDivisor(strike.hits);
    return plan;
  }
  // COMBIEN DE PARTS. Un acteur ne découpe son coup que si la SORTE du coup le
  // dit multiple ; une unité au sol découpe sur le nombre annoncé, quelle que
  // soit la sorte — son répartiteur ne la lit pas.
  plan.count = plan.split == HitSplit::kTotalOnce ? std::uint16_t{1}
               : sides.attacker_is_ground_unit
                   ? std::clamp(strike.hits, std::uint16_t{1}, ro::render::kLargestHitCount)
                   : ro::render::ShownHitCount(strike.type, strike.hits);
  // « Le total, une fois » est un choix du joueur : il montre le montant entier.
  plan.divisor = plan.split == HitSplit::kTotalOnce ? std::uint16_t{1}
                 : impact_divides                   ? AnnouncedDivisor(strike.hits)
                                                    : plan.count;
  plan.second_hand = ro::render::ShowsSecondHand(strike.skill_id, strike.type, strike.second_damage,
                                                 sides.attacker_job);
  // LE CUMUL À CÔTÉ DES PARTS, sur un monstre seulement : le client y fait
  // monter en jaune la somme des coups déjà portés à côté de chaque part, et ne
  // montre que les parts sur un joueur. La magie l'emporte sur sa couleur comme
  // sur les autres.
  //
  // 🔴 UNE UNITÉ AU SOL LE PORTE MÊME SEULE. Son répartiteur passe le cumul à
  // chaque coup, sans condition : un coup d'une seule frappe y montre donc DEUX
  // nombres du même montant — la part blanche et le cumul jaune. C'est ce qui
  // met les gros chiffres jaunes sur Storm Gust, dont le serveur n'annonce
  // qu'une frappe.
  //
  // ⚠ ET UNE POIGNÉE DE COMPÉTENCES AUSSI : voir `CarriesTotalOnEveryHit`.
  const bool carries_total = plan.count > 1 || sides.attacker_is_ground_unit ||
                             (skill_impact && CarriesTotalOnEveryHit(strike.skill_id));
  if (plan.split == HitSplit::kPartsAndTotal && carries_total && !on_player) {
    plan.total = Moving(sp_shown ? look.color_sp : look.color_total, DamagePath::kTotal);
  }
  return plan;
}

std::uint32_t StrikeHitOffsetMs(const StrikePlan& plan, std::uint16_t index,
                                std::uint32_t step_ms) noexcept {
  return plan.second_hand ? ro::render::TwoHandedHitOffsetMs(index, step_ms)
                          : static_cast<std::uint32_t>(index) * step_ms;
}

std::uint32_t StrikeValueAt(const ro::sim::Strike& strike, const StrikePlan& plan,
                            std::uint16_t index) noexcept {
  const std::uint32_t part = PartOf(strike, plan);
  if (plan.split == HitSplit::kTotalOnce || plan.count <= 1) {
    return part;
  }
  // Chaque coup montre sa part, seule ou ajoutée aux précédentes.
  return plan.split == HitSplit::kCumulative ? part * (index + 1U) : part;
}

std::uint32_t StrikeTotalAt(const ro::sim::Strike& strike, const StrikePlan& plan,
                            std::uint16_t index) noexcept {
  return PartOf(strike, plan) * (index + 1U);
}

std::uint32_t CarriedTotal(const TargetTally& tally, std::uint32_t lands_ms,
                           float forget_ms) noexcept {
  if (lands_ms <= tally.last_ms) {
    return tally.total;
  }
  const auto forgotten_after_ms = static_cast<std::uint32_t>(std::max(forget_ms, 0.0F));
  return lands_ms - tally.last_ms > forgotten_after_ms ? 0U : tally.total;
}

std::optional<ro::render::FloatingNumber> SupportNumber(std::uint16_t skill_id,
                                                        const HitPopupLook& look) {
  const std::optional<std::array<std::uint8_t, 3>> tint = ro::render::SupportNumberColor(skill_id);
  if (!look.shown || !look.heals || !tint.has_value()) {
    return std::nullopt;
  }
  // Le joueur règle les deux couleurs : le vert du client est celui du soin,
  // tout autre celui de la magie rendue.
  const std::uint32_t color =
      *tint == ro::render::kHealNumberColor ? look.color_heal : look.color_sp_recovery;
  // Le soin a SON allure, qui n'est ni celle d'un coup encaissé ni celle d'un
  // coup rendu : voir `DamagePath::kHeal`.
  return Moving(color, DamagePath::kHeal);
}

std::optional<ro::render::FloatingNumber> EffectNumber(std::uint32_t effect_id,
                                                       const HitPopupLook& look) {
  if (!look.shown || !look.effect_numbers) {
    return std::nullopt;
  }
  const std::optional<std::array<std::uint8_t, 3>> table = ro::render::EffectNumberColor(effect_id);
  if (!table.has_value()) {
    return std::nullopt;
  }
  std::uint32_t color = PackAbgr((*table)[0], (*table)[1], (*table)[2]);
  if (effect_id == ro::render::kHpOverTimeEffect) {
    color = look.color_heal;
  } else if (effect_id == ro::render::kSpOverTimeEffect) {
    color = look.color_sp_recovery;
  }
  // Le client donne à ce nombre l'allure du SOIN : c'est le type 114 qu'il
  // passe, le même que pour un montant rendu par compétence.
  return Moving(color, DamagePath::kHeal);
}

std::uint32_t ShownEffectAmount(std::int32_t value) noexcept {
  return value < 0 ? 0U : static_cast<std::uint32_t>(value);
}

std::array<float, 2> ScatterOf(const NumberPath& path, std::uint64_t seed) noexcept {
  if (!path.scatter) {
    return {0.0F, 0.0F};
  }
  // Un rayon sous le plus petit vaut le plus petit : le fichier peut porter
  // n'importe quoi, et le volet ne montre jamais moins.
  const std::uint64_t mixed = Mixed(seed);
  return {SignedFraction(static_cast<std::uint32_t>(mixed)) *
              std::max(path.scatter_x_pixels, kSmallestScatterPixels),
          SignedFraction(static_cast<std::uint32_t>(mixed >> kWordBits)) *
              std::max(path.scatter_y_pixels, kSmallestScatterPixels)};
}

std::uint64_t ScatterSeed(std::uint32_t target_id, std::uint32_t born_ms,
                          std::uint16_t index) noexcept {
  // La cible en poids fort, l'instant en poids faible, et le rang qui décale
  // le tout : trois nombres d'un même coup ont trois germes.
  return ((static_cast<std::uint64_t>(target_id) << kWordBits) | born_ms) +
         static_cast<std::uint64_t>(index) * kSplitMixIncrement;
}

float ClientFacingDegrees(std::uint8_t compass_direction) noexcept {
  return kClientFacingDegrees[compass_direction % ro::sim::kDirectionCount];
}

float ClientBodyDegrees(const ro::sim::Actor& actor, std::uint8_t shown_facing) noexcept {
  // Pendant la marche, et après elle tant que rien ne l'a tourné, c'est le pas
  // qui donne le regard, quelle que soit l'origine notée.
  const bool walked = actor.movement.has_value() && !actor.turned_since_walk_began;
  const ro::sim::FacingOrigin origin = walked ? ro::sim::FacingOrigin::kWalk : actor.facing_origin;
  switch (origin) {
    case ro::sim::FacingOrigin::kServer:
      return kDegreesPerEighth * static_cast<float>(shown_facing % ro::sim::kDirectionCount +
                                                    kServerFacingEighthsAdded);
    case ro::sim::FacingOrigin::kAim:
      if (actor.aimed_dx_cells != 0.0F || actor.aimed_dy_cells != 0.0F) {
        return ro::sim::ClientAimDegrees(actor.aimed_dx_cells, actor.aimed_dy_cells);
      }
      break;
    case ro::sim::FacingOrigin::kWalk:
      break;
  }
  return ClientFacingDegrees(shown_facing);
}

std::array<float, 2> BurstDriftInWorld(float client_body_degrees) noexcept {
  const std::array<float, 2> client = ro::render::BurstDriftClientDiagonal(client_body_degrees);
  // La profondeur du client est l'opposée de la nôtre : x reste, z se retourne.
  return {client[0], -client[1]};
}

Rehearsal RehearsalAt(std::size_t index) noexcept {
  Rehearsal shown;
  ro::sim::Strike& strike = shown.strike;
  // Dans l'ordre où l'on règle : ce que je porte, ce que je reçois, ce que les
  // autres portent, les mots, la magie, puis ce qui rend.
  switch (index % kRehearsalCount) {
    case 0:
      strike.damage = kRehearsedBlow;
      shown.sides = kRehearsedByMe;
      break;
    case 1:
      strike.damage = kRehearsedSkillBlow;
      strike.skill_id = kRehearsedSkillId;
      // La sorte qu'un paquet de compétence porte : aux autres, l'impact se tait.
      strike.type = kSkillBlow;
      shown.sides = kRehearsedByMe;
      break;
    case 2:
      strike.damage = kRehearsedMultipleBlow;
      strike.type = ro::render::kMultipleBlows;
      strike.hits = kRehearsedMultipleHits;
      shown.sides = kRehearsedByMe;
      break;
    case 3:
      strike.damage = kRehearsedCritical;
      strike.type = ro::render::kCriticalBlow;
      shown.sides = kRehearsedByMe;
      break;
    case 4:
      strike.damage = kRehearsedTakenBlow;
      shown.sides = kRehearsedOnMe;
      break;
    case 5:
      strike.damage = kRehearsedOthersBlow;
      shown.sides = kRehearsedByOther;
      break;
    case 6:
      strike.damage = 0;
      shown.sides = kRehearsedByMe;
      break;
    case 7:
      strike.type = ro::render::kLuckyDodgeBlow;
      shown.sides = kRehearsedOnMe;
      break;
    case 8:
      strike.damage = kRehearsedSpBlow;
      strike.sp_damage = true;
      shown.sides = kRehearsedOnMe;
      break;
    case 9:
      shown.support_skill_id = ro::render::kHealSkill;
      shown.support_amount = kRehearsedHeal;
      break;
    default:
      shown.support_skill_id = ro::render::kSpRecoverySkill;
      shown.support_amount = kRehearsedSpRecovery;
      break;
  }
  return shown;
}

std::uint32_t RehearsalStepMs(const HitPopupLook& look) noexcept {
  return static_cast<std::uint32_t>(
      std::max(look.rehearsal_interval_ms, kShortestRehearsalIntervalMs));
}

Rehearsal PathRehearsal(DamagePath path) noexcept {
  const auto rank = static_cast<std::size_t>(path);
  return RehearsalAt(kRehearsalOfPath[rank < kDamagePathCount ? rank : 0]);
}

}  // namespace moonclient
