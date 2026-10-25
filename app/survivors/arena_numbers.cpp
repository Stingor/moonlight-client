#include "survivors/arena_numbers.h"

#include <algorithm>
#include <array>
#include <string>
#include <utility>

#include "ro/render/damage_mark.h"
#include "ro/render/number_look.h"

namespace moonclient {
namespace {

// Tout coup sur un monstre jaillit, comme en partie.
constexpr ro::render::NumberStyle kArenaNumberStyle = ro::render::NumberStyle::kBursting;
// L'efficacité d'une blessure du héros : elle n'en a pas, sa couleur est fixe.
constexpr std::int32_t kHurtEfficiencyPct = 100;

constexpr std::size_t kDigitsSprite = 0;
constexpr std::size_t kMarkSprite = 1;

[[nodiscard]] bool Expired(const ArenaNumber& number, std::uint64_t now_ms) {
  const auto elapsed_ms = static_cast<float>(now_ms - number.born_ms);
  if (number.amount == 0) return elapsed_ms >= ro::render::kMissLifeTicks * ro::render::kMarkTickMs;
  return ro::render::FloatingNumberExpired(kArenaNumberStyle, elapsed_ms);
}

}  // namespace

ArenaNumberStyle ArenaNumberStyleOf(std::int32_t amount, std::int32_t efficiency_pct,
                                    bool hero_hurt) noexcept {
  if (amount == 0) return ArenaNumberStyle{.miss = true};
  if (hero_hurt) return ArenaNumberStyle{.color = kHeroHurtColor, .scale = kPlainHitScale};
  if (efficiency_pct >= kStrongHitFromPct) {
    return ArenaNumberStyle{.color = kStrongHitColor, .scale = kStrongHitScale};
  }
  if (efficiency_pct <= kWeakHitUpToPct) {
    return ArenaNumberStyle{.color = kWeakHitColor, .scale = kWeakHitScale};
  }
  return ArenaNumberStyle{.color = kPlainHitColor, .scale = kPlainHitScale};
}

void ArenaNumberPool::Add(std::uint32_t foe_serial, bool hero_hurt, ro::sim::FieldPoint at,
                          std::int32_t amount, std::int32_t efficiency_pct, std::uint64_t now_ms) {
  // LA FUSION : un nombre encore jeune de la même cible reçoit le coup.
  for (ArenaNumber& number : live_) {
    if (number.foe_serial == foe_serial && number.hero_hurt == hero_hurt &&
        now_ms - number.born_ms < kArenaNumberMergeMs) {
      number.amount += amount;
      number.efficiency_pct = std::max(number.efficiency_pct, efficiency_pct);
      number.at = at;
      return;
    }
  }
  const ArenaNumber fresh{.foe_serial = foe_serial,
                          .hero_hurt = hero_hurt,
                          .at = at,
                          .amount = amount,
                          .efficiency_pct = efficiency_pct,
                          .born_ms = now_ms};
  if (live_.size() < kMostArenaNumbers) {
    live_.push_back(fresh);
    return;
  }
  // LE PLUS PETIT NOMBRE DE MONSTRE CÈDE, le nouveau compris : un coup porté
  // plus faible que tous ceux qui montent déjà ne paraît pas.
  //
  // 🔴 UN COUP ENCAISSÉ PARAÎT TOUJOURS, et ne cède jamais à un coup porté :
  // c'est ce que le joueur doit voir. Il ne cède qu'à un autre coup encaissé,
  // quand la réserve n'en tient plus que de tels.
  const auto yielding =
      std::ranges::min_element(live_, [](const ArenaNumber& left, const ArenaNumber& right) {
        if (left.hero_hurt != right.hero_hurt) return !left.hero_hurt;
        return left.amount < right.amount;
      });
  if (fresh.hero_hurt) {
    if (!yielding->hero_hurt || yielding->amount < fresh.amount) {
      *yielding = fresh;
    }
    return;
  }
  if (!yielding->hero_hurt && yielding->amount < fresh.amount) {
    *yielding = fresh;
  }
}

void ArenaNumberPool::Expire(std::uint64_t now_ms) {
  std::erase_if(live_, [now_ms](const ArenaNumber& number) { return Expired(number, now_ms); });
}

ro::core::Result<void> ArenaNumbers::Open(ro::vfs::Vfs& vfs) {
  const std::array<ro::render::ActorPieceRequest, 2> request = {
      ro::render::ActorPieceRequest{ro::render::ActorPart::kBody,
                                    std::string(ro::render::kDigitsSpritePath)},
      ro::render::ActorPieceRequest{ro::render::ActorPart::kBody,
                                    std::string(ro::render::kDamageMarkSpritePath)}};
  auto loaded = ro::render::LoadActorSprites(vfs, request);
  if (!loaded) return std::unexpected(loaded.error());
  sprites_ = std::move(*loaded);
  return {};
}

void ArenaNumbers::Notice(const ro::sim::SurvivorsNews& news, const ro::sim::SurvivorsField& field,
                          std::uint64_t now_ms) {
  for (const ro::sim::SurvivorsHit& hit : news.hits) {
    pool_.Add(hit.foe_serial, false, hit.at, hit.amount, hit.efficiency_pct, now_ms);
  }
  if (news.hero_hurt > 0) {
    pool_.Add(0, true, field.hero.position, news.hero_hurt, kHurtEfficiencyPct, now_ms);
  }
}

void ArenaNumbers::Restart() noexcept {
  pool_.Clear();
  drawn_.clear();
}

void ArenaNumbers::Compose(std::uint64_t now_ms, const GroundOf& ground_of) {
  pool_.Expire(now_ms);
  drawn_.clear();
  if (sprites_.empty()) return;
  const ro::render::DigitSprite digits{&sprites_[kDigitsSprite].sprite,
                                       &sprites_[kDigitsSprite].animation};
  const bool has_mark = sprites_.size() > kMarkSprite;
  // Les corps d'abord, les vues ensuite : un tableau qui grandit déplace ce
  // qu'il porte.
  composed_.resize(pool_.live().size());
  for (std::size_t index = 0; index < pool_.live().size(); ++index) {
    const ArenaNumber& number = pool_.live()[index];
    const ArenaNumberStyle style =
        ArenaNumberStyleOf(number.amount, number.efficiency_pct, number.hero_hurt);
    const auto elapsed_ms = static_cast<float>(now_ms - number.born_ms);
    const ro::render::NumberLook look{.scale = style.scale};
    if (style.miss) {
      composed_[index] = has_mark
                             ? ro::render::ComposeDamageMark(
                                   sprites_[kMarkSprite].sprite, sprites_[kMarkSprite].animation,
                                   ro::render::DamageMark::kMiss, style.color, elapsed_ms, look)
                             : ro::render::ComposedActor{};
    } else {
      composed_[index] = ro::render::ComposeFloatingNumber(
          digits,
          ro::render::FloatingNumber{.value = static_cast<std::uint32_t>(number.amount),
                                     .color = style.color,
                                     .style = kArenaNumberStyle},
          elapsed_ms, look);
    }
  }
  for (std::size_t index = 0; index < composed_.size(); ++index) {
    drawn_.push_back(ro::render::DrawnActor{.actor = &composed_[index],
                                            .ground_position = ground_of(pool_.live()[index].at)});
  }
}

void ArenaNumbers::Draw(ro::render::SpriteRenderer& sprites, const ro::render::Camera& camera,
                        float aspect_ratio) const {
  if (drawn_.empty()) return;
  sprites.Draw(camera, drawn_, aspect_ratio, ro::render::SpriteRenderer::Blending::kOver,
               ro::render::SpriteRenderer::DepthWrite::kOff,
               ro::render::SpriteRenderer::DepthTest::kOff);
}

}  // namespace moonclient
