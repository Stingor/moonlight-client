// AVANT D'OUVRIR UNE ADRESSE, ET AVANT D'ADMETTRE UN SITE : ce que la boîte
// montre, ce qu'elle tait, et ce qu'elle rend.
//
// 🔴 UN GARDE-FOU NE SE VOIT PAS CASSER : un avertissement qui ne s'écrit plus,
// une case « toujours » offerte en http, une boîte congédiée qui ouvre quand
// même. Le banc les cherche dans ce que la boîte écrit et dans ce qu'elle rend.

#include "ro/ui/chat_url_prompt.h"

#include <string>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/web_address.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/ui_context.h"

using ro::ui::ChatUrlAnswer;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr int kSettleFrames = 3;
// Où le joueur vient de cliquer le lien.
constexpr ImVec2 kClickedAt(300.0F, 200.0F);
constexpr std::string_view kOpenable = "https://cdn.discordapp.com/attachments/1/2/pog.png";
constexpr std::string_view kHost = "cdn.discordapp.com";

// UNE BOÎTE QUI TOURNE : un vrai contexte ImGui, sans GPU. Ce qu'elle écrit se
// relit dans `logged`.
struct PromptBench {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  ro::ui::ChatUrlPrompt url;
  ro::ui::ChatHostPrompt host;
  std::string logged;

  PromptBench() {
    url.written = kOpenable;
    url.openable = kOpenable;
    url.host = kHost;
    host.host = kHost;
  }

  void Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    ImGui::LogToBuffer();
    ro::ui::DrawChatUrlPrompt(url, art);
    ro::ui::DrawChatHostPrompt(host, art);
    logged = ImGui::GetCurrentContext()->LogBuffer.c_str();
    ImGui::LogFinish();
    ui.EndFrame();
  }

  void Settle() {
    for (int frame = 0; frame < kSettleFrames; ++frame) {
      Frame();
    }
  }

  // Demande la boîte d'adresse sous le clic, et la laisse se poser.
  void AskUrl() {
    ImGui::GetIO().AddMousePosEvent(kClickedAt.x, kClickedAt.y);
    url.asked = true;
    Settle();
    REQUIRE_FALSE(GImGui->OpenPopupStack.empty());
  }

  // Presse ce bouton, ou cette case, de la boîte ouverte.
  void Press(const char* label) {
    REQUIRE_FALSE(GImGui->OpenPopupStack.empty());
    ImGuiWindow* box = GImGui->OpenPopupStack.back().Window;
    REQUIRE(box != nullptr);
    ImGui::ActivateItemByID(box->GetID(label));
    Settle();
  }

  [[nodiscard]] bool Shows(std::string_view text) const {
    return logged.find(text) != std::string::npos;
  }
};

}  // namespace

TEST_CASE("Adresse : la boîte montre l'adresse qui partira, et son site", "[ui][chat][url]") {
  PromptBench bench;
  bench.AskUrl();
  CHECK(bench.Shows("Ce lien vient d'un autre joueur."));
  CHECK(bench.Shows("Il ouvrira votre navigateur sur :"));
  CHECK(bench.Shows(kOpenable));
  CHECK(bench.Shows("Site :"));
  CHECK(bench.Shows(kHost));
  // Sans avertissement ni offre, rien d'autre.
  CHECK_FALSE(bench.Shows("Toujours ouvrir les liens de"));
  CHECK_FALSE(bench.Shows("Connexion non chiffrée"));
}

TEST_CASE("Adresse : les trois réponses", "[ui][chat][url]") {
  struct Case {
    const char* button;
    ChatUrlAnswer answer;
  };
  for (const Case& one :
       {Case{"Ouvrir", ChatUrlAnswer::kOpen}, Case{"Annuler", ChatUrlAnswer::kCancel},
        Case{"Copier", ChatUrlAnswer::kCopy}}) {
    PromptBench bench;
    bench.AskUrl();
    REQUIRE(bench.url.answer == ChatUrlAnswer::kNone);
    bench.Press(one.button);
    CHECK(bench.url.answer == one.answer);
    CHECK(GImGui->OpenPopupStack.empty());
    // ⚠ UNE RÉPONSE RENDUE N'EST PAS RÉÉCRITE : l'appelant la consomme, et la
    // boîte fermée par son bouton ne se change pas ensuite en « Annuler ».
    bench.url.answer = ChatUrlAnswer::kNone;
    bench.Settle();
    CHECK(bench.url.answer == ChatUrlAnswer::kNone);
  }
}

TEST_CASE("Adresse : congédiée par Échap, la boîte n'ouvre rien", "[ui][chat][url]") {
  PromptBench bench;
  bench.AskUrl();
  ImGuiIO& io = ImGui::GetIO();
  io.AddKeyEvent(ImGuiKey_Escape, true);
  bench.Frame();
  io.AddKeyEvent(ImGuiKey_Escape, false);
  bench.Settle();
  CHECK(GImGui->OpenPopupStack.empty());
  CHECK(bench.url.answer == ChatUrlAnswer::kCancel);
}

TEST_CASE("Adresse : la boîte naît sous le clic", "[ui][chat][url]") {
  PromptBench bench;
  bench.AskUrl();
  const ImGuiWindow* box = GImGui->OpenPopupStack.back().Window;
  REQUIRE(box != nullptr);
  CHECK(box->Pos.x == kClickedAt.x);
  CHECK(box->Pos.y == kClickedAt.y);
}

