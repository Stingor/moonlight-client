// LES GESTES DU FIL : l'infobulle d'une adresse, la vignette qui s'agrandit, le
// spoiler qui se révèle, la ligne qui se copie, la pastille des messages
// arrivés et le zoom.
//
// 🔴 UN GESTE NE SE VOIT PAS CASSER : le clic qui ne fait rien ressemble à un
// clic à côté. Le banc joue le geste et relit ce qu'il a produit — l'intention
// rendue, le journal d'ImGui, le presse-papiers.

#include <array>
#include <cfloat>
#include <cstdint>
#include <deque>
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
#include "ro/ui/skin_art.h"
#include "ro/ui/ui_context.h"

using ro::ui::ChatLineView;
using ro::ui::ChatLinkGesture;
using ro::ui::ChatLinkKind;
using ro::ui::ChatLinkSpan;
using ro::ui::ChatSpoilerKey;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr float kLogHeightPixels = 200.0F;
constexpr float kFontSize = 14.0F;
constexpr float kLineGap = 2.0F;
constexpr int kSettleFrames = 3;
constexpr float kSweepStepPixels = 3.0F;
constexpr float kSweepInsetPixels = 30.0F;
// Un peu à droite du bord d'un mot : dedans, sans dépendre du dernier pixel.
constexpr float kInsidePixels = 3.0F;
// Assez de lignes pour que le fil défile.
constexpr int kLongThread = 60;
constexpr std::uint8_t kHour = 12;
constexpr std::uint8_t kMinute = 34;
constexpr std::uint8_t kSecond = 56;

constexpr std::string_view kUrlLine = "voir https://x.y/a.png";
constexpr std::uint32_t kUrlAt = 5;
constexpr std::uint32_t kUrlToken = 9;
constexpr ImTextureID kPictureTexture = 0xFACE;
constexpr float kPicturePixels = 48.0F;
constexpr std::uint32_t kItemId = 7;

// UN FIL QUI TOURNE : un vrai contexte ImGui, sans GPU. Ce qu'il écrit se relit
// dans `logged`, ce qu'il copie dans `clipboard`.
struct ThreadBench {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  ro::ui::ChatLogStyle style{.font_size = kFontSize, .line_gap = kLineGap};
  // Les textes que les vues regardent : une file ne déplace pas ce qu'elle tient.
  std::deque<std::string> texts;
  std::vector<ChatLineView> lines;
  ro::ui::ChatLogLayout layout;
  ro::ui::ChatLogAction action;
  std::string logged;
  std::string clipboard;
  ImFont* font = nullptr;

  ThreadBench() {
    ImGuiPlatformIO& platform = ImGui::GetPlatformIO();
    platform.Platform_ClipboardUserData = &clipboard;
    platform.Platform_SetClipboardTextFn = [](ImGuiContext* /*context*/, const char* text) {
      *static_cast<std::string*>(ImGui::GetPlatformIO().Platform_ClipboardUserData) = text;
    };
  }

  ChatLineView& Add(std::string text, std::uint64_t id) {
    texts.push_back(std::move(text));
    lines.push_back(ChatLineView{.text = texts.back(), .id = id, .seq = id});
    return lines.back();
  }
  void AddMany(int count, std::uint64_t first_id) {
    for (int rank = 0; rank < count; ++rank) {
      Add("Ligne " + std::to_string(rank), first_id + static_cast<std::uint64_t>(rank));
    }
  }

