// La fenêtre habillée : sa barre reprise, et ce qu'elle laisse à son contenu.

#include "ro/ui/skinned_window.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/modal_box.h"
#include "ro/ui/skin_look_sample.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/ui_context.h"

using ro::render::SkinPiece;
using ro::ui::BeginSkinnedWindow;
using ro::ui::EndSkinnedWindow;
using ro::ui::ShownTitle;
using ro::ui::SkinArt;
using ro::ui::SkinImage;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;

// La hauteur de la barre de titre du client, en pixels d'art.
constexpr float kTitleBarPixels = 17.0F;
constexpr float kWideWindowPixels = 400.0F;

// Un habillage complet, aux tailles du client. Les identifiants ne désignent
// aucune texture : rien n'est peint ici, on ne mesure que la disposition.
[[nodiscard]] SkinArt WholeArt() {
  SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<SkinPiece>(rank), SkinImage{.id = 1, .width = 8.0F, .height = 8.0F});
  }
  art.Set(SkinPiece::kTitleBarLeft, SkinImage{.id = 1, .width = 12.0F, .height = kTitleBarPixels});
  art.Set(SkinPiece::kTitleBarMiddle,
          SkinImage{.id = 1, .width = 12.0F, .height = kTitleBarPixels});
  art.Set(SkinPiece::kTitleBarRight, SkinImage{.id = 1, .width = 12.0F, .height = kTitleBarPixels});
  return art;
}

}  // namespace

TEST_CASE("Fenêtre habillée : le titre montré s'arrête à l'identité", "[ui][skin]") {
  // ⚠ IMGUI SÉPARE LES DEUX PAR `##`, et c'est ce qui permet à un titre traduit
  // de changer sans que la fenêtre perde sa place. Montrer l'identité écrirait
  // « Dialogue###dialogue » dans la barre.
  CHECK(ShownTitle("Dialogue###dialogue") == "Dialogue");
  CHECK(ShownTitle("Journal##2") == "Journal");
  CHECK(ShownTitle("Sans identite") == "Sans identite");
  CHECK(ShownTitle("###tout-cache").empty());
}

TEST_CASE("Fenêtre habillée : sa barre a la hauteur de l'art", "[ui][skin]") {
  // ⚠ CE QU'ON MESURE, C'EST LE RÉGLAGE DE `FramePadding.y`. La hauteur de barre
  // d'ImGui vaut `FontSize + FramePadding.y * 2`, et c'est la SEULE prise qu'il
  // donne dessus : mal réglée, l'art serait étiré sur une barre plus haute que
  // celle du client, son dégradé déformé. Le contenu, lui, commence sous elle
  // tout seul — c'est ImGui qui le place, puisqu'il connaît sa barre.
  ro::ui::UiContext ui;
  const SkinArt art = WholeArt();

  ui.BeginFrame(kViewport, kFrameSeconds);
  ImGui::SetNextWindowPos(ImVec2(100.0F, 100.0F));
  ImGui::SetNextWindowSize(ImVec2(300.0F, 200.0F));
  bool open = true;
  const bool visible = BeginSkinnedWindow("Dialogue###essai", &open, art);
  REQUIRE(visible);
  const float content_y = ImGui::GetCursorScreenPos().y;
  const ImGuiWindow* window = ImGui::GetCurrentWindow();
  const float window_y = window->Pos.y;
  const float bar_height = window->TitleBarHeight;
  EndSkinnedWindow();
  ui.EndFrame();

  CHECK(content_y >= window_y + kTitleBarPixels);
  CHECK(bar_height == kTitleBarPixels);
}

