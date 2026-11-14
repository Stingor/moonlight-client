// LA MISE EN PAGE DU FIL : ce qui se clique, ce qui se mesure, ce qui se peint.
//
// 🔴 UNE ZONE CLIQUABLE TROUÉE NE SE VOIT PAS : le texte est bien là, et seul
// le clic qui tombe dans le trou ne répond pas. Le banc promène donc la souris
// pixel par pixel sur ce qui doit répondre.

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/chat_link.h"
#include "ro/ui/chat_log.h"
#include "ro/ui/chat_markup.h"
#include "ro/ui/chat_rich_art.h"
#include "ro/ui/chat_strip.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/ui_context.h"

using ro::ui::ChatInlineImage;
using ro::ui::ChatInlineKind;
using ro::ui::ChatLineView;
using ro::ui::ChatLinkKind;
using ro::ui::ChatLinkSpan;
using ro::ui::ChatLogStyle;
using ro::ui::ChatPictureState;
using ro::ui::ChatRichArt;
using ro::ui::ChatStyleRun;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr float kLogHeightPixels = 200.0F;
constexpr float kFontSize = 14.0F;
constexpr float kLineGap = 2.0F;
constexpr int kSettleFrames = 3;
constexpr std::uint32_t kItemId = 7;
// Un identifiant de texture que rien d'autre ne pose.
constexpr ImTextureID kEmoteTexture = 0xBEEF;
constexpr std::uint16_t kEmoteAction = 3;
constexpr float kEmoteBoxPixels = 10.0F;
constexpr float kEmoteFrameMs = 100.0F;
constexpr float kPicturePixels = 48.0F;
constexpr std::uint32_t kPictureKey = 2;
// Une teinte que seul le texte éprouvé porte.
constexpr ImU32 kProbeColor = IM_COL32(10, 200, 30, 255);

// UN FIL QUI TOURNE : un vrai contexte ImGui, sans GPU.
struct ThreadBench {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  ChatLogStyle style{.font_size = kFontSize, .line_gap = kLineGap};
  std::vector<ChatLineView> lines;
  ro::ui::ChatLogLayout layout;
  ro::ui::ChatLogAction action;
  // Mesurées dans l'image, où la police est prête.
  ImFont* font = nullptr;

  void Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
    ImGui::SetNextWindowSize(ImVec2(kViewport.width_px, kLogHeightPixels * 2.0F));
    ImGui::Begin("banc_fil");
    font = ImGui::GetFont();
    action = ro::ui::DrawChatLog(lines, style, kLogHeightPixels, art, &layout);
    ImGui::End();
    ui.EndFrame();
  }
  void Settle() {
    for (int frame = 0; frame < kSettleFrames; ++frame) {
      Frame();
    }
  }
  [[nodiscard]] static ImVec2 Origin() {
    const ImGuiWindow* host = ImGui::FindWindowByName("banc_fil");
    REQUIRE(host != nullptr);
    return host->DC.CursorStartPos;
  }
  [[nodiscard]] float Width(std::string_view text) const {
    return font->CalcTextSizeA(kFontSize, FLT_MAX, 0.0F, text.data(), text.data() + text.size()).x;
  }
  // La souris en `x` du fil, au milieu de la première rangée.
  void HoverAt(float x) {
    const ImVec2 origin = Origin();
    ImGui::GetIO().AddMousePosEvent(origin.x + x, origin.y + kFontSize * 0.5F);
    Frame();
  }
  [[nodiscard]] bool HoversItem() const {
    return action.link.hovering && action.link.hovered_id == kItemId;
  }
};

// Un sommet de cette teinte, dans n'importe quelle liste.
[[nodiscard]] bool Painted(ImU32 tint) {
  const ImDrawData* drawn = ImGui::GetDrawData();
  REQUIRE(drawn != nullptr);
  for (const ImDrawList* list : drawn->CmdLists) {
    for (const ImDrawVert& vertex : list->VtxBuffer) {
      if (vertex.col == tint) {
        return true;
      }
    }
  }
  return false;
}

