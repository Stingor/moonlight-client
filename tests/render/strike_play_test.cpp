// CE QU'UN COUP FAIT JOUER À QUI LE PORTE : le rang, sa cadence, l'image de
// frappe et l'instant où le coup touche — d'après la tenue et les faits du
// coup, sur des .act synthétiques.

#include "ro/render/strike_play.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/formats/act.h"
#include "ro/formats/spr.h"
#include "ro/render/actor_sprite.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/render/skill_use_motion.h"
#include "ro/render/sprite_catalog.h"
#include "ro/render/sprite_frame.h"

#include "support/sprite_builder.h"

using Catch::Approx;
using ro::render::ActorLook;
using ro::render::ActorOutfit;
using ro::render::kAnimationAttack2;
using ro::render::kAnimationCast;
using ro::render::kCreatureAnimationAttack;
using ro::render::kDirectionCount;
using ro::render::kHomunculusAnimationAttack2;
using ro::render::kHomunculusAnimationSkill;
using ro::render::kResetHitFrame;
using ro::render::kStrikeAttackState;
using ro::render::kStrikeRestState;
using ro::render::kStrikeUseState;
using ro::render::kUseStateHitFrame;
using ro::render::SkillHitMotionState;
using ro::render::SkillHitSetsMotionFactor;
using ro::render::SplashHitKeepsAction;
using ro::render::SpriteCatalog;
using ro::render::StrikeFacts;
using ro::render::StrikePlay;
using ro::render::StrikePlayOf;
using ro::render::UsePlayOf;
using ro::test_support::ActActionSpec;
using ro::test_support::ActBuildOptions;
using ro::test_support::ActFrameSpec;
using ro::test_support::ActLayerSpec;
using ro::test_support::BuildAct;
using ro::test_support::BuildSpr;
using ro::test_support::SprBuildOptions;

namespace {

// La cadence ordinaire des actions d'essai : quatre pas, soit quatre-vingt-seize
// millisecondes par image ; l'incantation du joueur va deux fois plus vite.
constexpr float kOrdinarySpeed = 4.0F;
constexpr float kOrdinaryIntervalMs = 96.0F;
constexpr float kCastSpeed = 2.0F;
constexpr float kCastIntervalMs = 48.0F;
constexpr std::size_t kPlayerAnimations = 13;
constexpr std::size_t kCreatureAnimations = 7;
constexpr std::size_t kFramesPerAction = 8;
// L'image du marqueur du coup dans le rang d'attaque de la créature, et dans
// le second rang d'attaque de l'homoncule.
constexpr std::int32_t kCreatureStrikeFrame = 1;
constexpr std::int32_t kHomunculusStrikeFrame = 3;
// La durée qu'un coup à vitesse un annonce, et sa moitié.
constexpr std::uint32_t kUnitAttackMotionMs = 432;
constexpr std::uint32_t kHalfAttackMotionMs = 216;

constexpr std::uint16_t kBash = 5;
constexpr std::uint16_t kColdBolt = 14;
constexpr std::uint16_t kDoubleStrafe = 46;
constexpr std::uint16_t kCallSpirits = 261;
constexpr std::uint16_t kChainCombo = 272;
constexpr std::uint16_t kSpiralPierceLike = 490;
constexpr std::uint16_t kMultiHitOnlyException = 2261;
constexpr std::uint16_t kRegistryException = 5411;
constexpr std::uint8_t kSingleBlow = 6;
constexpr std::uint8_t kMultiHitBlow = 8;
constexpr std::uint8_t kSplashBlow = 5;
constexpr std::uint8_t kEnduredSplashBlow = 14;
constexpr std::uint16_t kRaid = 214;
// Une compétence de la liste qui prend le chemin ordinaire même éclaboussée.
constexpr std::uint16_t kSplashOrdinaryPath = 11;
constexpr std::uint32_t kNovice = 0;
constexpr std::uint32_t kSummoner = 4218;
constexpr std::uint32_t kPoring = 1002;
constexpr std::uint32_t kFabre = 1007;
constexpr std::uint32_t kArcherSkeleton = 1016;
constexpr std::uint32_t kInjustice = 1285;
constexpr std::uint32_t kLif = 6001;

// Un sprite d'une image : seul le .act compte ici.
std::vector<std::uint8_t> SpriteBytes() {
  SprBuildOptions options;
  options.indexed = {{2, 2, {1, 1, 1, 1}}};
  options.palette = {{0, 0, 0}, {255, 0, 0}};
  return BuildSpr(options);
}

// Le marqueur du coup d'un rang : l'image qui le porte, ou moins un.
struct StrikeMarker {
  std::size_t animation;
  std::int32_t frame;
};

// Un .act de `animations` rangs de huit images : l'incantation d'un joueur va à
// la cadence rapide, et un marqueur du coup se pose où on le demande.
std::vector<std::uint8_t> AnimationBytes(std::size_t animations,
                                         const std::vector<StrikeMarker>& markers) {
  ActBuildOptions options;
  options.sound_files = {std::string(ro::render::kAttackSoundMarker)};
  for (std::size_t action = 0; action < animations * kDirectionCount; ++action) {
    const std::size_t animation = action / kDirectionCount;
    ActActionSpec spec;
    for (std::size_t frame = 0; frame < kFramesPerAction; ++frame) {
      ActLayerSpec layer;
      layer.frame_index = 0;
      std::int32_t sound = -1;
      for (const StrikeMarker& marker : markers) {
        if (marker.animation == animation && marker.frame == static_cast<std::int32_t>(frame)) {
          sound = 0;
        }
      }
      spec.frames.push_back(ActFrameSpec{{layer}, {}, sound});
    }
    options.actions.push_back(std::move(spec));
    options.speeds.push_back(animation == kAnimationCast && animations == kPlayerAnimations
                                 ? kCastSpeed
                                 : kOrdinarySpeed);
  }
  return BuildAct(options);
}

// Une tenue d'essai : un corps seul, sur un .act bâti ici.
struct Dressed {
  explicit Dressed(std::size_t animations, const std::vector<StrikeMarker>& markers = {})
      : sprite(*ro::formats::SprSprite::Parse(SpriteBytes())),
        animation(*ro::formats::ActAnimation::Parse(AnimationBytes(animations, markers))),
        outfit(Pieces(sprite, animation)) {}