TEST_CASE("Fenêtre habillée : sans art, la fenêtre d'ImGui telle quelle", "[ui][skin]") {
  // Une installation à qui il manque des bitmaps reste jouable : la fenêtre
  // s'ouvre avec sa barre ordinaire, et rien ne se plaint.
  ro::ui::UiContext ui;
  const SkinArt aucun;

  ui.BeginFrame(kViewport, kFrameSeconds);
  bool open = true;
  const bool visible = BeginSkinnedWindow("Nue###nue", &open, aucun);
  CHECK(visible);
  // ImGui a gardé sa barre : elle occupe une hauteur.
  const ImGuiWindow* window = ImGui::GetCurrentWindow();
  CHECK(window->TitleBarHeight > 0.0F);
  EndSkinnedWindow();
  ui.EndFrame();
}

TEST_CASE("Fenêtre habillée : l'habillage réglé peint le corps, le titre et l'art", "[ui][skin]") {
  // ⚠ RÉGLÉ EN JEU, SANS RIEN RECHARGER : la palette se lit à chaque fenêtre.
  ro::ui::UiContext ui;
  SkinArt art = WholeArt();
  ro::ui::SkinLook look;
  look.body = IM_COL32(44, 46, 54, 255);
  look.title_text = IM_COL32(255, 255, 255, 255);
  look.art_brightness = 0.5F;
  look.art_tint = IM_COL32(200, 180, 160, 255);
  look.opacity = 0.75F;
  art.SetLook(look);

  ui.BeginFrame(kViewport, kFrameSeconds);
  bool open = true;
  REQUIRE(BeginSkinnedWindow("Dialogue###habille", &open, art));
  CHECK(ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_WindowBg)) == look.body);
  CHECK(ImGui::GetStyle().Alpha == look.opacity);
  // ⚠ L'ART PORTE L'OPACITÉ, comme tout ce qu'ImGui peint dans la fenêtre : la
  // teinte, sous l'alpha que la fenêtre impose — lue ici, où elle l'impose.
  const ImU32 art_tint = ImGui::GetColorU32(look.ArtTint());
  const ImGuiWindow* window = ImGui::GetCurrentWindow();
  EndSkinnedWindow();
  bool titled = false;
  bool tinted = false;
  for (const ImDrawVert& vertex : window->DrawList->VtxBuffer) {
    titled = titled || vertex.col == look.title_text;
    tinted = tinted || vertex.col == art_tint;
  }
  CHECK(titled);
  CHECK(tinted);
  ui.EndFrame();
}

TEST_CASE("Fenêtre habillée : texte secondaire, liens et sélection viennent de l'habillage",
          "[ui][skin]") {
  // ⚠ CE QUI LES ÉCRIT LES LIT DANS LE STYLE : c'est la palette de la fenêtre
  // qui doit les poser, sans quoi ils restent au gris et au bleu d'ImGui.
  ro::ui::UiContext ui;
  SkinArt art = WholeArt();
  ro::ui::SkinLook look;
  look.faint_text = IM_COL32(150, 154, 166, 255);
  look.link = IM_COL32(110, 170, 255, 255);
  look.selection = IM_COL32(90, 120, 190, 140);
  art.SetLook(look);
  const auto style_color = [](ImGuiCol which) {
    return ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(which));
  };
  ui.BeginFrame(kViewport, kFrameSeconds);
  bool open = true;
  REQUIRE(BeginSkinnedWindow("Dialogue###textes", &open, art));
  CHECK(style_color(ImGuiCol_TextDisabled) == look.faint_text);
  CHECK(style_color(ImGuiCol_TextLink) == look.link);
  CHECK(style_color(ImGuiCol_Header) == look.selection);
  CHECK(style_color(ImGuiCol_HeaderHovered) == look.SelectionHover());
  CHECK(style_color(ImGuiCol_HeaderActive) == look.SelectionActive());
  EndSkinnedWindow();
  ui.EndFrame();
}

