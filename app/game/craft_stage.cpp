#include "game/craft_stage.h"

#include <algorithm>
#include <span>

#include "ro/net/craft_session.h"
#include "ro/sim/world.h"

namespace moonclient {

void CraftStage::See(const ro::vfs::ItemTable* items, const ro::vfs::ItemWords* words,
                     ro::render::ItemIcons* icons, const CraftBook* book, CreatorNames creators) {
  make_.emplace(items, words, icons);
  make_->SeeBook(book);
  refine_.emplace(items, words, icons);
  refine_->SeeBook(book);
  refine_->SeeCreators(creators);
}

void CraftStage::Watch(ro::net::Connection& link, ro::sim::World& world) {
  link.set_send_observer([this, &world](std::span<const std::uint8_t> sent) {
    ro::net::ObserveCraftTrigger(sent, world, now_ms_);
  });
}

void CraftStage::Stage(ro::sim::World& world, ro::net::Connection& link,
                       ro::ui::MakeItemWindowState& make_state,
                       ro::ui::WeaponRefineWindowState& refine_state,
                       const MakeItemSettings& make_settings,
                       const WeaponRefineSettings& refine_settings, std::uint32_t now_ms) {
  now_ms_ = now_ms;
  if (!make_.has_value() || !refine_.has_value()) {
    return;
  }
  // 🔴 UNE LISTE DE FABRICATION REFERME L'AFFINAGE : le serveur ne garde qu'un
  // skill en attente, et un affinage armé viderait la liste qui arrive.
  if (make_->Refresh(world, make_state, make_settings, now_ms)) {
    refine_->CloseForOtherCraft(refine_state, link);
  }
  refine_->Refresh(world, refine_state, refine_settings, now_ms);
}

CraftGestures CraftStage::Apply(const ro::ui::MakeItemWindowAction& make_asked,
                                const ro::ui::WeaponRefineWindowAction& refine_asked,
                                ro::ui::MakeItemWindowState& make_state,
                                ro::ui::WeaponRefineWindowState& refine_state,
                                const MakeItemSettings& make_settings,
                                const WeaponRefineSettings& refine_settings,
                                ro::net::Connection& link, ro::sim::World& world,
                                std::uint32_t now_ms) {
  CraftGestures gestures;
  if (!make_.has_value() || !refine_.has_value()) {
    return gestures;
  }
  // ── LA FABRICATION ──────────────────────────────────────────────────────────
  ro::ui::MakeItemWindowAction make = make_asked;
  // UNE FENÊTRE REFERMÉE PAR ÉCHAP répond comme par sa croix.
  if (make_was_open_ && !make_state.open && make.verb == ro::ui::MakeItemVerb::kNothing) {
    make_state.open = true;
    make.verb = ro::ui::MakeItemVerb::kClose;
  }
  make_->Apply(make, make_state, make_settings, link, world, now_ms);
  make_was_open_ = make_state.open;
  if (make.verb == ro::ui::MakeItemVerb::kOpenAtlas) {
    gestures.open_atlas = true;
    gestures.atlas_item_id = make_state.selected;
  }
  gestures.open_make_settings = make.open_settings;
  if (make.describe_item_id != 0) {
    gestures.describe_item_id = make.describe_item_id;
  }
  if (make.hovered_id != 0 && make_settings.hover_preview) {
    gestures.hovered_id = make.hovered_id;
  }

  // ── LE REFINE ───────────────────────────────────────────────────────────────
  ro::ui::WeaponRefineWindowAction refine = refine_asked;
  if (refine_was_open_ && !refine_state.open && refine.verb == ro::ui::WeaponRefineVerb::kNothing) {
    refine_state.open = true;
    refine.verb = ro::ui::WeaponRefineVerb::kClose;
  }
  refine_->Apply(refine, refine_state, refine_settings, link, world, now_ms);
  refine_was_open_ = refine_state.open;
  gestures.open_refine_settings = refine.open_settings;
  if (refine.verb == ro::ui::WeaponRefineVerb::kStartChain) {
    gestures.switch_auto_refine = true;
    gestures.auto_refine_on = true;
  } else if (refine.verb == ro::ui::WeaponRefineVerb::kStop && refine_settings.auto_refine) {
    gestures.switch_auto_refine = true;
    gestures.auto_refine_on = false;
  }
  if (refine.describe_slot > 0) {
    gestures.describe_slot = static_cast<std::uint16_t>(refine.describe_slot);
  } else if (refine.describe_item_id != 0) {
    gestures.describe_item_id = refine.describe_item_id;
  }
  if (refine.hovered_id != 0 && refine_settings.hover_preview) {
    gestures.hovered_id = refine.hovered_id;
    gestures.hovered_slot = static_cast<std::uint16_t>(std::max(refine.hovered_slot, 0));
  }
  return gestures;
}

}  // namespace moonclient
