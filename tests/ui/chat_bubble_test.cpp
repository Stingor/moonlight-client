// Combien de temps une parole flotte au-dessus d'une tête, et ce qu'elle y
// peint.

#include "ro/ui/chat_bubble.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include <imgui.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/chat_markup.h"
#include "ro/ui/ui_context.h"

namespace {

using ro::ui::ChatBubbleLifeMs;
using ro::ui::ChatBubbleLook;
using ro::ui::ChatBubbleView;
using ro::ui::ChatStyleRun;

// Ce que le client d'origine donne à toute bulle, quelle qu'en soit la
// longueur. Écrit ici plutôt que lu dans l'implémentation : un essai qui
// reprend la constante qu'il vérifie ne vérifie rien.
constexpr float kClientMs = 5000.0F;

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
// Là où monte la bulle : loin des bords, qu'aucune rangée ne sorte de l'écran.
constexpr ImVec2 kHead{640.0F, 400.0F};
// Des teintes que seul le texte éprouvé porte.
constexpr ImU32 kProbeColor = IM_COL32(10, 200, 30, 255);
constexpr ImU32 kOtherProbeColor = IM_COL32(200, 10, 130, 255);
// L'écart toléré entre le milieu d'une rangée et la tête : l'encre d'un glyphe
// ne coïncide pas tout à fait avec son avance.
constexpr float kCenterSlackPixels = 2.0F;

// UNE IMAGE D'IMGUI, sans GPU, où ne se peignent que des bulles.
struct BubbleBench {
  ro::ui::UiContext ui;

  void Frame(std::span<const ChatBubbleView> bubbles, const ChatBubbleLook& look) {
    ui.BeginFrame(kViewport, kFrameSeconds);
    ro::ui::DrawChatBubbles(bubbles, look);
    ui.EndFrame();
  }
  // La largeur d'un texte dans la police des bulles, mesurée dans une image.
  [[nodiscard]] float Width(std::string_view text) {
    ui.BeginFrame(kViewport, kFrameSeconds);
    const float width = ImGui::CalcTextSize(text.data(), text.data() + text.size()).x;
    ui.EndFrame();
    return width;
  }
  [[nodiscard]] float LineHeight() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    const float height = ImGui::GetTextLineHeight();
    ui.EndFrame();
    return height;
  }
};

// Le fond et le liseré dans des teintes que rien d'autre ne porte.
constexpr ImU32 kBackgroundProbe = IM_COL32(1, 2, 3, 255);
constexpr ImU32 kBorderProbe = IM_COL32(4, 5, 6, 255);
// Le fond lissé se pose un demi-pixel en deçà de chaque bord du rectangle
// demandé : sa largeur et sa hauteur mesurées perdent un pixel.
constexpr float kAntialiasInsetPixels = 1.0F;
// L'écart toléré sur un bord de cadre, une fois ce retrait rendu.
constexpr float kFrameSlackPixels = 0.5F;
// L'écart relatif toléré sur une largeur de texte : l'avance d'un glyphe se
// range au pixel, à chaque taille.
constexpr float kTextRatioSlack = 0.06F;
constexpr int kUsualPercent = 100;
constexpr int kLargestPercent = 160;
constexpr float kLargestScale = 1.6F;

[[nodiscard]] ChatBubbleLook ProbedLook(int text_scale_percent) {
  ChatBubbleLook look;
  look.background_color = kBackgroundProbe;
  look.border_color = kBorderProbe;
  look.text_scale_percent = text_scale_percent;
  return look;
}

// Les bornes horizontales et verticales des sommets d'une teinte.
struct Extent {
  float left = 0.0F;
  float right = 0.0F;
  float top = 0.0F;
  float bottom = 0.0F;
  std::size_t vertices = 0;

  [[nodiscard]] float Width() const { return right - left; }
  [[nodiscard]] float Height() const { return bottom - top; }
};

[[nodiscard]] Extent ExtentOf(ImU32 tint) {
  Extent extent{.left = 1.0e9F, .right = -1.0e9F, .top = 1.0e9F, .bottom = -1.0e9F};
  const ImDrawData* drawn = ImGui::GetDrawData();
  REQUIRE(drawn != nullptr);
  for (const ImDrawList* list : drawn->CmdLists) {
    for (const ImDrawVert& vertex : list->VtxBuffer) {
      if (vertex.col != tint) {
        continue;
      }
      extent.left = std::min(extent.left, vertex.pos.x);
      extent.right = std::max(extent.right, vertex.pos.x);
      extent.top = std::min(extent.top, vertex.pos.y);
      extent.bottom = std::max(extent.bottom, vertex.pos.y);
      ++extent.vertices;
    }
  }
  return extent;
}

