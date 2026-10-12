// CE QU'UN ACTEUR JOUE À UN INSTANT : sa posture, sa marche, les coups qu'il
// porte et qu'il encaisse, ses postures de compétence, les poses menées — dans
// l'ordre où la scène les consulte, et l'image qu'une compétence de case leur
// impose.

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "ro/render/actor_effect_table.h"
#include "ro/render/ailment_sprites.h"
#include "ro/render/sprite_billboard.h"
#include "ro/sim/movement.h"
#include "ro/sim/status_effects.h"
#include "ro/sim/world.h"

#include "world/actor_view.h"

namespace moonclient {
namespace {

// La motion que la posture et le chemin donnent : mort ou assis, en marche, en
// garde ou en attente — selon le dernier geste, voir `RestMotionAt`. Un mort
// dont l'agonie n'a pas commencé garde la motion d'un vivant.
ro::render::Motion RestingMotion(const ro::sim::Actor& actor, const ro::sim::WalkPath& path,
                                 const MotionEvents& events, std::uint32_t agony_start_ms,
                                 std::uint32_t now_ms) {
  switch (actor.posture) {
    case ro::sim::Posture::kDead:
      if (now_ms >= agony_start_ms) {
        return ro::render::Motion::kDie;
      }
      break;
    case ro::sim::Posture::kSitting:
      return ro::render::Motion::kSit;
    case ro::sim::Posture::kStanding:
      break;
  }
  if (ro::sim::IsWalking(actor, path, now_ms)) {
    // QUI GLISSE MARCHE AUSSI, tourné vers où il va : c'est l'image qui change,
    // voir `ro::render::GlideHeldWalkFrame`.
    return ro::render::Motion::kWalk;
  }
  return RestMotionAt(actor, events, now_ms);
}

// `MotionAt`, et l'image qu'une compétence de case impose quand `with_imposed`.
PlayedMotion ComposedMotionAt(ro::render::ActorWardrobe* wardrobe, const ro::sim::World& world,
                              const ro::sim::Actor& actor, ro::render::ActorOutfit& outfit,
                              const ro::sim::WalkPath& path, std::uint32_t now_ms,
                              float looping_elapsed_ms,
                              std::span<const ro::render::ActorEffectPlay> played_effects,
                              bool with_imposed, const ReadableMotion& readable) {
  // Pétrifié ou gelé, l'acteur garde l'image où l'état l'a pris : tout se
  // calcule comme à cet instant-là.
  const bool frozen =
      actor.body_state == ro::sim::kBodyStone || actor.body_state == ro::sim::kBodyFrozen;
  if (frozen && now_ms > actor.body_state_since_ms) {
    looping_elapsed_ms -= static_cast<float>(now_ms - actor.body_state_since_ms);
    now_ms = actor.body_state_since_ms;
  }
  // CELUI DONT BLADE STOP SAISIT LA LAME se fige de même, à l'instant de la prise.
  if (actor.blade_stop_since_ms.has_value() && !actor.blade_stop_caster &&
      now_ms > *actor.blade_stop_since_ms) {
    looping_elapsed_ms -= static_cast<float>(now_ms - *actor.blade_stop_since_ms);
    now_ms = *actor.blade_stop_since_ms;
  }
  const ro::render::SpriteLayout layout = LayoutOf(actor);
  MotionEvents events{std::nullopt, played_effects, now_ms};
  PlayedMotion played;
  const std::uint32_t agony_start_ms = AgonyStartMs(wardrobe, world, actor);
  played.motion = RestingMotion(actor, path, events, agony_start_ms, now_ms);
  played.animation = ro::render::AnimationFor(played.motion, layout);
  // Une posture s'anime depuis l'instant où le serveur l'a annoncée — la mort,
  // depuis le début de l'agonie.
  played.elapsed_ms = static_cast<float>(now_ms - (played.motion == ro::render::Motion::kDie
                                                       ? agony_start_ms
                                                       : actor.posture_since_ms));
  const ro::render::SpriteCatalog* catalog = wardrobe != nullptr ? &wardrobe->catalog() : nullptr;
  // UNE CRÉATURE QUI DORT se fige, jusqu'à la motion suivante : voir
  // `AsleepCreatureMotionAt`.
  played = AsleepCreatureMotionAt(catalog, actor, layout, events, now_ms).value_or(played);
  // QUI FAIT LE MORT tient la pose du mort, que rien ci-dessous ne change.
  played = TrickDeadMotionAt(actor, layout, now_ms).value_or(played);
  if (played.motion != ro::render::Motion::kDie && wardrobe != nullptr) {
    // Le coup que cet acteur PORTE, s'il en montre encore la motion. Sa fenêtre
    // sert deux fois : à jouer le rang du coup, et à savoir si la blessure a le
    // droit de le couper. Un coup qui laisse la motion en place — un
    // enchaînement, le repos — ne s'y montre pas.
    std::optional<ro::render::StrikePlay> strike_play;
    float strike_since_ms = 0.0F;
    if (const ro::sim::Strike* posing = PosingStrikeOf(world, actor)) {
      if (auto play = StrikePlayFor(*wardrobe, world, actor, *posing)) {
        // Le plancher du joueur ne touche que ce qui se MONTRE : `HitLandsAtMs`
        // lit l'instant où le coup porte sans lui.
        const float unfloored_duration_ms = play->duration_ms;
        ro::render::FloorSkillGesture(*play, readable.skill_gesture_floor_ms);
        const bool floored = play->duration_ms > unfloored_duration_ms;
        const auto since_ms = static_cast<float>(now_ms - posing->started_ms);
        // 🔴 UNE MOTION POSÉE APRÈS LE COUP LE TERMINE, comme pour toute posture :
        // le client remplace l'action en cours dès qu'une autre arrive. Sans
        // cela, une incantation lancée pendant un coup n'apparaissait qu'à la fin
        // de son animation — ou jamais, sous une barre plus courte.
        const bool superseded = MotionChangedSince(catalog, actor, layout, played.motion, events,
                                                   ChangeWindow{posing->started_ms, now_ms, true});
        // 🔴 LA POSE MENÉE MINUTÉE DU COUP LE DÉCLARE FINI à son échéance, et
        // l'acteur se repose sans jouer la fin de la passe — sauf quand le
        // plancher du joueur allonge exprès le geste.
        const bool finished_by_pose =
            !floored && StrikeFinishedByLedPose(actor, layout, played.motion, now_ms);
        if (play->plays_motion && since_ms < play->duration_ms && !superseded &&
            !finished_by_pose) {
          strike_play = play;
          strike_since_ms = since_ms;
        }
      }
    }
    // UN JOUEUR QUI FRAPPE NE BRONCHE PAS, et une créature si.
    //
    // Le client refuse la posture de blessure à qui est déjà dans celle de
    // l'attaque, mais pour un métier de JOUEUR seulement — la créature, elle,
    // bronche toujours et son geste est coupé (l'état deux de la cible n'écarte
    // la blessure que pour un métier de joueur). Le coup d'une compétence
    // d'emploi n'est pas l'attaque : un mage qui achève Cold Bolt bronche.
    const bool swinging = strike_play.has_value() && strike_play->attack;
    const bool flinches = actor.hit.has_value() && actor.hit->flinches &&
                          !(layout == ro::render::SpriteLayout::kPlayer && swinging);
    // La cible bronche quand le coup touche — l'image de frappe que le coup
    // laisse sur son auteur, d'après sa tenue —, à la cadence que le serveur
    // lui donne.
    if (flinches) {
      float lands_at_ms = 0.0F;
      if (const ro::sim::Actor* attacker = world.Find(actor.hit->attacker)) {
        if (const auto play = StrikePlayFor(*wardrobe, world, *attacker, *actor.hit)) {
          lands_at_ms = play->lands_at_ms;
        }
      }
      // UNE POSE MENÉE ÉCARTE LA BLESSURE, qui n'a alors pas lieu : voir
      // `LedPoseHoldsAt`.
      const std::uint32_t lands_ms =
          actor.hit->started_ms + static_cast<std::uint32_t>(lands_at_ms);
      if (!LedPoseHoldsAt(&wardrobe->catalog(), actor, layout, played.motion, events, lands_ms)) {
        events.hurt_at_ms = lands_ms;
      }
      const std::size_t hurt = ro::render::AnimationFor(ro::render::Motion::kHurt, layout);
      // Sa cadence et sa tenue : voir `ro::render::HurtPlayFor`.
      const ro::render::HurtPlay hurt_play = ro::render::HurtPlayFor(
          ro::render::FileCadenceSteps(outfit.FrameIntervalMs(hurt)), actor.hit->hurt_motion_ms,
          actor.hit->type, ro::render::IsPlayerClass(actor.look.job));
      const float since_ms = static_cast<float>(now_ms - actor.hit->started_ms) - lands_at_ms;
      if (const auto image = ro::render::HurtFrameAt(hurt_play, outfit.FrameCount(hurt), since_ms);
          image.has_value() && events.hurt_at_ms.has_value()) {
        played = {ro::render::Motion::kHurt, hurt, since_ms, *image, false};
      }
    }
    // Qui frappe le montre à la cadence que le serveur donne au coup : l'attaque
    // pour un coup ordinaire ou d'état deux, le rang de l'état d'emploi pour
    // le coup d'une compétence — une passe, depuis sa première image.
    if (played.motion != ro::render::Motion::kHurt && strike_play.has_value()) {
      played.motion = strike_play->attack ? ro::render::Motion::kAttack : ro::render::Motion::kCast;
      played.animation = strike_play->animation;
      played.elapsed_ms = strike_since_ms / strike_play->factor;
      played.action_state = strike_play->state;
    }
  }
  // QUI INCANTE, PUIS QUI EMPLOIE, le montrent : voir `CastPostureAt` et
  // `SkillUsePostureAt`. Rien d'autre ne dit qu'un sort se prépare, et une
  // compétence instantanée n'a que le second chemin pour sa posture.
  if (played.motion != ro::render::Motion::kDie && played.motion != ro::render::Motion::kHurt &&
      played.motion != ro::render::Motion::kAttack && actor.cast.has_value()) {
    played = CastPostureAt(catalog, actor, outfit, layout, played.motion, events, now_ms)
                 .value_or(played);
    if (readable.cast_spans_bar) {
      SpreadCastOverBar(actor, outfit, layout, now_ms, played);
    }
  }
  if (played.motion != ro::render::Motion::kDie && played.motion != ro::render::Motion::kHurt &&
      played.motion != ro::render::Motion::kAttack && played.motion != ro::render::Motion::kCast &&
      actor.skill_use.has_value()) {
    played = SkillUsePostureAt(catalog, actor, outfit, layout, played.motion, events, now_ms)
                 .value_or(played);
  }
  // Qui ramasse se baisse, le temps de l'animation.
  if (played.motion != ro::render::Motion::kDie && played.motion != ro::render::Motion::kHurt &&
      played.motion != ro::render::Motion::kAttack && played.motion != ro::render::Motion::kCast &&
      actor.pickup_since_ms.has_value()) {
    const std::size_t pickup = ro::render::AnimationFor(ro::render::Motion::kPickup, layout);
    const auto since_ms = static_cast<float>(now_ms - *actor.pickup_since_ms);
    if (since_ms < outfit.AnimationDurationMs(pickup)) {
      played.motion = ro::render::Motion::kPickup;
      played.animation = pickup;
      played.elapsed_ms = since_ms;
    }
  }

  // UNE POSE MENÉE, image par image, passe devant l'animation du fichier :
  // voir `LedPoseAt`. La motion suivante la termine.
  if (played.motion != ro::render::Motion::kDie) {
    if (const auto led = LedPoseAt(catalog, actor, layout, played.motion, events, now_ms)) {
      played = {ro::render::Motion::kCast, led->rank, 0.0F, led->frame, false};
    }
  }
  // LE MOINE QUI TIENT UNE LAME : voir `BladeStopPoseAt`.
  if (const auto blade = BladeStopPoseAt(catalog, actor, layout, played.motion, events, now_ms)) {
    played = {ro::render::Motion::kCast, blade->rank, 0.0F, blade->frame, false};
  }
  // Et CHAQUE FRAPPE d'un enchaînement qui rejoue son coup : voir `ReplayedSwingAt`.
  //
  // ⚠ UN EFFET NE FORCE PAS DE POSE À PART : ce qu'il écrit dans l'état de
  // l'acteur décide du repos qui suit (`RestMotionAt`), et la pose qu'il mène
  // passe par `LedPoseAt` comme celle d'une compétence. Le coup de pied sauté
  // tient ainsi l'image de son élan du cinquième au trente-cinquième tic, puis
  // se repose en garde.
  played = ReplayedSwingAt(wardrobe, world, actor, outfit, layout, now_ms).value_or(played);

  // Une posture qui se joue UNE FOIS ou tient une image le dit elle-même : elle
  // garde son horloge, que l'horloge commune des boucles remplacerait par une
  // phase quelconque.
  played.loops = played.loops && ro::render::MotionLoops(played.motion);
  if (played.loops) {
    played.elapsed_ms = looping_elapsed_ms;
  }
  // La MARCHE ne se joue pas sur une horloge : son image vient de la DISTANCE
  // parcourue. C'est ce qui fait qu'un personnage rapide lève les pieds plus
  // souvent et qu'un personnage lent traîne, sans qu'aucune cadence ne soit
  // réglée nulle part.
  if (played.motion == ro::render::Motion::kWalk) {
    const float interval_ms = outfit.FrameIntervalMs(played.animation);
    const float cycle_ms = outfit.AnimationDurationMs(played.animation);
    const auto images =
        interval_ms > 0.0F ? static_cast<std::size_t>(std::lround(cycle_ms / interval_ms)) : 0;
    const float travelled_units =
        ro::sim::TravelledCells(actor, path, now_ms) * ro::render::kWorldUnitsPerCell;
    played.held_frame = ro::render::WalkFrameAt(travelled_units, interval_ms, images);
    // 🔴 QUI GLISSE TIENT UNE IMAGE DE SA MARCHE au lieu de lever les pieds.
    if (actor.glide_ms_per_cell != 0) {
      played.held_frame = ro::render::GlideHeldWalkFrame(images);
    }
  }
  // Au repos et assis, un JOUEUR ne joue AUCUNE animation : les trois images de
  // ces deux actions — corps, tête et coiffes en portent les mêmes trois — sont
  // les trois orientations du REGARD, et le serveur dit laquelle. Les faire
  // défiler donne un personnage qui dit non sans arrêt. Toutes les tenues de
  // l'installation en portent exactement trois, ce qui interdit d'y lire une
  // animation ; c'est là, et là seulement, que /doridori se voit. Cinq comptes
  // font exception à l'attente : voir `IdleHoldsLookImage`.
  if ((played.motion == ro::render::Motion::kIdle && IdleHoldsLookImage(actor)) ||
      (played.motion == ro::render::Motion::kSit && ComposedAsPlayer(actor))) {
    played.held_frame = actor.head_direction;
  }
  // UN MERCENAIRE HUMAIN, AU REPOS, SE LIT SUR SON IMAGE ZÉRO : sa tête est
  // celle d'un joueur, dont les images de repos sont les orientations du
  // regard. Voir `ro::render::IsHumanMercenaryClass`.
  if (played.motion == ro::render::Motion::kIdle && actor.kind != ro::sim::ActorKind::kItem &&
      ro::render::IsHumanMercenaryClass(actor.look.job)) {
    played.held_frame = std::size_t{0};
  }
  if (!with_imposed) {
    return played;
  }
  // L'IMAGE QU'IMPOSE UNE COMPÉTENCE DE CASE : bornée par une motion qui tient
  // la sienne (`GroundSkillHeldFrameAt`), dessinée telle quelle sur une motion
  // figée (`GroundSkillFrozenImageAt`).
  const MotionAtInstant before = [&](std::uint32_t at_ms,
                                     std::span<const ro::render::ActorEffectPlay> effects) {
    return ComposedMotionAt(wardrobe, world, actor, outfit, path, at_ms, 0.0F, effects, false,
                            readable);
  };
  if (const auto image =
          GroundSkillHeldFrameAt(catalog, actor, outfit, layout, played, events, now_ms, before)) {
    played.held_frame = image;
  }
  return GroundSkillFrozenImageAt(catalog, actor, outfit, layout, played, events, now_ms, before)
      .value_or(played);
}

}  // namespace

PlayedMotion MotionAt(ro::render::ActorWardrobe* wardrobe, const ro::sim::World& world,
                      const ro::sim::Actor& actor, ro::render::ActorOutfit& outfit,
                      const ro::sim::WalkPath& path, std::uint32_t now_ms, float looping_elapsed_ms,
                      std::span<const ro::render::ActorEffectPlay> played_effects,
                      const ReadableMotion& readable) {
  return ComposedMotionAt(wardrobe, world, actor, outfit, path, now_ms, looping_elapsed_ms,
                          played_effects, true, readable);
}

bool PlayerRestsAt(ro::render::ActorWardrobe* wardrobe, const ro::sim::World& world,
                   ro::sim::WalkPlanner& planner, std::uint32_t now_ms) {
  const ro::sim::Actor* player = world.player();
  if (player == nullptr) {
    return false;
  }
  const ro::sim::Actor& actor = *player;
  const ro::sim::WalkPath& path = planner.PathFor(actor);
  ro::render::ActorOutfit* outfit =
      wardrobe != nullptr ? wardrobe->OutfitFor(LookOf(actor)) : nullptr;
  if (outfit == nullptr) {
    return actor.posture == ro::sim::Posture::kStanding && !ro::sim::IsWalking(actor, path, now_ms);
  }
  std::vector<ro::render::ActorEffectPlay> played;
  PlayedEffectsOf(actor, now_ms, played);
  const ro::render::Motion motion =
      MotionAt(wardrobe, world, actor, *outfit, path, now_ms, 0.0F, played).motion;
  return motion == ro::render::Motion::kIdle || motion == ro::render::Motion::kReady;
}

}  // namespace moonclient