  static std::vector<ro::render::ActorPiece> Pieces(const ro::formats::SprSprite& body_sprite,
                                                    const ro::formats::ActAnimation& body_act) {
    ro::render::ActorPiece body;
    body.sprite = &body_sprite;
    body.animation = &body_act;
    body.part = ro::render::ActorPart::kBody;
    return {body};
  }

  ro::formats::SprSprite sprite;
  ro::formats::ActAnimation animation;
  ActorOutfit outfit;
};

ActorLook Player(std::uint32_t job) {
  ActorLook look;
  look.player = true;
  look.job = job;
  look.sex = 0;
  return look;
}

ActorLook Creature(std::uint32_t job) {
  ActorLook look;
  look.job = job;
  return look;
}

StrikeFacts Ordinary(std::uint32_t attack_motion_ms = kUnitAttackMotionMs) {
  StrikeFacts facts;
  facts.attack_motion_ms = attack_motion_ms;
  return facts;
}

StrikeFacts Skill(std::uint16_t skill_id, std::uint8_t damage_type,
                  std::uint32_t attack_motion_ms = kUnitAttackMotionMs) {
  StrikeFacts facts;
  facts.skill_id = skill_id;
  facts.damage_type = damage_type;
  facts.attack_motion_ms = attack_motion_ms;
  return facts;
}

}  // namespace

TEST_CASE("Coup : l'attaque d'un joueur porte à l'image de sa table", "[render][strike]") {
  const SpriteCatalog catalog;
  Dressed novice(kPlayerAnimations);
  const StrikePlay play = StrikePlayOf(catalog, novice.outfit, Player(kNovice), Ordinary());
  CHECK(play.state == std::optional<std::int8_t>{kStrikeAttackState});
  CHECK(play.plays_motion);
  CHECK(play.attack);
  CHECK(play.animation == kAnimationAttack2);
  CHECK(play.factor == Approx(1.0F));
  CHECK(play.hit_frame == Approx(6.0F));
  CHECK(play.lands_at_ms == Approx(6.0F * kOrdinaryIntervalMs));
  CHECK(play.duration_ms == Approx(kFramesPerAction * kOrdinaryIntervalMs));
}