TEST_CASE("Lisibilité : une encre lisible ne bouge pas, une autre s'éclaircit juste assez",
          "[ui][skin]") {
  CHECK(ro::ui::ContrastRatio(IM_COL32_WHITE, IM_COL32_BLACK) > 20.9F);
  CHECK(ro::ui::ContrastRatio(IM_COL32_BLACK, IM_COL32_WHITE) > 20.9F);
  CHECK(ro::ui::ContrastRatio(IM_COL32(120, 30, 30, 255), IM_COL32(120, 30, 30, 255)) == 1.0F);
  constexpr ImU32 kPureBlue = IM_COL32(0, 0, 255, 255);
  CHECK(ro::ui::ReadableOn(kPureBlue, IM_COL32_WHITE) == kPureBlue);
  // Sur un papier sombre, le bleu s'éclaircit sans changer de teinte, et garde
  // sa transparence.
  constexpr ImU32 kDarkPaper = IM_COL32(40, 42, 50, 255);
  constexpr ImU32 kHalfBlue = IM_COL32(0, 0, 255, 128);
  const ImU32 shown = ro::ui::ReadableOn(kHalfBlue, kDarkPaper);
  CHECK(ro::ui::ContrastRatio(shown, kDarkPaper) >= ro::ui::kSkinLeastContrast);
  CHECK(((shown >> IM_COL32_B_SHIFT) & 0xFFU) >= ((shown >> IM_COL32_R_SHIFT) & 0xFFU));
  CHECK((shown & IM_COL32_A_MASK) == (kHalfBlue & IM_COL32_A_MASK));
  // Un gris presque blanc sur le blanc se fonce.
  const ImU32 grey = ro::ui::ReadableOn(IM_COL32(235, 235, 235, 255), IM_COL32_WHITE);
  CHECK(ro::ui::ContrastRatio(grey, IM_COL32_WHITE) >= ro::ui::kSkinLeastContrast);
}

TEST_CASE("Papier : une couleur choisie pour du blanc se traduit sur le papier", "[ui][skin]") {
  constexpr ImU32 kNavy = IM_COL32(29, 50, 85, 255);
  constexpr ImU32 kCream = IM_COL32(252, 248, 236, 255);
  constexpr ImU32 kDarkPaper = IM_COL32(40, 42, 50, 255);
  CHECK_FALSE(ro::ui::PaperIsDark(IM_COL32_WHITE));
  CHECK_FALSE(ro::ui::PaperIsDark(kCream));
  CHECK(ro::ui::PaperIsDark(kDarkPaper));
  // Sur du blanc, la couleur telle ; sur un papier clair, multipliée par lui.
  CHECK(ro::ui::OnPaper(kNavy, IM_COL32_WHITE) == kNavy);
  CHECK(ro::ui::OnPaper(IM_COL32_WHITE, kCream) == kCream);
  // Sur un papier sombre, le blanc devient le papier, et le bleu nuit un bleu
  // clair qui s'y lit.
  CHECK(ro::ui::OnPaper(IM_COL32_WHITE, kDarkPaper) == kDarkPaper);
  const ImU32 navy = ro::ui::OnPaper(kNavy, kDarkPaper);
  CHECK(ro::ui::ContrastRatio(navy, kDarkPaper) >= ro::ui::kSkinLeastContrast);
  CHECK(((navy >> IM_COL32_B_SHIFT) & 0xFFU) > ((navy >> IM_COL32_R_SHIFT) & 0xFFU));
  // Un voile garde sa transparence.
  constexpr ImU32 kVeil = IM_COL32(156, 184, 234, 44);
  CHECK((ro::ui::OnPaper(kVeil, kDarkPaper) & IM_COL32_A_MASK) == (kVeil & IM_COL32_A_MASK));
}

