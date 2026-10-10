// LE FIL D'UN SALON, trié comme un onglet de la conversation : ses balises
// décorées, ses liens et ses vignettes numérotés avec les autres.

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <imgui.h>

#include "chat/chat_item_link.h"
#include "chat/chat_tab_view.h"

namespace moonclient {
namespace {

constexpr int kSecondsPerMinute = 60;
constexpr int kSecondsPerHour = 60 * kSecondsPerMinute;
constexpr int kSecondsPerDay = 24 * kSecondsPerHour;
constexpr std::uint32_t kMsPerSecond = 1000;

// Une couleur du serveur, rouge, vert puis bleu, et l'opacité qui est la nôtre.
constexpr int kRedShift = 16;
constexpr int kGreenShift = 8;
constexpr std::uint32_t kByteMask = 0xFF;
constexpr int kOpaque = 255;

// L'heure du jour d'une ligne, en secondes, depuis la seconde du jour relevée
// au démarrage.
[[nodiscard]] int DaySecondOf(int day_second_at_boot, std::uint32_t said_ms) {
  return ((day_second_at_boot + static_cast<int>(said_ms / kMsPerSecond)) % kSecondsPerDay +
          kSecondsPerDay) %
         kSecondsPerDay;
}

}  // namespace

void ChatTabsView::RefreshRoom(std::span<const ro::sim::ChatLine> lines, std::uint32_t room_seq) {
  if (room_seq != room_seq_) {
    room_seq_ = room_seq;
    room_.layout = ro::ui::ChatLogLayout();
  }
  room_.lines.clear();
  room_.decorated.clear();
  room_.lines.reserve(lines.size());
  // L'IDENTITÉ D'UNE LIGNE POUR LE CACHE DU FIL : son aspect, puis son rang.
  // Les tables d'objets peuvent arriver après les lignes, et une ligne qui
  // change d'aspect doit changer d'identité, ou le cache garde la hauteur de sa
  // version brute.
  const std::uint64_t look_bits = LookBits(false);
  for (const ro::sim::ChatLine& line : lines) {
    ro::ui::ChatLineView view{.color = IM_COL32((line.color >> kRedShift) & kByteMask,
                                                (line.color >> kGreenShift) & kByteMask,
                                                line.color & kByteMask, kOpaque)};
    // L'heure, comme dans la conversation : le repère est la seconde du jour
    // relevée au démarrage, et le salon n'en tient que si elle l'a été.
    if (day_second_at_boot_ >= 0) {
      const int second = DaySecondOf(day_second_at_boot_, line.said_ms);
      view.hour = static_cast<std::uint8_t>(second / kSecondsPerHour);
      view.minute = static_cast<std::uint8_t>((second / kSecondsPerMinute) % kSecondsPerMinute);
      view.second = static_cast<std::uint8_t>(second % kSecondsPerMinute);
    }
    // UNE LIGNE DE SALON PORTE TOUJOURS SON LOCUTEUR EN TÊTE, « Nom : texte ». ⚠
    // IL NE SE CHERCHE QUE SI LA LIGNE PORTE UN `<MVPL>` : la question se pose à
    // chaque ligne, à chaque image.
    const std::string sender = FindMvpLink(line.text, 0) != std::string_view::npos
                                   ? MvpRoomSenderOf(line.text)
                                   : std::string();
    ShowText(line.text, view, room_, sender, false, line.author ? &*line.author : nullptr);
    view.id = CarriesMvpLink(view) ? 0 : look_bits | line.seq;
    view.seq = line.seq;
    room_.lines.push_back(view);
  }
}

ChatLogDump ChatTabsView::SaveRoomLog(std::span<const ro::sim::ChatLine> lines,
                                      std::string_view room_title) const {
  std::vector<PastChatLine> written;
  written.reserve(lines.size());
  for (const ro::sim::ChatLine& line : lines) {
    PastChatLine past{.type = line.type, .text = line.text};
    // Sans repère d'heure, la ligne porte minuit, comme elle n'en porte aucune
    // à l'écran.
    if (day_second_at_boot_ >= 0) {
      const int second = DaySecondOf(day_second_at_boot_, line.said_ms);
      past.hour = static_cast<std::uint8_t>(second / kSecondsPerHour);
      past.minute = static_cast<std::uint8_t>((second / kSecondsPerMinute) % kSecondsPerMinute);
      past.second = static_cast<std::uint8_t>(second % kSecondsPerMinute);
    }
    written.push_back(std::move(past));
  }
  return DumpChatRoomLog(client_root_, room_title, written);
}

}  // namespace moonclient