// Le cadre et le texte d'une bulle peinte seule, sous ce réglage.
struct PaintedBubble {
  Extent frame;
  Extent text;
};

[[nodiscard]] PaintedBubble Paint(BubbleBench& bench, std::string_view text,
                                  const ChatBubbleLook& look) {
  const std::array<ChatBubbleView, 1> bubbles{
      ChatBubbleView{.head = kHead, .text = text, .text_color = kProbeColor}};
  bench.Frame(bubbles, look);
  return PaintedBubble{.frame = ExtentOf(kBackgroundProbe), .text = ExtentOf(kProbeColor)};
}

[[nodiscard]] ChatStyleRun Run(std::string_view text, std::string_view piece, std::uint8_t style,
                               ImU32 color) {
  const std::size_t at = text.find(piece);
  REQUIRE(at != std::string_view::npos);
  return ChatStyleRun{.offset = static_cast<std::uint32_t>(at),
                      .length = static_cast<std::uint32_t>(piece.size()),
                      .style = style,
                      .color = color,
                      .spoiler = static_cast<std::uint16_t>((style & ro::ui::kChatSpoiler) != 0)};
}

}  // namespace

TEST_CASE("Bulle : la couleur d'une suite se peint", "[ui][bulle]") {
  BubbleBench bench;
  constexpr std::string_view kText = "Ana : du rouge ici";
  const std::array<ChatStyleRun, 1> runs{Run(kText, "rouge", 0, kProbeColor)};
  const std::array<ChatBubbleView, 1> bubbles{
      ChatBubbleView{.head = kHead, .text = kText, .styles = runs}};
  bench.Frame(bubbles, ChatBubbleLook{});
  CHECK(ExtentOf(kProbeColor).vertices > 0);
}

TEST_CASE("Bulle : un spoiler reste masqué", "[ui][bulle]") {
  // 🔴 UNE BULLE NE SE CLIQUE PAS : rien ne la révèle, et le texte caché ne
  // se peint pas du tout.
  BubbleBench bench;
  constexpr std::string_view kText = "Ana : la fin est triste";
  const std::array<ChatStyleRun, 1> hidden{Run(kText, "triste", ro::ui::kChatSpoiler, kProbeColor)};
  std::array<ChatBubbleView, 1> bubbles{
      ChatBubbleView{.head = kHead, .text = kText, .styles = hidden}};
  bench.Frame(bubbles, ChatBubbleLook{});
  CHECK(ExtentOf(kProbeColor).vertices == 0);

  // La même suite hors spoiler se lit : c'est bien le masque qui la cache.
  const std::array<ChatStyleRun, 1> shown{Run(kText, "triste", 0, kProbeColor)};
  bubbles[0].styles = shown;
  bench.Frame(bubbles, ChatBubbleLook{});
  CHECK(ExtentOf(kProbeColor).vertices > 0);
}

TEST_CASE("Bulle : chaque rangée repliée se centre sur la tête", "[ui][bulle]") {
  BubbleBench bench;
  constexpr std::string_view kText = "AAAAAA BB";
  // Juste assez large pour la première rangée : « BB » descend seul.
  const float first_row = bench.Width("AAAAAA");
  ChatBubbleLook look;
  look.width_fraction = (first_row + 1.0F) / kViewport.width_px;
  const std::array<ChatStyleRun, 2> runs{Run(kText, "AAAAAA", 0, kOtherProbeColor),
                                         Run(kText, "BB", ro::ui::kChatBold, kProbeColor)};
  const std::array<ChatBubbleView, 1> bubbles{
      ChatBubbleView{.head = kHead, .text = kText, .styles = runs}};
  bench.Frame(bubbles, look);
  const Extent wide = ExtentOf(kOtherProbeColor);
  const Extent narrow = ExtentOf(kProbeColor);
  REQUIRE(wide.vertices > 0);
  REQUIRE(narrow.vertices > 0);
  CHECK(narrow.top > wide.top);
  CHECK(std::fabs((wide.left + wide.right) * 0.5F - kHead.x) < kCenterSlackPixels);
  CHECK(std::fabs((narrow.left + narrow.right) * 0.5F - kHead.x) < kCenterSlackPixels);
}

