#include "world/member_head.h"

#include "world/absent_styles.h"
#include "world/actor_styles.h"
#include "world/actor_view.h"

namespace moonclient {
namespace {

// Ce qu'une vignette de tête lit d'une apparence : le reste n'y paraît pas.
[[nodiscard]] ro::render::ActorLook HeadOnly(const ro::render::ActorLook& look) {
  ro::render::ActorLook head;
  head.player = true;
  head.job = look.job;
  head.sex = look.sex;
  head.hair = look.hair;
  head.hair_palette = look.hair_palette;
  head.style = look.style;
  head.gm_outfit = look.gm_outfit;
  head.orc_head = look.orc_head;
  return head;
}

[[nodiscard]] ro::render::ActorLook LookOfListed(const ro::sim::AbsentLook& listed) {
  ro::render::ActorLook look;
  look.player = true;
  look.job = listed.job;
  look.sex = listed.sex;
  look.hair = listed.hair;
  look.hair_palette = static_cast<std::int16_t>(listed.hair_color);
  return look;
}

}  // namespace

std::optional<ro::render::ActorLook> MemberHeadLook(
    const MemberHeadPieces& pieces, std::uint32_t account_id, std::uint32_t char_id,
    const std::optional<ro::sim::AbsentLook>& listed, std::uint32_t now_ms) {
  if (pieces.world == nullptr) {
    return std::nullopt;
  }
  const ro::sim::World& world = *pieces.world;
  const ro::sim::Actor* actor = world.Find(account_id);
  if (actor != nullptr && actor->kind == ro::sim::ActorKind::kPlayer) {
    ro::render::ActorLook look = LookOf(*actor);
    // 🔴 LA COIFFURE ET LA COULEUR QUE SA RECETTE IMPOSE, comme sur la carte.
    if (pieces.wardrobe != nullptr) {
      ImposeStyleNumbers(account_id, look, *pieces.wardrobe, world.styles());
    }
    // 🔴 LE STYLE QUE LA CARTE LUI A POSÉ : sa teinture de cheveux suit la coupe et
    // la couleur qu'il porte, et sa peau le suit partout.
    look.style = pieces.styles != nullptr ? pieces.styles->Worn(account_id) : 0;
    if (pieces.styles == nullptr) {
      look.gm_outfit = false;
      look.orc_head = false;
    }
    return HeadOnly(look);
  }
  if (pieces.absent != nullptr) {
    pieces.absent->Want(char_id, now_ms);
  }
  const ro::sim::AbsentStyle* absent = world.styles().FindAbsent(char_id);
  if (absent != nullptr && absent->shown) {
    ro::render::ActorLook look = LookOfListed(absent->look);
    if (pieces.styles != nullptr && pieces.wardrobe != nullptr) {
      pieces.styles->DressAbsent(char_id, look, *pieces.wardrobe, world.styles());
    }
    return HeadOnly(look);
  }
  if (listed.has_value()) {
    return HeadOnly(LookOfListed(*listed));
  }
  return std::nullopt;
}

}  // namespace moonclient
