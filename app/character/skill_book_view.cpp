#include "character/skill_book_view.h"

#include <cstddef>
#include <optional>
#include <span>
#include <string>

#include "ro/core/error.h"
#include "ro/core/i18n.h"
#include "ro/net/movement.h"
#include "ro/net/skill.h"
#include "ro/sim/cart_choice.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/skill_tree.h"
#include "ro/sim/walk_map.h"
#include "ro/sim/world.h"

#include "world/actor_carts.h"

namespace moonclient {
namespace {

using ro::sim::KnownSkill;

// Ce que le grimoire dit tout haut. Le locuteur est ZÉRO : cela ne vient de
// personne, comme les réponses du serveur à une commande.
void Say(ro::sim::World& world, std::string text, std::uint32_t now_ms) {
  world.RecordChat(ro::sim::ChatKind::kServer, 0, std::move(text), now_ms);
}

// Le nom d'une compétence, entre guillemets, tel qu'il s'écrit dans une phrase.
std::string Named(const KnownSkill& skill) {
  return "« " + (skill.name.empty() ? std::to_string(skill.id) : skill.name) + " »";
}

// Envoie, et dit ce qui s'est passé. Une demande qui ne part pas n'a qu'une
// cause — une disposition de paquet qui ne s'accorde plus avec le serveur — et
// elle doit se lire, pas se deviner.
void Sent(ro::sim::World& world, const ro::core::Result<void>& sent, const KnownSkill& skill,
          std::uint32_t now_ms) {
  if (sent) {
    Say(world, Named(skill) + " lancé", now_ms);
    return;
  }
  Say(world, Named(skill) + " n'est pas partie : " + ro::core::Describe(sent.error()), now_ms);
}

// Une MARCHE d'approche qui ne part pas. Muette quand elle part : on la voit.
void Refused(ro::sim::World& world, const ro::core::Result<void>& sent, const KnownSkill& skill,
             std::uint32_t now_ms) {
  if (!sent) {
    Say(world,
        Named(skill) + " : l'approche n'est pas partie : " + ro::core::Describe(sent.error()),
        now_ms);
  }
}

// Le titre du panneau, qui dit aussi ce qui ne tient pas dedans.
std::string TitleFor(std::size_t first, std::size_t shown, std::size_t total) {
  std::string title = "Grimoire";
  if (shown < total) {
    // La fenêtre est trop courte : dire OÙ l'on est dans la liste, sinon
    // défiler ne se voit nulle part.
    return title + " " + std::to_string(first + 1) + "-" + std::to_string(first + shown) + "/" +
           std::to_string(total);
  }
  return title + " (" + std::to_string(total) + ")";
}

// Ce qui s'écrit à droite d'une ligne : le rang, ce que lancer coûte, et le
// temps qu'il reste à ATTENDRE quand le serveur en a posé un.
//
// L'attente CHASSE le coût : les deux ne tiennent pas côte à côte, et savoir
// qu'une compétence n'est pas prête vaut mieux que savoir ce qu'elle coûterait.
// Elle s'arrondit à la seconde SUPÉRIEURE — montrer « 0s » pendant presque une
// seconde ferait croire à une compétence prête qui refuse.
std::string DetailFor(const KnownSkill& skill, std::uint32_t cooldown_remaining_ms) {
  std::string detail = "Nv" + std::to_string(skill.level);
  if (cooldown_remaining_ms != 0) {
    constexpr std::uint32_t kMsPerSecond = 1000;
    const std::uint32_t seconds = (cooldown_remaining_ms + kMsPerSecond - 1) / kMsPerSecond;
    return detail + " " + std::to_string(seconds) + "s";
  }
  if (skill.sp_cost != 0) {
    detail += " " + std::to_string(skill.sp_cost) + "PM";
  }
  return detail;
}

}  // namespace

void SkillBookPanel::Toggle() {
  open_ = !open_;
  if (!open_) {
    Disarm();
  }
}

SkillAim SkillBookPanel::aim() const {
  if (!armed_.has_value()) {
    return SkillAim::kNothing;
  }
  if (ro::sim::SkillNeedsGround(*armed_)) {
    return SkillAim::kGround;
  }
  return ro::sim::SkillTargetsTrap(*armed_) ? SkillAim::kTrap : SkillAim::kTarget;
}

const KnownSkill* SkillBookPanel::armed() const { return armed_.has_value() ? &*armed_ : nullptr; }

void SkillBookPanel::Rebuild(const ro::sim::World& world, std::uint32_t now_ms) {
  const std::span<const KnownSkill> known = world.skill_book().skills();
  labels_.clear();
  details_.clear();
  rows_.clear();
  labels_.reserve(known.size());
  details_.reserve(known.size());
  rows_.reserve(known.size());
  for (const KnownSkill& skill : known) {
    labels_.push_back(skill.name.empty() ? std::to_string(skill.id) : skill.name);
    details_.push_back(DetailFor(skill, world.skill_book().CooldownRemainingMs(skill.id, now_ms)));
  }
  // Les vues désignent les chaînes ci-dessus : elles ne se remplissent qu'une
  // fois les deux tableaux au complet, sans quoi un agrandissement les
  // laisserait pointer sur du vide.
  for (std::size_t index = 0; index < known.size(); ++index) {
    ro::render::SkillPanelRow row;
    row.label = labels_[index];
    row.detail = details_[index];
    row.armed = armed_.has_value() && armed_->id == known[index].id;
    row.passive = ro::sim::SkillIsPassive(known[index]);
    rows_.push_back(row);
  }
}

bool SkillBookPanel::ClickInPanel(ro::net::Connection& link, ro::sim::World& world,
                                  glm::vec2 mouse_px, float viewport_width, float viewport_height,
                                  std::uint32_t now_ms) {
  if (!open_) {
    return false;
  }
  Rebuild(world, now_ms);
  const ro::render::SkillPanelLayout layout =
      ro::render::SkillPanelPlacement(viewport_width, viewport_height, rows_.size());
  if (!ro::render::SkillPanelHolds(layout, mouse_px)) {
    return false;
  }
  const std::optional<std::size_t> row = ro::render::SkillPanelRowAt(layout, mouse_px);
  if (!row.has_value()) {
    // Le bandeau de titre : il retient le clic sans rien choisir, sinon
    // attraper le panneau ferait marcher le personnage derrière lui.
    return true;
  }
  const std::span<const KnownSkill> known = world.skill_book().skills();
  // Le panneau compte à partir de la première ligne MONTRÉE ; le grimoire, lui,
  // compte depuis le début. Confondre les deux ferait lancer une compétence
  // pour une autre dès qu'on a fait défiler.
  const std::size_t chosen_row = FirstRowWithin(layout) + *row;
  if (chosen_row >= known.size()) {
    return true;
  }
  ChooseSkill(link, world, known[chosen_row], now_ms);
  return true;
}

void SkillBookPanel::ChooseSkill(ro::net::Connection& link, ro::sim::World& world,
                                 const KnownSkill& chosen, std::uint32_t now_ms) {
  if (armed_.has_value() && armed_->id == chosen.id) {
    // La même compétence une seconde fois : on renonce.
    Say(world, Named(chosen) + " : abandonnée", now_ms);
    Disarm();
    return;
  }
  if (ro::sim::SkillIsPassive(chosen)) {
    Say(world, Named(chosen) + " est passive : rien à lancer", now_ms);
    Disarm();
    return;
  }
  if (ro::sim::SkillCastsOnSelf(chosen)) {
    // Rien à désigner : elle part tout de suite. La cible écrite est la nôtre,
    // et le serveur la remplacerait de toute façon par elle.
    Disarm();
    // 🔴 `MC_CHANGECART` OUVRE LA LISTE DES CHARRETTES, et ne part qu'avec elle.
    // Sans assez de points de magie, ou sans charrette, ni l'une ni l'autre : le
    // refus s'écrit, et rien ne part.
    if (chosen.id == ro::sim::kChangeCartSkill) {
      const ro::sim::Actor* self = world.Find(world.player_id());
      switch (ro::sim::ChangeCartRefusalFor(world.player_status().sp,
                                            self != nullptr ? CartTypeOf(*self) : 0)) {
        case ro::sim::ChangeCartRefusal::kNotEnoughSp:
          Say(world, std::string(ro::core::Tr("SP insuffisants.")), now_ms);
          return;
        case ro::sim::ChangeCartRefusal::kNoCart:
          Say(world, std::string(ro::core::Tr("Vous ne possédez pas de Cart.")), now_ms);
          return;
        case ro::sim::ChangeCartRefusal::kNone:
          cart_choice_asked_ = true;
          break;
      }
    }
    const ro::core::Result<void> sent =
        ro::net::RequestSkillOnTarget(link, chosen.id, chosen.level, world.player_id());
    if (sent) {
      ++entity_casts_;
    }
    Sent(world, sent, chosen, now_ms);
    return;
  }
  armed_ = chosen;
  ++arms_;
  const char* asked = ro::sim::SkillNeedsGround(chosen)   ? " : désignez une case"
                      : ro::sim::SkillTargetsTrap(chosen) ? " : désignez un piège"
                                                          : " : désignez une cible";
  Say(world, Named(chosen) + asked, now_ms);
}

bool SkillBookPanel::ReleaseOnActor(const ro::sim::Actor& target) {
  if (aim() != SkillAim::kTarget) {
    return false;
  }
  const KnownSkill chosen = *armed_;
  Disarm();
  CastOn(chosen, target);
  return true;
}

bool SkillBookPanel::ReleaseOnGroundUnit(const ro::sim::SkillUnit& unit) {
  if (aim() != SkillAim::kTrap) {
    return false;
  }
  const KnownSkill chosen = *armed_;
  Disarm();
  CastOnId(chosen, unit.id);
  return true;
}

void SkillBookPanel::CastOn(const KnownSkill& skill, const ro::sim::Actor& target) {
  CastOnId(skill, target.id);
}

void SkillBookPanel::CastOnId(const KnownSkill& skill, std::uint32_t target_id) {
  pending_skill_ = skill;
  ro::sim::PendingAction action;
  action.target_id = target_id;
  action.skill_id = skill.id;
  action.skill_level = skill.level;
  action.range_cells = skill.range_cells;
  pending_ = action;
  // RIEN NE PART ENCORE, et rien ne se dit : le tic mesurera la distance à la
  // prochaine image. Annoncer une visée noierait la conversation, et
  // l'approche, elle, se voit.
}

void SkillBookPanel::AdvancePending(ro::net::Connection& link, ro::sim::World& world,
                                    ro::sim::WalkPlanner& planner, const ro::sim::WalkMap* walk,
                                    std::uint32_t now_ms) {
  // LA COMPÉTENCE QU'UN OBJET PRÊTE s'arme comme si on l'avait choisie : voir
  // `ro::sim::SkillBook::OfferItemSkill`.
  if (std::optional<KnownSkill> offered = world.skill_book().TakeOfferedSkill()) {
    Disarm();
    ChooseSkill(link, world, *offered, now_ms);
  }
  // Sans grille de franchissement — le temps qu'une carte se charge — la case
  // d'approche ne se calcule pas, et marcher au hasard vaut moins qu'attendre.
  if (!pending_.has_value() || !pending_skill_.has_value() || walk == nullptr) {
    return;
  }
  const ro::sim::PendingMove move =
      ro::sim::AdvancePendingAction(world, planner, *walk, *pending_, approach_clock_, now_ms);
  switch (move.step) {
    case ro::sim::PendingStep::kWait:
      return;
    case ro::sim::PendingStep::kApproach:
      Refused(world, ro::net::RequestMove(link, move.toward), *pending_skill_, now_ms);
      return;
    case ro::sim::PendingStep::kRelease: {
      const KnownSkill chosen = *pending_skill_;
      const std::uint32_t target_id = pending_->target_id;
      const std::optional<ro::sim::CellPosition> cell = pending_->ground_cell;
      CancelPending();
      if (cell.has_value()) {
        Sent(world, ro::net::RequestSkillOnGround(link, chosen.id, chosen.level, *cell), chosen,
             now_ms);
        return;
      }
      const ro::core::Result<void> sent =
          ro::net::RequestSkillOnTarget(link, chosen.id, chosen.level, target_id);
      if (sent) {
        launched_target_ = target_id;
        ++entity_casts_;
      }
      Sent(world, sent, chosen, now_ms);
      return;
    }
    case ro::sim::PendingStep::kGiveUp: {
      const KnownSkill abandoned = *pending_skill_;
      CancelPending();
      Say(world, Named(abandoned) + " : la cible n'est plus là", now_ms);
      return;
    }
  }
}

bool SkillBookPanel::ReleaseOnGround(ro::sim::World& world, ro::sim::CellPosition where,
                                     std::uint32_t now_ms) {
  switch (aim()) {
    case SkillAim::kNothing:
      return false;
    case SkillAim::kTarget: {
      // Une compétence qui visait quelqu'un se désarme sur un clic au sol,
      // comme dans le jeu : le clic sert à renoncer, pas à marcher.
      const KnownSkill abandoned = *armed_;
      Disarm();
      Say(world, Named(abandoned) + " : abandonnée", now_ms);
      return true;
    }
    case SkillAim::kTrap:
      // ⚠ LA VISÉE D'UN PIÈGE SE DÉSARME ET LAISSE MARCHER : le client ne
      // retient le clic au sol que pour les visées d'une cible ou d'un soutien.
      Disarm();
      return false;
    case SkillAim::kGround: {
      const KnownSkill chosen = *armed_;
      Disarm();
      CastOnGround(chosen, where);
      return true;
    }
  }
  return false;
}

void SkillBookPanel::CastOnGround(const KnownSkill& skill, ro::sim::CellPosition where) {
  // 🔴 TALKIE BOX ET GRAFFITI NE PARTENT PAS ENCORE : le client ouvre d'abord
  // la fenêtre où l'on écrit son message, et c'est elle qui l'envoie, texte
  // compris. Sans texte, le serveur pose une unité muette.
  if (skill.id == ro::sim::kTalkieBoxSkill || skill.id == ro::sim::kGraffitiSkill) {
    talkbox_asked_ = GroundCast{.skill_id = skill.id, .skill_level = skill.level, .cell = where};
    return;
  }
  // RIEN NE PART ENCORE : le serveur ignore en silence une case hors de portée.
  // Le tic approche, puis lâche la demande au sol — voir
  // `PendingAction::ground_cell`.
  pending_skill_ = skill;
  ro::sim::PendingAction action;
  action.skill_id = skill.id;
  action.skill_level = skill.level;
  action.range_cells = skill.range_cells;
  action.ground_cell = where;
  pending_ = action;
}

bool SkillBookPanel::Covers(const ro::sim::World& world, glm::vec2 mouse_px, float viewport_width,
                            float viewport_height, std::uint32_t now_ms) {
  if (!open_) {
    return false;
  }
  Rebuild(world, now_ms);
  return ro::render::SkillPanelHolds(
      ro::render::SkillPanelPlacement(viewport_width, viewport_height, rows_.size()), mouse_px);
}

std::vector<ro::render::ScreenQuad> SkillBookPanel::Drawn(const ro::render::FontAtlas& font,
                                                          const ro::sim::World& world,
                                                          float viewport_width,
                                                          float viewport_height,
                                                          std::uint32_t now_ms) {
  if (!open_) {
    return {};
  }
  Rebuild(world, now_ms);
  const ro::render::SkillPanelLayout layout =
      ro::render::SkillPanelPlacement(viewport_width, viewport_height, rows_.size());
  first_row_ = FirstRowWithin(layout);
  title_ = TitleFor(first_row_, layout.rows_shown, rows_.size());
  return ro::render::ComposeSkillPanel(font, layout, title_, rows_, first_row_);
}

std::size_t SkillBookPanel::FirstRowWithin(const ro::render::SkillPanelLayout& layout) const {
  return ro::render::SkillPanelScrolled(layout, rows_.size(), first_row_, 0.0F);
}

bool SkillBookPanel::Scroll(const ro::sim::World& world, const ro::sim::SkillTree* tree,
                            glm::vec2 mouse_px, float wheel_steps, float viewport_width,
                            float viewport_height, std::uint32_t now_ms) {
  if (open_) {
    Rebuild(world, now_ms);
    const ro::render::SkillPanelLayout layout =
        ro::render::SkillPanelPlacement(viewport_width, viewport_height, rows_.size());
    if (ro::render::SkillPanelHolds(layout, mouse_px)) {
      first_row_ = ro::render::SkillPanelScrolled(layout, rows_.size(), first_row_, wheel_steps);
      // Prise même quand la liste ne peut plus bouger : reculer la caméra parce
      // que le grimoire est au bout serait une surprise.
      return true;
    }
  }
  // Hors du panneau : le RANG de ce qui attend sa cible.
  if (!armed_.has_value()) {
    return false;
  }
  // Le plafond est le rang APPRIS, et il se relit dans le livre : `armed_` porte
  // le rang auquel on veut lancer, qui peut être plus bas.
  const KnownSkill* known = world.skill_book().Find(armed_->id);
  const ro::sim::SkillNode* node = tree != nullptr ? tree->Find(armed_->id) : nullptr;
  armed_->level = ArmedLevelAfterWheel(armed_->level, known != nullptr ? known->level : 0,
                                       node != nullptr && node->level_matters, wheel_steps);
  // Prise même quand le rang ne bouge pas : voir l'en-tête.
  return true;
}

}  // namespace moonclient