TEST_CASE("Bulle : une icône d'objet se peint, et la rangée reste centrée", "[ui][bulle]") {
  // 🔴 LA RANGÉE SE MESURE ICÔNE COMPRISE : sans elle, la phrase serait
  // décalée de la moitié d'une icône.
  constexpr std::string_view kText = "<Knife>";
  constexpr ImTextureID kIcon = 0xC0DE;
  constexpr std::uint32_t kKnifeId = 1201;
  const std::array<ro::ui::ChatInlineImage, 1> icon{
      ro::ui::ChatInlineImage{.offset = 0,
                              .kind = ro::ui::ChatInlineKind::kItemIcon,
                              .item_id = kKnifeId,
                              .texture = kIcon}};
  const std::array<ChatBubbleView, 1> bubbles{
      ChatBubbleView{.head = kHead, .text = kText, .images = icon, .text_color = kProbeColor}};
  const ro::ui::ChatRichArt rich;
  BubbleBench bench;
  bench.ui.BeginFrame(kViewport, kFrameSeconds);
  ro::ui::DrawChatBubbles(bubbles, ChatBubbleLook{}, &rich);
  bench.ui.EndFrame();
  // L'icône : les seuls sommets blancs, l'image n'étant pas teintée.
  const Extent drawn_icon = ExtentOf(IM_COL32_WHITE);
  const Extent words = ExtentOf(kProbeColor);
  REQUIRE(drawn_icon.vertices > 0);
  REQUIRE(words.vertices > 0);
  CHECK(drawn_icon.right <= words.left);
  CHECK(std::fabs((drawn_icon.left + words.right) * 0.5F - kHead.x) < kCenterSlackPixels);
}

TEST_CASE("Bulle : la durée du client vaut pour tout le monde", "[app][bulle]") {
  const ChatBubbleLook look{.client_timing = true};
  CHECK(ChatBubbleLifeMs(look, "ok") == kClientMs);
  CHECK(ChatBubbleLifeMs(look, std::string(300, 'a')) == kClientMs);
}

TEST_CASE("Bulle : la rallonge suit la longueur", "[app][bulle]") {
  // 🔴 C'EST LE DÉFAUT QU'ELLE RÉPARE. Sous la durée du client, un pavé de trois
  // lignes disparaît aussi vite qu'un « ok » — or c'est lui qu'on met le plus de
  // temps à lire.
  const ChatBubbleLook look{.client_timing = false, .base_ms = 1000.0F, .per_character_ms = 100.0F};
  CHECK(ChatBubbleLifeMs(look, "") == 1000.0F);
  CHECK(ChatBubbleLifeMs(look, "abc") == 1300.0F);
  CHECK(ChatBubbleLifeMs(look, "abcdef") > ChatBubbleLifeMs(look, "abc"));
}

TEST_CASE("Bulle : la durée reste entre ses bornes", "[app][bulle]") {
  // ⚠ LE PLAFOND EST CE QUI EMPÊCHE UNE BULLE ÉTERNELLE : un message long, une
  // rallonge poussée au maximum, et elle ne partirait plus de l'écran.
  const ChatBubbleLook stingy{.client_timing = false, .base_ms = 0.0F, .per_character_ms = 0.0F};
  CHECK(ChatBubbleLifeMs(stingy, "") == ro::ui::kShortestBubbleMs);

  const ChatBubbleLook greedy{.client_timing = false,
                              .base_ms = ro::ui::kLongestBubbleMs,
                              .per_character_ms = ro::ui::kMostBubblePerCharacterMs};
  CHECK(ChatBubbleLifeMs(greedy, std::string(500, 'a')) == ro::ui::kLongestBubbleMs);
}

TEST_CASE("Bulle : à cent pour cent, la mise en page reste celle d'origine", "[ui][bulle]") {
  // Le cadre tient le texte à la taille de l'interface, ses marges et sa
  // montée telles que réglées, sans rien multiplier.
  BubbleBench bench;
  constexpr std::string_view kText = "Salut tout le monde";
  const float text_width = bench.Width(kText);
  const float line_height = bench.LineHeight();
  const ChatBubbleLook look = ProbedLook(kUsualPercent);
  const PaintedBubble painted = Paint(bench, kText, look);
  REQUIRE(painted.frame.vertices > 0);
  const float bottom = painted.frame.bottom + kAntialiasInsetPixels * 0.5F;
  CHECK(std::fabs(bottom - (kHead.y - look.rise_pixels)) <= kFrameSlackPixels);
  CHECK(std::fabs(painted.frame.Height() + kAntialiasInsetPixels -
                  (line_height + look.padding_y * 2.0F)) <= kFrameSlackPixels);
  CHECK(std::fabs(painted.frame.Width() + kAntialiasInsetPixels -
                  (text_width + look.padding_x * 2.0F)) <= kFrameSlackPixels);
}

