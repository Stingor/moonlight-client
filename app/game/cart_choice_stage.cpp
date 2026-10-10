#include "game/cart_choice_stage.h"

#include <imgui.h>

#include "ro/core/log.h"
#include "ro/formats/act.h"
#include "ro/formats/spr.h"
#include "ro/net/cart_choice.h"
#include "ro/render/companion_cart.h"

namespace moonclient {

const ro::render::GlTexture* CartChoiceStage::ImageFor(ro::vfs::Vfs& vfs, std::uint8_t cart_type) {
  if (const auto found = images_.find(cart_type); found != images_.end()) {
    return &found->second;
  }
  ro::render::GlTexture texture;
  if (const std::optional<ro::render::CartArt> art = ro::render::CartListArtFor(cart_type)) {
    auto spr_bytes = vfs.Read(std::string(art->spr_path));
    auto act_bytes = vfs.Read(std::string(art->act_path));
    if (spr_bytes && act_bytes) {
      auto sprite = ro::formats::SprSprite::Parse(*spr_bytes);
      auto animation = ro::formats::ActAnimation::Parse(*act_bytes);
      if (sprite && animation) {
        if (auto uploaded =
                ro::render::GlTexture::Upload(ro::render::CartButtonImage(*sprite, *animation))) {
          texture = std::move(*uploaded);
        }
      }
    }
    if (!texture.valid()) {
      ro::core::LogWarning("liste des charrettes : {} illisible — son bouton restera vide",
                           art->spr_path);
    }
  }
  return &images_.emplace(cart_type, std::move(texture)).first->second;
}

void CartChoiceStage::Stage(ro::vfs::Vfs* vfs, const ro::vfs::ItemWords& words,
                            ro::ui::CartChoiceWindowState& state, ro::ui::CartChoiceView& view) {
  state.open = choice_.open();
  shown_.clear();
  if (!choice_.open()) {
    view = ro::ui::CartChoiceView{};
    return;
  }
  const std::vector<std::uint8_t>& types = choice_.offer().cart_types;
  for (const std::uint8_t type : types) {
    const ro::render::GlTexture* texture = vfs != nullptr ? ImageFor(*vfs, type) : nullptr;
    if (texture == nullptr || !texture->valid()) {
      shown_.emplace_back();
      continue;
    }
    shown_.push_back(
        ro::ui::SkinImage{.id = static_cast<ImTextureID>(texture->name()),
                          .width = static_cast<float>(ro::render::kCartButtonSidePixels),
                          .height = static_cast<float>(ro::render::kCartButtonSidePixels)});
  }
  view = ro::ui::CartChoiceView{
      .title = ro::vfs::CartListTitle(words), .types = types, .images = shown_};
}

void CartChoiceStage::Apply(const ro::ui::CartChoiceWindowAction& asked,
                            ro::net::Connection& link) {
  if (asked.chosen.has_value()) {
    if (auto sent = ro::net::RequestCartChoice(link, choice_.offer().identity, *asked.chosen);
        !sent) {
      ro::core::LogWarning("liste des charrettes : choix non émis — {}",
                           ro::core::Describe(sent.error()));
    }
    choice_.Close();
    return;
  }
  if (asked.closed) {
    choice_.Close();
  }
}

}  // namespace moonclient