  void Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    ImGui::LogToBuffer();
    ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
    ImGui::SetNextWindowSize(ImVec2(kViewport.width_px, kLogHeightPixels * 2.0F));
    ImGui::Begin("banc_fil");
    font = ImGui::GetFont();
    action = ro::ui::DrawChatLog(lines, style, kLogHeightPixels, art, &layout);
    ImGui::End();
    logged = ImGui::GetCurrentContext()->LogBuffer.c_str();
    ImGui::LogFinish();
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
  // La fenêtre du fil lui-même, enfant du banc.
  [[nodiscard]] static ImGuiWindow* Thread() {
    ImGuiWindow* found = nullptr;
    for (ImGuiWindow* window : GImGui->Windows) {
      if (window->ParentWindow != nullptr &&
          std::string_view(window->ParentWindow->Name) == "banc_fil" &&
          std::string_view(window->Name).find("##fil") != std::string_view::npos) {
        found = window;
      }
    }
    REQUIRE(found != nullptr);
    return found;
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
  void Click(ImGuiMouseButton button) {
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseButtonEvent(button, true);
    Frame();
    io.AddMouseButtonEvent(button, false);
    Frame();
  }
  [[nodiscard]] bool Shows(std::string_view text) const {
    return logged.find(text) != std::string::npos;
  }

  // Survole les entrées du menu ouvert de haut en bas jusqu'à `entry`.
  bool HoverEntry(const char* entry) {
    REQUIRE_FALSE(GImGui->OpenPopupStack.empty());
    ImGuiWindow* menu = GImGui->OpenPopupStack.back().Window;
    REQUIRE(menu != nullptr);
    const ImGuiID wanted = menu->GetID(entry);
    const float x = menu->Pos.x + kSweepInsetPixels;
    for (float y = menu->Pos.y; y < menu->Pos.y + menu->Size.y; y += kSweepStepPixels) {
      ImGui::GetIO().AddMousePosEvent(x, y);
      Frame();
      Frame();
      if (GImGui->HoveredId == wanted) {
        return true;
      }
    }
    return false;
  }
};

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

// Une ligne qui porte une adresse, et ce qu'on en sait.
[[nodiscard]] ChatLinkSpan UrlLink(const ro::ui::ChatUrlFacts* facts) {
  return ChatLinkSpan{.offset = kUrlAt,
                      .length = static_cast<std::uint32_t>(kUrlLine.size()) - kUrlAt,
                      .kind = ChatLinkKind::kUrl,
                      .token = kUrlToken,
                      .url = facts};
}

}  // namespace

TEST_CASE("Gestes du fil : l'infobulle d'une adresse dit l'adresse, puis l'aperçu",
          "[ui][chatlog]") {
  ThreadBench bench;
  ro::ui::ChatUrlFacts facts{.host = "x.y"};
  const std::array<ChatLinkSpan, 1> links{UrlLink(&facts)};
  bench.Add(std::string(kUrlLine), 1).links = links;
  bench.Settle();
  const float on_url = bench.Width("voir ") + kInsidePixels;

  SECTION("sans aperçu demandé, l'adresse seule") {
    bench.HoverAt(on_url);
    CHECK(bench.action.link.hovering);
    CHECK(bench.action.link.hovered_kind == ChatLinkKind::kUrl);
    CHECK(bench.Shows("https://x.y/a.png"));
    CHECK_FALSE(bench.Shows("Aperçu"));
  }
  SECTION("un site non admis le dit, et dit comment l'afficher") {
    facts.statuses = true;
    bench.HoverAt(on_url);
    CHECK(bench.Shows("https://x.y/a.png"));
    CHECK(bench.Shows("x.y n'est pas dans vos sites autorisés"));
  }
  SECTION("un aperçu en route le dit") {
    facts.statuses = true;
    facts.allowed = true;
    facts.preview = ro::ui::ChatPictureState::kPending;
    bench.HoverAt(on_url);
    CHECK(bench.Shows("Chargement de l'aperçu"));
  }
  SECTION("un aperçu prêt se montre seul, sans l'adresse") {
    facts.statuses = true;
    facts.allowed = true;
    facts.preview = ro::ui::ChatPictureState::kReady;
    facts.texture = kPictureTexture;
    facts.width_px = kPicturePixels;
    facts.height_px = kPicturePixels;
    bench.HoverAt(on_url);
    CHECK(Drew(kPictureTexture));
    CHECK_FALSE(bench.Shows("https://x.y/a.png"));
  }
  SECTION("un aperçu en échec le dit") {
    facts.statuses = true;
    facts.allowed = true;
    facts.preview = ro::ui::ChatPictureState::kFailed;
    bench.HoverAt(on_url);
    CHECK(bench.Shows("Aperçu indisponible."));
  }
}

