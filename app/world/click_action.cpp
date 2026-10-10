#include "world/click_action.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "ro/core/log.h"
#include "ro/net/action.h"
#include "ro/net/movement.h"
#include "ro/net/npc_dialog.h"
#include "ro/net/turn.h"
#include "ro/render/actor_pick.h"
#include "ro/render/ground_pick.h"
#include "ro/render/map_scene.h"
#include "ro/sim/movement.h"
#include "ro/sim/skill_book.h"
#include "ro/sim/world.h"

#include "world/actor_visibility.h"
#include "world/attack_intent.h"
#include "world/blocked_npcs.h"
#include "world/cast_bar_view.h"
#include "world/ground_unit_view.h"
#include "world/overlay_view.h"

namespace moonclient {
namespace {

// Le clic lève l'engagement de la souris sur l'entité où il agit, sauf sous ALT
// et au second appui d'un double-clic. C'est aussi le seul clic qui désigne la
// cible du mode Ciblage : un PNJ, un portail, un objet ou un familier ne
// l'engagent pas, et ne se désignent pas.
void Engage(const WorldClick& scene, const ro::sim::Actor& target) {
  if (scene.engagement == nullptr || scene.alt_held || scene.second_of_double) {
    return;
  }
  NoteEngagingClick(*scene.engagement, target.id, scene.attack_repeats);
  if (scene.targeting != nullptr) {
    static_cast<void>(scene.targeting->Designate(*scene.world, target.id));
  }
}

// Une compétence ne lève la flèche que sur un monstre ou un joueur, soi compris.
[[nodiscard]] bool SkillEngages(const ro::sim::Actor& target) {
  return target.kind == ro::sim::ActorKind::kMonster || target.kind == ro::sim::ActorKind::kPlayer;
}

}  // namespace

std::optional<ro::sim::CellPosition> GroundCellAt(const MapStage& stage,
                                                  const ro::render::Camera& camera,
                                                  float aspect_ratio, float screen_x,
                                                  float screen_y, float viewport_width,
                                                  float viewport_height) {
  const auto ground =
      ro::render::PickGround(stage.scene.gnd, stage.gat.has_value() ? &*stage.gat : nullptr, camera,
                             aspect_ratio, screen_x, screen_y, viewport_width, viewport_height);
  if (!ground.has_value()) {
    return std::nullopt;
  }
  const std::array<std::int32_t, 2> cell =
      ro::render::CellAtPosition(stage.scene.gnd, (*ground)[0], (*ground)[2]);
  ro::sim::CellPosition destination;
  destination.x = static_cast<std::uint16_t>(cell[0]);
  destination.y = static_cast<std::uint16_t>(cell[1]);
  return destination;
}

void ReportRefusal(const ro::core::Result<void>& sent) {
  if (!sent) {
    // ⚠ AU JOURNAL, et non sur la sortie d'erreur : TOUTE la famille des refus
    // d'émission passe par ici — la marche, le demi-tour, le ramassage, le
    // battement d'horloge, l'émoticône, la renaissance, les réponses de PNJ —,
    // et rien de tout cela n'entrait dans le fichier qu'un joueur joint à son
    // rapport ni dans la fenêtre de journal.
    //
    // La cadence tient au fait que chacun de ces sites est gardé par
    // `link_.is_open()` : une liaison tombée les arrête tous, et la perte
    // elle-même est dite une fois, en erreur, par `game_network.cpp`.
    ro::core::LogWarning("demande au serveur non émise : {} — le geste restera sans effet",
                         ro::core::Describe(sent.error()));
  }
}

bool ActorTakesClick(const ro::sim::Actor& actor, VersusGate versus) {
  // LA FORME DU CURSEUR RÉPOND DÉJÀ, et ce n'est pas une commodité : chez le
  // client, c'est la MÊME fonction qui pose la forme et qui rend le code que
  // la marche au sol consulte. Là où la
  // flèche reste, il n'y a rien à faire de cet acteur.
  //
  // Le sol ne se consulte pas ici — il ne décide qu'en l'absence d'acteur.
  return CursorShapeOver(&actor, GroundHover::kUnknown, versus) != ro::render::CursorShape::kArrow;
}

bool ClickEngagesAttack(const ro::sim::Actor& actor) {
  return (actor.kind == ro::sim::ActorKind::kMonster ||
          actor.kind == ro::sim::ActorKind::kPlayer) &&
         actor.posture != ro::sim::Posture::kDead;
}

void NoteDeedOfClickOn(MouseEngagement* engagement, const ro::sim::Actor& target, bool repeat) {
  if (engagement == nullptr) {
    return;
  }
  if (ClickEngagesAttack(target)) {
    NoteDeed(*engagement, PlayerDeed::kAttack, target.id, /*stops_on_release=*/!repeat);
    engagement->action.target_seen_alive = true;
  } else if (target.kind == ro::sim::ActorKind::kItem) {
    NoteDeed(*engagement, PlayerDeed::kPickup, target.id, /*stops_on_release=*/false);
  } else if (target.kind == ro::sim::ActorKind::kNonPlayer &&
             target.look.job == ro::sim::kPortalJob) {
    // Un portail se franchit en marchant : c'est une marche.
    EndDeed(*engagement);
  }
}

MouseRoute MouseRouteOf(const ro::sim::World& world, const SkillBookPanel& skills,
                        const MouseEngagement& engagement, std::uint32_t now_ms) {
  return {.frozen = WorldMouseFrozen(engagement, world, OwnBarShown(world, now_ms)),
          .suspended = skills.aim() == SkillAim::kGround};
}

void RequestGroundWalk(ro::net::Connection& link, const ro::sim::World& world,
                       ro::sim::ApproachWalkClock& clock, DeferredWalk* deferred,
                       std::optional<PlayerGesture> player_gesture,
                       ro::sim::CellPosition destination, std::uint32_t now_ms) {
  if (const auto due = GroundWalkDue(world, clock, deferred, player_gesture, destination, now_ms)) {
    ReportRefusal(ro::net::RequestMove(link, *due));
  }
}

bool ReleaseDeferredWalk(ro::net::Connection& link, const ro::sim::World& world,
                         ro::sim::ApproachWalkClock& clock, DeferredWalk& deferred,
                         std::optional<PlayerGesture> player_gesture, std::uint32_t now_ms) {
  const auto released = ReleasedWalk(world, clock, deferred, player_gesture, now_ms);
  if (!released.has_value()) {
    return false;
  }
  ReportRefusal(ro::net::RequestMove(link, *released));
  return true;
}

ro::core::Result<void> ClickOn(ro::net::Connection& link, const ro::sim::World& world,
                               const ro::sim::Actor& target, std::uint32_t now_ms,
                               const ClickIntents& intents, bool repeat) {
  switch (target.kind) {
    // 🔴 LE JOUEUR ET LE MONSTRE PARTAGENT LE CAS, ET C'EST VOULU. Le serveur
    // n'en fait pas non plus deux chemins : `clif_parse_ActionRequest` ne
    // regarde pas l'espèce de la cible. Deux cas jumeaux finiraient par
    // diverger, et l'un des deux oublierait la poursuite.
    //
    // ⚠ ON N'ARRIVE ICI SUR UN JOUEUR QUE SI LA PORTE EST OUVERTE :
    // `ActorTakesClick` a déjà laissé descendre au sol le clic sur un passant.
    case ro::sim::ActorKind::kPlayer:
    case ro::sim::ActorKind::kMonster:
      if (target.posture == ro::sim::Posture::kDead) {
        return {};
      }
      // RIEN NE PART ENCORE : le tic mesurera la distance à la prochaine
      // image, et n'émettra le coup qu'une fois à portée. Voir
      // `world/attack_intent.h`.
      intents.attack = ArmAttack(world, target, repeat);
      return {};
    case ro::sim::ActorKind::kItem: {
      const ro::sim::Actor* self = world.player();
      if (self == nullptr) {
        return {};
      }
      if (ro::sim::WithinPickupReach(ro::sim::StandingCell(*self), target.position)) {
        return ro::net::RequestPickup(link, target.id);
      }
      // La marche vers l'objet passe le plancher ; retenue, elle attend dans
      // l'intention que le tic la redemande — voir `ro::sim::DuePickupWalk`.
      intents.pickup = ro::sim::PickupIntent{
          .item_id = target.id, .since_ms = now_ms, .walk_toward = target.position};
      if (const auto toward =
              ro::sim::DuePickupWalk(world, *intents.pickup, intents.walk_clock, now_ms)) {
        return ro::net::RequestMove(link, *toward);
      }
      return {};
    }
    case ro::sim::ActorKind::kNonPlayer:
      // Un portail désigné par le menu d'entité ou le mode Ciblage : y marcher
      // suffit, le serveur fait passer. C'est une marche vers sa case, retenue
      // et soumise au plancher comme un clic au sol. Sous la souris, un warp ne
      // prend pas le clic : voir `LeftClick`.
      if (target.look.job == ro::sim::kPortalJob) {
        RequestGroundWalk(link, world, intents.walk_clock, intents.deferred_walk,
                          intents.player_gesture, target.position, now_ms);
        return {};
      }
      // Tout le reste : on lui parle. C'est le seul geste qui ouvre un
      // dialogue, et il ne part QUE d'ici — voir ContactNpc pour ce que le
      // serveur fait du même paquet visé sur un joueur.
      return ro::net::ContactNpc(link, target.id);
    default:
      return {};
  }
}

bool TurnTowardCell(const WorldClick& scene, ro::sim::CellPosition toward) {
  const ro::sim::Actor* self = scene.world->player();
  if (self == nullptr || scene.planner == nullptr) {
    return false;
  }
  const ro::sim::WalkPath& path = scene.planner->PathFor(*self);
  if (ro::sim::IsWalking(*self, path, scene.now_ms)) {
    return false;
  }
  const ro::sim::CellPosition standing = ro::sim::StandingCell(*self);
  if (standing.x == toward.x && standing.y == toward.y) {
    return false;
  }
  // ⚠ ON PART DE CE QUI EST MONTRÉ, pas de ce que la case annonce. Un chemin
  // garde le regard de son dernier pas, et `position.direction` reste alors
  // celui d'AVANT la marche : y lire le corps ferait compter le demi-tour
  // depuis une direction que personne ne voit.
  const ro::sim::Facing shown{ro::sim::FacingAt(*self, path, scene.now_ms), self->head_direction};
  const ro::sim::Facing wanted =
      ro::sim::TurnToward(shown, ro::sim::DirectionTowards(standing, toward, shown.direction));
  if (wanted == shown) {
    return false;
  }
  ReportRefusal(ro::net::TurnPlayer(*scene.link, *scene.world, wanted));
  return true;
}

HeldClick LeftClick(const WorldClick& scene, glm::vec2 mouse_px, const ClickIntents& intents) {
  if (scene.viewport_width <= 0.0F || scene.viewport_height <= 0.0F) {
    return HeldClick::kNothing;
  }
  if (scene.skills->ClickInPanel(*scene.link, *scene.world, mouse_px, scene.viewport_width,
                                 scene.viewport_height, scene.now_ms)) {
    return HeldClick::kNothing;
  }
  // Un clic remplace l'intention d'avant : on ne ramasse plus ce qu'on a cessé
  // de viser, et on ne court plus après une cible qu'on a cessé de désigner.
  // Le client natif en fait autant — une action en attente ÉCRASE la
  // précédente, elle ne s'y ajoute pas.
  intents.pickup.reset();
  intents.attack.reset();
  scene.skills->CancelPending();

  // 🔴 UN SORT AU SOL VISE LA CASE, ET RIEN NE S'INTERPOSE. Le client n'y met
  // qu'une condition : la case doit se franchir. Tout le reste — un monstre
  // dessus, un joueur, un PNJ — le laisse passer, et c'est ce qui permet de
  // poser Storm Gust SUR ce qu'on veut toucher plutôt qu'à côté.
  //
  // ⚠ SANS CETTE SORTIE, LE CHOIX DE CIBLE MANGEAIT LE CLIC : la compétence
  // armée ne se relâche pas sur un acteur — elle vise le sol —, si bien que
  // l'acteur prenait le clic et qu'on ATTAQUAIT le monstre visé au lieu de
  // lancer le sort. Le cas ne se voit que sur une case occupée, c'est-à-dire
  // exactement celle qu'on vise.
  const ro::sim::KnownSkill* armed = scene.skills->armed();
  const bool aims_at_ground = armed != nullptr && ro::sim::SkillNeedsGround(*armed);

  const std::optional<std::size_t> picked =
      aims_at_ground
          ? std::nullopt
          : ro::render::PickActor(*scene.camera, scene.aspect_ratio, scene.drawn, mouse_px.x,
                                  mouse_px.y, scene.viewport_width, scene.viewport_height);
  const ro::sim::Actor* under_mouse = picked.has_value() && *picked < scene.drawn_ids.size()
                                          ? scene.world->Find(scene.drawn_ids[*picked])
                                          : nullptr;
  // Un warp laisse le clic descendre au sol : voir `ClickTakerUnderMouse`.
  const ro::sim::Actor* target = ClickTakerUnderMouse(under_mouse);
  // Le double-clic ne rejoue une attaque que sur ce que l'appui a touché.
  if (scene.targeting != nullptr) {
    scene.targeting->NotePressedOn(target != nullptr ? target->id : 0);
  }
  // UNE UNITÉ AU SOL QUI PASSE DEVANT L'ACTEUR — un bloc d'Ice Wall, un piège
  // — prend le clic, et il s'y arrête : ni acteur, ni marche. Celle qu'on
  // frappe fait approcher le personnage puis frapper, en levant l'engagement de
  // la souris SANS changer ce que la flèche désigne ; un piège hors siège ne
  // fait rien du tout. Une compétence armée garde le clic pour elle — sauf celle
  // qui vise un piège posé, que l'unité désigne.
  const SkillAim aim = scene.skills->aim();
  const std::optional<std::uint32_t> unit_id =
      aim != SkillAim::kNothing && aim != SkillAim::kTrap
          ? std::nullopt
          : PickGroundUnit(*scene.camera, scene.aspect_ratio, scene.ground_units, mouse_px.x,
                           mouse_px.y, scene.ground_unit_floor_pixels, scene.viewport_width,
                           scene.viewport_height);
  const GroundUnitClick on_unit = GroundUnitClickOf(
      unit_id.has_value() ? std::optional(scene.world->skill_units().at(*unit_id).unit_id)
                          : std::nullopt,
      target, aim);
  if (on_unit == GroundUnitClick::kCastSkill) {
    static_cast<void>(scene.skills->ReleaseOnGroundUnit(scene.world->skill_units().at(*unit_id)));
    return HeldClick::kNothing;
  }
  // 🔴 TOUT AUTRE CLIC DÉSARME LA VISÉE D'UN PIÈGE, et ne la lance nulle part.
  // Sur un monstre ou un joueur, il lève la flèche sans frapper ; sur un objet
  // au sol, il ne ramasse rien ; ailleurs, le clic suit son cours ordinaire —
  // un PNJ répond, le sol fait marcher.
  if (aim == SkillAim::kTrap) {
    scene.skills->Disarm();
    if (target != nullptr && target->id != scene.world->player_id() &&
        ActorTakesClick(*target, scene.versus) &&
        (ClickEngagesAttack(*target) || target->kind == ro::sim::ActorKind::kItem)) {
      if (ClickEngagesAttack(*target)) {
        Engage(scene, *target);
      }
      return HeldClick::kNothing;
    }
  }
  if (on_unit == GroundUnitClick::kAttack) {
    if (scene.engagement != nullptr) {
      scene.engagement->engaged = true;
      scene.engagement->continuous = scene.attack_repeats;
    }
    intents.attack = ArmAttackOnGroundUnit(*scene.world, scene.world->skill_units().at(*unit_id),
                                           scene.attack_repeats);
  }
  if (on_unit != GroundUnitClick::kNotTaken) {
    return HeldClick::kNothing;
  }
  // 🔴 UN PNJ RENDU SOURD NE PREND RIEN, ET LE SOL NON PLUS : ni dialogue, ni
  // désignation, ni marche vers lui. C'est le clic entier qui est avalé — le
  // joueur qui vient de dire « celui-là, je ne veux plus le cliquer » n'attend
  // pas que son personnage traverse la place pour rien.
  if (target != nullptr && scene.entity_menu != nullptr && scene.entity_menu->blocked != nullptr &&
      SwallowsLeftClick(*scene.entity_menu->blocked, scene.entity_menu->npc_block, *target,
                        *scene.world, scene.now_ms)) {
    return HeldClick::kNothing;
  }
  if (target != nullptr) {
    const bool takes_click =
        target->id != scene.world->player_id() && ActorTakesClick(*target, scene.versus);
    // La compétence ARMÉE d'abord, et sur N'IMPORTE QUI : c'est une visée, et
    // un soin se lance justement sur un autre joueur ou sur soi-même.
    if (scene.skills->ReleaseOnActor(*target)) {
      if (SkillEngages(*target)) {
        Engage(scene, *target);
      }
      if (scene.engagement != nullptr) {
        NoteDeed(*scene.engagement, PlayerDeed::kSkillOnTarget, target->id,
                 /*stops_on_release=*/false);
        scene.engagement->action.target_seen_alive = target->posture != ro::sim::Posture::kDead;
      }
      return HeldClick::kNothing;
    }
    // Puis seulement ceux qui PRENNENT le clic. Les autres le laissent
    // descendre au sol : on marche à travers son propre personnage, à travers
    // les autres joueurs, à travers un cadavre. Voir ActorTakesClick.
    if (takes_click) {
      const bool attacks = ClickEngagesAttack(*target);
      // La flèche se lève AVANT que le mode Ciblage ne retienne le coup : elle
      // suit le geste, pas ce qu'il déclenche.
      if (attacks) {
        Engage(scene, *target);
      }
      // « Le clic cible sans attaquer » : la cible est prise, rien ne s'arme.
      if (scene.targeting != nullptr && attacks &&
          scene.targeting->SuppressClickEngage(/*skill_armed=*/false)) {
        return HeldClick::kNothing;
      }
      ReportRefusal(
          ClickOn(*scene.link, *scene.world, *target, scene.now_ms, intents, scene.attack_repeats));
      NoteDeedOfClickOn(scene.engagement, *target, scene.attack_repeats);
      return HeldClick::kNothing;
    }
  }

  const std::optional<ro::sim::CellPosition> destination =
      GroundCellAt(*scene.stage, *scene.camera, scene.aspect_ratio, mouse_px.x, mouse_px.y,
                   scene.viewport_width, scene.viewport_height);
  if (!destination.has_value()) {
    return HeldClick::kNothing;
  }
  // ⚠ ET LA SEULE CHOSE QUI REFUSE EST UNE CASE INFRANCHISSABLE. Le serveur la
  // refuserait de toute façon ; la garder ici évite de DÉSARMER pour rien — le
  // sort reste en main, et le clic suivant peut viser juste.
  // ⚠ ET PAS DE CARTE N'EST PAS UNE CASE BLOQUÉE. Sans GAT — le temps qu'une
  // carte se charge —, la grille est VIDE et Passable rendrait faux partout :
  // plus aucun sort au sol ne partirait. On laisse alors passer, et le serveur
  // tranche comme il le ferait de toute façon.
  const ro::sim::WalkMap* walk = PlannedMapOf(*scene.stage);
  if (aims_at_ground && walk != nullptr &&
      !walk->Passable(static_cast<std::int32_t>(destination->x),
                      static_cast<std::int32_t>(destination->y))) {
    return HeldClick::kNothing;
  }
  if (scene.skills->ReleaseOnGround(*scene.world, *destination, scene.now_ms)) {
    // Un sort au sol qui part achève l'action du personnage : l'engagement
    // retombe, sans que le clic l'ait jamais levé.
    if (aims_at_ground && scene.engagement != nullptr) {
      EndDeed(*scene.engagement);
    }
    return HeldClick::kNothing;
  }
  // MAJ TENUE : on tourne vers la case au lieu d'y aller.
  //
  // 🔴 ET LE MAINTIEN S'OUVRE QUAND MÊME, alors qu'un demi-tour est instantané.
  // C'est justement ce qui le rend utilisable : le tour n'avance que d'un
  // huitième par demande, et sans la répétition il faudrait recliquer pour
  // chaque huitième. Voir kTurnRepeatMs.
  //
  // ASSIS, LE CLIC TOURNE DE MÊME, sans Maj : on ne marche pas assis, et c'est
  // le seul geste qui reste au personnage. Se relever passe par sa touche.
  const ro::sim::Actor* self = scene.world->player();
  const bool sitting = self != nullptr && self->posture == ro::sim::Posture::kSitting;
  if (scene.shift_held || sitting) {
    static_cast<void>(TurnTowardCell(scene, *destination));
    return HeldClick::kTurn;
  }
  // Retenu par le geste, ou trop tôt après la marche précédente, le clic ne
  // part pas : il fait pourtant retomber l'engagement et ouvre le maintien.
  RequestGroundWalk(*scene.link, *scene.world, intents.walk_clock, intents.deferred_walk,
                    intents.player_gesture, *destination, scene.now_ms);
  if (scene.engagement != nullptr) {
    EndDeed(*scene.engagement);
  }
  return HeldClick::kWalk;
}

bool MouseWheel(SkillBookPanel& skills, const ro::sim::World& world,
                const ro::sim::SkillTree* skill_tree, glm::vec2 mouse_px, float wheel_steps,
                float viewport_width, float viewport_height, std::uint32_t now_ms) {
  return skills.Scroll(world, skill_tree, mouse_px, wheel_steps, viewport_width, viewport_height,
                       now_ms);
}

}  // namespace moonclient