TEST_CASE("Papier : un texte choisi pour un corps clair reste tel sur un corps clair",
          "[ui][skin]") {
  // ⚠ NI MULTIPLIÉ NI ÉCLAIRCI : le classique et le sépia le gardent au pixel.
  constexpr ImU32 kSlate = IM_COL32(86, 96, 124, 255);
  constexpr ImU32 kLightBody = IM_COL32(243, 245, 250, 255);
  constexpr ImU32 kDarkBody = IM_COL32(44, 46, 54, 255);
  CHECK(ro::ui::OnBackground(kSlate, kLightBody) == kSlate);
  CHECK(ro::ui::InkOn(kSlate, kLightBody) == kSlate);
  // Sur un corps sombre, retourné, et lisible.
  const ImU32 shown = ro::ui::InkOn(kSlate, kDarkBody);
  CHECK(ro::ui::ContrastRatio(shown, kDarkBody) >= ro::ui::kSkinLeastContrast);
  CHECK(ro::ui::OnBackground(kSlate, kDarkBody) == ro::ui::OnPaper(kSlate, kDarkBody));
}

TEST_CASE("Fenêtre habillée : une description se pose sur le papier", "[ui][skin]") {
  // ⚠ PAS LE CORPS DES FENÊTRES : le papier, et son texte. Le texte grisé et les
  // liens de l'habillage y sont rendus lisibles.
  ro::ui::UiContext ui;
  SkinArt art = WholeArt();
  ro::ui::SkinLook look;
  look.body = IM_COL32(44, 46, 54, 255);
  look.text = IM_COL32(226, 228, 235, 255);
  look.paper = IM_COL32(250, 248, 240, 255);
  look.paper_text = IM_COL32(20, 22, 28, 255);
  look.link = IM_COL32(110, 170, 255, 255);
  art.SetLook(look);
  const auto style_color = [](ImGuiCol which) {
    return ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(which));
  };
  ui.BeginFrame(kViewport, kFrameSeconds);
  bool open = true;
  REQUIRE(
      BeginSkinnedWindow("Description###papier", &open, art, ro::ui::SkinnedTitle::kDescription));
  CHECK(style_color(ImGuiCol_WindowBg) == look.paper);
  CHECK(style_color(ImGuiCol_Text) == look.paper_text);
  CHECK(ro::ui::ContrastRatio(style_color(ImGuiCol_TextLink), look.paper) >=
        ro::ui::kSkinLeastContrast);
  EndSkinnedWindow();
  ui.EndFrame();
}

TEST_CASE("Aperçu de l'habillage : il peint le papier et la sélection du thème", "[ui][skin]") {
  ro::ui::UiContext ui;
  SkinArt art = WholeArt();
  ro::ui::SkinLook look;
  look.paper = IM_COL32(40, 42, 50, 255);
  look.selection = IM_COL32(90, 120, 190, 140);
  art.SetLook(look);
  constexpr float kRoomPixels = 600.0F;
  ui.BeginFrame(kViewport, kFrameSeconds);
  ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
  ImGui::SetNextWindowSize(ImVec2(kRoomPixels, kRoomPixels));
  ImGui::Begin("apercu");
  ImDrawList* const into = ImGui::GetWindowDrawList();
  const int before = into->VtxBuffer.Size;
  CHECK_FALSE(ro::ui::DrawSkinLookSample(art));
  bool paper = false;
  bool selection = false;
  for (int rank = before; rank < into->VtxBuffer.Size; ++rank) {
    paper = paper || into->VtxBuffer[rank].col == look.paper;
    selection = selection || into->VtxBuffer[rank].col == ImGui::GetColorU32(look.selection);
  }
  CHECK(paper);
  CHECK(selection);
  ImGui::End();
  ui.EndFrame();
}