TEST_CASE("Gestes du fil : le clic sur une vignette l'agrandit", "[ui][chatlog]") {
  ThreadBench bench;
  const ro::ui::ChatUrlFacts facts{.host = "x.y", .allowed = true};
  const std::array<ChatLinkSpan, 1> links{UrlLink(&facts)};
  const std::array<ro::ui::ChatInlineImage, 1> images{
      ro::ui::ChatInlineImage{.offset = kUrlAt,
                              .length = static_cast<std::uint32_t>(kUrlLine.size()) - kUrlAt,
                              .kind = ro::ui::ChatInlineKind::kPicture,
                              .state = ro::ui::ChatPictureState::kReady,
                              .texture = kPictureTexture,
                              .width_px = kPicturePixels,
                              .height_px = kPicturePixels}};
  const ro::ui::ChatRichArt rich{.picture_pixels = kPicturePixels};
  bench.style.rich = &rich;
  ChatLineView& line = bench.Add(std::string(kUrlLine), 1);
  line.links = links;
  line.images = images;
  bench.Settle();
  bench.HoverAt(bench.Width("voir ") + kInsidePixels);
  // L'intention se rend à l'image du relâchement : `Click` s'arrête sur elle.
  bench.Click(ImGuiMouseButton_Left);
  CHECK(bench.action.link.gesture == ChatLinkGesture::kViewImage);
  CHECK(bench.action.link.kind == ChatLinkKind::kUrl);
  CHECK(bench.action.link.token == kUrlToken);
}

TEST_CASE("Gestes du fil : un spoiler se révèle au clic, sans autre geste", "[ui][chatlog]") {
  ThreadBench bench;
  constexpr std::string_view kText = "le secret est ici";
  constexpr std::uint32_t kSpoilerAt = 14;
  const std::array<ro::ui::ChatStyleRun, 1> runs{ro::ui::ChatStyleRun{
      .offset = kSpoilerAt, .length = 3, .style = ro::ui::kChatSpoiler, .spoiler = 1}};
  // Le spoiler cache un lien : le clic qui révèle ne l'ouvre pas.
  const std::array<ChatLinkSpan, 1> links{ChatLinkSpan{.offset = kSpoilerAt,
                                                       .length = 3,
                                                       .kind = ChatLinkKind::kItem,
                                                       .item_id = kItemId,
                                                       .token = 1}};
  constexpr std::uint64_t kLineId = 5;
  ChatLineView& line = bench.Add(std::string(kText), kLineId);
  line.styles = runs;
  line.links = links;
  bench.Settle();
  const ChatSpoilerKey key{.line_id = kLineId};
  REQUIRE_FALSE(bench.layout.Revealed(key));

  const float on_spoiler = bench.Width("le secret est ") + kInsidePixels;
  bench.HoverAt(on_spoiler);
  CHECK(bench.Shows("Cliquer pour révéler"));
  CHECK_FALSE(bench.action.link.hovering);
  ImGuiIO& io = ImGui::GetIO();
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  bench.Frame();
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  bench.Frame();
  CHECK(bench.layout.Revealed(key));
  CHECK(bench.action.link.gesture == ChatLinkGesture::kNothing);

  // Révélé, le lien répond de nouveau.
  bench.HoverAt(on_spoiler);
  CHECK(bench.action.link.hovering);
  CHECK(bench.action.link.hovered_id == kItemId);
}

