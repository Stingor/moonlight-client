#include "world/actor_view.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "ro/core/log.h"
#include "ro/render/actor_effect_table.h"
#include "ro/render/actor_scale.h"
#include "ro/render/afterimage_blur.h"
#include "ro/render/ailment_sprites.h"
#include "ro/render/effect_frame.h"
#include "ro/render/map_effect_math.h"
#include "ro/render/monster_skill_motion.h"
#include "ro/render/shown_job.h"
#include "ro/render/skill_damage_timing.h"
#include "ro/render/skill_held_pose.h"
#include "ro/render/skill_use_motion.h"
#include "ro/render/sprite_billboard.h"
#include "ro/render/terrain_mesh.h"
#include "ro/render/vanish_fade.h"
#include "ro/sim/movement.h"
#include "ro/sim/status_effects.h"
#include "ro/sim/world.h"

#include "world/actor_effect_finish.h"
#include "world/actor_tint.h"
#include "world/battle_log_rules.h"
#include "world/corpse_agony.h"
#include "world/struck_place.h"

namespace moonclient {
namespace {

// Le client tire un dixième au sort à chaque coup de taekwon ; l'instant du
// coup en tient lieu, pour qu'un rejeu donne les mêmes images.
constexpr std::uint32_t kChanceTenthMs = 100;
constexpr std::uint32_t kTenths = 10;

// Un alpha qui ne cache rien.
constexpr int kOpaqueAlpha = 255;

// Les reculs de désignation, en unités du monde — les marges que le client
// ajoute à la profondeur des rectangles cliquables, par genre.
constexpr float kNonPlayerPickBias = 100.0F;
constexpr float kMonsterPickBias = 200.0F;
// UN FAMILIER CÈDE LE PIXEL À UN MONSTRE, et le prend encore à son maître : qui
// chasse vise le monstre derrière son pet, et qui veut son pet le trouve
// par-dessus lui-même. L'écart de cent avec le monstre dépasse ce qui sépare en
// profondeur deux acteurs voisins à l'écran.
constexpr float kCompanionPickBias = 300.0F;
constexpr float kPlayerPickBias = 400.0F;
// UN MONSTRE QUI AGONISE RECULE AU-DELÀ DE TOUT ce que la caméra peut voir :
// il ne prend le pixel que si personne d'autre ne le tient. Un monstre vivant
// posé sur la case d'un mort reste donc désignable, quel que soit le plus
// proche de l'œil.
constexpr float kCorpsePickBias = 1.0e6F;

// L'ALPHA D'UNE PLANTE D'HALLUCINATION, que son propre gestionnaire de messages
// pose : cent quatre-vingts, et non la pleine opacité.
constexpr std::uint8_t kHallucinationAlpha = 180;

// 🔴 UN VOILE PASSE D'UN CALQUE DEVANT LE CORPS QU'IL COUVRE. Il en reprend les
// quads, donc leur profondeur exacte, et le corps vient d'écrire sa silhouette :
// à profondeur égale, l'épreuve le rejetait partout où il devait se voir.
constexpr float kVeilDepthBias = ro::render::kLayerDepthStep;

// L'ARME DOUBLÉE PARAÎT UNE IMAGE SUR DEUX.
constexpr std::uint32_t kWeaponEchoPeriodFrames = 2;

constexpr std::uint32_t kJitterActorStride = 2654435761U;
constexpr std::uint32_t kJitterTimeStride = 40503U;

}  // namespace

namespace {

[[nodiscard]] std::uint8_t ChanceTenthAt(std::uint32_t at_ms) {
  return static_cast<std::uint8_t>((at_ms / kChanceTenthMs) % kTenths);
}

ro::render::StrikeFacts StrikeFactsOf(const ro::sim::Actor& attacker,
                                      const ro::sim::Strike& strike) {
  ro::render::StrikeFacts facts;
  facts.skill_id = strike.skill_id;
  facts.damage_type = strike.type;
  facts.attack_motion_ms = strike.attack_motion_ms;
  facts.chance_tenth = ChanceTenthAt(strike.started_ms);
  facts.shoots_souls = attacker.status_effects.contains(ro::sim::kEfstSoulAttack);
  // Assis ou terré, l'emploi ordinaire posé par le coup ne touche à rien.
  facts.keeps_posture = attacker.posture == ro::sim::Posture::kSitting ||
                        attacker.status_effects.contains(ro::sim::kEfstSuHide);
  facts.skill_led_familiar = attacker.familiar == ro::sim::Familiar::kHomunculus ||
                             attacker.familiar == ro::sim::Familiar::kElemental;
  return facts;
}

// LE DERNIER COUP QUI A POSÉ UNE ACTION à cet auteur, au plus tard à cet
// instant : un coup d'éclaboussure n'en pose pas, et ne le cache donc pas.
const ro::sim::Strike* LastPosingStrike(const ro::sim::World& world, std::uint32_t attacker_id,
                                        std::uint32_t at_ms) {
  const std::span<const ro::sim::RecordedStrike> journal = world.strikes().Since(0);
  for (auto it = journal.rbegin(); it != journal.rend(); ++it) {
    const ro::sim::Strike& strike = it->strike;
    if (strike.attacker == attacker_id && strike.attacker != strike.target &&
        strike.started_ms <= at_ms &&
        !ro::render::SplashHitKeepsAction(strike.skill_id, strike.type)) {
      return &strike;
    }
  }
  return nullptr;
}

// L'ACTION QUE L'AUTEUR JOUE ENCORE à l'instant d'un coup d'éclaboussure : la
// passe du dernier coup qui en a posé une, ou la pose de son dernier emploi —
// la plus récente des deux. Rien quand la passe est finie : le retour au repos
// a remis l'image de frappe.
void FillOngoingAction(ro::render::ActorWardrobe& wardrobe, const ro::sim::World& world,
                       const ro::sim::Actor& attacker, const ro::render::ActorOutfit& outfit,
                       const ro::render::ActorLook& look, const ro::sim::Strike& strike,
                       ro::render::StrikeFacts& facts) {
  std::optional<std::uint32_t> posed_ms;
  const auto take = [&](std::uint32_t since_ms, const ro::render::StrikePlay& play) {
    if (!play.plays_motion ||
        static_cast<float>(strike.started_ms - since_ms) >= play.duration_ms) {
      return;
    }
    posed_ms = since_ms;
    facts.ongoing_hit_frame = play.hit_frame;
    facts.ongoing_frame_interval_ms = outfit.FrameIntervalMs(play.animation) * play.factor;
  };
  if (const ro::sim::Strike* posing = LastPosingStrike(world, attacker.id, strike.started_ms)) {
    take(posing->started_ms, ro::render::StrikePlayOf(wardrobe.catalog(), outfit, look,
                                                      StrikeFactsOf(attacker, *posing)));
  }
  // ⚠ UNE CRÉATURE QUE SA TABLE FAIT JOUER prend son geste par un autre
  // chemin, qui remet l'image de frappe.
  if (attacker.skill_use.has_value() && attacker.skill_use->started_ms <= strike.started_ms &&
      (!posed_ms.has_value() || attacker.skill_use->started_ms >= *posed_ms) &&
      !ro::render::SkillUsePosedByTable(attacker.look.job, attacker.skill_use->skill_id)) {
    const ro::sim::SkillUse& use = *attacker.skill_use;
    facts.ongoing_hit_frame = ro::render::kResetHitFrame;
    take(use.started_ms,
         ro::render::UsePlayOf(wardrobe.catalog(), outfit, look,
                               ro::render::SkillUseStateFor(use.skill_id),
                               ChanceTenthAt(use.started_ms),
                               attacker.status_effects.contains(ro::sim::kEfstSoulAttack)));
  }
}

}  // namespace

std::optional<glm::vec2> HeadShiftOf(ro::render::ActorWardrobe& wardrobe,
                                     const ro::render::ActorOutfit& outfit,
                                     const ro::render::ActorLook& look,
                                     const ro::render::ComposedActor& composed) {
  if (!composed.head_place.has_value()) {
    return std::nullopt;
  }
  ro::render::ActorLook on_foot = look;
  on_foot.job = ro::render::FootJobOf(look.job);
  on_foot.cash_mount = false;
  const ro::render::ActorOutfit* foot = wardrobe.OutfitFor(on_foot);
  const std::optional<glm::vec2> rest = outfit.HeadRestingPlace(foot != nullptr ? *foot : outfit);
  if (!rest.has_value()) {
    return std::nullopt;
  }
  return *composed.head_place - *rest;
}

const ro::sim::Strike* PosingStrikeOf(const ro::sim::World& world, const ro::sim::Actor& actor) {
  if (!actor.attack.has_value()) {
    return nullptr;
  }
  if (!ro::render::SplashHitKeepsAction(actor.attack->skill_id, actor.attack->type)) {
    return &*actor.attack;
  }
  return LastPosingStrike(world, actor.id, actor.attack->started_ms);
}

std::optional<ro::render::StrikePlay> StrikePlayFor(ro::render::ActorWardrobe& wardrobe,
                                                    const ro::sim::World& world,
                                                    const ro::sim::Actor& attacker,
                                                    const ro::sim::Strike& strike) {
  const ro::render::ActorLook look = LookOf(attacker);
  ro::render::ActorOutfit* outfit = wardrobe.OutfitFor(look);
  if (outfit == nullptr) {
    return std::nullopt;
  }
  ro::render::StrikeFacts facts = StrikeFactsOf(attacker, strike);
  if (ro::render::SplashHitKeepsAction(strike.skill_id, strike.type)) {
    FillOngoingAction(wardrobe, world, attacker, *outfit, look, strike, facts);
  }
  ro::render::StrikePlay play = ro::render::StrikePlayOf(wardrobe.catalog(), *outfit, look, facts);
  // UN COUP ORDINAIRE QUE L'ON SE PORTE À SOI-MÊME ne pose pas l'attaque : le
  // client ne change la motion que si la cible est un autre.
  if (strike.skill_id == 0 && strike.attacker == strike.target) {
    play.plays_motion = false;
    play.attack = false;
  }
  return play;
}

std::uint32_t AgonyStartMs(ro::render::ActorWardrobe* wardrobe, const ro::sim::World& world,
                           const ro::sim::Actor& actor) {
  std::uint32_t start_ms = actor.posture_since_ms;
  if (actor.posture != ro::sim::Posture::kDead || ComposedAsPlayer(actor) || wardrobe == nullptr) {
    return start_ms;
  }
  // Les coups reçus AVANT l'annonce : ce sont eux que la file tient encore.
  const auto wait_for = [&](const ro::sim::Strike& strike) {
    if (strike.target != actor.id || strike.started_ms > actor.posture_since_ms) {
      return;
    }
    const std::uint32_t first_hit_ms =
        strike.started_ms + static_cast<std::uint32_t>(HitLandsAtMs(*wardrobe, world, strike));
    start_ms = AgonyStartAfter(
        start_ms, actor.id,
        BattleMessagesOf(strike, SidesOfStrike(world, strike), first_hit_ms, false, false));
  };
  for (const ro::sim::RecordedStrike& recorded : world.strikes().Since(0)) {
    wait_for(recorded.strike);
  }
  if (actor.hit.has_value()) {
    wait_for(*actor.hit);
  }
  return start_ms;
}

// LA CLASSE QU'UN EFFET FAIT PORTER À L'ACTEUR à cet instant, ou zéro.
//
// Cinq effets posent une PLANTE là où il se tient — l'hallucination. Le client
// en construit un acteur à part, de la classe que sa table donne, et le
// range dans sa liste de scène à la place où l'autre se tenait.
//
// ⚠ SA VIE N'EST PAS MESURÉE. Le nœud du client vit tant que sa scène le garde,
// et rien dans son constructeur ne dit combien de temps. La plante vit donc ici
// tant que le MONDE garde le fait, et c'est dit plutôt que remplacé par une
// durée inventée.
[[nodiscard]] std::uint16_t AppearanceJobAt(
    std::span<const ro::render::ActorEffectPlay> played_effects) {
  for (const ro::render::ActorEffectPlay& play : played_effects) {
    const ro::render::ActorEffectChange* change = ro::render::ActorEffectChangeFor(play.effect_id);
    if (change != nullptr && change->appearance_job != 0) {
      return change->appearance_job;
    }
  }
  return 0;
}

float PickBiasOf(const ro::sim::Actor& actor) {
  // ⚠ L'AGONIE D'UN MONSTRE SEUL — son cadavre, lui, ne se désigne plus : un
  // joueur mort doit rester désignable, on le vise pour le relever.
  if (actor.kind == ro::sim::ActorKind::kMonster && actor.posture == ro::sim::Posture::kDead) {
    return kCorpsePickBias;
  }
  if (actor.familiar != ro::sim::Familiar::kNone) {
    return kCompanionPickBias;
  }
  switch (actor.kind) {
    case ro::sim::ActorKind::kPlayer:
      return kPlayerPickBias;
    case ro::sim::ActorKind::kMonster:
      return kMonsterPickBias;
    case ro::sim::ActorKind::kNonPlayer:
      return kNonPlayerPickBias;
    default:
      return 0.0F;
  }
}

glm::vec2 BodyJitter(std::uint32_t actor_id, std::uint32_t now_ms, float amplitude_pixels) {
  if (!(amplitude_pixels > 0.0F)) {
    return {0.0F, 0.0F};
  }
  // Le client tire un entier dans zéro..dix et lui retranche cinq : le
  // tremblement est donc un nombre ENTIER de pixels, de part et d'autre.
  const auto amplitude = static_cast<std::uint32_t>(amplitude_pixels);
  const std::uint32_t value_count = 2 * amplitude + 1;
  // Un mélange bon marché, dont on ne demande que de ne pas se répéter d'un
  // acteur à l'autre ni d'une image à la suivante.
  std::uint32_t seed = actor_id * kJitterActorStride + now_ms * kJitterTimeStride;
  const auto draw = [&]() {
    seed = ScrambleSeed(seed);
    return static_cast<float>(static_cast<std::int32_t>(seed % value_count) -
                              static_cast<std::int32_t>(amplitude));
  };
  const float x = draw();
  return {x, draw()};
}

float HitLandsAtMs(ro::render::ActorWardrobe& wardrobe, const ro::sim::World& world,
                   const ro::sim::Strike& strike) {
  const ro::sim::Actor* attacker = world.Find(strike.attacker);
  if (attacker == nullptr) {
    return 0.0F;
  }
  const std::optional<ro::render::StrikePlay> play =
      StrikePlayFor(wardrobe, world, *attacker, strike);
  return (play.has_value() ? play->lands_at_ms : 0.0F) +
         static_cast<float>(ro::render::SkillDamageDelayMs(strike.skill_id));
}

// La hauteur d'un corps composé, en pixels de sprite.
//
// ⚠ ELLE SERT À CONVERTIR UN ÉLARGISSEMENT EN ÉCHELLE. Le client pousse les
// quatre bords d'un halo vers l'extérieur d'un nombre de PIXELS ; nous n'avons
// que l'échelle, et sans la hauteur du corps le même halo serait énorme sur un
// poring et invisible sur un dragon.
float BodyHeightPixels(const ro::render::ComposedActor& composed) {
  float top = 0.0F;
  float bottom = 0.0F;
  bool vu = false;
  for (const ro::render::SpriteQuad& quad : composed.quads) {
    for (const glm::vec2& corner : quad.corners) {
      top = vu ? std::min(top, corner.y) : corner.y;
      bottom = vu ? std::max(bottom, corner.y) : corner.y;
      vu = true;
    }
  }
  return bottom - top;
}

// LE MESSAGE QUI GÈRE LA LISTE D'EFFETS d'un acteur.
//
// Le client le reçoit et, selon un sous-cas tiré du message, AJOUTE ou RETIRE
// un effet de la liste. Celui-ci
// retire : `EF_NPC_STOP2_DEL` y passe le numéro de `EF_NPC_STOP2`, et
// l'énumération du serveur nomme les deux — le `_DEL` et son jumeau.
constexpr std::uint16_t kEffectListCommand = 0x6D;

void ApplyEffectCancellations(ro::sim::World& world) {
  // Ce qu'on retire se collecte d'abord : on ne retire pas ce qu'on parcourt.
  std::vector<std::pair<std::uint32_t, std::uint32_t>> cancelled;
  for (const auto& [id, actor] : world.actors()) {
    for (const ro::sim::PlayedEffect& effect : actor.effects) {
      const ro::render::ActorEffectChange* change = ro::render::ActorEffectChangeFor(effect.id);
      if (change != nullptr && change->command == kEffectListCommand &&
          change->command_value != 0) {
        cancelled.emplace_back(id, static_cast<std::uint32_t>(change->command_value));
        cancelled.emplace_back(id, effect.id);
      }
    }
  }
  for (const auto& [id, cancelled_effect] : cancelled) {
    world.RemoveEffect(id, cancelled_effect);
  }
}

SceneCensus ComposeScene(ro::render::ActorWardrobe* wardrobe, const ro::sim::World& world,
                         ro::sim::WalkPlanner& planner, const MapStage& stage,
                         float camera_yaw_degrees, std::uint32_t now_ms, float looping_elapsed_ms,
                         ActorStyles* styles, ComposedScene& scene, ActorGhosts& ghosts,
                         ItemDrops& drops, FrozenPoses& frozen_poses,
                         const ActorLighting& lighting) {
  // Chacun porte SES sprites. Un acteur sans corps — pièce absente, ou catalogue
  // illisible — ne se dessine pas ; un objet au sol ou une zone
  // de compétence n'est pas un personnage et ne passe pas par là. Les POSITIONS
  // sont celles que le serveur a annoncées.
  SceneCensus census;
  // L'arme doublée ne paraît qu'une image sur deux : voir `ComposedScene`.
  ++scene.composed_frames;
  // Et une image qui ne doit aucun tic laisse les teintes remonter.
  scene.tint_clock.NoteFrameAt(now_ms);
  const ro::render::TintIdleFramesSince idle_frames_since = [&scene](std::uint32_t ticks_since) {
    return scene.tint_clock.IdleFramesSince(ticks_since);
  };
  const bool weapon_echo_frame = scene.composed_frames % kWeaponEchoPeriodFrames == 0;
  // Les effets de l'acteur en cours, relus pour chacun dans le même tampon.
  std::vector<ro::render::ActorEffectPlay> played_effects;
  census.actors = world.actors().size();
  frozen_poses.Retain(world);
  // UN ACTEUR, sous l'identifiant que la scène lui donne : le sien, ou zéro pour
  // une copie qui s'efface et que plus rien ne désigne — ni nom, ni jauge, ni
  // clic. `leaving_alpha` multiplie son opacité.
  const auto compose = [&](const ro::sim::Actor& actor, std::uint32_t scene_id,
                           std::uint8_t leaving_alpha) {
    const std::uint32_t id = actor.id;
    // Qui se cache ne se voit pas, sauf à garder sa place : voir `ConcealmentOf`.
    const Concealment concealment = ConcealmentOf(actor, world);
    if (concealment != Concealment::kVisible && id != world.player_id()) {
      ++census.hidden;
      return;
    }
    if (!HasSprite(actor.kind) || IsUnseenClass(actor)) {
      ++census.without_sprite;
      return;
    }
    // Un warp n'a pas de corps : voir `KeepWarp`, qui le garde désignable.
    if (IsWarp(actor)) {
      const ro::sim::FractionalCell cell =
          ro::sim::InterpolatedCell(actor, planner.PathFor(actor), now_ms);
      const std::array<float, 3> ground = GroundPositionAt(stage, cell.x, cell.y);
      KeepWarp(scene_id, glm::vec3(ground[0], ground[1], ground[2]), scene.drawn, scene.ids);
      return;
    }
    ro::render::ActorLook look = LookOf(actor);
    // LES COULEURS QUE CE JOUEUR A COMPOSÉES, s'il en a. ⚠ SUR L'APPARENCE
    // SANS STYLE : c'est elle qui décide du corps, et c'est le corps qui
    // décide de la variante. `Dress` y pose l'identifiant ET les trois
    // numéros que la recette impose.
    if (styles != nullptr && wardrobe != nullptr) {
      styles->Dress(id, look, *wardrobe, world.styles());
    }
    ro::render::ActorOutfit* outfit = wardrobe != nullptr ? wardrobe->OutfitFor(look) : nullptr;
    if (outfit == nullptr) {
      ++census.without_body;
      return;
    }
    const ro::sim::WalkPath& path = planner.PathFor(actor);
    const ro::sim::FractionalCell cell = ro::sim::InterpolatedCell(actor, path, now_ms);
    // CE QU'UN OBJET AU SOL FAIT EN ARRIVANT : il tombe du ciel, ou il jaillit
    // de qui l'a lâché. Le module POSSÈDE sa position tant qu'il vole ; pour
    // tout autre acteur il rend la case telle quelle.
    const ItemDropPose drop =
        drops.PoseAt(world, stage, actor, GroundPositionAt(stage, cell.x, cell.y), now_ms);
    const std::array<float, 3>& ground = drop.ground;
    // CE QUE LES EFFETS FONT DU CORPS. La liste vient AVANT la direction : un
    // effet peut faire pivoter l'acteur sur lui-même, et l'image à composer en
    // dépend.
    PlayedEffectsOf(actor, now_ms, played_effects);
    PlayedMotion played = MotionAt(wardrobe, world, actor, *outfit, path, now_ms,
                                   looping_elapsed_ms, played_effects, scene.readable_motion);
    // Le geste du joueur se note AVANT qu'il ne se cache : dissimulé, il garde
    // l'état de ce qu'il fait, et une marche retenue le reste avec lui.
    if (id == world.player_id()) {
      scene.player_gesture = PlayerGesture{played.motion, played.action_state};
    }
    // Le joueur dissimulé garde sa place, sans corps : voir `KeepConcealedSelf`.
    if (concealment != Concealment::kVisible) {
      KeepConcealedSelf(concealment, scene_id, outfit->Shadow(),
                        glm::vec3(ground[0], ground[1], ground[2]), scene.drawn, scene.ids);
      ++census.hidden;
      return;
    }
    // ⚠ FAIRE TOURNER L'ACTEUR REVIENT À TOURNER LA CAMÉRA EN SENS INVERSE, et
    // c'est exact plutôt que commode : dans le calcul de la direction, la face
    // de l'acteur entre en NÉGATIF et l'angle de caméra en positif. Retrancher
    // l'un fait donc précisément ce qu'ajouter à l'autre ferait, et le huitième
    // de tour se prend sur la somme — comme chez le client, dont l'angle de face
    // est continu.
    const float yaw_degrees =
        camera_yaw_degrees - ro::render::SpinDegreesAfterEffects(played_effects);
    // 🔴 UNE VISÉE SE DESSINE PAR SON ANGLE EXACT, et non par sa boussole : le
    // client découpe cet angle en huit, et pas de la même façon selon le rang
    // joué — un frappeur montre une face pendant son coup et parfois une autre
    // au repos. Voir `SpriteDirectionOfDegrees`. Le pas en cours, lui, garde
    // son regard : la visée n'est montrée qu'une fois la marche finie.
    const bool aim_shown = actor.facing_origin == ro::sim::FacingOrigin::kAim &&
                           (!actor.movement.has_value() || actor.turned_since_walk_began) &&
                           !ro::sim::IsWalking(actor, path, now_ms) &&
                           (actor.aimed_dx_cells != 0.0F || actor.aimed_dy_cells != 0.0F);
    // Une image qui impose sa direction la garde, quels que soient le regard et
    // la caméra : voir `PlayedMotion::held_direction`.
    std::size_t facing =
        played.held_direction.has_value() ? *played.held_direction
        : aim_shown
            ? ro::render::SpriteDirectionOfDegrees(
                  ro::sim::ClientAimDegrees(actor.aimed_dx_cells, actor.aimed_dy_cells),
                  yaw_degrees,
                  ro::render::ScreenDirectionRounds(look.job, look.player, played.animation))
            : ro::render::SpriteDirection(ro::sim::FacingAt(actor, path, now_ms), yaw_degrees);
    // UNE POSE FIGÉE garde la direction de l'instant où elle s'est figée, et
    // parfois l'image d'alors : voir `FrozenPoses`.
    if (played.frozen && !played.held_direction.has_value()) {
      const FrozenPoses::Shown held =
          frozen_poses.Hold(id, played.frozen_since_ms, played.keeps_shown_image,
                            {played.animation, facing, played.held_frame.value_or(0)});
      facing = held.direction;
      played.animation = held.animation;
      played.held_frame = held.frame;
    }
    // ⚠ AVANT TOUT CE QUI SE PEINT, halos et voiles compris : l'agonie finie,
    // l'acteur s'en va et son CADAVRE prend sa place, dessiné à part dès cette
    // image. Une copie ne repasse pas par là.
    if (scene_id != 0) {
      if (const std::optional<Agony> agony =
              AgonyOf(actor, *outfit, facing, AgonyStartMs(wardrobe, world, actor));
          agony.has_value() && now_ms >= agony->EndMs()) {
        scene.vanished.push_back(id);
        if (std::ranges::none_of(scene.corpses, [id](const ro::sim::FadingActor& corpse) {
              return corpse.actor.id == id;
            })) {
          scene.corpses.push_back(ro::sim::FadingActor{actor, agony->EndMs()});
        }
        return;
      }
    }
    const ro::render::ComposedActor& composed = outfit->Composed(
        played.animation, facing, played.elapsed_ms, played.loops, played.held_frame);
    frozen_poses.NoteShown(id, {played.animation, facing, composed.frame_index});
    if (const std::optional<glm::vec2> head_shift = HeadShiftOf(*wardrobe, *outfit, look, composed);
        head_shift.has_value() && scene_id != 0) {
      scene.head_shifts[id] = *head_shift;
    }
    if (composed.quads.empty()) {
      ++census.without_image;
    }
    // La hauteur et l'alpha : le saut monte le corps en l'effaçant, la
    // réception le ramène — deux effets, une paire.
    const ro::render::ActorBodyPose pose = ro::render::BodyPoseAfterEffects(played_effects);
    if (pose.hidden) {
      ++census.hidden;
      return;
    }
    // LA LÉVITATION DU VOL DE L'UNION : le corps et les copies qu'un effet pose
    // autour de lui montent à l'écran, son ombre et son emprise restent au sol.
    const float hover_pixels =
        scene.union_hovers.PixelsOf(actor, played.motion, pose.red_veiled, composed, now_ms);
    if (hover_pixels != 0.0F && scene_id != 0) {
      scene.hover_pixels[id] = hover_pixels;
    }
    const glm::vec2 hover_offset(0.0F, hover_pixels);
    // LA TRAÎNÉE RÉÉCRIT LA COULEUR DU CORPS à chaque tic, et donne la sienne à
    // ses copies : voir `ro/render/afterimage_blur.h`. ⚠ À SON RANG dans la
    // liste de l'acteur : ce qui teint après elle la recouvre, ce qui teint
    // avant, elle le recouvre.
    const std::optional<ro::render::AfterimageBlur> blur = ghosts.BlurOf(
        id, actor,
        ro::render::MapEffectFrame{stage.scene.gnd.width(), stage.scene.gnd.height()}.ToNative(
            glm::vec3(ground[0], ground[1], ground[2])),
        now_ms);
    std::optional<ro::render::TintEveryTickWrite> blur_write;
    std::array<std::uint8_t, 4> blur_ghost = kPlainTint;
    if (blur.has_value()) {
      blur_write = BlurTintWriteOf(actor, *blur, BlurFlickerSeed(id, blur->age_ticks));
      blur_ghost = ro::render::AfterimageGhostTint(*blur);
    }
    // LA COULEUR DU QUAD DU CORPS : voir `BodyTintOf`, qui se vérifie hors scène.
    BodyTintInputs body_inputs{
        .blur_write = blur_write,
        .idle_frames_since = idle_frames_since,
        .pose_alpha = pose.alpha,
        .vanish_alpha = static_cast<std::uint8_t>(ArrivalAlpha(actor, now_ms, scene) *
                                                  leaving_alpha / kOpaqueAlpha),
        .sparkling = drop.sparkling,
        .cold = lighting.cold,
        .ailment_tints = lighting.ailment_tints};
    if (lighting.ground != nullptr) {
      body_inputs.ground_light = lighting.ground->Under(cell.x, cell.y);
    }
    // Les copies de la traînée prennent la même lumière, sauf sous l'éclat d'un
    // objet au sol, qui la remplace.
    if (body_inputs.ground_light.has_value() && !drop.sparkling) {
      blur_ghost = ro::render::LitTint(blur_ghost, *body_inputs.ground_light, lighting.cold);
    }
    const std::array<std::uint8_t, 4> tint = BodyTintOf(actor, now_ms, body_inputs);
    // ⚠ LE SOULÈVEMENT SE COMPTE EN FRACTION DE LA HAUTEUR DU CORPS, et non en
    // pixels : le client déplace les bords du quad d'une part de leur écart, si
    // bien qu'un grand corps monte plus haut qu'un petit.
    const float body_pixels = BodyHeightPixels(composed);
    // CE QUI LÂCHE UN BUTIN LUI DONNE SA TAILLE ET SA PLACE. Le module du
    // jaillissement ne connaît ni tenue ni chemin planifié : les deux mesures ne
    // peuvent venir que d'ici, et ce sont elles qui font sortir le butin d'un
    // CORPS, à l'endroit où ce corps est VU.
    drops.NoteBody(id, ground, body_pixels);
    const float lifted = pose.lift_fraction * body_pixels;
    const float height =
        ground[1] + (pose.up_pixels + lifted) / ro::render::kSpritePixelsPerWorldUnit;
    const float scale = ro::render::BodyScaleAfterEffects(played_effects);
    const glm::vec3 feet(ground[0], height, ground[2]);
    // LA PLANTE DE L'HALLUCINATION, posée où l'acteur se tient.
    //
    // ⚠ ELLE EST TRANSLUCIDE : le client donne cent quatre-vingts d'alpha à son
    // nœud de plante, et non la pleine opacité.
    if (const std::uint16_t plant_job = AppearanceJobAt(played_effects);
        plant_job != 0 && wardrobe != nullptr) {
      ro::render::ActorLook foliage;
      foliage.job = plant_job;
      if (ro::render::ActorOutfit* grown = wardrobe->OutfitFor(foliage)) {
        const ro::render::ComposedActor& composed_look =
            grown->Composed(ro::render::kAnimationIdle, facing, looping_elapsed_ms);
        scene.front.push_back(ro::render::DrawnActor{
            &composed_look, nullptr, feet, {255, 255, 255, kHallucinationAlpha}});
      }
    }
    // LES HALOS D'ABORD : le client soumet ses copies avant le corps qu'elles
    // entourent, et elles n'ont ni ombre ni boîte de clic à elles.
    std::array<ro::render::ActorBodyCopy, ro::render::kMaxBodyCopies> halos;
    const std::size_t count =
        ro::render::BodyCopiesAfterEffects(played_effects, body_pixels, halos);
    for (std::size_t copy_rank = 0; copy_rank < count; ++copy_rank) {
      // ⚠ UN CONTOUR PASSE DEVANT LE CORPS, comme un voile et du même pas : voir
      // `ActorBodyCopy::in_front`. Les autres copies restent derrière lui.
      const bool in_front = halos[copy_rank].in_front;
      std::vector<ro::render::DrawnActor>& layer = in_front ? scene.front : scene.behind;
      layer.push_back(ro::render::DrawnActor{
          &composed, nullptr, feet, halos[copy_rank].tint, 0.0F, scale * halos[copy_rank].scale,
          glm::vec2(halos[copy_rank].offset_pixels[0], halos[copy_rank].offset_pixels[1]) +
              hover_offset});
      // Un halo se range dans un seau à mélange propre, hors du brouillard.
      layer.back().fogged = false;
      if (in_front) {
        layer.back().depth_bias = kVeilDepthBias;
      }
      CopyLayers(halos[copy_rank], layer.back());
    }
    // LES VOILES ENSUITE, par-dessus le corps : même sprite, même taille, une
    // couleur et une transparence en plus.
    std::array<ro::render::ActorBodyCopy, ro::render::kMaxBodyCopies> veils;
    const std::size_t veil_count = ro::render::BodyVeilsAfterEffects(played_effects, veils);
    // LE CERCLE DES CALQUES : la tête et le reste tournent autour du corps, qui
    // reste en place ; et chaque calque fait la culbute sur lui-même.
    const auto swing_layers = [&pose](ro::render::DrawnActor& drawn) {
      drawn.layer_spin_degrees = pose.layer_spin_degrees;
      for (std::size_t part = 0; part < ro::render::kActorPartCount; ++part) {
        if (part == static_cast<std::size_t>(ro::render::ActorPart::kBody)) {
          continue;
        }
        const std::array<float, 2>& swing =
            part == static_cast<std::size_t>(ro::render::ActorPart::kHead)
                ? pose.swing_pixels
                : pose.layer_swing_pixels;
        drawn.part_offset_pixels[part] = glm::vec2(swing[0], swing[1]);
      }
    };
    for (std::size_t copy_rank = 0; copy_rank < veil_count; ++copy_rank) {
      scene.front.push_back(
          ro::render::DrawnActor{&composed, nullptr, feet, veils[copy_rank].tint, 0.0F, scale});
      scene.front.back().fogged = false;
      scene.front.back().depth_bias = kVeilDepthBias;
      scene.front.back().screen_offset_pixels = hover_offset;
      CopyLayers(veils[copy_rank], scene.front.back());
      swing_layers(scene.front.back());
    }
    // LES RÉMANENCES : une copie du corps, semée derrière lui. Elle vit un tour
    // de l'animation qu'il joue, ce que la tenue seule sait dire.
    ghosts.Notice(id, played_effects, composed, feet, tint,
                  outfit->AnimationDurationMs(played.animation), now_ms);
    if (blur.has_value()) {
      ghosts.NoticeBlur(id, *blur, played.motion, composed, feet, blur_ghost, now_ms);
    }
    ro::render::DrawnActor body{&composed,
                                &outfit->Shadow(),
                                feet,
                                tint,
                                PickBiasOf(actor),
                                scale,
                                BodyJitter(id, now_ms, pose.jitter_pixels) + hover_offset,
                                pose.height_scale};
    swing_layers(body);
    BlendLayers(pose, weapon_echo_frame, body);
    scene.drawn.push_back(body);
    scene.ids.push_back(scene_id);
  };
  for (const auto& [id, actor] : world.actors()) {
    compose(actor, id, static_cast<std::uint8_t>(kOpaqueAlpha));
  }
  // LES ACTEURS SORTIS DE VUE, qui s'effacent encore. Le monde les a oubliés :
  // ils se dessinent sans identifiant, puis s'en vont au bout du fondu.
  std::erase_if(scene.fading, [&scene, now_ms](const ro::sim::FadingActor& fading) {
    return now_ms >= fading.since_ms && now_ms - fading.since_ms >= scene.leave_fade_ms;
  });
  for (const ro::sim::FadingActor& fading : scene.fading) {
    const std::uint32_t since = now_ms > fading.since_ms ? now_ms - fading.since_ms : 0;
    compose(fading.actor, 0, ro::render::LeaveAlphaAt(since, scene.leave_fade_ms));
  }
  // LES CADAVRES, de même, au fondu du cadavre — compté depuis la fin de
  // l'agonie, d'où l'agonie nulle.
  constexpr float kAgonyOver = 0.0F;
  std::erase_if(scene.corpses, [&scene, now_ms](const ro::sim::FadingActor& corpse) {
    return now_ms >= corpse.since_ms &&
           ro::render::VanishIsOver(kAgonyOver, now_ms - corpse.since_ms, scene.corpse_fade_ms);
  });
  for (const ro::sim::FadingActor& corpse : scene.corpses) {
    const std::uint32_t since = now_ms > corpse.since_ms ? now_ms - corpse.since_ms : 0;
    compose(corpse.actor, 0, ro::render::VanishAlphaAt(kAgonyOver, since, scene.corpse_fade_ms));
  }
  scene.union_hovers.ForgetUnseen();
  census.drawn = scene.drawn.size();
  return census;
}

void SettleWorld(ro::sim::World& world, const ComposedScene& previous, std::uint32_t now_ms) {
  for (const std::uint32_t id : previous.vanished) {
    world.Remove(id);
  }
  // Ce qu'un effet ANNULE s'éteint avant qu'on ne compose : sans cela l'image
  // montrerait encore ce que le serveur vient de faire cesser.
  ApplyEffectCancellations(world);
  // Et ce qui a FINI de soi-même s'en va aussi : voir `EffectFinishedAt`.
  static_cast<void>(ForgetFinishedEffects(world, now_ms));
}

std::uint8_t ArrivalAlpha(const ro::sim::Actor& actor, std::uint32_t now_ms,
                          const ComposedScene& scene) {
  if (!actor.appeared_ms.has_value()) {
    return static_cast<std::uint8_t>(kOpaqueAlpha);
  }
  const std::uint32_t since = now_ms > *actor.appeared_ms ? now_ms - *actor.appeared_ms : 0;
  return ro::render::AppearAlphaAt(since, scene.appear_fade_ms);
}

}  // namespace moonclient