TEST_CASE("Coup : l'attaque d'une créature porte à l'image du marqueur de son .act",
          "[render][strike]") {
  // 🔴 LA SIXIÈME IMAGE EST UNE VALEUR DE JOUEUR. Une créature frappe à la
  // première image dont le son est « atk » — ici la deuxième —, et sans
  // marqueur à l'avant-dernière.
  const SpriteCatalog catalog;
  Dressed poring(kCreatureAnimations, {{kCreatureAnimationAttack, kCreatureStrikeFrame}});
  const StrikePlay marked = StrikePlayOf(catalog, poring.outfit, Creature(kPoring), Ordinary());
  CHECK(marked.attack);
  CHECK(marked.animation == kCreatureAnimationAttack);
  CHECK(marked.hit_frame == Approx(static_cast<float>(kCreatureStrikeFrame)));
  CHECK(marked.lands_at_ms ==
        Approx(static_cast<float>(kCreatureStrikeFrame) * kOrdinaryIntervalMs));

  Dressed fabre(kCreatureAnimations);
  const StrikePlay unmarked = StrikePlayOf(catalog, fabre.outfit, Creature(kFabre), Ordinary());
  CHECK(unmarked.hit_frame == Approx(static_cast<float>(kFramesPerAction - 2)));
  CHECK(unmarked.lands_at_ms ==
        Approx(static_cast<float>(kFramesPerAction - 2) * kOrdinaryIntervalMs));

  // Le rattrapage par espèce s'ajoute au coup, hors de l'image de frappe.
  const StrikePlay skeleton =
      StrikePlayOf(catalog, poring.outfit, Creature(kArcherSkeleton), Ordinary());
  CHECK(skeleton.hit_frame == Approx(static_cast<float>(kCreatureStrikeFrame)));
  CHECK(skeleton.lands_at_ms ==
        Approx(static_cast<float>(kCreatureStrikeFrame) * kOrdinaryIntervalMs + 192.0F));
  const StrikePlay injustice =
      StrikePlayOf(catalog, poring.outfit, Creature(kInjustice), Ordinary());
  CHECK(injustice.lands_at_ms ==
        Approx(static_cast<float>(kCreatureStrikeFrame) * kOrdinaryIntervalMs + 912.0F));
}

TEST_CASE("Coup : le coup d'une compétence d'état sept joue le rang d'emploi et l'image six",
          "[render][strike]") {
  // 🔴 COLD BOLT NE FRAPPE PAS : son coup pose l'état d'emploi, une passe du
  // rang d'incantation, et l'image de frappe SIX de ce rang — dont la cadence
  // est la sienne, et non celle de l'attaque.
  const SpriteCatalog catalog;
  Dressed novice(kPlayerAnimations);
  const StrikePlay bolt =
      StrikePlayOf(catalog, novice.outfit, Player(kNovice), Skill(kColdBolt, kMultiHitBlow));
  CHECK(bolt.state == std::optional<std::int8_t>{kStrikeUseState});
  CHECK(bolt.plays_motion);
  CHECK_FALSE(bolt.attack);
  CHECK(bolt.animation == kAnimationCast);
  CHECK(bolt.hit_frame == Approx(kUseStateHitFrame));
  CHECK(bolt.lands_at_ms == Approx(kUseStateHitFrame * kCastIntervalMs));
  CHECK(bolt.duration_ms == Approx(kFramesPerAction * kCastIntervalMs));

  // Bash, lui, frappe en état deux : l'attaque, à l'image de la table.
  const StrikePlay bash =
      StrikePlayOf(catalog, novice.outfit, Player(kNovice), Skill(kBash, kSingleBlow));
  CHECK(bash.state == std::optional<std::int8_t>{kStrikeAttackState});
  CHECK(bash.attack);
  CHECK(bash.animation == kAnimationAttack2);
  CHECK(bash.lands_at_ms == Approx(6.0F * kOrdinaryIntervalMs));

  // Assis, l'emploi ne touche à rien, et le coup touche aussitôt.
  StrikeFacts seated = Skill(kColdBolt, kMultiHitBlow);
  seated.keeps_posture = true;
  const StrikePlay sitting = StrikePlayOf(catalog, novice.outfit, Player(kNovice), seated);
  CHECK_FALSE(sitting.plays_motion);
  CHECK(sitting.hit_frame == Approx(kResetHitFrame));
  CHECK(sitting.lands_at_ms == Approx(0.0F));

  // Un Doram emploie en frappant : le rang d'attaque, l'image six, sans âme.
  StrikeFacts souls = Skill(kColdBolt, kMultiHitBlow);
  souls.shoots_souls = true;
  const StrikePlay doram = StrikePlayOf(catalog, novice.outfit, Player(kSummoner), souls);
  CHECK(doram.animation == kAnimationAttack2);
  CHECK(doram.hit_frame == Approx(kUseStateHitFrame));
  CHECK(doram.lands_at_ms == Approx(kUseStateHitFrame * kOrdinaryIntervalMs));
}