TEST_CASE("Fenêtre habillée : un rôle de texte se résout dans l'habillage", "[ui][skin]") {
  ro::ui::SkinLook look;
  look.text = IM_COL32(1, 2, 3, 255);
  look.faint_text = IM_COL32(4, 5, 6, 255);
  look.good_text = IM_COL32(7, 8, 9, 255);
  look.bad_text = IM_COL32(10, 11, 12, 255);
  look.warn_text = IM_COL32(13, 14, 15, 255);
  look.info_text = IM_COL32(16, 17, 18, 255);
  CHECK(look.Tone(ro::ui::SkinTone::kText) == look.text);
  CHECK(look.Tone(ro::ui::SkinTone::kFaint) == look.faint_text);
  CHECK(look.Tone(ro::ui::SkinTone::kGood) == look.good_text);
  CHECK(look.Tone(ro::ui::SkinTone::kBad) == look.bad_text);
  CHECK(look.Tone(ro::ui::SkinTone::kWarn) == look.warn_text);
  CHECK(look.Tone(ro::ui::SkinTone::kInfo) == look.info_text);
}

TEST_CASE("Fenêtre habillée : la sélection d'origine rend le survol et l'appui du jeu",
          "[ui][skin]") {
  // ⚠ DÉDUITS ET NON RÉGLÉS : un habillage jamais touché doit rendre exactement
  // les couleurs du jeu.
  const ro::ui::SkinLook look;
  CHECK(look.selection == ro::ui::kSkinSelectionColor);
  CHECK(look.SelectionHover() == ro::ui::kSkinSelectionHoverColor);
  CHECK(look.SelectionActive() == ro::ui::kSkinSelectionActiveColor);
}

TEST_CASE("Fenêtre habillée : la barre est celle d'ImGui, repeinte", "[ui][skin]") {
  // ⚠ ELLE A LONGTEMPS ÉTÉ PEINTE DANS LA ZONE DE CONTENU, la barre native
  // retirée. Cela marchait, au prix de trois choses : la fenêtre ne pouvait plus
  // se REPLIER, son ascenseur se plaçait par-dessus le bouton de fermeture, et le
  // début du contenu se reposait à la main. Garder la barre native rend tout cela
  // à ImGui — d'où ce qu'on vérifie ici.
  ro::ui::UiContext ui;
  const SkinArt art = WholeArt();

  ui.BeginFrame(kViewport, kFrameSeconds);
  bool open = true;
  REQUIRE(BeginSkinnedWindow("Dialogue###essai", &open, art));
  const ImGuiWindow* window = ImGui::GetCurrentWindow();
  CHECK((window->Flags & ImGuiWindowFlags_NoTitleBar) == 0);
  // Le déplacement redevient celui d'ImGui, par la barre et par elle seule.
  CHECK((window->Flags & ImGuiWindowFlags_NoMove) == 0);
  // ⚠ ET LE REPLI RESTE POSSIBLE : `NoCollapse` le figerait à faux à chaque
  // image, et le bouton de minimisation qu'on peint ne servirait à rien.
  CHECK((window->Flags & ImGuiWindowFlags_NoCollapse) == 0);
  EndSkinnedWindow();
  ui.EndFrame();
}

TEST_CASE("Fenêtre habillée : repliée, il ne reste que sa barre", "[ui][skin]") {
  // ⚠ C'EST CE QUE LE BOUTON DE MINIMISATION DEMANDE. Une fenêtre repliée n'est
  // plus QUE sa barre de titre : si celle-ci cessait d'être peinte et d'écouter
  // la souris, rien au monde ne permettrait plus de la déplier.
  ro::ui::UiContext ui;
  const SkinArt art = WholeArt();

  // ⚠ UNE IMAGE POUR LA FAIRE NAÎTRE D'ABORD : ImGui déplie une fenêtre qu'il
  // découvre, et l'essai ne mesurerait alors que sa première apparition.
  bool open = true;
  ui.BeginFrame(kViewport, kFrameSeconds);
  static_cast<void>(BeginSkinnedWindow("Dialogue###replie", &open, art));
  EndSkinnedWindow();
  ui.EndFrame();

  ui.BeginFrame(kViewport, kFrameSeconds);
  ImGui::SetNextWindowCollapsed(true);
  // Le contenu ne se dessine pas — c'est ce que « replié » veut dire.
  CHECK_FALSE(BeginSkinnedWindow("Dialogue###replie", &open, art));
  const ImGuiWindow* window = ImGui::GetCurrentWindow();
  REQUIRE(window != nullptr);
  CHECK(window->Collapsed);
  // La barre garde sa hauteur : c'est tout ce qui reste à voir et à cliquer.
  CHECK(window->TitleBarHeight == kTitleBarPixels);
  // 🔴 ET LA POIGNÉE DE REDIMENSIONNEMENT NE SE PEINT PAS. Le coin bas-droit
  // d'une fenêtre repliée est celui de sa BARRE : la poignée s'y posait parmi
  // les boutons système, à promettre un geste qu'ImGui a déjà coupé.
  //
  // ⚠ ON MESURE LES SOMMETS, et non un pixel : c'est la seule preuve qu'un
  // dessin n'a pas eu lieu qui ne dépende d'aucune texture.
  const int before = window->DrawList->VtxBuffer.Size;
  ro::ui::PaintWindowResizeGrip(art);
  CHECK(window->DrawList->VtxBuffer.Size == before);
  EndSkinnedWindow();
  ui.EndFrame();
}

