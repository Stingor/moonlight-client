#include "world/broadcast_balloons.h"

#include <algorithm>
#include <optional>
#include <span>
#include <string>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/sim/world.h"

namespace moonclient {
namespace {

using ro::sim::BalloonFamily;
using ro::sim::BalloonNotice;

// Le temps écoulé depuis la réception ; zéro pour une bulle « du futur », que
// seule une horloge qui recule peut produire.
[[nodiscard]] std::uint32_t ElapsedMs(std::uint32_t heard_ms, std::uint32_t now_ms) {
  return now_ms >= heard_ms ? now_ms - heard_ms : 0;
}

// `0xRRGGBB` en couleur pleine d'ImGui.
[[nodiscard]] ImU32 OpaqueColor(std::uint32_t rgb) {
  constexpr unsigned kRedShift = 16;
  constexpr unsigned kGreenShift = 8;
  constexpr std::uint32_t kChannel = 0xFF;
  return IM_COL32((rgb >> kRedShift) & kChannel, (rgb >> kGreenShift) & kChannel, rgb & kChannel,
                  kChannel);
}

// Le jaune des annonces du client, pour les bulles d'exemple.
constexpr std::uint32_t kPreviewColor = 0xFFFF00;

// UNE BULLE À PEINDRE, avant que ses vues ne soient prises.
struct Planned {
  const BalloonNotice* notice = nullptr;
  std::size_t first_line = 0;
  float alpha = 1.0F;
  bool hunt = false;
};

}  // namespace

std::size_t MostBalloonsOf(BalloonFamily family, const ro::ui::BroadcastBalloonLook& look) {
  if (family == BalloonFamily::kHunt) {
    return 1;
  }
  return static_cast<std::size_t>(std::clamp(look.most_per_family, ro::ui::kLeastBalloonsPerFamily,
                                             ro::ui::kMostBalloonsPerFamily));
}

bool BalloonExpired(std::uint32_t heard_ms, std::uint32_t now_ms,
                    const ro::ui::BroadcastBalloonLook& look) {
  const int duration_ms =
      std::clamp(look.duration_ms, ro::ui::kShortestBalloonMs, ro::ui::kLongestBalloonMs);
  return ElapsedMs(heard_ms, now_ms) >= static_cast<std::uint32_t>(duration_ms);
}

float BalloonAlpha(std::uint32_t heard_ms, std::uint32_t now_ms,
                   const ro::ui::BroadcastBalloonLook& look) {
  if (!look.fade || look.fade_ms <= 0) {
    return 1.0F;
  }
  const int duration_ms =
      std::clamp(look.duration_ms, ro::ui::kShortestBalloonMs, ro::ui::kLongestBalloonMs);
  const auto elapsed = static_cast<float>(ElapsedMs(heard_ms, now_ms));
  const float left_ms = std::max(0.0F, static_cast<float>(duration_ms) - elapsed);
  // ⚠ LE FONDU NE MANGE PAS TOUTE LA VIE : plus long qu'elle, il commencerait
  // dès l'apparition, et une bulle qui pâlit en naissant se lit comme un défaut.
  const float fade_ms = static_cast<float>(
      std::min(std::min(look.fade_ms, ro::ui::kLongestBalloonFadeMs), duration_ms / 2));
  return fade_ms <= 0.0F ? 1.0F : std::min(1.0F, left_ms / fade_ms);
}

void BroadcastBalloons::Notice(ro::sim::World& world, const ro::ui::BroadcastBalloonLook& look) {
  // ⚠ PRISES MÊME ÉTEINTES : la file se vide, sinon une bulle de l'heure
  // passée paraîtrait au rallumage.
  std::vector<BalloonNotice> arrived = world.balloon_news().Take();
  if (!look.shown) {
    return;
  }
  for (BalloonNotice& notice : arrived) {
    const auto family = static_cast<std::size_t>(notice.family);
    if (family >= held_.size()) {
      continue;
    }
    std::deque<BalloonNotice>& held = held_[family];
    held.push_back(std::move(notice));
    // LA PLUS ANCIENNE PART pour la nouvelle, comme chez le client.
    while (held.size() > MostBalloonsOf(held.back().family, look)) {
      held.pop_front();
    }
  }
}

void BroadcastBalloons::Compose(std::uint32_t now_ms, const ro::ui::BroadcastBalloonLook& look,
                                const BalloonDecor& decor, bool preview_announcements,
                                bool preview_hunts) {
  decorated_.clear();
  line_views_.clear();
  announcement_views_.clear();
  hunt_views_.clear();
  for (std::deque<BalloonNotice>& held : held_) {
    std::erase_if(held, [&](const BalloonNotice& notice) {
      return !look.shown || BalloonExpired(notice.heard_ms, now_ms, look);
    });
    // Un plafond baissé pendant que des bulles sont là s'applique tout de suite.
    while (!held.empty() && held.size() > MostBalloonsOf(held.front().family, look)) {
      held.pop_front();
    }
  }

  // LES EXEMPLES, quand une ancre se place sur un bandeau vide. ⚠ GARDÉS DANS
  // LA CLASSE : leur texte doit survivre à l'image.
  previews_.clear();
  if (preview_announcements &&
      held_[static_cast<std::size_t>(BalloonFamily::kAnnouncement)].empty() &&
      held_[static_cast<std::size_t>(BalloonFamily::kStaffWhisper)].empty()) {
    BalloonNotice& example = previews_.emplace_back();
    example.family = BalloonFamily::kAnnouncement;
    example.lines.push_back(ro::sim::BalloonLine{
        .text = ro::core::Tr("Exemple d'annonce : glissez la poignée pour placer le bandeau."),
        .color = kPreviewColor});
  }
  if (preview_hunts && held_[static_cast<std::size_t>(BalloonFamily::kHunt)].empty()) {
    BalloonNotice& example = previews_.emplace_back();
    example.family = BalloonFamily::kHunt;
    example.lines.push_back(ro::sim::BalloonLine{
        .text = ro::core::Tr("Exemple de chasse [ 3 / 10 ]"), .color = kPreviewColor});
  }

  // D'ABORD TOUT LE TEXTE, ensuite les vues : voir les tampons dans l'en-tête.
  std::vector<Planned> planned;
  const auto plan = [&](const BalloonNotice& notice, float alpha) {
    planned.push_back(Planned{.notice = &notice,
                              .first_line = decorated_.size(),
                              .alpha = alpha,
                              .hunt = notice.family == BalloonFamily::kHunt});
    for (const ro::sim::BalloonLine& line : notice.lines) {
      // Sans balise ni balisage, la décoration n'écrit rien : la ligne se
      // montre telle quelle.
      DecoratedChatLine& kept = decorated_.emplace_back();
      if (!DecorateBubbleLine(line.text, decor.tables, decor.item_icon, &kept)) {
        kept.text = line.text;
      }
    }
  };
  for (const std::deque<BalloonNotice>& held : held_) {
    for (const BalloonNotice& notice : held) {
      plan(notice, BalloonAlpha(notice.heard_ms, now_ms, look));
    }
  }
  for (const BalloonNotice& example : previews_) {
    plan(example, 1.0F);
  }

  line_views_.reserve(decorated_.size());
  for (const Planned& balloon : planned) {
    for (std::size_t rank = 0; rank < balloon.notice->lines.size(); ++rank) {
      const DecoratedChatLine& kept = decorated_[balloon.first_line + rank];
      line_views_.push_back(
          ro::ui::BalloonLineView{.text = kept.text,
                                  .color = OpaqueColor(balloon.notice->lines[rank].color),
                                  .links = kept.links,
                                  .styles = kept.styles,
                                  .images = kept.images});
    }
  }
  std::optional<BalloonFamily> last_family;
  for (const Planned& balloon : planned) {
    std::vector<ro::ui::BalloonView>& into = balloon.hunt ? hunt_views_ : announcement_views_;
    const bool new_family =
        !balloon.hunt && last_family.has_value() && *last_family != balloon.notice->family;
    if (!balloon.hunt) {
      last_family = balloon.notice->family;
    }
    into.push_back(ro::ui::BalloonView{
        .lines = std::span<const ro::ui::BalloonLineView>(line_views_)
                     .subspan(balloon.first_line, balloon.notice->lines.size()),
        .font_px = ro::ui::BalloonFontPixels(balloon.notice->font_px, look.font_scale_percent),
        .wraps = balloon.notice->wraps,
        .alpha = balloon.alpha,
        .new_family = new_family});
  }
}

void DrawBalloonBoards(BalloonBoards& boards, const ro::ui::ChatRichArt* rich) {
  if (boards.look == nullptr) {
    return;
  }
  const ro::ui::BroadcastBalloonLook& look = *boards.look;
  ro::ui::DrawBroadcastBalloons(boards.announcements, look, look.announcements,
                                ro::ui::AnnouncementBalloonTop(look.announcements),
                                "###balloon_announcements_anchor",
                                ro::core::Tr("Annonces — glisser pour placer"),
                                boards.announcement_grip, &boards.announcement_ask, rich);
  ro::ui::DrawBroadcastBalloons(
      boards.hunts, look, look.hunts,
      ro::ui::HuntBalloonTop(look.hunts, ImGui::GetIO().DisplaySize.y), "###balloon_hunts_anchor",
      ro::core::Tr("Chasses — glisser pour placer"), boards.hunt_grip, &boards.hunt_ask, rich);
}

}  // namespace moonclient
