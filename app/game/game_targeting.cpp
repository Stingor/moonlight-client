// LE MODE CIBLAGE, étape par étape de l'image : ce que les gestes ont laissé,
// la cible revalidée, ce qu'on redemande au serveur, et ce que le HUD montre.
// Et la flèche du jeu, que le mode détourne.

#include <algorithm>
#include <array>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <SDL3/SDL.h>

#include "ro/net/actor_name.h"
#include "ro/net/target_info.h"
#include "ro/render/actor_pick.h"
#include "ro/render/terrain_mesh.h"
#include "ro/sim/movement.h"

#include "game/game.h"
#include "world/click_action.h"
#include "world/map_stage.h"
#include "world/name_plate_view.h"
#include "world/versus_gate.h"

namespace moonclient {
namespace {

// Deux cellules de franchissement par côté de cube du terrain.
constexpr int kCellsPerCube =
    static_cast<int>(ro::render::kWorldUnitsPerCube / ro::render::kWorldUnitsPerCell);

}  // namespace

void Game::AdvanceTargeting(std::uint32_t now_ms) {
  TargetOptions& options = settings_->options().targeting();
  windows_.target_hud.look = &options.look();
  if (!link_.is_open()) {
    windows_.target_hud.view = nullptr;
    return;
  }

  // Ce que le HUD a reçu à l'image précédente.
  const ro::ui::TargetHudAnswer answer = std::exchange(windows_.target_hud.answer, {});
  target_hud_hovered_ = answer.hovered;
  // Un cadre déplacé s'écrit, une fois : `Touch` arme l'anti-rebond du fichier.
  if (answer.moved) {
    settings_->file().Touch();
  }
  targeting_.NoteFrameClicks(world_, answer.left_click, answer.right_click);

  // Un sort parti désigne sa cible, quoi qu'en dise le clic.
  targeting_.NoteSkillTarget(world_, grimoire_.TakeLaunchedTarget());

  const bool continuous = Targeting::WantsContinuousAttack(
      SiegeMap(world_mouse_.engagement, world_.map_mode()),
      (SDL_GetModState() & SDL_KMOD_CTRL) != 0, settings_->options().no_ctrl());

  // Le double-clic dans le monde, rejoué après le clic qui l'accompagne.
  if (const std::uint32_t id =
          targeting_.TakeDoubleClickTarget(world_, grimoire_.armed() != nullptr);
      id != 0) {
    if (const ro::sim::Actor* target = world_.Find(id);
        target != nullptr && ClickEngagesAttack(*target)) {
      EngageTargetAttack(*target, continuous, now_ms);
    }
  }

  // Les cadres qui agissent comme la cible. Leur clic droit ouvre le menu de
  // l'entité : voir `AdvanceEntityMenu`, qui le prend juste après.
  const std::uint32_t clicked = targeting_.TakeFrameClick();
  const TargetingSettings& settings = targeting_.settings();
  if (clicked != 0 && settings.enabled && settings.proxy_click) {
    if (const ro::sim::Actor* target = world_.Find(clicked); target != nullptr) {
      if (grimoire_.aim() == SkillAim::kTarget) {
        static_cast<void>(grimoire_.ReleaseOnActor(*target));
      } else if (grimoire_.aim() == SkillAim::kNothing) {
        // Sur un cadre, un seul clic attaque : la cible est déjà désignée, il
        // n'y a aucune ambiguïté à lever.
        if (ClickEngagesAttack(*target)) {
          EngageTargetAttack(*target, continuous, now_ms);
        } else {
          pickup_.reset();
          attack_.reset();
          grimoire_.CancelPending();
          ReportRefusal(
              ClickOn(link_, world_, *target, now_ms,
                      ClickIntents{pickup_, attack_, grimoire_.approach_clock(),
                                   &world_mouse_.deferred_walk, scene_actors_.player_gesture},
                      continuous));
          NoteDeedOfClickOn(&world_mouse_.engagement, world_, *target, continuous);
        }
      }
    }
  }

  AdvanceQuickCast(now_ms);
  targeting_.Revalidate(world_);

  const bool statuses_shown =
      options.look().parts[static_cast<std::size_t>(ro::ui::TargetPart::kStatuses)].shown;
  const TargetRequests asked = targeting_.Poll(world_, now_ms, statuses_shown);
  if (asked.name_id != 0) {
    ReportRefusal(ro::net::RequestActorName(link_, asked.name_id));
  }
  if (asked.info_id != 0) {
    ReportRefusal(ro::net::RequestTargetInfo(link_, asked.info_id));
  }
  if (asked.statuses_id != 0) {
    ReportRefusal(ro::net::RequestTargetStatuses(link_, asked.statuses_id));
  }

  if (target_hud_.has_value()) {
    target_hud_->Refresh(world_, targeting_, options, grimoire_.aim() == SkillAim::kGround, styles_,
                         now_ms);
  }
  // Éteint, le mode ne dessine rien, pas même en placement.
  windows_.target_hud.view =
      targeting_.enabled() && target_hud_.has_value() ? &target_hud_->view() : nullptr;
}

void Game::EngageTargetAttack(const ro::sim::Actor& target, bool continuous, std::uint32_t now_ms) {
  // La dispense ouvre UN passage dans le chemin du clic, qui le consomme
  // aussitôt, et laisse partir le coup — une fois, ou sans fin.
  targeting_.NoteExplicitAttack(target.id, !continuous);
  if (targeting_.SuppressClickEngage(/*skill_armed=*/false)) {
    return;
  }
  pickup_.reset();
  attack_.reset();
  grimoire_.CancelPending();
  ReportRefusal(ClickOn(link_, world_, target, now_ms,
                        ClickIntents{pickup_, attack_, grimoire_.approach_clock(),
                                     &world_mouse_.deferred_walk, scene_actors_.player_gesture},
                        continuous));
  // L'attaque part sans engager la souris : la flèche reste où elle était.
  NoteDeedOfClickOn(&world_mouse_.engagement, world_, target, continuous);
}

void Game::NoteScreenTargets() {
  const std::span<const ro::render::DrawnActor> drawn = scene_actors_.drawn;
  const std::span<const std::uint32_t> ids = scene_actors_.ids;
  const std::size_t count = std::min(drawn.size(), ids.size());
  std::vector<TargetCandidate> found;
  if (!targeting_.enabled()) {
    targeting_.NoteOnScreen(std::move(found));
    return;
  }
  const glm::vec3* own = nullptr;
  for (std::size_t rank = 0; rank < count; ++rank) {
    if (ids[rank] == world_.player_id()) {
      own = &drawn[rank].ground_position;
      break;
    }
  }
  const auto width = static_cast<float>(shell_.width());
  const auto height = static_cast<float>(shell_.height());
  for (std::size_t rank = 0; own != nullptr && rank < count; ++rank) {
    const ro::sim::Actor* actor = world_.Find(ids[rank]);
    // Vivant et visible : un cadavre ou un monstre caché ne se cible pas au
    // clavier.
    if (actor == nullptr || actor->kind != ro::sim::ActorKind::kMonster ||
        actor->posture == ro::sim::Posture::kDead || !HasNamePlate(*actor) ||
        TargetIsConcealed(*actor)) {
      continue;
    }
    // À l'écran, et rien d'autre : une cible qu'on ne voit pas ne se jauge pas.
    const std::optional<glm::vec2> screen = ro::render::ProjectToScreen(
        camera_, aspect_ratio_, drawn[rank].ground_position, width, height);
    if (!screen.has_value() || screen->x <= 0.0F || screen->y <= 0.0F || screen->x >= width ||
        screen->y >= height) {
      continue;
    }
    const float dx = drawn[rank].ground_position.x - own->x;
    const float dz = drawn[rank].ground_position.z - own->z;
    found.push_back(TargetCandidate{.id = actor->id, .distance_squared = dx * dx + dz * dz});
  }
  targeting_.NoteOnScreen(std::move(found));
}

EngagementFrame Game::UnderMouse() const {
  EngagementFrame frame;
  frame.mouse = PointerPx{mouse_.x, mouse_.y};
  frame.over_interface = interface_->gate().mouse_to_ui;
  // Le même survol que celui des étiquettes et du curseur : la scène de cette
  // image, fenêtres ou non par-dessus.
  const std::optional<std::size_t> picked = ro::render::PickActor(
      camera_, aspect_ratio_, scene_actors_.drawn, mouse_.x, mouse_.y,
      static_cast<float>(shell_.width()), static_cast<float>(shell_.height()));
  if (!picked.has_value() || *picked >= scene_actors_.ids.size()) {
    return frame;
  }
  frame.anything_under = true;
  const ro::sim::Actor* actor = world_.Find(scene_actors_.ids[*picked]);
  if (actor == nullptr || actor->kind != ro::sim::ActorKind::kPlayer ||
      actor->id == world_.player_id()) {
    return frame;
  }
  frame.player_under = actor->id;
  const bool shift_held = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
  frame.player_attackable =
      ClickEngagesAttack(*actor) &&
      ActorTakesClick(*actor, VersusGateOf(world_, shift_held, settings_->options().no_shift()));
  return frame;
}

void Game::PlaceSelectionMarker(MarkerCall call, std::uint32_t now_ms) {
  if (call.order != MarkerOrder::kShow || call.on == nullptr) {
    ApplyMarker(marker_pose_, call.order, {});
    return;
  }
  const ro::sim::FractionalCell cell =
      ro::sim::InterpolatedCell(*call.on, planner_.PathFor(*call.on), now_ms);
  const int width_cells =
      stage_->gat.has_value() ? stage_->gat->width() : stage_->scene.gnd.width() * kCellsPerCube;
  const int height_cells =
      stage_->gat.has_value() ? stage_->gat->height() : stage_->scene.gnd.height() * kCellsPerCube;
  const ro::sim::FractionalCell placed =
      MarkerCell(cell, width_cells, height_cells, ro::render::kWorldUnitsPerCell);
  ApplyMarker(marker_pose_, MarkerOrder::kShow, GroundPositionAt(*stage_, placed.x, placed.y));
}

void Game::DrawSelectionMarker(std::uint32_t now_ms) {
  if (!link_.is_open()) {
    return;
  }
  MouseEngagement& engagement = world_mouse_.engagement;
  NoteMapMode(engagement, world_.map_mode());
  const ro::sim::Actor* hud_marked = targeting_.MarkedActor(world_);
  // La sélection que le clavier ou un menu du mode Ciblage vient de réécrire.
  if (const std::optional<std::uint32_t> written = targeting_.TakeSelectionWrite();
      written.has_value()) {
    engagement.clicked_id = *written;
  }

  // L'ACTION DU PERSONNAGE D'ABORD : c'est sa fin qui fait le plus souvent
  // retomber l'engagement. Une compétence qui attend sa portée est l'action en
  // cours, d'où qu'elle soit partie — un clic, une touche, un cadre —, et elle
  // s'achève en partant ; un ramassage s'achève fait ou abandonné.
  const PlayerDeed deed = engagement.action.deed;
  if (const std::uint32_t aimed = grimoire_.pending_target_id(); aimed != 0) {
    if (deed != PlayerDeed::kSkillOnTarget || engagement.action.target_id != aimed) {
      NoteDeed(engagement, PlayerDeed::kSkillOnTarget, aimed, /*stops_on_release=*/false);
    }
  } else if (deed == PlayerDeed::kSkillOnTarget ||
             (deed == PlayerDeed::kPickup && !pickup_.has_value())) {
    EndDeed(engagement);
  }
  AdvanceDeed(engagement, world_);

  // LA MORT fait retomber et cache la flèche, une fois ; puis tout se fige.
  const ro::sim::Actor* self = world_.player();
  const bool dead = self != nullptr && self->posture == ro::sim::Posture::kDead;
  if (dead && !player_was_dead_) {
    EndDeed(engagement);
    engagement.death_freeze = true;
    PlaceSelectionMarker(UnderTargetingMode({MarkerOrder::kHide, nullptr}, hud_marked), now_ms);
  } else if (!dead && player_was_dead_) {
    engagement.death_freeze = false;
  }
  player_was_dead_ = dead;
  // UN COMMERCE OUVERT FIGE LA SOURIS, la boutique de vote comprise — et
  // l'écriture d'un courrier, pendant laquelle le serveur refuse la marche,
  // et le formulaire de salon : le salon naît sur la case du personnage.
  engagement.trade_freeze = pieces_.trade.HoldsWorld() || pieces_.cash_shop.shop().open() ||
                            pieces_.mail.HoldsWorld() || windows_.chat_room.make.open;

  const MouseRoute route = MouseRouteOf(world_, grimoire_, engagement, now_ms);
  if (route.runs()) {
    Settle(engagement, UnderMouse(), world_);
  } else {
    SkipSettle(engagement, PointerPx{mouse_.x, mouse_.y});
  }
  if (!route.frozen) {
    PlaceSelectionMarker(UnderTargetingMode(NativeMarkerCall(engagement, world_), hud_marked),
                         now_ms);
  }
  if (!marker_epoch_ms_.has_value()) {
    marker_epoch_ms_ = now_ms;
  }
  if (!marker_pose_.shown) {
    return;
  }
  const std::array<float, 3>& ground = marker_pose_.ground;
  // LA LUMIÈRE DE LA CARTE, prise sous la flèche et non sous sa cible : c'est là
  // qu'elle se pose.
  std::array<std::uint8_t, 4> tint = {255, 255, 255, 255};
  if (stage_.has_value() && settings_->options().graphics().lightmaps()) {
    const std::array<float, 2> cell =
        ro::render::FractionalCellAt(stage_->scene.gnd, ground[0], ground[2]);
    tint = ro::render::LitTint(tint, stage_->ground_light.Under(cell[0], cell[1]),
                               settings_->options().graphics().cold_light());
  }
  const std::span<const ro::render::DrawnActor> shown = selection_marker_.Drawn(
      pieces_.cursor, glm::vec3(ground[0], ground[1], ground[2]), now_ms - *marker_epoch_ms_, tint);
  // ⚠ NI LE DÉCOR NI UN CORPS NE LA CACHENT : elle se peint au plan proche, et
  // seuls les effets peints après elle peuvent la recouvrir.
  if (!shown.empty()) {
    sprites_.Draw(camera_, shown, aspect_ratio_, ro::render::SpriteRenderer::Blending::kOver,
                  ro::render::SpriteRenderer::DepthWrite::kOff,
                  ro::render::SpriteRenderer::DepthTest::kOff);
  }
}

}  // namespace moonclient