TEST_CASE("Coup : les compétences qui gardent la motion laissent l'image d'avant",
          "[render][strike]") {
  // Un enchaînement du moine ne pose aucune motion ; le coup touche à l'image
  // de l'attaque qui l'a déclenché.
  const SpriteCatalog catalog;
  Dressed novice(kPlayerAnimations);
  const StrikePlay combo =
      StrikePlayOf(catalog, novice.outfit, Player(kNovice), Skill(kChainCombo, kSingleBlow));
  CHECK_FALSE(combo.state.has_value());
  CHECK_FALSE(combo.plays_motion);
  CHECK_FALSE(combo.attack);
  CHECK(combo.hit_frame == Approx(6.0F));
  CHECK(combo.lands_at_ms == Approx(6.0F * kOrdinaryIntervalMs));

  // La liste : treize en dur, une pour la sorte multiple seulement, douze du
  // registre des actions.
  CHECK_FALSE(SkillHitMotionState(kChainCombo, kSingleBlow).has_value());
  CHECK_FALSE(SkillHitMotionState(2317, kSingleBlow).has_value());
  CHECK_FALSE(SkillHitMotionState(kRegistryException, kSingleBlow).has_value());
  CHECK_FALSE(SkillHitMotionState(kMultiHitOnlyException, kMultiHitBlow).has_value());
  CHECK(SkillHitMotionState(kMultiHitOnlyException, kSingleBlow) ==
        std::optional<std::int8_t>{kStrikeAttackState});
  CHECK(SkillHitMotionState(kBash, kSingleBlow) == std::optional<std::int8_t>{kStrikeAttackState});
  CHECK(SkillHitMotionState(kColdBolt, kMultiHitBlow) ==
        std::optional<std::int8_t>{kStrikeUseState});
}

TEST_CASE("Coup : l'état zéro ramène au repos, et le coup touche aussitôt", "[render][strike]") {
  const SpriteCatalog catalog;
  Dressed novice(kPlayerAnimations);
  const StrikePlay rest =
      StrikePlayOf(catalog, novice.outfit, Player(kNovice), Skill(kCallSpirits, kSingleBlow));
  CHECK(rest.state == std::optional<std::int8_t>{kStrikeRestState});
  CHECK_FALSE(rest.plays_motion);
  CHECK(rest.hit_frame == Approx(kResetHitFrame));
  CHECK(rest.lands_at_ms == Approx(0.0F));
}

TEST_CASE("Coup : la cadence du coup suit la durée annoncée, sauf pour quatre compétences",
          "[render][strike]") {
  const SpriteCatalog catalog;
  Dressed novice(kPlayerAnimations);
  const StrikePlay quick = StrikePlayOf(catalog, novice.outfit, Player(kNovice),
                                        Skill(kBash, kSingleBlow, kHalfAttackMotionMs));
  CHECK(quick.factor == Approx(0.5F));
  CHECK(quick.lands_at_ms == Approx(6.0F * kOrdinaryIntervalMs * 0.5F));
  CHECK(quick.duration_ms == Approx(kFramesPerAction * kOrdinaryIntervalMs * 0.5F));
  const StrikePlay kept = StrikePlayOf(catalog, novice.outfit, Player(kNovice),
                                       Skill(kSpiralPierceLike, kSingleBlow, kHalfAttackMotionMs));
  CHECK(kept.factor == Approx(1.0F));
  CHECK_FALSE(SkillHitSetsMotionFactor(kSpiralPierceLike));
  CHECK_FALSE(SkillHitSetsMotionFactor(6004));
  CHECK(SkillHitSetsMotionFactor(kBash));
}