namespace {

// Dessine une fenetre habillee posee a `where`, sa puce cliquable, et rend ce
// que la puce a repondu. La souris est deja placee par l'appelant.
[[nodiscard]] bool FrameWithBullet(ro::ui::UiContext& ui, const SkinArt& art, ImVec2 where) {
  ui.BeginFrame(kViewport, kFrameSeconds);
  ImGui::SetNextWindowPos(where);
  ImGui::SetNextWindowSize(ImVec2(300.0F, 200.0F));
  bool open = true;
  ro::ui::SetNextWindowBulletClickable("Reglages");
  static_cast<void>(BeginSkinnedWindow("Reglages###puce", &open, art));
  const bool clicked = ro::ui::TitleBulletClicked();
  EndSkinnedWindow();
  ui.EndFrame();
  return clicked;
}

}  // namespace

TEST_CASE("Fenêtre habillée : la puce réclamée répond au clic", "[ui][skin]") {
  // ⚠ ELLE EST DÉCORATIVE PAR DÉFAUT : une puce qui se clique sur TOUTE fenêtre
  // promettrait à chacune une action qu'elle n'a pas. Celle-ci a été réclamée.
  ro::ui::UiContext ui;
  const SkinArt art = WholeArt();
  const ImVec2 where(200.0F, 150.0F);

  // Une image pour faire naître la fenêtre, souris ailleurs.
  ImGui::GetIO().MousePos = ImVec2(0.0F, 0.0F);
  ImGui::GetIO().MouseDown[0] = false;
  CHECK_FALSE(FrameWithBullet(ui, art, where));

  // La souris sur la puce, sans appuyer : rien.
  ImGui::GetIO().MousePos =
      ImVec2(where.x + ro::ui::kBulletMarginPixels + 1.0F, where.y + kTitleBarPixels * 0.5F);
  CHECK_FALSE(FrameWithBullet(ui, art, where));

  // Le clic : la puce répond, et une seule fois.
  ImGui::GetIO().MouseDown[0] = true;
  CHECK(FrameWithBullet(ui, art, where));
  CHECK_FALSE(FrameWithBullet(ui, art, where));
  ImGui::GetIO().MouseDown[0] = false;
}