TEST_CASE("Gestes du fil : les révélations se comparent champ à champ, et sont bornées",
          "[ui][chatlog]") {
  ro::ui::ChatLogLayout layout;
  layout.Reveal(ChatSpoilerKey{.line_text = "abc"});
  CHECK(layout.Revealed(ChatSpoilerKey{.line_text = "abc"}));
  CHECK_FALSE(layout.Revealed(ChatSpoilerKey{.line_text = "abd"}));
  CHECK_FALSE(layout.Revealed(ChatSpoilerKey{.line_id = 1}));

  // Au-delà de la borne, les plus anciennes partent.
  constexpr std::uint64_t kMany = ro::ui::kMostRevealedSpoilers + 10;
  for (std::uint64_t id = 1; id <= kMany; ++id) {
    layout.Reveal(ChatSpoilerKey{.line_id = id});
  }
  CHECK_FALSE(layout.Revealed(ChatSpoilerKey{.line_text = "abc"}));
  CHECK_FALSE(layout.Revealed(ChatSpoilerKey{.line_id = 1}));
  CHECK(layout.Revealed(ChatSpoilerKey{.line_id = kMany}));
  CHECK(layout.Revealed(ChatSpoilerKey{.line_id = kMany - ro::ui::kMostRevealedSpoilers + 1}));
}

TEST_CASE("Gestes du fil : ce que copie une ligne", "[ui][chatlog]") {
  constexpr std::string_view kText = "Bob : salut";
  const ChatLineView line{.text = kText,
                          .tag = "Public",
                          .hour = kHour,
                          .minute = kMinute,
                          .second = kSecond,
                          .body_offset = 6};
  CHECK(ro::ui::ChatLineCopyText(line, true, false) == "[12:34:56] [Public] Bob : salut");
  CHECK(ro::ui::ChatLineCopyText(line, false, false) == "[Public] Bob : salut");
  CHECK(ro::ui::ChatLineCopyText(line, true, true) == "salut");
  // Une borne hors du texte ne fait pas lire à côté.
  ChatLineView beyond = line;
  beyond.body_offset = 1000;
  CHECK(ro::ui::ChatLineCopyText(beyond, true, true).empty());
}

TEST_CASE("Gestes du fil : un spoiler copié reprend ses barres", "[ui][chatlog]") {
  // 🔴 COLLÉ AILLEURS, UN SPOILER DOIT RESTER CACHÉ : sans ses barres, la copie
  // le livrerait en clair.
  constexpr std::string_view kText = "Ana : le secret est ici, et la";
  // « ici » en spoiler, coupé en deux suites dont une en gras ; « la » en
  // second spoiler.
  const std::array<ro::ui::ChatStyleRun, 3> runs{
      ro::ui::ChatStyleRun{.offset = 20, .length = 1, .style = ro::ui::kChatSpoiler, .spoiler = 1},
      ro::ui::ChatStyleRun{
          .offset = 21,
          .length = 2,
          .style = static_cast<std::uint8_t>(ro::ui::kChatSpoiler | ro::ui::kChatBold),
          .spoiler = 1},
      ro::ui::ChatStyleRun{.offset = 28, .length = 2, .style = ro::ui::kChatSpoiler, .spoiler = 2}};
  const ChatLineView line{.text = kText, .styles = runs, .body_offset = 6};
  CHECK(ro::ui::ChatLineCopyText(line, false, false) == "Ana : le secret est ||ici||, et ||la||");
  CHECK(ro::ui::ChatLineCopyText(line, false, true) == "le secret est ||ici||, et ||la||");
}