TEST_CASE("Coup : l'âme d'un Doram recule l'image de frappe de huit pas", "[render][strike]") {
  const SpriteCatalog catalog;
  Dressed novice(kPlayerAnimations);
  StrikeFacts souls = Ordinary();
  souls.shoots_souls = true;
  const StrikePlay with_soul = StrikePlayOf(catalog, novice.outfit, Player(kSummoner), souls);
  CHECK(with_soul.hit_frame == Approx(6.0F + 192.0F / kOrdinaryIntervalMs));
  CHECK(with_soul.lands_at_ms == Approx(6.0F * kOrdinaryIntervalMs + 192.0F));
  const StrikePlay without = StrikePlayOf(catalog, novice.outfit, Player(kSummoner), Ordinary());
  CHECK(without.hit_frame == Approx(6.0F));
  CHECK(without.lands_at_ms == Approx(6.0F * kOrdinaryIntervalMs));
}

TEST_CASE("Coup : une créature n'a que l'attaque, un homoncule son rang d'emploi",
          "[render][strike]") {
  const SpriteCatalog catalog;
  Dressed poring(kCreatureAnimations, {{kCreatureAnimationAttack, kCreatureStrikeFrame},
                                       {kHomunculusAnimationAttack2, kHomunculusStrikeFrame}});
  // Cold Bolt lancé par un Poring : rien ne bouge, le coup touche aussitôt.
  const StrikePlay bolt =
      StrikePlayOf(catalog, poring.outfit, Creature(kPoring), Skill(kColdBolt, kMultiHitBlow));
  CHECK(bolt.state == std::optional<std::int8_t>{kStrikeUseState});
  CHECK_FALSE(bolt.plays_motion);
  CHECK(bolt.hit_frame == Approx(kResetHitFrame));
  CHECK(bolt.lands_at_ms == Approx(0.0F));
  // Double Strafe par un Archer Squelette : l'attaque, l'image du marqueur, et
  // le rattrapage du coup d'une compétence — pas celui de l'attaque ordinaire.
  const StrikePlay strafe = StrikePlayOf(catalog, poring.outfit, Creature(kArcherSkeleton),
                                         Skill(kDoubleStrafe, kMultiHitBlow));
  CHECK(strafe.attack);
  CHECK(strafe.animation == kCreatureAnimationAttack);
  CHECK(strafe.lands_at_ms ==
        Approx(static_cast<float>(kCreatureStrikeFrame) * kOrdinaryIntervalMs + 192.0F));
  const StrikePlay injustice = StrikePlayOf(catalog, poring.outfit, Creature(kInjustice),
                                            Skill(kDoubleStrafe, kMultiHitBlow));
  CHECK(injustice.lands_at_ms ==
        Approx(static_cast<float>(kCreatureStrikeFrame) * kOrdinaryIntervalMs));
  // L'homoncule : le rang cinq à l'emploi, et son second rang d'attaque — avec
  // son propre marqueur — quand le dixième est pair.
  StrikeFacts familiar = Skill(kColdBolt, kMultiHitBlow);
  familiar.skill_led_familiar = true;
  const StrikePlay lif_bolt = StrikePlayOf(catalog, poring.outfit, Creature(kLif), familiar);
  CHECK(lif_bolt.plays_motion);
  CHECK(lif_bolt.animation == kHomunculusAnimationSkill);
  CHECK(lif_bolt.lands_at_ms == Approx(0.0F));
  StrikeFacts even = Ordinary();
  even.chance_tenth = 2;
  const StrikePlay lif_attack = StrikePlayOf(catalog, poring.outfit, Creature(kLif), even);
  CHECK(lif_attack.animation == kHomunculusAnimationAttack2);
  CHECK(lif_attack.hit_frame == Approx(static_cast<float>(kHomunculusStrikeFrame)));
}