TEST_CASE("Bulle : à cent soixante pour cent, texte, cadre et montée grandissent ensemble",
          "[ui][bulle]") {
  BubbleBench bench;
  constexpr std::string_view kText = "Salut tout le monde";
  const PaintedBubble usual = Paint(bench, kText, ProbedLook(kUsualPercent));
  const PaintedBubble large = Paint(bench, kText, ProbedLook(kLargestPercent));
  REQUIRE(usual.text.vertices > 0);
  REQUIRE(large.text.vertices > 0);
  REQUIRE(large.frame.vertices > 0);

  // Le texte à sa vraie taille de police : plus large et plus haut d'autant.
  CHECK(std::fabs(large.text.Width() / usual.text.Width() - kLargestScale) <= kTextRatioSlack);
  CHECK(std::fabs(large.text.Height() / usual.text.Height() - kLargestScale) <= kTextRatioSlack);

  // Le cadre : sa hauteur suit exactement, sa largeur suit le texte.
  // ⚠ LES BORDS MESURÉS SONT UN DEMI-PIXEL EN DEÇÀ : on compare les rectangles.
  const float usual_height = usual.frame.Height() + kAntialiasInsetPixels;
  const float large_height = large.frame.Height() + kAntialiasInsetPixels;
  CHECK(std::fabs(large_height - usual_height * kLargestScale) <= kFrameSlackPixels);
  const float usual_width = usual.frame.Width() + kAntialiasInsetPixels;
  const float large_width = large.frame.Width() + kAntialiasInsetPixels;
  CHECK(std::fabs(large_width / usual_width - kLargestScale) <= kTextRatioSlack);

  // La montée au-dessus de la tête.
  const float usual_rise = kHead.y - (usual.frame.bottom + kAntialiasInsetPixels * 0.5F);
  const float large_rise = kHead.y - (large.frame.bottom + kAntialiasInsetPixels * 0.5F);
  CHECK(std::fabs(large_rise - usual_rise * kLargestScale) <= kFrameSlackPixels);
}

TEST_CASE("Bulle : la largeur de coupure suit le texte", "[ui][bulle]") {
  // 🔴 SANS CELA, GROSSIR LE TEXTE CHANGERAIT SES RETOURS À LA LIGNE : la
  // première rangée, qui tenait juste, se casserait au caractère.
  BubbleBench bench;
  constexpr std::string_view kText = "AAAAAA BB";
  // Juste assez large, à la taille d'origine, pour la première rangée.
  const float fraction = (bench.Width("AAAAAA") + 1.0F) / kViewport.width_px;
  const std::array<ChatStyleRun, 2> runs{Run(kText, "AAAAAA", 0, kOtherProbeColor),
                                         Run(kText, "BB", 0, kProbeColor)};
  const std::array<ChatBubbleView, 1> bubbles{
      ChatBubbleView{.head = kHead, .text = kText, .styles = runs}};
  const auto rows_at = [&](int text_scale_percent) {
    ChatBubbleLook look = ProbedLook(text_scale_percent);
    look.width_fraction = fraction;
    bench.Frame(bubbles, look);
    return std::array{ExtentOf(kOtherProbeColor), ExtentOf(kProbeColor)};
  };
  const auto [usual_wide, usual_narrow] = rows_at(kUsualPercent);
  const auto [large_wide, large_narrow] = rows_at(kLargestPercent);
  REQUIRE(usual_wide.vertices > 0);
  REQUIRE(large_wide.vertices > 0);
  REQUIRE(large_narrow.vertices > 0);
  // « BB » descend seul, et « AAAAAA » reste d'un seul tenant, à sa taille.
  CHECK(usual_narrow.top > usual_wide.bottom);
  CHECK(large_narrow.top > large_wide.bottom);
  CHECK(std::fabs(large_wide.Width() / usual_wide.Width() - kLargestScale) <= kTextRatioSlack);
}

TEST_CASE("Bulle : la taille du texte reste entre ses bornes", "[ui][bulle]") {
  CHECK(ro::ui::ChatBubbleTextPercent(100) == 100);
  CHECK(ro::ui::ChatBubbleTextPercent(0) == 80);
  CHECK(ro::ui::ChatBubbleTextPercent(1000) == 160);
  CHECK(ro::ui::ChatBubbleScale(ProbedLook(1000)) ==
        ro::ui::ChatBubbleScale(ProbedLook(kLargestPercent)));
  CHECK(ro::ui::ChatBubbleScale(ProbedLook(5)) == ro::ui::ChatBubbleScale(ProbedLook(80)));
}