// Une commande qui dessine cette texture-là, posée par l'appelant.
[[nodiscard]] bool Drew(ImTextureID texture) {
  const ImDrawData* drawn = ImGui::GetDrawData();
  REQUIRE(drawn != nullptr);
  for (const ImDrawList* list : drawn->CmdLists) {
    for (const ImDrawCmd& command : list->CmdBuffer) {
      if (command.TexRef._TexData == nullptr && command.TexRef._TexID == texture) {
        return true;
      }
    }
  }
  return false;
}

constexpr std::string_view kSpacedLine = "Voir A B fin";
constexpr std::string_view kBeforeLink = "Voir ";
constexpr std::string_view kThroughA = "Voir A";
constexpr std::string_view kThroughB = "Voir A B";

[[nodiscard]] ChatLinkSpan SpacedLink() {
  return ChatLinkSpan{.offset = static_cast<std::uint32_t>(kBeforeLink.size()),
                      .length = 3,
                      .kind = ChatLinkKind::kItem,
                      .item_id = kItemId,
                      .token = 1};
}

}  // namespace

TEST_CASE("Fil : l'espace d'un lien se clique comme ses lettres", "[ui][chatlog]") {
  ThreadBench bench;
  const std::array<ChatLinkSpan, 1> links{SpacedLink()};
  bench.lines.push_back(ChatLineView{.text = kSpacedLine, .links = links, .id = 1});
  bench.Settle();
  // 🔴 AU MILIEU DE L'ESPACE entre « A » et « B » : c'était un trou.
  const float space = bench.Width(" ");
  bench.HoverAt(bench.Width(kThroughA) + space * 0.5F);
  CHECK(bench.HoversItem());
}

TEST_CASE("Fil : un lien se survole d'un bout à l'autre, et pas avant", "[ui][chatlog]") {
  ThreadBench bench;
  const std::array<ChatLinkSpan, 1> links{SpacedLink()};
  bench.lines.push_back(ChatLineView{.text = kSpacedLine, .links = links, .id = 1});
  bench.Settle();
  constexpr float kPixel = 1.0F;
  constexpr float kOutsidePixels = 3.0F;
  const float from = bench.Width(kBeforeLink);
  const float to = bench.Width(kThroughB);
  int missed = 0;
  for (float x = std::ceil(from); x < to; x += kPixel) {
    bench.HoverAt(x);
    if (!bench.HoversItem()) {
      ++missed;
    }
  }
  CHECK(missed == 0);
  bench.HoverAt(from - kOutsidePixels);
  CHECK_FALSE(bench.action.link.hovering);
}

TEST_CASE("Fil : le faux gras compte dans la mesure", "[ui][chatlog]") {
  ThreadBench bench;
  bench.Frame();
  ro::ui::UiContext& ui = bench.ui;
  ui.BeginFrame(kViewport, kFrameSeconds);
  constexpr std::string_view kWords = "un deux trois quatre";
  const float plain_width = bench.Width(kWords);
  // Assez pour le texte nu, pas pour un pixel de plus par mot.
  constexpr float kSlackPixels = 1.5F;
  const float wrap_width = plain_width + kSlackPixels;
  const std::array<ChatStyleRun, 1> bold{
      ChatStyleRun{.offset = 0,
                   .length = static_cast<std::uint32_t>(kWords.size()),
                   .style = ro::ui::kChatBold}};
  const ChatLineView plain{.text = kWords};
  const ChatLineView heavy{.text = kWords, .styles = bold};
  const float plain_height = ro::ui::ChatLogHeight(std::span(&plain, 1), bench.style, wrap_width);
  const float heavy_height = ro::ui::ChatLogHeight(std::span(&heavy, 1), bench.style, wrap_width);
  CHECK(heavy_height > plain_height);
  ui.EndFrame();
}