TEST_CASE("Gestes du fil : le clic droit hors d'un lien copie la ligne", "[ui][chatlog]") {
  // ⚠ UNE LIGNE SANS IDENTITÉ, qui part avant le choix : ce qui se copie a été
  // retenu au clic, et ne se retrouve pas par un rang devenu faux.
  ThreadBench bench;
  bench.style.timestamps = true;
  ChatLineView& line = bench.Add("Bob : salut", 0);
  line.hour = kHour;
  line.minute = kMinute;
  line.second = kSecond;
  line.body_offset = 6;
  bench.Settle();
  bench.HoverAt(kSweepInsetPixels);
  bench.Click(ImGuiMouseButton_Right);
  bench.Settle();
  REQUIRE_FALSE(GImGui->OpenPopupStack.empty());
  CHECK(bench.Shows("Copier la ligne"));
  bench.lines.clear();
  bench.Add("Une autre ligne a pris sa place", 0);

  SECTION("la ligne entière") {
    REQUIRE(bench.HoverEntry("Copier la ligne"));
    bench.Click(ImGuiMouseButton_Left);
    CHECK(bench.clipboard == "[12:34:56] Bob : salut");
  }
  SECTION("le texte seul") {
    REQUIRE(bench.HoverEntry("Copier le texte seul"));
    bench.Click(ImGuiMouseButton_Left);
    CHECK(bench.clipboard == "salut");
  }
}

namespace {

// Remonte le fil d'un cran de molette, et le laisse se poser.
void ScrollUp(ThreadBench& bench) {
  const ImGuiWindow* thread = ThreadBench::Thread();
  ImGuiIO& io = ImGui::GetIO();
  io.AddMousePosEvent(thread->Pos.x + kSweepInsetPixels, thread->Pos.y + kSweepInsetPixels);
  bench.Frame();
  io.AddMouseWheelEvent(0.0F, 1.0F);
  bench.Settle();
  REQUIRE(thread->Scroll.y < thread->ScrollMax.y);
}

}  // namespace

TEST_CASE("Gestes du fil : décollé du bas, la pastille compte ce qui arrive, et recolle",
          "[ui][chatlog]") {
  ThreadBench bench;
  bench.AddMany(kLongThread, 1);
  bench.Settle();
  CHECK(bench.layout.seen_through() == static_cast<std::uint64_t>(kLongThread));
  ScrollUp(bench);
  CHECK_FALSE(bench.Shows("nouveau"));

  constexpr int kArrived = 3;
  bench.AddMany(kArrived, kLongThread + 1);
  bench.Settle();
  CHECK(bench.Shows("3 nouveaux messages"));
  const ImGuiWindow* thread = ThreadBench::Thread();
  CHECK(thread->Scroll.y < thread->ScrollMax.y);

  // Le clic sur la pastille recolle le fil en bas.
  constexpr float kPillFromBottom = 12.0F;
  ImGuiIO& io = ImGui::GetIO();
  io.AddMousePosEvent(thread->Pos.x + thread->Size.x * 0.5F,
                      thread->Pos.y + thread->Size.y - kPillFromBottom);
  bench.Frame();
  bench.Click(ImGuiMouseButton_Left);
  bench.Settle();
  CHECK(thread->Scroll.y == thread->ScrollMax.y);
  CHECK_FALSE(bench.Shows("nouveaux messages"));
  CHECK(bench.layout.seen_through() == static_cast<std::uint64_t>(kLongThread + kArrived));
}

TEST_CASE("Gestes du fil : les non-lus se comptent par la place dans le fil", "[ui][chatlog]") {
  ThreadBench bench;
  bench.AddMany(kLongThread, 1);
  bench.Settle();
  ScrollUp(bench);

  SECTION("une ligne qui change d'aspect n'est pas nouvelle") {
    // Les bits hauts de l'identité changent — le diagnostic, la décoration —,
    // sa place non.
    constexpr std::uint64_t kAspectBit = std::uint64_t{1} << 56;
    for (ChatLineView& line : bench.lines) {
      line.id |= kAspectBit;
    }
    bench.Settle();
    CHECK_FALSE(bench.Shows("nouveau"));
  }
  SECTION("une ligne sans identité, mais placée, compte") {
    ChatLineView& arrived = bench.Add("Sans identité", 0);
    arrived.seq = kLongThread + 1;
    bench.Settle();
    CHECK(bench.Shows("1 nouveau message"));
  }
  SECTION("une ligne sans place ne compte pas") {
    ChatLineView& arrived = bench.Add("Sans place", kLongThread + 1);
    arrived.seq = 0;
    bench.Settle();
    CHECK_FALSE(bench.Shows("nouveau"));
  }
}

