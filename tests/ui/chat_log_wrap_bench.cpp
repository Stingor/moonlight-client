// Combien coûte le repli du fil, par image ?
//
// ⚠ MESURÉ, PAS SUPPOSÉ : la conversation garde mille lignes PAR GENRE, et
// l'onglet qui montre les dix genres les additionne. Ou bien c'est négligeable,
// ou bien il faut un cache — et c'est le chiffre qui tranche, pas l'intuition.

#include <chrono>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/chat_log.h"
#include "ro/ui/ui_context.h"

namespace {

// Le coût moyen, par image, de la hauteur de `count` lignes ordinaires : tout
// remis en page, ou par le cache une fois chaud — ce que paie une image où rien
// n'arrive.
double FallbackPerFrameMs(int count, bool cached) {
  std::vector<std::string> texts;
  for (int rank = 0; rank < count; ++rank) {
    texts.push_back("Une phrase de chat parfaitement ordinaire, avec son compte " +
                    std::to_string(rank) + " et quelques mots de plus pour faire vrai.");
  }
  std::vector<ro::ui::ChatLineView> lines;
  for (std::size_t rank = 0; rank < texts.size(); ++rank) {
    lines.push_back(ro::ui::ChatLineView{.text = texts[rank], .id = rank + 1});
  }
  const ro::ui::ChatLogStyle style{.font_size = 14.0F, .line_gap = 2.0F, .timestamps = true};
  ro::ui::ChatLogLayout layout;
  static_cast<void>(layout.Update(lines, style, 600.0F));

  const auto start = std::chrono::steady_clock::now();
  constexpr int kRounds = 30;
  float total = 0.0F;
  for (int round = 0; round < kRounds; ++round) {
    total +=
        cached ? layout.Update(lines, style, 600.0F) : ro::ui::ChatLogHeight(lines, style, 600.0F);
  }
  const auto elapsed = std::chrono::steady_clock::now() - start;
  CHECK(total > 0.0F);
  return std::chrono::duration<double, std::milli>(elapsed).count() / kRounds;
}

}  // namespace

TEST_CASE("Mesure : le repli du fil entier", "[ui][mesure][.]") {
  ro::ui::UiContext ui;
  ui.BeginFrame(ro::ui::Viewport{.width_px = 1280.0F, .height_px = 720.0F}, 1.0F / 60.0F);
  for (const int count : {1000, 10000}) {
    WARN("repli de " << count << " lignes : " << FallbackPerFrameMs(count, false)
                     << " ms par image, tout remis en page ; " << FallbackPerFrameMs(count, true)
                     << " ms par le cache");
  }
  ui.EndFrame();
}
