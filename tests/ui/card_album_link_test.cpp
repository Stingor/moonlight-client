// « VOIR DANS L'ALBUM » AU MENU D'UN OBJET : l'entrée paraît pour une carte et
// pour elle seule, se grise quand le catalogue ne lui connaît pas de pochette,
// et rend son geste au clic.

#include "ro/ui/card_album_link.h"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/chat_link.h"
#include "ro/ui/ui_context.h"

using ro::ui::CardAlbumEntry;
using ro::ui::CardAlbumEntryOf;
using ro::ui::CardAlbumLinkFacts;
using ro::ui::ChatLinkAsk;
using ro::ui::ChatLinkGesture;
using ro::ui::ChatLinkKind;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr const char* kMenuId = "menu_objet";
constexpr const char* kEntry = "Voir dans l'album";
constexpr const char* kNoPocketTip = "Cette carte n'a pas de pochette dans l'album";
constexpr std::uint32_t kPoring = 4001;
constexpr std::uint32_t kPhreeoni = 4121;
// Une carte que le serveur range sans que les tables la nomment carte.
constexpr std::uint32_t kServerOnlyCard = 4999;
constexpr std::uint32_t kRedPotion = 501;
constexpr ImVec2 kOpenAt(100.0F, 100.0F);
constexpr float kSweepStepPixels = 3.0F;
constexpr float kSweepInsetPixels = 30.0F;
constexpr int kSettleFrames = 3;

constexpr std::array<std::uint32_t, 2> kCards = {kPoring, kPhreeoni};
constexpr std::array<std::uint32_t, 2> kPockets = {kPoring, kServerOnlyCard};

[[nodiscard]] CardAlbumLinkFacts Facts(bool pockets_known) {
  return CardAlbumLinkFacts{.available = true,
                            .card_ids = kCards,
                            .pocket_ids = pockets_known ? std::span<const std::uint32_t>(kPockets)
                                                        : std::span<const std::uint32_t>(),
                            .pockets_known = pockets_known};
}

// LE MENU D'UN LIEN D'OBJET QUI TOURNE : un vrai contexte ImGui, sans GPU. Ce
// qu'il écrit — entrées et infobulles — se relit dans `logged`.
struct ItemMenuBench {
  ro::ui::UiContext ui;
  std::uint32_t item_id = kPoring;
  ro::ui::ItemLinkFacts facts{.album = Facts(true)};
  bool open_asked = true;
  std::string logged;
  ChatLinkAsk answered;

  void Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    ImGui::LogToBuffer();
    ImGui::Begin("banc_objet");
    if (open_asked) {
      ImGui::OpenPopup(kMenuId);
      open_asked = false;
    }
    const ChatLinkAsk asked =
        ro::ui::DrawChatLinkMenu(kMenuId, ChatLinkKind::kItem, item_id, 0, "Carte", facts);
    ImGui::End();
    logged = ImGui::GetCurrentContext()->LogBuffer.c_str();
    ImGui::LogFinish();
    ui.EndFrame();
    if (asked.gesture != ChatLinkGesture::kNothing) {
      answered = asked;
    }
  }

  void Open() {
    ImGui::GetIO().AddMousePosEvent(kOpenAt.x, kOpenAt.y);
    for (int frame = 0; frame < kSettleFrames; ++frame) {
      Frame();
    }
    REQUIRE_FALSE(GImGui->OpenPopupStack.empty());
  }

  [[nodiscard]] bool Shows(std::string_view text) const {
    return logged.find(text) != std::string::npos;
  }

  // Survole les entrées de haut en bas jusqu'à `entry` — ou, grisée, jusqu'à
  // ce que `seen` paraisse.
  bool Hover(const char* entry, const char* seen = nullptr) {
    ImGuiWindow* menu = GImGui->OpenPopupStack.back().Window;
    REQUIRE(menu != nullptr);
    const ImGuiID wanted = menu->GetID(entry);
    const float x = menu->Pos.x + kSweepInsetPixels;
    for (float y = menu->Pos.y; y < menu->Pos.y + menu->Size.y; y += kSweepStepPixels) {
      ImGui::GetIO().AddMousePosEvent(x, y);
      Frame();
      Frame();
      if ((seen == nullptr && GImGui->HoveredId == wanted) || (seen != nullptr && Shows(seen))) {
        return true;
      }
    }
    return false;
  }

  void Click() {
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    Frame();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    for (int frame = 0; frame < kSettleFrames; ++frame) {
      Frame();
    }
  }
};

}  // namespace

TEST_CASE("Voir dans l'album : une carte seulement, et l'album présent", "[ui][album]") {
  const CardAlbumLinkFacts known = Facts(true);
  CHECK(CardAlbumEntryOf(known, kPoring) == CardAlbumEntry::kOffered);
  CHECK(CardAlbumEntryOf(known, kRedPotion) == CardAlbumEntry::kAbsent);
  CHECK(CardAlbumEntryOf(known, 0) == CardAlbumEntry::kAbsent);
  // Une pochette du catalogue est une carte, même inconnue des tables.
  CHECK(CardAlbumEntryOf(known, kServerOnlyCard) == CardAlbumEntry::kOffered);
  // Le serveur a dit d'un objet du sac que c'est une carte.
  CHECK(CardAlbumEntryOf(known, kRedPotion, /*known_card=*/true) == CardAlbumEntry::kNoPocket);
  CardAlbumLinkFacts withdrawn = known;
  withdrawn.available = false;
  CHECK(CardAlbumEntryOf(withdrawn, kPoring) == CardAlbumEntry::kAbsent);
}

TEST_CASE("Voir dans l'album : sans pochette, grisée — sauf catalogue inconnu", "[ui][album]") {
  CHECK(CardAlbumEntryOf(Facts(true), kPhreeoni) == CardAlbumEntry::kNoPocket);
  // 🔴 TANT QUE LE SERVEUR N'A RIEN DIT, une absence ne prouve rien.
  CHECK(CardAlbumEntryOf(Facts(false), kPhreeoni) == CardAlbumEntry::kOffered);
}

TEST_CASE("Voir dans l'album : le menu d'une carte la propose et rend son geste",
          "[ui][album][chat]") {
  ItemMenuBench bench;
  bench.Open();
  CHECK(bench.Shows(kEntry));
  REQUIRE(bench.Hover(kEntry));
  bench.Click();
  CHECK(bench.answered.gesture == ChatLinkGesture::kSeeInAlbum);
  CHECK(bench.answered.kind == ChatLinkKind::kItem);
  CHECK(bench.answered.item_id == kPoring);
}

TEST_CASE("Voir dans l'album : absente du menu d'un autre objet", "[ui][album][chat]") {
  ItemMenuBench bench;
  bench.item_id = kRedPotion;
  bench.Open();
  CHECK(bench.Shows("Ouvrir dans la base de données"));
  CHECK_FALSE(bench.Shows(kEntry));
}

TEST_CASE("Voir dans l'album : sans pochette, grisée et dite au survol", "[ui][album][chat]") {
  ItemMenuBench bench;
  bench.item_id = kPhreeoni;
  bench.Open();
  CHECK(bench.Shows(kEntry));
  CHECK(bench.Hover(kEntry, kNoPocketTip));
  bench.Click();
  CHECK(bench.answered.gesture != ChatLinkGesture::kSeeInAlbum);
}
