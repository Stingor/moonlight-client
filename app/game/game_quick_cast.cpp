// QUICK CAST, BRANCHÉ SUR LA PARTIE : ce que la machine de `world/quick_cast.h`
// demande au jeu — la souris, le grimoire, la barre, le droit du compte —, et
// l'ordre dans lequel elle joue à chaque image.

#include <cstddef>
#include <cstdint>
#include <optional>

#include <SDL3/SDL.h>

#include "ro/render/actor_pick.h"

#include "game/game.h"
#include "world/click_action.h"
#include "world/map_stage.h"
#include "world/mouse_engagement.h"

namespace moonclient {
namespace {

// Le joueur incante : une relance attendrait de toute façon la fin du sort.
[[nodiscard]] bool PlayerCasting(const ro::sim::World& world, std::uint32_t now_ms) {
  const ro::sim::Actor* self = world.player();
  return self != nullptr && self->cast.has_value() && self->cast->duration_ms > 0 &&
         now_ms - self->cast->started_ms < self->cast->duration_ms;
}

// UNE ENTITÉ QU'UN SORT PEUT VISER À LA SOURIS : un joueur ou un monstre.
//
// ⚠ NI UN PNJ, NI UN FAMILIER, NI UN ITEM AU SOL : le serveur refuse tout sort
// sur eux, et laisser la souris répondre « ce PNJ » volerait le sort à la cible
// du mode Ciblage, qui, elle, l'aurait fait partir.
//
// Un sort offensif ne vise jamais soi-même, ni un cadavre. Un soutien peut
// viser les deux : se soigner, relever un mort.
[[nodiscard]] bool SkillAimsAt(const ro::sim::Actor& actor, QuickCastAim aim,
                               std::uint32_t own_id) {
  if (actor.kind != ro::sim::ActorKind::kPlayer && actor.kind != ro::sim::ActorKind::kMonster) {
    return false;
  }
  if (TargetIsConcealed(actor)) {
    return false;
  }
  if (aim == QuickCastAim::kOffensive) {
    return actor.id != own_id && actor.posture != ro::sim::Posture::kDead;
  }
  return true;
}

[[nodiscard]] SkillAimKind AimKindOf(QuickCastAim aim) {
  return aim == QuickCastAim::kOffensive ? SkillAimKind::kOffensive : SkillAimKind::kSupport;
}

}  // namespace

void Game::AdvanceQuickCast(std::uint32_t now_ms) {
  // ⚠ UNE CLASSE LOCALE, parce qu'elle lit ce que seul `Game` possède : elle a
  // le même accès que la fonction qui la déclare, et ne vit que le temps d'une
  // image.
  class Hands final : public QuickCastHands {
   public:
    Hands(Game& game, std::uint32_t now_ms) : game_(game), now_ms_(now_ms) {}

    [[nodiscard]] bool InPlay() const override {
      return game_.link_.is_open() && game_.stage_.has_value() && game_.world_.player() != nullptr;
    }
    [[nodiscard]] bool HasFocus() const override {
      return SDL_GetKeyboardFocus() == game_.gl_->window();
    }
    [[nodiscard]] bool Typing() const override { return game_.interface_->gate().text_to_ui; }
    [[nodiscard]] bool Staff() const override { return game_.Rights().staff_tools; }
    [[nodiscard]] bool KeyHeld(std::uint32_t key) const override {
      int key_count = 0;
      const bool* keys = SDL_GetKeyboardState(&key_count);
      return key != 0 && keys != nullptr && static_cast<int>(key) < key_count && keys[key];
    }

    [[nodiscard]] std::optional<ro::sim::KnownSkill> Armed() const override {
      const ro::sim::KnownSkill* armed = game_.grimoire_.armed();
      return armed != nullptr ? std::optional<ro::sim::KnownSkill>(*armed) : std::nullopt;
    }
    void Disarm() override { game_.grimoire_.Disarm(); }
    [[nodiscard]] bool CoolingDown(const ro::sim::KnownSkill& skill) const override {
      return game_.world_.skill_book().CooldownRemainingMs(skill.id, now_ms_) > 0;
    }
    [[nodiscard]] bool Casting() const override { return PlayerCasting(game_.world_, now_ms_); }

    [[nodiscard]] bool MouseOnWorld() const override {
      // Une fenêtre sous le curseur, ou le panneau du grimoire : le joueur ne
      // désigne pas le monde qu'elle cache.
      return !game_.interface_->gate().mouse_to_ui &&
             !game_.grimoire_.Covers(game_.world_, game_.mouse_, Width(), Height(), now_ms_);
    }
    [[nodiscard]] std::optional<ro::sim::CellPosition> GroundUnderMouse() const override {
      const std::optional<ro::sim::CellPosition> cell =
          GroundCellAt(*game_.stage_, game_.camera_, game_.aspect_ratio_, game_.mouse_.x,
                       game_.mouse_.y, Width(), Height());
      // ⚠ LA MÊME RÈGLE QUE LE CLIC : seule une case infranchissable refuse, et
      // pas de carte n'est pas une case bloquée.
      const ro::sim::WalkMap* walk = PlannedMapOf(*game_.stage_);
      if (cell.has_value() && walk != nullptr &&
          !walk->Passable(static_cast<std::int32_t>(cell->x), static_cast<std::int32_t>(cell->y))) {
        return std::nullopt;
      }
      return cell;
    }
    [[nodiscard]] std::uint32_t ActorUnderMouse(QuickCastAim aim) const override {
      const ComposedScene& scene = game_.scene_actors_;
      const std::optional<std::size_t> picked =
          ro::render::PickActor(game_.camera_, game_.aspect_ratio_, scene.drawn, game_.mouse_.x,
                                game_.mouse_.y, Width(), Height());
      if (!picked.has_value() || *picked >= scene.ids.size()) {
        return 0;
      }
      const ro::sim::Actor* actor = game_.world_.Find(scene.ids[*picked]);
      return actor != nullptr && SkillAimsAt(*actor, aim, game_.world_.player_id()) ? actor->id : 0;
    }
    [[nodiscard]] bool HudCastArmed() const override { return game_.targeting_.CastsOnTarget(); }
    [[nodiscard]] std::uint32_t HudTarget(QuickCastAim aim) const override {
      return aim == QuickCastAim::kGround
                 ? 0
                 : game_.targeting_.SkillTargetId(game_.world_, AimKindOf(aim));
    }
    [[nodiscard]] bool TileCastArmed() const override {
      const ro::ui::PartyGridLook& grid = game_.settings_->options().pages().party.grid();
      return grid.shown && grid.cast_on_tile;
    }
    [[nodiscard]] std::uint32_t TileTarget() const override {
      if (!game_.party_panel_.has_value()) {
        return 0;
      }
      // L'acteur doit être là, moi compris : c'est lui que le sort vise.
      const ro::sim::Actor* actor = game_.world_.Find(game_.party_panel_->cast_account_id());
      return actor != nullptr &&
                     SkillAimsAt(*actor, QuickCastAim::kSupport, game_.world_.player_id())
                 ? actor->id
                 : 0;
    }

    bool CastOnGround(const ro::sim::KnownSkill& skill, ro::sim::CellPosition where) override {
      // ⚠ COMME SUR UN ACTEUR : l'attaque ou le ramassage d'avant tombe, sans
      // quoi deux poursuites enverraient chacune leur marche, vers deux cases.
      game_.pickup_.reset();
      game_.attack_.reset();
      game_.grimoire_.CastOnGround(skill, where);
      // Comme un clic : un sort au sol qui part achève l'action du personnage.
      EndDeed(game_.world_mouse_.engagement);
      return true;
    }
    bool CastOnActor(const ro::sim::KnownSkill& skill, std::uint32_t id) override {
      const ro::sim::Actor* target = game_.world_.Find(id);
      if (target == nullptr) {
        return false;
      }
      // Refusé — trop chargé, assis, caché —, rien ne part ; rendre vrai
      // désarme la visée comme après un lancer : voir `SkillOnActorPasses`.
      if (!SkillOnActorPasses(game_.world_, skill.id, now_ms_)) {
        return true;
      }
      // Comme un clic : le personnage n'a qu'une action en attente, et le sort
      // remplace l'attaque ou le ramassage d'avant.
      game_.pickup_.reset();
      game_.attack_.reset();
      game_.grimoire_.CastOn(skill, *target);
      return true;
    }
    bool RepeatItem(std::size_t slot, std::uint32_t item_id) override {
      return game_.shortcuts_.has_value() &&
             game_.shortcuts_->RepeatItemSlot(slot, item_id, game_.link_, game_.world_);
    }

   private:
    [[nodiscard]] float Width() const { return static_cast<float>(game_.shell_.width()); }
    [[nodiscard]] float Height() const { return static_cast<float>(game_.shell_.height()); }

    Game& game_;
    std::uint32_t now_ms_;
  };

  Hands hands(*this, now_ms);
  // Les armements viennent de TOUS les chemins — une touche, une case, une ligne
  // du panneau — et la machine sait, par la frappe retenue, lequel est le sien.
  QuickCastSignals seen{.arms = grimoire_.arms()};
  if (shortcuts_.has_value()) {
    seen.item_uses = shortcuts_->item_uses();
    seen.item_slot = shortcuts_->last_item_slot();
    seen.item_id = shortcuts_->last_item_id();
  }
  quick_cast_.Advance(hands, seen, now_ms);
}

}  // namespace moonclient