TEST_CASE("Fil : une rangée à vignette vaut un multiple entier du pas", "[ui][chatlog]") {
  ThreadBench bench;
  bench.Frame();
  bench.ui.BeginFrame(kViewport, kFrameSeconds);
  constexpr std::string_view kText = "regarde";
  const std::array<ChatInlineImage, 1> images{
      ChatInlineImage{.offset = static_cast<std::uint32_t>(kText.size()),
                      .kind = ChatInlineKind::kPicture,
                      .picture_key = kPictureKey,
                      .state = ChatPictureState::kPending}};
  const ChatLineView line{.text = kText, .images = images};
  const float step = ro::ui::ChatLineStep(bench.style);
  const float overhang = ro::ui::ChatLineOverhang(bench.style);

  const ChatRichArt rich{.picture_pixels = kPicturePixels};
  ChatLogStyle shown = bench.style;
  shown.rich = &rich;
  const float row =
      ro::ui::ChatLogHeight(std::span(&line, 1), shown, kViewport.width_px) - overhang;
  CHECK(row >= kPicturePixels);
  CHECK(row < kPicturePixels + step);
  CHECK(std::fmod(row, step) == 0.0F);

  // Vignettes éteintes : un pas, comme toute ligne.
  const ChatRichArt off{.picture_pixels = 0.0F};
  shown.rich = &off;
  CHECK(ro::ui::ChatLogHeight(std::span(&line, 1), shown, kViewport.width_px) - overhang == step);
  bench.ui.EndFrame();
}

TEST_CASE("Fil : un avatar tient dans sa rangée, et se charge comme une vignette",
          "[ui][chatlog]") {
  ThreadBench bench;
  std::vector<std::uint32_t> seen;
  const ChatRichArt rich{.picture_pixels = kPicturePixels, .seen_pictures = &seen};
  constexpr std::string_view kText = "[#gonryun][Ana] salut";
  const std::array<ChatInlineImage, 1> images{ChatInlineImage{.kind = ChatInlineKind::kPicture,
                                                              .picture_key = kPictureKey,
                                                              .line_high = true,
                                                              .state = ChatPictureState::kPending}};
  bench.lines.push_back(ChatLineView{.text = kText, .id = 1, .images = images});
  bench.style.rich = &rich;
  bench.Frame();
  REQUIRE(seen.size() == 1);
  CHECK(seen.front() == kPictureKey);

  bench.ui.BeginFrame(kViewport, kFrameSeconds);
  const ChatLineView line{.text = kText, .images = images};
  // ⚠ LA RANGÉE NE GRANDIT PAS : un pas, comme une ligne sans image.
  CHECK(ro::ui::ChatLogHeight(std::span(&line, 1), bench.style, kViewport.width_px) -
            ro::ui::ChatLineOverhang(bench.style) ==
        ro::ui::ChatLineStep(bench.style));
  bench.ui.EndFrame();
}

TEST_CASE("Fil : une vignette qui attend se signale à qui la charge", "[ui][chatlog]") {
  ThreadBench bench;
  std::vector<std::uint32_t> seen;
  const ChatRichArt rich{.picture_pixels = kPicturePixels, .seen_pictures = &seen};
  bench.style.rich = &rich;
  constexpr std::string_view kText = "regarde";
  const std::array<ChatInlineImage, 1> images{
      ChatInlineImage{.offset = static_cast<std::uint32_t>(kText.size()),
                      .kind = ChatInlineKind::kPicture,
                      .picture_key = kPictureKey,
                      .state = ChatPictureState::kPending}};
  bench.lines.push_back(ChatLineView{.text = kText, .id = 1, .images = images});
  bench.Frame();
  REQUIRE(seen.size() == 1);
  CHECK(seen.front() == kPictureKey);
}

