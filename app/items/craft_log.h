#pragma once

// CE QUE LES DEUX ATELIERS — fabrication et affinage — PARTAGENT : le journal
// de session, daté à l'heure locale, et les échéances de leurs relances sur
// l'horloge du jeu, qui reboucle.

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <deque>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "ro/core/local_time.h"
#include "ro/ui/craft_log_view.h"
#include "ro/ui/skin_look.h"

namespace moonclient {

// ⚠ PAR SOUSTRACTION : l'horloge reboucle, et une échéance juste avant le
// rebouclage doit rester échue juste après.
[[nodiscard]] inline bool Due(std::uint32_t at, std::uint32_t now_ms) {
  return at != 0 && static_cast<std::int32_t>(now_ms - at) >= 0;
}

// Une échéance ; zéro voulant dire « rien d'armé », elle n'en vaut jamais zéro.
[[nodiscard]] inline std::uint32_t Deadline(std::uint32_t now_ms, std::uint32_t delay_ms) {
  const std::uint32_t at = now_ms + delay_ms;
  return at == 0 ? 1 : at;
}

// LE JOURNAL DE SESSION : chaque ligne datée « HH:MM:SS » à son ajout, et
// seules les `kMostLines` dernières gardées.
class CraftLog {
 public:
  void Add(std::string text, ro::ui::SkinTone tone) {
    lines_.push_back(Line{.text = std::move(text), .tone = tone, .time = ClockNow()});
    while (lines_.size() > kMostLines) {
      lines_.pop_front();
    }
  }

  // Les lignes telles que la fenêtre les lit. ⚠ LES VUES POINTENT DANS LE
  // JOURNAL : elles se reprennent à chaque composition.
  [[nodiscard]] std::span<const ro::ui::CraftLogLineView> Views() {
    views_.clear();
    for (const Line& line : lines_) {
      views_.push_back(
          ro::ui::CraftLogLineView{.text = line.text, .tone = line.tone, .time = line.time});
    }
    return views_;
  }

 private:
  static constexpr std::size_t kMostLines = 200;
  static constexpr std::size_t kTimeBytes = 16;

  struct Line {
    std::string text;
    ro::ui::SkinTone tone = ro::ui::SkinTone::kText;
    std::string time;
  };

  // L'heure locale, vide quand le système ne sait pas la dire.
  [[nodiscard]] static std::string ClockNow() {
    const std::time_t stamp =
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm local{};
    if (!ro::core::ToLocalTime(stamp, local)) {
      return {};
    }
    std::array<char, kTimeBytes> text{};
    std::strftime(text.data(), text.size(), "%H:%M:%S", &local);
    return text.data();
  }

  std::deque<Line> lines_;
  std::vector<ro::ui::CraftLogLineView> views_;
};

}  // namespace moonclient