TEST_CASE("Gestes du fil : Ctrl + molette rend un cran, sans faire défiler", "[ui][chatlog]") {
  ThreadBench bench;
  bench.AddMany(kLongThread, 1);
  bench.Settle();
  const ImGuiWindow* thread = ThreadBench::Thread();
  ImGuiIO& io = ImGui::GetIO();
  io.AddMousePosEvent(thread->Pos.x + kSweepInsetPixels, thread->Pos.y + kSweepInsetPixels);
  bench.Frame();
  const float before = thread->Scroll.y;

  io.AddKeyEvent(ImGuiMod_Ctrl, true);
  bench.Frame();
  io.AddMouseWheelEvent(0.0F, 1.0F);
  bench.Frame();
  CHECK(bench.action.zoom_steps == 1);
  CHECK(bench.Shows("100 %"));
  io.AddMouseWheelEvent(0.0F, -1.0F);
  bench.Frame();
  CHECK(bench.action.zoom_steps == -1);
  bench.Settle();
  CHECK(bench.action.zoom_steps == 0);
  // ImGui ne fait pas défiler sous Ctrl.
  CHECK(thread->Scroll.y == before);

  // Sans Ctrl, la même molette défile et ne zoome pas.
  io.AddKeyEvent(ImGuiMod_Ctrl, false);
  bench.Frame();
  io.AddMouseWheelEvent(0.0F, 1.0F);
  bench.Frame();
  CHECK(bench.action.zoom_steps == 0);
  bench.Settle();
  CHECK(thread->Scroll.y < before);
}

TEST_CASE("Gestes du fil : la molette fine se cumule en un seul cran", "[ui][chatlog]") {
  ThreadBench bench;
  bench.AddMany(kLongThread, 1);
  bench.Settle();
  const ImGuiWindow* thread = ThreadBench::Thread();
  ImGuiIO& io = ImGui::GetIO();
  io.AddMousePosEvent(thread->Pos.x + kSweepInsetPixels, thread->Pos.y + kSweepInsetPixels);
  io.AddKeyEvent(ImGuiMod_Ctrl, true);
  bench.Frame();
  constexpr int kTenths = 10;
  constexpr float kTenth = 0.1F;
  int steps = 0;
  int frames_with_step = 0;
  for (int rank = 0; rank < kTenths; ++rank) {
    io.AddMouseWheelEvent(0.0F, kTenth);
    bench.Frame();
    steps += bench.action.zoom_steps;
    frames_with_step += bench.action.zoom_steps != 0 ? 1 : 0;
  }
  CHECK(steps == 1);
  CHECK(frames_with_step == 1);
  io.AddKeyEvent(ImGuiMod_Ctrl, false);
  bench.Frame();
}

// ── Le menu d'un lien, ouvert sur plusieurs images ──────────────────────────