namespace {

// Le haut et le bas de ce que dessine cette texture, en pixels d'écran.
struct Span {
  float top = FLT_MAX;
  float bottom = -FLT_MAX;
};
[[nodiscard]] Span DrawnSpan(ImTextureID texture) {
  Span span;
  const ImDrawData* drawn = ImGui::GetDrawData();
  REQUIRE(drawn != nullptr);
  for (const ImDrawList* list : drawn->CmdLists) {
    for (const ImDrawCmd& command : list->CmdBuffer) {
      if (command.TexRef._TexData != nullptr || command.TexRef._TexID != texture) {
        continue;
      }
      for (unsigned int rank = 0; rank < command.ElemCount; ++rank) {
        const ImDrawIdx index = list->IdxBuffer[static_cast<int>(command.IdxOffset + rank)];
        const ImDrawVert& vertex =
            list->VtxBuffer[static_cast<int>(command.VtxOffset + static_cast<unsigned int>(index))];
        span.top = std::min(span.top, vertex.pos.y);
        span.bottom = std::max(span.bottom, vertex.pos.y);
      }
    }
  }
  return span;
}

// Les sommets de cette teinte, quatre par glyphe.
[[nodiscard]] std::vector<ImDrawVert> VerticesOf(ImU32 tint) {
  std::vector<ImDrawVert> found;
  const ImDrawData* drawn = ImGui::GetDrawData();
  REQUIRE(drawn != nullptr);
  for (const ImDrawList* list : drawn->CmdLists) {
    for (const ImDrawVert& vertex : list->VtxBuffer) {
      if (vertex.col == tint) {
        found.push_back(vertex);
      }
    }
  }
  return found;
}

// Un glyphe penché : le coin haut-gauche de son quad n'est pas au-dessus du
// coin bas-gauche. Un quad de glyphe se pose haut-gauche, haut-droit,
// bas-droit, bas-gauche.
[[nodiscard]] bool AnySlanted(const std::vector<ImDrawVert>& vertices) {
  constexpr std::size_t kQuad = 4;
  constexpr std::size_t kBottomLeft = 3;
  constexpr float kStraight = 0.01F;
  for (std::size_t first = 0; first + kQuad <= vertices.size(); first += kQuad) {
    if (std::fabs(vertices[first].pos.x - vertices[first + kBottomLeft].pos.x) > kStraight) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST_CASE("Fil : l'emote du jeu tient dans sa rangée, ou reste du texte", "[ui][chatlog]") {
  ThreadBench bench;
  const ro::ui::ChatEmotionLayer layer{
      .id = kEmoteTexture,
      .corners = {ImVec2(0.0F, 0.0F), ImVec2(kEmoteBoxPixels, 0.0F),
                  ImVec2(kEmoteBoxPixels, kEmoteBoxPixels), ImVec2(0.0F, kEmoteBoxPixels)}};
  const ro::ui::ChatEmotionFrame frame{.layers = std::span(&layer, 1)};
  std::array<ro::ui::ChatEmotion, kEmoteAction + 1> emotes{};
  emotes[kEmoteAction] = ro::ui::ChatEmotion{.frames = std::span(&frame, 1),
                                             .frame_ms = kEmoteFrameMs,
                                             .box_max = ImVec2(kEmoteBoxPixels, kEmoteBoxPixels)};
  ChatRichArt rich{.emotes_by_action = emotes};
  bench.style.rich = &rich;
  constexpr std::string_view kText = "salut :smile:";
  const std::array<ChatInlineImage, 1> images{ChatInlineImage{
      .offset = 6, .length = 7, .kind = ChatInlineKind::kGameEmote, .emote_action = kEmoteAction}};
  bench.lines.push_back(ChatLineView{.text = kText, .id = 1, .images = images});
  bench.Frame();
  CHECK(Drew(kEmoteTexture));
  // La ligne ne grandit pas : elle tient sur son pas.
  const float step = ro::ui::ChatLineStep(bench.style);
  REQUIRE(bench.layout.heights().size() == 1);
  CHECK(bench.layout.heights()[0] == step);
  // 🔴 ET L'EMOTE NE MORD PAS SUR LA RANGÉE SUIVANTE : elle reste dans le pas.
  const float row_top = ThreadBench::Origin().y;
  const Span drawn = DrawnSpan(kEmoteTexture);
  CHECK(drawn.top >= row_top);
  CHECK(drawn.bottom <= row_top + step);

  rich.emote_images = false;
  bench.Frame();
  CHECK_FALSE(Drew(kEmoteTexture));
}

TEST_CASE("Fil : un spoiler se masque tant qu'il n'est pas révélé", "[ui][chatlog]") {
  ThreadBench bench;
  constexpr std::string_view kText = "le secret est ici";
  const std::array<ChatStyleRun, 1> runs{ChatStyleRun{.offset = 14,
                                                      .length = 3,
                                                      .style = ro::ui::kChatSpoiler,
                                                      .color = kProbeColor,
                                                      .spoiler = 1}};
  constexpr std::uint64_t kLineId = 5;
  bench.lines.push_back(ChatLineView{.text = kText, .id = kLineId, .styles = runs});
  bench.Frame();
  CHECK_FALSE(Painted(kProbeColor));
  bench.layout.Reveal(ro::ui::ChatSpoilerKey{.line_id = kLineId});
  bench.Frame();
  CHECK(Painted(kProbeColor));
}

TEST_CASE("Fil : la couleur d'une suite passe, sauf sur un lien", "[ui][chatlog]") {
  ThreadBench bench;
  constexpr std::string_view kText = "rouge et lien";
  constexpr std::uint32_t kLinkAt = 9;
  constexpr std::uint32_t kLinkLength = 4;
  // La suite colorée ne couvre QUE le lien : sa teinte ne doit pas paraître.
  const std::array<ChatStyleRun, 1> runs{
      ChatStyleRun{.offset = kLinkAt, .length = kLinkLength, .color = kProbeColor}};
  const std::array<ChatLinkSpan, 1> links{ChatLinkSpan{.offset = kLinkAt,
                                                       .length = kLinkLength,
                                                       .kind = ChatLinkKind::kItem,
                                                       .item_id = kItemId,
                                                       .token = 1}};
  bench.lines.push_back(ChatLineView{.text = kText, .links = links, .id = 1, .styles = runs});
  bench.Frame();
  CHECK_FALSE(Painted(kProbeColor));
  CHECK(Painted(ro::ui::kChatLinkColor));

  // Hors lien, la même suite se peint de sa teinte.
  const std::array<ChatStyleRun, 1> plain{
      ChatStyleRun{.offset = 0, .length = 5, .color = kProbeColor}};
  bench.lines.back().styles = plain;
  bench.lines.back().id = 2;
  bench.Frame();
  CHECK(Painted(kProbeColor));
}

TEST_CASE("Fil : une police de style égale à celle du fil simule le style", "[ui][chatlog]") {
  ThreadBench bench;
  bench.Frame();
  constexpr std::string_view kText = "penche";
  const std::array<ChatStyleRun, 1> italic{
      ChatStyleRun{.offset = 0,
                   .length = static_cast<std::uint32_t>(kText.size()),
                   .style = ro::ui::kChatItalic,
                   .color = kProbeColor}};
  const std::array<ChatStyleRun, 1> bold{
      ChatStyleRun{.offset = 0,
                   .length = static_cast<std::uint32_t>(kText.size()),
                   .style = ro::ui::kChatBold,
                   .color = kProbeColor}};
  const std::array<ChatStyleRun, 1> plain{ChatStyleRun{
      .offset = 0, .length = static_cast<std::uint32_t>(kText.size()), .color = kProbeColor}};
  // 🔴 L'ÉTAGE D'INTERFACE REPOSE LA POLICE ORDINAIRE faute de fichier : elle
  // ne doit pas passer pour un vrai italique ni un vrai gras.
  ChatRichArt rich{.bold = bench.font, .italic = bench.font};
  bench.style.rich = &rich;
  bench.lines.push_back(ChatLineView{.text = kText, .id = 1, .styles = plain});
  bench.Frame();
  const std::size_t plain_vertices = VerticesOf(kProbeColor).size();
  REQUIRE(plain_vertices > 0);
  CHECK_FALSE(AnySlanted(VerticesOf(kProbeColor)));

  SECTION("l'italique se penche") {
    bench.lines.back().styles = italic;
    bench.lines.back().id = 2;
    bench.Frame();
    CHECK(AnySlanted(VerticesOf(kProbeColor)));
  }
  SECTION("le gras se repasse") {
    bench.lines.back().styles = bold;
    bench.lines.back().id = 2;
    bench.Frame();
    CHECK(VerticesOf(kProbeColor).size() == 2 * plain_vertices);
  }
}