TEST_CASE("Coup : le plancher du joueur allonge le geste d'un sort, jamais une attaque",
          "[render][strike]") {
  // Un geste de sort qu'une forte ASPD fait passer en 150 ms, sous un plancher
  // de 450 : trois fois plus lent, et il dure 450.
  StrikePlay spell;
  spell.attack = false;
  spell.factor = 0.5F;
  spell.duration_ms = 150.0F;
  spell.lands_at_ms = 60.0F;
  ro::render::FloorSkillGesture(spell, 450.0F);
  CHECK(spell.factor == Approx(1.5F));
  CHECK(spell.duration_ms == Approx(450.0F));
  // L'instant où le coup porte ne bouge pas.
  CHECK(spell.lands_at_ms == Approx(60.0F));

  // Déjà assez long, ou sans plancher : rien ne change.
  StrikePlay slow = spell;
  ro::render::FloorSkillGesture(slow, 300.0F);
  CHECK(slow.duration_ms == Approx(450.0F));
  StrikePlay untouched;
  untouched.duration_ms = 150.0F;
  ro::render::FloorSkillGesture(untouched, 0.0F);
  CHECK(untouched.duration_ms == Approx(150.0F));

  // Une attaque ordinaire garde la cadence du serveur.
  StrikePlay blow;
  blow.attack = true;
  blow.factor = 0.5F;
  blow.duration_ms = 150.0F;
  ro::render::FloorSkillGesture(blow, 450.0F);
  CHECK(blow.factor == Approx(0.5F));
  CHECK(blow.duration_ms == Approx(150.0F));
}

TEST_CASE("Coup : seuls les états qui posent une action remettent l'image de frappe", "[render]") {
  // 🔴 LE CLIENT REMET L'IMAGE À CHAQUE ACTION POSÉE : 45, 52 à 54, 58 et 59 en
  // posent une, comme les états de 0 à 13 hors 10 ; ceux qui mènent une pose,
  // et ceux qu'il laisse tels quels, gardent l'image d'avant. Nous tranchions
  // à seize.
  for (const std::int8_t state :
       std::to_array<std::int8_t>({0, 2, 7, 9, 11, 12, 13, 45, 52, 53, 54, 58, 59})) {
    CHECK(ro::render::StatePosesAction(state));
  }
  for (const std::int8_t state :
       std::to_array<std::int8_t>({10, 14, 15, 16, 29, 31, 46, 49, 51, 57, 60})) {
    CHECK_FALSE(ro::render::StatePosesAction(state));
  }
}

TEST_CASE("Coup : une éclaboussure ne pose rien et touche à l'image de l'action en cours",
          "[render][strike]") {
  // 🔴 RAID N'A QUE DES ÉCLABOUSSURES : son geste est la pose d'emploi, une
  // attaque à la cadence du fichier, et ses dégâts tombent à l'image de frappe
  // de cette pose — non à celle d'une attaque réglée sur la durée du coup.
  const SpriteCatalog catalog;
  Dressed novice(kPlayerAnimations);
  const StrikePlay use = UsePlayOf(catalog, novice.outfit, Player(kNovice), kStrikeAttackState,
                                   /*chance_tenth=*/0, /*shoots_souls=*/false);
  CHECK(use.plays_motion);
  CHECK(use.factor == Approx(1.0F));
  CHECK(use.hit_frame == Approx(6.0F));

  StrikeFacts raid = Skill(kRaid, kSplashBlow, kHalfAttackMotionMs);
  raid.ongoing_hit_frame = use.hit_frame;
  raid.ongoing_frame_interval_ms = novice.outfit.FrameIntervalMs(use.animation) * use.factor;
  const StrikePlay splash = StrikePlayOf(catalog, novice.outfit, Player(kNovice), raid);
  CHECK_FALSE(splash.plays_motion);
  CHECK_FALSE(splash.attack);
  CHECK(splash.lands_at_ms == Approx(6.0F * kOrdinaryIntervalMs));

  // Sans action en cours, l'image est remise : le coup touche aussitôt.
  const StrikePlay idle =
      StrikePlayOf(catalog, novice.outfit, Player(kNovice), Skill(kRaid, kEnduredSplashBlow));
  CHECK_FALSE(idle.plays_motion);
  CHECK(idle.lands_at_ms == Approx(0.0F));
}

TEST_CASE("Coup : qui garde l'action en éclaboussant, et qui non", "[render][strike]") {
  CHECK(SplashHitKeepsAction(kRaid, kSplashBlow));
  CHECK(SplashHitKeepsAction(kRaid, kEnduredSplashBlow));
  CHECK_FALSE(SplashHitKeepsAction(kRaid, kSingleBlow));
  CHECK_FALSE(SplashHitKeepsAction(kSplashOrdinaryPath, kSplashBlow));
  CHECK_FALSE(SplashHitKeepsAction(0, kSplashBlow));
}