TEST_CASE("Adresse : en http, un avertissement et pas de « toujours »", "[ui][chat][url]") {
  PromptBench bench;
  bench.url.openable = "http://example.com/a";
  bench.url.host = "example.com";
  bench.url.warnings = ro::core::kWebWarnPlainHttp;
  // 🔴 MÊME OFFERTE PAR L'APPELANT : la boîte tient la règle elle-même.
  bench.url.offer_always = true;
  bench.AskUrl();
  CHECK(bench.Shows("Connexion non chiffrée (http)"));
  CHECK_FALSE(bench.Shows("Toujours ouvrir les liens de"));
}

TEST_CASE("Adresse : une ligne par avertissement", "[ui][chat][url]") {
  PromptBench bench;
  bench.url.openable = "http://paypal.com@xn--pypal-4ve.com/";
  bench.url.host = "xn--pypal-4ve.com";
  bench.url.warnings = ro::core::kWebWarnPlainHttp | ro::core::kWebWarnUserInfo |
                       ro::core::kWebWarnPunycode | ro::core::kWebWarnNonAsciiHost;
  bench.AskUrl();
  CHECK(bench.Shows("Connexion non chiffrée (http)"));
  // 🔴 LE PIÈGE DU « @ » NOMME LE SITE QUI S'OUVRIRA VRAIMENT.
  CHECK(bench.Shows("c'est xn--pypal-4ve.com qui s'ouvrira."));
  CHECK(bench.Shows("Nom de domaine internationalisé (xn--)"));
  CHECK(bench.Shows("Le nom du site contient des caractères non latins"));
}

TEST_CASE("Adresse : « toujours » se coche, et voyage avec la réponse", "[ui][chat][url]") {
  PromptBench bench;
  bench.url.offer_always = true;
  bench.AskUrl();
  const std::string always = "Toujours ouvrir les liens de " + std::string(kHost);
  REQUIRE(bench.Shows(always));
  CHECK_FALSE(bench.url.always);
  bench.Press(always.c_str());
  CHECK(bench.url.always);
  bench.Press("Ouvrir");
  CHECK(bench.url.answer == ChatUrlAnswer::kOpen);
  CHECK(bench.url.always);
  // UNE NOUVELLE DEMANDE REPART DÉCOCHÉE : l'accord ne vaut que pour son site.
  bench.url.answer = ChatUrlAnswer::kNone;
  bench.AskUrl();
  CHECK_FALSE(bench.url.always);
}

TEST_CASE("Adresse : « toujours » coché ne survit qu'à « Ouvrir »", "[ui][chat][url]") {
  for (const char* button : {"Annuler", "Copier", "Échap"}) {
    PromptBench bench;
    bench.url.offer_always = true;
    bench.AskUrl();
    bench.Press(("Toujours ouvrir les liens de " + std::string(kHost)).c_str());
    REQUIRE(bench.url.always);
    if (std::string_view(button) == "Échap") {
      ImGuiIO& io = ImGui::GetIO();
      io.AddKeyEvent(ImGuiKey_Escape, true);
      bench.Frame();
      io.AddKeyEvent(ImGuiKey_Escape, false);
      bench.Settle();
    } else {
      bench.Press(button);
    }
    CHECK(bench.url.answer != ChatUrlAnswer::kOpen);
    CHECK_FALSE(bench.url.always);
  }
}

TEST_CASE("Adresse : sans site connu, pas de « toujours »", "[ui][chat][url]") {
  PromptBench bench;
  bench.url.offer_always = true;
  bench.url.host.clear();
  bench.AskUrl();
  CHECK_FALSE(bench.Shows("Toujours ouvrir les liens de"));
}

TEST_CASE("Les deux boîtes laissent voir le jeu : aucun voile", "[ui][chat][url]") {
  // 🔴 ASSOMBRIR L'ÉCRAN POUR UN CLIC EST DISPROPORTIONNÉ : le chat doit rester
  // lisible pendant qu'on décide. La boîte reste modale pour autant.
  PromptBench bench;
  bench.AskUrl();
  const ImGuiWindow* url_box = GImGui->OpenPopupStack.back().Window;
  REQUIRE(url_box != nullptr);
  CHECK(url_box->DC.ModalDimBgColor == 0U);
  CHECK((url_box->Flags & ImGuiWindowFlags_Modal) != 0);
  bench.Press("Annuler");
  bench.host.asked = true;
  bench.Settle();
  REQUIRE_FALSE(GImGui->OpenPopupStack.empty());
  CHECK(GImGui->OpenPopupStack.back().Window->DC.ModalDimBgColor == 0U);
}

TEST_CASE("Site : « Autoriser » l'admet, « Annuler » non", "[ui][chat][url]") {
  for (const bool grant : {true, false}) {
    PromptBench bench;
    bench.host.asked = true;
    bench.Settle();
    REQUIRE_FALSE(GImGui->OpenPopupStack.empty());
    CHECK(bench.Shows("Les images de ce site se chargeront"));
    CHECK(bench.Shows(kHost));
    CHECK(bench.Shows("Ce serveur pourra alors voir que vous êtes"));
    bench.Press(grant ? "Autoriser" : "Annuler");
    CHECK(bench.host.granted == grant);
    CHECK(GImGui->OpenPopupStack.empty());
  }
}