TEST_CASE("Fenêtre habillée : un bouton de barre laisse la barre hors du contenu", "[ui][skin]") {
  // 🔴 LE CONTENU DÉFILÉ SOUS LA BARRE NE PREND PAS SON CLIC : la découpe de la
  // fenêtre s'arrête sous elle, bouton de barre ou non.
  ro::ui::UiContext ui;
  const SkinArt art = WholeArt();
  for (const bool with_action : {false, true}) {
    ui.BeginFrame(kViewport, kFrameSeconds);
    if (with_action) {
      ro::ui::SetNextWindowTitleAction("Signaler", "infobulle", true);
    }
    // ⚠ ASSEZ LARGE POUR QUE LE BOUTON SE POSE : sans place, la barre le saute.
    ImGui::SetNextWindowSize(ImVec2(kWideWindowPixels, kWideWindowPixels));
    bool open = true;
    REQUIRE(BeginSkinnedWindow("Dialogue###decoupe", &open, art));
    const ImGuiWindow* window = ImGui::GetCurrentWindow();
    CHECK(window->ClipRect.Min.y >= window->TitleBarRect().Max.y);
    CHECK(window->ClipRect.Min.y == window->DrawList->_ClipRectStack.back().y);
    EndSkinnedWindow();
    ui.EndFrame();
  }
}

TEST_CASE("Fenêtre habillée : la demande de puce ne déborde pas sur la suivante", "[ui][skin]") {
  // ⚠ CONSOMMÉE PAR LE PREMIER `Begin` QUI LA VOIT, comme les `SetNextWindow…`
  // d'ImGui. Une demande qui survivrait rendrait cliquable la puce de la fenêtre
  // suivante — celle qui n'a rien à offrir derrière.
  ro::ui::UiContext ui;
  const SkinArt art = WholeArt();
  ImGui::GetIO().MousePos = ImVec2(0.0F, 0.0F);

  ui.BeginFrame(kViewport, kFrameSeconds);
  bool open = true;
  ro::ui::SetNextWindowBulletClickable("Reglages");
  static_cast<void>(BeginSkinnedWindow("Premiere###une", &open, art));
  EndSkinnedWindow();
  static_cast<void>(BeginSkinnedWindow("Seconde###deux", &open, art));
  CHECK_FALSE(ro::ui::TitleBulletClicked());
  EndSkinnedWindow();
  ui.EndFrame();
}

namespace {

// Une image de la modale `id`, demandée d'abord si `ask`, Échap tenu si
// `escape_down`. Rend vrai quand elle s'est ouverte.
[[nodiscard]] bool FrameWithModal(ro::ui::UiContext& ui, const SkinArt& art, const char* id,
                                  ro::ui::ModalEscape escape, bool ask, bool escape_down) {
  ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, escape_down);
  ui.BeginFrame(kViewport, kFrameSeconds);
  if (ask) {
    ro::ui::AskModal(id);
  }
  const bool shown = ro::ui::BeginSkinnedModal(id, art, ImGuiWindowFlags_AlwaysAutoResize, escape);
  if (shown) {
    ImGui::TextUnformatted("Contenu");
    ro::ui::EndSkinnedModal();
  }
  ui.EndFrame();
  return shown;
}

}  // namespace

TEST_CASE("Modale habillée : Échap la referme, sauf si l'appelant l'ignore", "[ui][skin]") {
  // ⚠ UNE DÉCISION QUI NE DOIT PAS DISPARAÎTRE D'UNE TOUCHE n'a que ses boutons
  // pour sortir. Le défaut, lui, reste la fermeture : c'est ce qu'attendent
  // toutes les autres boîtes.
  const auto run = [](ro::ui::ModalEscape escape) {
    ro::ui::UiContext ui;
    const SkinArt art = WholeArt();
    ImGui::GetIO().MousePos = ImVec2(0.0F, 0.0F);
    REQUIRE(FrameWithModal(ui, art, "Invitation##essai", escape, true, false));
    REQUIRE(FrameWithModal(ui, art, "Invitation##essai", escape, false, false));
    // La touche, une image ; puis relâchée, l'image d'après dit ce qu'il reste.
    static_cast<void>(FrameWithModal(ui, art, "Invitation##essai", escape, false, true));
    return FrameWithModal(ui, art, "Invitation##essai", escape, false, false);
  };
  CHECK(run(ro::ui::ModalEscape::kIgnored));
  CHECK_FALSE(run(ro::ui::ModalEscape::kCloses));
}
