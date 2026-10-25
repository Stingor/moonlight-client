// LE CURSEUR DE LA SOURIS : sa forme, décidée là où l'on sait ce qu'elle
// désigne, le rang du sort en main qui l'accompagne, et sa peinture, après
// toutes les fenêtres (`PaintCursor`).

#include <cstdint>
#include <optional>
#include <vector>

#include <SDL3/SDL.h>

#include "ro/render/actor_sprite.h"
#include "ro/sim/actor.h"
#include "ro/sim/skill_book.h"
#include "ro/sim/world.h"

#include "game/game.h"
#include "world/armed_cursor.h"
#include "world/click_action.h"
#include "world/ground_unit_view.h"
#include "world/overlay_view.h"
#include "world/signboard_pick.h"

namespace moonclient {

bool Game::CameraTurning() const {
  // Seule la rotation change le curseur ; incliner ou reculer gardent le survol.
  float x = 0.0F;
  float y = 0.0F;
  const SDL_MouseButtonFlags held = SDL_GetMouseState(&x, &y);
  const SDL_Keymod mods = SDL_GetModState();
  const auto turns = [&](MouseGesture gesture) {
    const std::optional<ActionRef> action = settings_->bindings().ActionForGesture(
        gesture, (mods & SDL_KMOD_CTRL) != 0, (mods & SDL_KMOD_SHIFT) != 0,
        (mods & SDL_KMOD_ALT) != 0);
    return action == ActionRef(GameAction::kCameraOrbit) ||
           action == ActionRef(GameAction::kCameraOrbitZoom);
  };
  return ((held & SDL_BUTTON_RMASK) != 0 && turns(MouseGesture::kRightDrag)) ||
         ((held & SDL_BUTTON_MMASK) != 0 && turns(MouseGesture::kMiddleDrag));
}

void Game::ComposeCursor(const ro::sim::Actor* designated,
                         const std::optional<ro::sim::CellPosition>& hovered_cell,
                         std::uint32_t now_ms) {
  // ⚠ LA VEILLE PEUT L'ÉTEINDRE, et elle le fait en ne le composant pas : on
  // vise avec le curseur, et il n'y a plus rien à viser. Il revient dès le
  // réveil, sans attendre que la caméra se soit reposée.
  if (afk_screen_.hiding_cursor(settings_->options().afk())) {
    cursor_quads_.clear();
    return;
  }
  // La forme du curseur se décide ICI, où l'on sait ce que la souris désigne ;
  // sa peinture attend l'interface.
  if (pieces_.cursor.sprite != nullptr) {
    // AU-DESSUS D'UN PANNEAU, la flèche NEUTRE. Les autres formes annoncent ce
    // qu'un clic ferait au monde, et le clic n'ira pas au monde : les garder
    // promettrait un coup qui ne partira jamais.
    //
    // ⚠ SAUF AU-DESSUS DE CE QUI SE CLIQUE, où c'est la main qui pointe. Sur un
    // panneau du jeu, un bouton ne s'annonce ni par un cadre qui s'allume ni par
    // un soulignement : le curseur est le seul indice qu'une chose répondra.
    //
    // 🔴 ET UNE COMPÉTENCE ARMÉE PREND TOUT LE RESTE. Lancer se fait en deux
    // temps — choisir, puis désigner —, et entre les deux le clic ne fait plus
    // ni marcher ni frapper. Rien à l'écran ne le disait : le curseur gardait la
    // forme de ce qu'il survolait, et promettait donc un geste que l'armement
    // venait justement de détourner.
    //
    // ⚠ DEUX FORMES ET NON UNE, comme au client : l'anneau qui tourne tant
    // qu'on cherche, le même deux fois plus vif sur une cible atteignable. Ce
    // sont ses actions 10 et 11 ; voir ArmedCursor.
    const ro::ui::InputGate gate = interface_->gate();
    // 🔴 SON PROPRE PERSONNAGE NE SE SURVOLE PAS, comme au client d'origine dont
    // la première ligne de survol compare le GID désigné à celui du compte. Le
    // clic l'écarte déjà (`LeftClick`) ; sans la même règle ici, une porte PvP
    // ouverte montrait l'épée sur soi, alors qu'on ne peut pas s'attaquer. La
    // compétence armée, elle, le garde : un soin se lance sur soi.
    //
    // ⚠ SOUS UNE SOURIS FIGÉE, AUCUNE ENTITÉ NE CHANGE LA FORME : le clic ne
    // l'atteindra pas. Le sol décide seul, et la compétence armée garde son
    // anneau lent. Voir `WorldTakesClicks`.
    const bool world_takes_clicks =
        WorldTakesClicks(MouseRouteOf(world_, grimoire_, world_mouse_.engagement, now_ms));
    if (!world_takes_clicks) {
      designated = nullptr;
    }
    const ro::sim::Actor* hovered_other =
        designated != nullptr && designated->id == world_.player_id() ? nullptr : designated;
    // Un cadre du HUD qui agit comme la cible en a aussi l'air : l'épée.
    const ro::render::CursorShape over_interface =
        target_hud_hovered_ ? ro::render::CursorShape::kAttack
        : gate.pointing     ? ro::render::CursorShape::kPoint
                            : ro::render::CursorShape::kArrow;
    // UNE UNITÉ AU SOL QUI PASSE DEVANT L'ACTEUR DÉSIGNÉ décide seule : l'épée
    // sur ce qui se frappe — un bloc d'Ice Wall, un piège en siège —, la flèche
    // sur le reste. Voir `GroundUnitOutranks`.
    const std::optional<std::uint32_t> unit_id =
        PickGroundUnit(camera_, aspect_ratio_, ground_units_->picks(), mouse_.x, mouse_.y,
                       scene_actors_.ground_unit_floor_pixels, static_cast<float>(shell_.width()),
                       static_cast<float>(shell_.height()));
    // ⚠ SOUS UNE VISÉE DE PIÈGE, TOUTE UNITÉ MONTRE L'ANNEAU LENT, épée ou non :
    // c'est elle que le clic désignera.
    const std::optional<ro::render::CursorShape> on_unit =
        world_takes_clicks && unit_id.has_value() && GroundUnitOutranks(hovered_other)
            ? std::optional(
                  grimoire_.aim() == SkillAim::kTrap ? ro::render::CursorShape::kGroundTarget
                  : GroundUnitShowsSword(world_.skill_units().at(*unit_id).unit_id,
                                         SiegeMap(world_mouse_.engagement, world_.map_mode()))
                      ? ro::render::CursorShape::kAttack
                      : ro::render::CursorShape::kArrow)
            : std::nullopt;
    // UNE ENSEIGNE D'ÉCHOPPE, OU LE MARCHAND QUI LA PORTE, SE CLIQUE : la main
    // qui pointe. Les mêmes règles que le clic (`OpensStore`) : une compétence
    // armée garde la souris, et un marchand qui se frappe garde l'épée.
    // L'enseigne d'un salon aussi, dont le double-clic fait entrer — sauf celle
    // d'un salon sans place, étiquette qu'un script pose au-dessus d'un PNJ.
    const std::optional<SignboardHit> top_board =
        TopSignboardAt(pieces_.trade.signboards(), mouse_);
    const ro::sim::Actor* room_host =
        top_board.has_value() && top_board->kind == ro::sim::SignboardKind::kChatRoom
            ? world_.Find(top_board->owner_id)
            : nullptr;
    const bool over_room =
        room_host != nullptr && room_host->signboard.has_value() && room_host->signboard->limit > 0;
    const bool shift_held = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
    const bool over_store =
        grimoire_.aim() == SkillAim::kNothing &&
        (over_room || pieces_.trade.SignboardAt(mouse_).has_value() ||
         (hovered_other != nullptr && hovered_other->signboard.has_value() &&
          SignboardOpensStore(hovered_other->signboard->kind) &&
          !ActorTakesClick(*hovered_other,
                           VersusGateOf(world_, shift_held, settings_->options().no_shift()))));
    const SkillAim aim =
        grimoire_.aim() != SkillAim::kNothing ? grimoire_.aim() : pieces_.companion_ai.cursor_aim();
    // L'ÉPÉE TANT QU'UN ENGAGEMENT QUI NE SE POURSUIT PAS EST TENU, quoi que la
    // souris désigne : voir `EngagementShowsSword`. Une compétence armée garde
    // son anneau.
    const bool held_sword = world_takes_clicks && aim == SkillAim::kNothing &&
                            EngagementShowsSword(world_mouse_.engagement);
    ro::render::CursorShape over_actor = ShapeUnderShift(
        CursorShapeOver(hovered_other, GroundHoverOn((*stage_), hovered_cell),
                        VersusGateOf(world_, shift_held, settings_->options().no_shift())),
        hovered_other, shift_held, settings_->options().no_shift());
    // UNE COMPÉTENCE VISÉE SUR UN JOUEUR OU UN MONSTRE : l'anneau vif là où le
    // clic la lancerait, le lent ailleurs — voir `AimedSkillReachesActor`.
    if (const ro::sim::KnownSkill* aimed = grimoire_.armed();
        aimed != nullptr && grimoire_.aim() == SkillAim::kTarget && hovered_other != nullptr &&
        (hovered_other->kind == ro::sim::ActorKind::kPlayer ||
         hovered_other->kind == ro::sim::ActorKind::kMonster) &&
        hovered_other->posture != ro::sim::Posture::kDead) {
      // ⚠ ALT NE CHANGE PAS LA FORME : seul le clic le lit.
      over_actor = AimedSkillReachesActor(world_, *hovered_other, *aimed, world_mouse_.ties,
                                          AimKeys{.shift_held = shift_held,
                                                  .no_shift = settings_->options().no_shift(),
                                                  .alt_held = false},
                                          SiegeMap(world_mouse_.engagement, world_.map_mode()),
                                          /*for_cursor=*/true)
                       ? ro::render::CursorShape::kAttack
                       : ro::render::CursorShape::kArrow;
    }
    // LE MODE ÉDITEUR DE CARTE garde la flèche sur le monde : un clic y désigne
    // un modèle, et rien de ce que la souris survole n'y répondrait comme au jeu.
    const ro::render::CursorShape shape =
        gate.mouse_to_ui ? over_interface
        : map_editor_.active()
            ? (CameraTurning() ? ro::render::CursorShape::kRotate : ro::render::CursorShape::kArrow)
            : ArmedCursor(aim, designated,
                          // ⚠ MAJ SE RELIT À CHAQUE IMAGE ICI, et non à l'appui :
                          // presser la touche change la forme du curseur sans
                          // qu'on bouge la souris, et c'est ainsi qu'on voit
                          // qu'une cible est devenue atteignable avant de
                          // cliquer. Le clic, lui, relève l'état de son APPUI.
                          CameraTurning()       ? ro::render::CursorShape::kRotate
                          : held_sword          ? ro::render::CursorShape::kAttack
                          : over_store          ? ro::render::CursorShape::kPoint
                          : on_unit.has_value() ? *on_unit
                                                : over_actor);
    if (shape != cursor_shape_) {
      cursor_shape_ = shape;
      cursor_since_ms_ = now_ms;
    }
    cursor_quads_ =
        DrawnCursor(pieces_.cursor, cursor_shape_, mouse_,
                    static_cast<float>(now_ms - cursor_since_ms_), world_mouse_.pressed);
    // LE RANG DU SORT EN MAIN, PAR-DESSUS LE CURSEUR : c'est là que le client
    // le met, et c'est le seul endroit où la molette se voit agir.
    //
    // ⚠ DANS LES MÊMES QUADS, à la suite : ils se dessinent en un seul envoi,
    // tout en dernier, et l'ordre du vecteur est celui de la peinture.
    const ro::sim::KnownSkill* armed = grimoire_.armed();
    if (armed != nullptr && !pieces_.hit_sprites.empty()) {
      const ro::render::DigitSprite digits{&pieces_.hit_sprites.front().sprite,
                                           &pieces_.hit_sprites.front().animation};
      const std::vector<ro::render::ScreenQuad> level =
          DrawnArmedLevel(digits, settings_->options().armed_skill(), armed->level, mouse_);
      cursor_quads_.insert(cursor_quads_.end(), level.begin(), level.end());
    }
  }
}

// ET LE CURSEUR APRÈS TOUT, y compris après les fenêtres. Il n'y en a qu'un dans
// ce client, et c'est celui-ci.
void Game::PaintCursor() {
  if (cursor_quads_.empty()) {
    return;
  }
  pieces_.overlay.Draw(cursor_quads_, static_cast<float>(shell_.width()),
                       static_cast<float>(shell_.height()));
  cursor_quads_.clear();
}

}  // namespace moonclient