namespace {

constexpr std::string_view kAliceLine = "Alice : salut";
constexpr std::string_view kBobLine = "Bob : salut";
constexpr std::uint32_t kAliceNameLength = 5;
constexpr std::uint32_t kBobNameLength = 3;
constexpr std::uint32_t kFirstToken = 12;
constexpr std::uint32_t kSecondToken = 13;
constexpr std::uint64_t kAliceSeq = 1;
constexpr std::uint64_t kBobSeq = 2;
// Le premier rang des lignes qui arrivent ensuite.
constexpr std::uint64_t kLaterSeq = 100;
constexpr const char* kWhisperEntry = "Chuchoter…";

[[nodiscard]] ChatLinkSpan PlayerLink(std::uint32_t length, std::uint32_t token) {
  return ChatLinkSpan{.offset = 0, .length = length, .kind = ChatLinkKind::kPlayer, .token = token};
}

// Deux lignes qui nomment chacune un joueur, et le menu ouvert sur la première.
struct OpenMenuBench {
  ThreadBench bench;
  std::array<ChatLinkSpan, 1> alice{PlayerLink(kAliceNameLength, kFirstToken)};
  std::array<ChatLinkSpan, 1> bob{PlayerLink(kBobNameLength, kSecondToken)};

  OpenMenuBench() {
    bench.Add(std::string(kAliceLine), kAliceSeq).links = alice;
    bench.Add(std::string(kBobLine), kBobSeq).links = bob;
    bench.Settle();
    bench.HoverAt(kInsidePixels);
    bench.Click(ImGuiMouseButton_Right);
    bench.Settle();
    REQUIRE_FALSE(GImGui->OpenPopupStack.empty());
    REQUIRE(bench.Shows("Alice"));
  }
  // Ce que le menu rend quand on choisit « Chuchoter ».
  [[nodiscard]] ro::ui::ChatLinkAsk ChooseWhisper() {
    REQUIRE(bench.HoverEntry(kWhisperEntry));
    ImGuiIO& io = ImGui::GetIO();
    ro::ui::ChatLinkAsk chosen;
    const auto frame = [&] {
      bench.Frame();
      if (bench.action.link.gesture != ChatLinkGesture::kNothing) {
        chosen = bench.action.link;
      }
    };
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    frame();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    for (int settle = 0; settle < kSettleFrames; ++settle) {
      frame();
    }
    return chosen;
  }
};

}  // namespace

TEST_CASE("Menu d'un lien : il suit son lien quand les jetons se renumérotent", "[ui][chatlog]") {
  OpenMenuBench open;
  // UNE LIGNE ARRIVÉE AILLEURS a décalé les jetons : celui d'Alice est à présent
  // celui que Bob portait à l'ouverture.
  open.alice[0].token = kSecondToken;
  open.bob[0].token = kSecondToken + 1;
  open.bench.Frame();
  CHECK(open.bench.Shows("Alice"));
  CHECK_FALSE(open.bench.Shows("Bob :"));
  const ro::ui::ChatLinkAsk chosen = open.ChooseWhisper();
  CHECK(chosen.kind == ChatLinkKind::kPlayer);
  // 🔴 LE JETON DE CETTE IMAGE-CI, celui qu'`app` résoudra vers Alice.
  CHECK(chosen.token == kSecondToken);
}

TEST_CASE("Menu d'un lien : remontée hors de la vue, sa ligne reste la cible", "[ui][chatlog]") {
  OpenMenuBench open;
  // Le fil collé au bas pousse les deux lignes hors de la vue.
  open.bench.AddMany(kLongThread, kLaterSeq);
  open.bench.Settle();
  REQUIRE_FALSE(GImGui->OpenPopupStack.empty());
  CHECK(open.bench.Shows("Alice"));
  const ro::ui::ChatLinkAsk chosen = open.ChooseWhisper();
  CHECK(chosen.kind == ChatLinkKind::kPlayer);
  CHECK(chosen.token == kFirstToken);
}

TEST_CASE("Menu d'un lien : sa ligne partie, il se ferme sans rien viser", "[ui][chatlog]") {
  OpenMenuBench open;
  // L'ÉVICTION : la ligne d'Alice quitte le fil, et Bob prend son rang.
  open.bench.lines.erase(open.bench.lines.begin());
  open.bench.Settle();
  CHECK(GImGui->OpenPopupStack.empty());
  CHECK(open.bench.action.link.gesture == ChatLinkGesture::kNothing);
}
