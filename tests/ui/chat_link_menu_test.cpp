// LE MENU D'UN LIEN DU FIL : ses entrées, dans leur ordre, celles qui se
// grisent avec leur raison, et la tête qui nomme le lien.
//
// 🔴 UN MENU NE SE VOIT PAS CASSER : une entrée glissée d'un cran, une autre
// offerte à l'aveugle ou une raison muette passent inaperçues tant qu'on ne les
// cherche pas. Le banc les cherche dans ce que le menu écrit.

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/chat_link.h"
#include "ro/ui/chat_log.h"
#include "ro/ui/chat_rich_art.h"
#include "ro/ui/chat_window.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/ui_context.h"

using ro::ui::ChatLinkAsk;
using ro::ui::ChatLinkGesture;
using ro::ui::ChatLinkKind;
using ro::ui::MvpLinkMenuFacts;
using ro::ui::PlayerLinkRefusals;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr const char* kMenuId = "menu_lien";
constexpr std::uint32_t kBaphomet = 1039;
constexpr std::uint32_t kToken = 4;
// Où le menu s'ouvre, puis le balayage qui cherche une entrée.
constexpr ImVec2 kOpenAt(100.0F, 100.0F);
constexpr float kSweepStepPixels = 3.0F;
constexpr float kSweepInsetPixels = 30.0F;
constexpr int kSettleFrames = 3;

// UN MENU DE LIEN QUI TOURNE : un vrai contexte ImGui, sans GPU. Ce que ses
// fenêtres écrivent — entrées et infobulles — se relit dans `logged`.
struct MenuBench {
  ro::ui::UiContext ui;
  ChatLinkKind kind = ChatLinkKind::kMvp;
  std::uint32_t item_id = kBaphomet;
  std::string label = "[MVP: Baphomet (prt_maze03)]";
  const PlayerLinkRefusals* refusals = nullptr;
  const MvpLinkMenuFacts* facts = nullptr;
  ro::ui::NaviSearchMenuFacts navi;
  const ro::ui::ChatUrlFacts* url = nullptr;
  bool staff_warp = false;
  bool open_asked = true;
  std::string logged;
  ChatLinkAsk answered;

  ChatLinkAsk Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    ImGui::LogToBuffer();
    ImGui::Begin("banc_lien");
    if (open_asked) {
      ImGui::OpenPopup(kMenuId);
      open_asked = false;
    }
    const ChatLinkAsk asked = ro::ui::DrawChatLinkMenu(kMenuId, kind, item_id, kToken, label, {},
                                                       refusals, facts, navi, url, staff_warp);
    ImGui::End();
    logged = ImGui::GetCurrentContext()->LogBuffer.c_str();
    ImGui::LogFinish();
    ui.EndFrame();
    if (asked.gesture != ChatLinkGesture::kNothing) {
      answered = asked;
    }
    return asked;
  }

  // Ouvre le menu sous la souris et le laisse se poser.
  void Open() {
    ImGui::GetIO().AddMousePosEvent(kOpenAt.x, kOpenAt.y);
    for (int frame = 0; frame < kSettleFrames; ++frame) {
      static_cast<void>(Frame());
    }
    REQUIRE_FALSE(GImGui->OpenPopupStack.empty());
  }

  [[nodiscard]] bool Shows(std::string_view text) const {
    return logged.find(text) != std::string::npos;
  }
  // `first` est écrit avant `second`.
  [[nodiscard]] bool Before(std::string_view first, std::string_view second) const {
    const std::size_t at_first = logged.find(first);
    const std::size_t at_second = logged.find(second);
    return at_first != std::string::npos && at_second != std::string::npos && at_first < at_second;
  }

  // Survole les entrées de haut en bas jusqu'à `label` — ou, grisée, jusqu'à
  // ce que `seen` paraisse.
  bool Hover(const char* entry, const char* seen = nullptr) {
    ImGuiWindow* menu = GImGui->OpenPopupStack.back().Window;
    REQUIRE(menu != nullptr);
    const ImGuiID wanted = menu->GetID(entry);
    const float x = menu->Pos.x + kSweepInsetPixels;
    for (float y = menu->Pos.y; y < menu->Pos.y + menu->Size.y; y += kSweepStepPixels) {
      ImGui::GetIO().AddMousePosEvent(x, y);
      static_cast<void>(Frame());
      static_cast<void>(Frame());
      if ((seen == nullptr && GImGui->HoveredId == wanted) || (seen != nullptr && Shows(seen))) {
        return true;
      }
    }
    return false;
  }

  void Click() {
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    static_cast<void>(Frame());
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    for (int frame = 0; frame < kSettleFrames; ++frame) {
      static_cast<void>(Frame());
    }
  }
};

}  // namespace

TEST_CASE("Lien de MVP : sans faits, l'import reste grisé", "[ui][chat][mvp]") {
  MenuBench bench;
  bench.Open();
  CHECK(bench.Before("Ajouter à mon carnet", "Ouvrir le carnet"));
  CHECK(bench.Before("Ouvrir le carnet", "Fiche du monstre"));
  CHECK(bench.Before("Fiche du monstre", "Lien dans le chat"));
  // Sans faits, ni la tombe ni la carte ne sont connues.
  CHECK_FALSE(bench.Shows("Guider vers la tombe"));
  CHECK_FALSE(bench.Shows("Y aller"));
  // 🔴 JAMAIS OFFERT À L'AVEUGLE : grisé, il dit pourquoi.
  CHECK(bench.Hover("Ajouter à mon carnet", "Créez un groupe de chasse pour tenir un carnet."));
  bench.Click();
  CHECK(bench.answered.gesture != ChatLinkGesture::kAddToMvpLog);
}

TEST_CASE("Lien de MVP : offert, l'import se joue ; la tombe connue se rejoint",
          "[ui][chat][mvp]") {
  MenuBench bench;
  const MvpLinkMenuFacts facts{.tomb_known = true};
  bench.facts = &facts;
  bench.Open();
  CHECK(bench.Before("Ouvrir le carnet", "Guider vers la tombe"));
  CHECK(bench.Before("Guider vers la tombe", "Fiche du monstre"));
  REQUIRE(bench.Hover("Ajouter à mon carnet"));
  bench.Click();
  CHECK(bench.answered.gesture == ChatLinkGesture::kAddToMvpLog);
  CHECK(bench.answered.kind == ChatLinkKind::kMvp);
  CHECK(bench.answered.token == kToken);
}

TEST_CASE("Lien de MVP : tombe inconnue, « Y aller » guide vers la carte", "[ui][chat][mvp]") {
  MenuBench bench;
  const MvpLinkMenuFacts facts{.map_known = true};
  bench.facts = &facts;
  bench.Open();
  CHECK_FALSE(bench.Shows("Guider vers la tombe"));
  CHECK(bench.Before("Ouvrir le carnet", "Y aller"));
  CHECK(bench.Before("Y aller", "Fiche du monstre"));
  REQUIRE(bench.Hover("Y aller"));
  bench.Click();
  // LE MÊME GESTE QUE LA TOMBE : `app` guide vers la carte entière.
  CHECK(bench.answered.gesture == ChatLinkGesture::kGuideToTomb);
}

TEST_CASE("Lien de MVP : tombe connue, pas de « Y aller » en double", "[ui][chat][mvp]") {
  MenuBench bench;
  const MvpLinkMenuFacts facts{.tomb_known = true, .map_known = true};
  bench.facts = &facts;
  bench.Open();
  CHECK(bench.Shows("Guider vers la tombe"));
  CHECK_FALSE(bench.Shows("Y aller"));
  REQUIRE(bench.Hover("Guider vers la tombe"));
  bench.Click();
  CHECK(bench.answered.gesture == ChatLinkGesture::kGuideToTomb);
}

TEST_CASE("Lien de MVP : refusé, l'import porte SA raison", "[ui][chat][mvp]") {
  MenuBench bench;
  const MvpLinkMenuFacts facts{.import_refusal = "Ce lien ne porte pas d'heure de mort."};
  bench.facts = &facts;
  bench.Open();
  CHECK(bench.Hover("Ajouter à mon carnet", "Ce lien ne porte pas d'heure de mort."));
}

TEST_CASE("Lien de MVP : sans monstre, pas de fiche à promettre", "[ui][chat][mvp]") {
  MenuBench bench;
  bench.item_id = 0;
  bench.Open();
  CHECK(bench.Shows("Ouvrir le carnet"));
  CHECK_FALSE(bench.Shows("Fiche du monstre"));
}

TEST_CASE("Lien de MVP : carnet éteint, ce qui passe par lui se grise", "[ui][chat][mvp]") {
  constexpr const char* kOff = "Le carnet de chasse est éteint.";
  // ⚠ UNE RAISON À LA FOIS : le survol s'arrête à la première infobulle qui la
  // montre, et deux entrées qui la partagent se confondraient.
  SECTION("« Ajouter à mon carnet »") {
    MenuBench bench;
    const MvpLinkMenuFacts facts{.import_refusal = kOff, .tomb_known = true, .map_known = true};
    bench.facts = &facts;
    bench.Open();
    CHECK(bench.Hover("Ajouter à mon carnet", kOff));
    bench.Click();
    CHECK(bench.answered.gesture != ChatLinkGesture::kAddToMvpLog);
    // LE LIEN, LUI, RESTE À SON AUTEUR : guidage et republication offerts.
    CHECK(bench.Shows("Guider vers la tombe"));
    CHECK(bench.Shows("Lien dans le chat"));
  }
  SECTION("« Ouvrir le carnet »") {
    MenuBench bench;
    const MvpLinkMenuFacts facts{.open_refusal = kOff, .tomb_known = true, .map_known = true};
    bench.facts = &facts;
    bench.Open();
    CHECK(bench.Hover("Ouvrir le carnet", kOff));
    bench.Click();
    CHECK(bench.answered.gesture != ChatLinkGesture::kDescribe);
  }
}

TEST_CASE("Lien de MVP : le clic sur « Ouvrir le carnet » décrit", "[ui][chat][mvp]") {
  MenuBench bench;
  bench.Open();
  REQUIRE(bench.Hover("Ouvrir le carnet"));
  bench.Click();
  CHECK(bench.answered.gesture == ChatLinkGesture::kDescribe);
}

TEST_CASE("Lien de monstre : la recherche et les commandes, rien du carnet", "[ui][chat][mvp]") {
  MenuBench bench;
  bench.kind = ChatLinkKind::kMonster;
  bench.label = "[MVP] Baphomet";
  bench.Open();
  CHECK(bench.Before("Fiche du monstre", "Bestiaire du site"));
  CHECK(bench.Before("Bestiaire du site", "Lien dans le chat"));
  // LA PARTIE BASSE VAUT POUR TOUT NOM DE MONSTRE, sans faits du carnet.
  CHECK(bench.Before("Lien dans le chat", "Où le trouver"));
  CHECK(bench.Before("Où le trouver", "Lien « où le trouver » dans le chat"));
  CHECK(bench.Before("Lien « où le trouver » dans le chat", "@mobinfo"));
  CHECK(bench.Before("@mobinfo", "@whereis"));
  CHECK_FALSE(bench.Shows("Partager le respawn dans le chat"));
  CHECK_FALSE(bench.Shows("Guider vers la tombe"));
  REQUIRE(bench.Hover("Lien « où le trouver » dans le chat"));
  bench.Click();
  CHECK(bench.answered.gesture == ChatLinkGesture::kPostWhereToFind);
}

TEST_CASE("Lien de monstre : sans nom, rien à chercher", "[ui][chat][mvp]") {
  MenuBench bench;
  bench.kind = ChatLinkKind::kMonster;
  bench.label.clear();
  bench.Open();
  CHECK(bench.Shows("Fiche du monstre"));
  CHECK_FALSE(bench.Shows("Où le trouver"));
  CHECK(bench.Before("Lien dans le chat", "@mobinfo"));
}

TEST_CASE("Monstre de la navigation : une classe de sprite n'offre ni bestiaire ni commandes",
          "[ui][chat][navi]") {
  MenuBench bench;
  bench.kind = ChatLinkKind::kMonster;
  bench.label = "Baphomet";
  const MvpLinkMenuFacts facts{.named = true, .sprite_class_only = true};
  bench.facts = &facts;
  bench.Open();
  // LA FICHE SAIT REMONTER D'UNE CLASSE, la recherche part sur le nom.
  CHECK(bench.Before("Fiche du monstre", "Où le trouver"));
  CHECK(bench.Shows("Lien « où le trouver » dans le chat"));
  // 🔴 CE QUI NE LIT QUE LA BASE n'est pas offert : un monstre maison qui
  // emprunte la classe d'un autre ferait décrire cet autre — chez nous comme
  // chez le lecteur d'un lien relayé.
  CHECK_FALSE(bench.Shows("Lien dans le chat"));
  CHECK_FALSE(bench.Shows("Bestiaire du site"));
  CHECK_FALSE(bench.Shows("@mobinfo"));
  CHECK_FALSE(bench.Shows("@whereis"));
}

TEST_CASE("Nom d'un MVP du carnet : le respawn, la tombe, la recherche, les commandes",
          "[ui][chat][mvp]") {
  MenuBench bench;
  bench.kind = ChatLinkKind::kMonster;
  bench.label = "Baphomet";
  const MvpLinkMenuFacts facts{.tomb_known = true, .carries_respawn = true, .named = true};
  bench.facts = &facts;
  bench.Open();
  CHECK(bench.Before("Lien dans le chat", "Partager le respawn dans le chat"));
  CHECK(bench.Before("Partager le respawn dans le chat", "Guider vers la tombe"));
  CHECK(bench.Before("Guider vers la tombe", "Où le trouver"));
  CHECK(bench.Before("Où le trouver", "Lien « où le trouver » dans le chat"));
  CHECK(bench.Before("Lien « où le trouver » dans le chat", "@mobinfo"));
  CHECK(bench.Before("@mobinfo", "@whereis"));
  REQUIRE(bench.Hover("Partager le respawn dans le chat"));
  bench.Click();
  CHECK(bench.answered.gesture == ChatLinkGesture::kShareRespawn);
}

TEST_CASE("Nom d'un MVP jamais vu : rien à partager, mais sa carte se rejoint", "[ui][chat][mvp]") {
  MenuBench bench;
  bench.kind = ChatLinkKind::kMonster;
  bench.label = "Baphomet";
  // SANS HEURE, rien à partager ; la carte du créneau, elle, est connue.
  const MvpLinkMenuFacts facts{.named = false, .map_known = true};
  bench.facts = &facts;
  bench.Open();
  CHECK_FALSE(bench.Shows("Partager le respawn dans le chat"));
  CHECK_FALSE(bench.Shows("Guider vers la tombe"));
  CHECK_FALSE(bench.Shows("Où le trouver"));
  CHECK(bench.Before("Lien dans le chat", "Y aller"));
  CHECK(bench.Before("Y aller", "@whereis"));
  REQUIRE(bench.Hover("Y aller"));
  bench.Click();
  CHECK(bench.answered.gesture == ChatLinkGesture::kGuideToTomb);
}

TEST_CASE("Nom d'un MVP du carnet : sans carte ni tombe, aucun guidage", "[ui][chat][mvp]") {
  MenuBench bench;
  bench.kind = ChatLinkKind::kMonster;
  bench.label = "Baphomet";
  const MvpLinkMenuFacts facts{.carries_respawn = true, .named = true};
  bench.facts = &facts;
  bench.Open();
  CHECK(bench.Shows("Partager le respawn dans le chat"));
  CHECK_FALSE(bench.Shows("Guider vers la tombe"));
  CHECK_FALSE(bench.Shows("Y aller"));
}

TEST_CASE("Recherche de navigation : une carte se rejoint", "[ui][chat][navi]") {
  MenuBench bench;
  bench.kind = ChatLinkKind::kNaviSearch;
  bench.item_id = 0;
  bench.label = "[Carte: Prontera]";
  bench.navi = ro::ui::NaviSearchMenuFacts{.family = 0, .bounded = false};
  bench.Open();
  CHECK(bench.Before("Chercher dans la navigation", "Y aller"));
  CHECK(bench.Before("Y aller", "Copier le nom"));
  CHECK(bench.Before("Copier le nom", "Lien dans le chat"));
  CHECK_FALSE(bench.Shows("Voir ce qu'il y a là-bas"));
  REQUIRE(bench.Hover("Y aller"));
  bench.Click();
  CHECK(bench.answered.gesture == ChatLinkGesture::kGoThere);
  CHECK(bench.answered.kind == ChatLinkKind::kNaviSearch);
}

TEST_CASE("Recherche de navigation : un PNJ borné redevient un lieu", "[ui][chat][navi]") {
  MenuBench bench;
  bench.kind = ChatLinkKind::kNaviSearch;
  bench.item_id = 0;
  bench.label = "[PNJ: Kafra Employee (Prontera)]";
  bench.navi = ro::ui::NaviSearchMenuFacts{.family = 1, .bounded = true};
  bench.Open();
  CHECK(bench.Before("Chercher dans la navigation", "Y aller"));
  CHECK(bench.Before("Y aller", "Voir ce qu'il y a là-bas"));
  CHECK(bench.Before("Voir ce qu'il y a là-bas", "Copier le nom"));
  CHECK(bench.Before("Copier le nom", "Lien dans le chat"));
  REQUIRE(bench.Hover("Voir ce qu'il y a là-bas"));
  bench.Click();
  CHECK(bench.answered.gesture == ChatLinkGesture::kPlaceContents);
}

TEST_CASE("Recherche de navigation : un monstre libre n'a pas de lieu", "[ui][chat][navi]") {
  MenuBench bench;
  bench.kind = ChatLinkKind::kNaviSearch;
  bench.item_id = 0;
  bench.label = "[Monstre: Baphomet]";
  bench.navi = ro::ui::NaviSearchMenuFacts{.family = 2, .bounded = false};
  bench.Open();
  CHECK(bench.Before("Chercher dans la navigation", "Copier le nom"));
  CHECK(bench.Before("Copier le nom", "Lien dans le chat"));
  CHECK_FALSE(bench.Shows("Y aller"));
  CHECK_FALSE(bench.Shows("Voir ce qu'il y a là-bas"));
  REQUIRE(bench.Hover("Chercher dans la navigation"));
  bench.Click();
  CHECK(bench.answered.gesture == ChatLinkGesture::kDescribe);
}

TEST_CASE("Lieu : le téléport n'est offert qu'au staff", "[ui][chat][navi]") {
  MenuBench bench;
  bench.kind = ChatLinkKind::kPlace;
  bench.item_id = 0;
  bench.label = "[Prontera (150, 180)]";
  bench.Open();
  CHECK(bench.Shows("Y aller"));
  CHECK_FALSE(bench.Shows("Téléporter à cet endroit"));
}

TEST_CASE("Lieu : le staff s'y téléporte", "[ui][chat][navi]") {
  MenuBench bench;
  bench.kind = ChatLinkKind::kPlace;
  bench.item_id = 0;
  bench.label = "[Prontera (150, 180)]";
  bench.staff_warp = true;
  bench.Open();
  CHECK(bench.Before("Copier le nom de la carte", "Téléporter à cet endroit"));
  CHECK(bench.Before("Téléporter à cet endroit", "Lien dans le chat"));
  REQUIRE(bench.Hover("Téléporter à cet endroit"));
  bench.Click();
  CHECK(bench.answered.gesture == ChatLinkGesture::kStaffWarp);
  CHECK(bench.answered.kind == ChatLinkKind::kPlace);
}

TEST_CASE("Pseudo : le carnet MVP s'invite entre le groupe et la guilde", "[ui][chat][mvp]") {
  MenuBench bench;
  bench.kind = ChatLinkKind::kPlayer;
  bench.item_id = 0;
  bench.label = "Bob";
  bench.Open();
  CHECK(bench.Before("Inviter dans le groupe", "Inviter dans mon carnet MVP"));
  CHECK(bench.Before("Inviter dans mon carnet MVP", "Inviter dans la guilde"));
  REQUIRE(bench.Hover("Inviter dans mon carnet MVP"));
  // OFFERTE, SON INFOBULLE DIT LA RÈGLE.
  static_cast<void>(bench.Frame());
  CHECK(bench.Shows("Guilde et liste d'amis uniquement."));
  bench.Click();
  CHECK(bench.answered.gesture == ChatLinkGesture::kInviteMvp);
  CHECK(bench.answered.kind == ChatLinkKind::kPlayer);
}

TEST_CASE("Pseudo : refusée, l'invitation au carnet porte sa raison", "[ui][chat][mvp]") {
  MenuBench bench;
  bench.kind = ChatLinkKind::kPlayer;
  bench.item_id = 0;
  bench.label = "Bob";
  const PlayerLinkRefusals refused{.mvp = "Créez un groupe de chasse."};
  bench.refusals = &refused;
  bench.Open();
  CHECK(bench.Hover("Inviter dans mon carnet MVP", "Créez un groupe de chasse."));
  bench.Click();
  CHECK(bench.answered.gesture != ChatLinkGesture::kInviteMvp);
}

TEST_CASE("Pseudo : carnet éteint, l'invitation au carnet n'est pas offerte", "[ui][chat][mvp]") {
  MenuBench bench;
  bench.kind = ChatLinkKind::kPlayer;
  bench.item_id = 0;
  bench.label = "Bob";
  const PlayerLinkRefusals refused{.mvp = "Le carnet de chasse est éteint.", .mvp_withdrawn = true};
  bench.refusals = &refused;
  bench.Open();
  CHECK_FALSE(bench.Shows("Inviter dans mon carnet MVP"));
  // Les autres invitations restent.
  CHECK(bench.Shows("Inviter dans le groupe"));
  CHECK(bench.Shows("Inviter dans la guilde"));
}

// ── Une adresse web, un pseudo Discord ──────────────────────────────────────

namespace {

constexpr std::string_view kImgurHost = "i.imgur.com";
const std::string kImgurUrl = "https://i.imgur.com/AbCdEfG.png";

// Le banc vise une adresse, faits fournis.
void AimAtUrl(MenuBench& bench, const ro::ui::ChatUrlFacts& facts) {
  bench.kind = ChatLinkKind::kUrl;
  bench.item_id = 0;
  bench.label = kImgurUrl;
  bench.url = &facts;
}

}  // namespace

TEST_CASE("Adresse : sans accord, l'image se demande, une fois ou pour le site",
          "[ui][chat][url]") {
  const ro::ui::ChatUrlFacts facts{.host = kImgurHost, .allowed = false};
  MenuBench bench;
  AimAtUrl(bench, facts);
  bench.Open();
  CHECK(bench.Shows(kImgurUrl));
  CHECK(bench.Before("Afficher cette image", "Toujours afficher i.imgur.com…"));
  CHECK(bench.Before("Toujours afficher i.imgur.com…", "Ouvrir dans le navigateur"));
  CHECK(bench.Before("Ouvrir dans le navigateur", "Copier l'adresse"));
  // RIEN À AGRANDIR tant que l'aperçu n'est pas prêt.
  CHECK_FALSE(bench.Shows("Agrandir l'image"));
  REQUIRE(bench.Hover("Toujours afficher i.imgur.com…"));
  bench.Click();
  CHECK(bench.answered.gesture == ChatLinkGesture::kAllowImageHost);
  CHECK(bench.answered.kind == ChatLinkKind::kUrl);
}

TEST_CASE("Adresse : « Afficher cette image » vaut pour cette fois", "[ui][chat][url]") {
  const ro::ui::ChatUrlFacts facts{.host = kImgurHost, .allowed = false};
  MenuBench bench;
  AimAtUrl(bench, facts);
  bench.Open();
  REQUIRE(bench.Hover("Afficher cette image"));
  bench.Click();
  CHECK(bench.answered.gesture == ChatLinkGesture::kShowImageOnce);
}

TEST_CASE("Adresse : admise et prête, elle s'agrandit", "[ui][chat][url]") {
  const ro::ui::ChatUrlFacts facts{
      .host = kImgurHost, .allowed = true, .preview = ro::ui::ChatPictureState::kReady};
  MenuBench bench;
  AimAtUrl(bench, facts);
  bench.Open();
  CHECK_FALSE(bench.Shows("Afficher cette image"));
  CHECK_FALSE(bench.Shows("Toujours afficher"));
  CHECK(bench.Before("Agrandir l'image", "Ouvrir dans le navigateur"));
  REQUIRE(bench.Hover("Agrandir l'image"));
  bench.Click();
  CHECK(bench.answered.gesture == ChatLinkGesture::kViewImage);
}

TEST_CASE("Adresse : ouvrir décrit, copier copie", "[ui][chat][url]") {
  const ro::ui::ChatUrlFacts facts{.host = kImgurHost, .allowed = true};
  SECTION("Ouvrir dans le navigateur") {
    MenuBench bench;
    AimAtUrl(bench, facts);
    bench.Open();
    REQUIRE(bench.Hover("Ouvrir dans le navigateur"));
    bench.Click();
    CHECK(bench.answered.gesture == ChatLinkGesture::kDescribe);
  }
  SECTION("Copier l'adresse") {
    MenuBench bench;
    AimAtUrl(bench, facts);
    bench.Open();
    REQUIRE(bench.Hover("Copier l'adresse"));
    bench.Click();
    CHECK(bench.answered.gesture == ChatLinkGesture::kCopyCode);
  }
}

TEST_CASE("Adresse : une adresse à rallonge est coupée en tête du menu", "[ui][chat][url]") {
  constexpr std::size_t kShownChars = 60;
  const ro::ui::ChatUrlFacts facts{.host = "cdn.discordapp.com", .allowed = true};
  MenuBench bench;
  AimAtUrl(bench, facts);
  // ⚠ DES CARACTÈRES, PAS DES OCTETS : l'accent compte pour un.
  bench.label = "https://cdn.discordapp.com/é/" + std::string(200, 'a');
  bench.Open();
  // Les 60 premiers caractères font 61 octets : « é » en prend deux.
  const std::string head = bench.label.substr(0, kShownChars + 1);
  CHECK(bench.Shows(head + "…"));
  CHECK_FALSE(bench.Shows(bench.label.substr(0, kShownChars + 2)));
}

TEST_CASE("Pseudo Discord : le mentionner, ou copier son nom", "[ui][chat][discord]") {
  MenuBench bench;
  bench.kind = ChatLinkKind::kDiscordName;
  bench.item_id = 0;
  bench.label = "Ana";
  bench.Open();
  CHECK(bench.Before("Mentionner", "Copier le nom"));
  // Rien de ce qu'on fait à un joueur du jeu.
  CHECK_FALSE(bench.Shows("Chuchoter…"));
  CHECK_FALSE(bench.Shows("Inviter dans le groupe"));
  REQUIRE(bench.Hover("Mentionner"));
  bench.Click();
  CHECK(bench.answered.gesture == ChatLinkGesture::kDescribe);
  CHECK(bench.answered.kind == ChatLinkKind::kDiscordName);
}

TEST_CASE("Réglage : l'ouvrir, ou reposer son lien", "[ui][chat][reglage]") {
  MenuBench bench;
  bench.kind = ChatLinkKind::kSetting;
  bench.item_id = 0;
  bench.label = "[Réglage: Conversation]";
  bench.Open();
  CHECK(bench.Before("Ouvrir", "Lien dans le chat"));
  // Rien de ce qu'on fait à un objet.
  CHECK_FALSE(bench.Shows("Description"));
  REQUIRE(bench.Hover("Ouvrir"));
  bench.Click();
  CHECK(bench.answered.gesture == ChatLinkGesture::kDescribe);
  CHECK(bench.answered.kind == ChatLinkKind::kSetting);
}

// ── Le fil ──────────────────────────────────────────────────────────────────

namespace {

constexpr float kLogHeightPixels = 200.0F;
const ro::ui::ChatLogStyle kLogStyle{.font_size = 14.0F, .line_gap = 2.0F};
constexpr std::string_view kSpentLabel = "[MVP: Baphomet (prt_maze03) — retour 21:12–21:22]";
constexpr std::string_view kMark = " · passé";
// Où la marque s'insère : avant le « ] » final.
constexpr std::uint32_t kMarkAt = static_cast<std::uint32_t>(kSpentLabel.size() - 1);

// Un fil d'une ligne : un respawn échu, marqué.
struct LogBench {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  std::string text;
  std::array<ro::ui::ChatLinkSpan, 1> spans{};
  ro::ui::ChatLineView line;
  // ⚠ LE MENU D'UN LIEN RETIENT SA CIBLE DANS LE CACHE DU FIL.
  ro::ui::ChatLogLayout layout;
  std::string logged;

  LogBench() {
    text = std::string(kSpentLabel.substr(0, kMarkAt)) + std::string(kMark) + "]";
    spans[0] = ro::ui::ChatLinkSpan{.offset = 0,
                                    .length = static_cast<std::uint32_t>(text.size()),
                                    .kind = ChatLinkKind::kMvp,
                                    .item_id = kBaphomet,
                                    .token = kToken,
                                    .spent = true,
                                    .mark_at = kMarkAt,
                                    .mark_length = static_cast<std::uint32_t>(kMark.size())};
    line.text = text;
    line.links = spans;
  }

  void Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    ImGui::LogToBuffer();
    ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
    ImGui::SetNextWindowSize(ImVec2(kViewport.width_px, kLogHeightPixels * 2.0F));
    ImGui::Begin("banc_fil");
    static_cast<void>(
        ro::ui::DrawChatLog(std::span(&line, 1), kLogStyle, kLogHeightPixels, art, &layout));
    ImGui::End();
    logged = ImGui::GetCurrentContext()->LogBuffer.c_str();
    ImGui::LogFinish();
    ui.EndFrame();
  }

  // Le premier sommet peint de cette teinte, dans n'importe quelle liste.
  [[nodiscard]] static bool Painted(ImU32 tint) {
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
};

}  // namespace

TEST_CASE("Fil : un respawn échu s'éteint, et son menu le nomme sans sa marque",
          "[ui][chat][mvp]") {
  LogBench bench;
  for (int frame = 0; frame < kSettleFrames; ++frame) {
    bench.Frame();
  }
  // L'ORANGE ÉTEINT, et pas celui des liens.
  CHECK(LogBench::Painted(ro::ui::kChatLinkSpentColor));
  CHECK_FALSE(LogBench::Painted(ro::ui::kChatLinkColor));

  // Le clic droit sur le lien ouvre son menu.
  const ImGuiWindow* host = ImGui::FindWindowByName("banc_fil");
  REQUIRE(host != nullptr);
  const ImVec2 on_link(host->DC.CursorStartPos.x + kSweepInsetPixels,
                       host->DC.CursorStartPos.y + kLogStyle.font_size * 0.5F);
  ImGuiIO& io = ImGui::GetIO();
  io.AddMousePosEvent(on_link.x, on_link.y);
  bench.Frame();
  io.AddMouseButtonEvent(ImGuiMouseButton_Right, true);
  bench.Frame();
  io.AddMouseButtonEvent(ImGuiMouseButton_Right, false);
  for (int frame = 0; frame < kSettleFrames; ++frame) {
    bench.Frame();
  }
  REQUIRE_FALSE(GImGui->OpenPopupStack.empty());
  // 🔴 LA TÊTE NOMME LE LIEN, PAS SON ÉTAT : le libellé mot pour mot, sans la
  // marque que le fil peint.
  CHECK(bench.logged.find(kSpentLabel) != std::string::npos);
  CHECK(bench.logged.find("passé") == std::string::npos);
  CHECK(bench.logged.find("Ajouter à mon carnet") != std::string::npos);
}

// ── La barre de la conversation ─────────────────────────────────────────────

TEST_CASE("Barre : le geste d'une pastille garde le genre de son lien", "[ui][chat][mvp]") {
  // ⚠ SANS LE GENRE, une pastille de MVP se lisait comme un objet.
  const std::array<ro::ui::ChatInputLink, 1> mvp = {
      ro::ui::ChatInputLink{.label = "[MVP: Baphomet]",
                            .kind = ChatLinkKind::kMvp,
                            .item_id = kBaphomet,
                            .token = kToken}};
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  ro::ui::ChatWindows windows;
  const ro::ui::ChatLinePreview preview{.target = "Alentour"};
  const std::array<ro::ui::ChatStripTab, 1> tabs = {ro::ui::ChatStripTab{.name = "Tout"}};
  std::ranges::copy(std::string_view("vu [MVP: Baphomet] !"), windows.main.input.text.begin());
  ro::ui::ChatWindowAction asked;
  const auto frame = [&] {
    ui.BeginFrame(kViewport, kFrameSeconds);
    windows.board.strips.clear();
    asked =
        ro::ui::DrawChatWindow(windows.main, ro::ui::ChatAppearance{}, preview, {}, nullptr, tabs,
                               1, {}, ro::ui::ChatTabLook{}, {}, {}, windows.board, art, mvp);
    ui.EndFrame();
  };
  for (int settle = 0; settle < kSettleFrames; ++settle) {
    frame();
  }
  const ImGuiWindow* window = ImGui::FindWindowByName(ro::ui::kChatWindowId);
  REQUIRE(window != nullptr);
  // La barre est la dernière rangée de la fenêtre ; on la balaie jusqu'à la
  // pastille.
  const float row_y = window->DC.CursorMaxPos.y - ImGui::GetFrameHeight() * 0.5F;
  bool over_chip = false;
  for (float x = window->Pos.x; x < window->Pos.x + window->Size.x && !over_chip;
       x += kSweepStepPixels) {
    ImGui::GetIO().AddMousePosEvent(x, row_y);
    frame();
    over_chip = asked.link.hovering;
  }
  REQUIRE(over_chip);
  ImGuiIO& io = ImGui::GetIO();
  ChatLinkAsk clicked;
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  frame();
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  frame();
  if (asked.link.gesture != ChatLinkGesture::kNothing) {
    clicked = asked.link;
  }
  CHECK(clicked.gesture == ChatLinkGesture::kDescribe);
  CHECK(clicked.kind == ChatLinkKind::kMvp);
  CHECK(clicked.item_id == kBaphomet);
  CHECK(clicked.from_input);
}

TEST_CASE("Lien d'item : le Marché se cherche depuis le menu", "[ui][chat][market]") {
  constexpr std::uint32_t kRedPotion = 501;
  MenuBench bench;
  bench.kind = ChatLinkKind::kItem;
  bench.item_id = kRedPotion;
  bench.label = "[Red Potion]";
  bench.Open();
  CHECK(bench.Before("Lien dans le chat", "Rechercher sur le Marché"));
  CHECK(bench.Before("Rechercher sur le Marché", "@iteminfo"));
  REQUIRE(bench.Hover("Rechercher sur le Marché"));
  bench.Click();
  CHECK(bench.answered.gesture == ChatLinkGesture::kMarketSearch);
  CHECK(bench.answered.item_id == kRedPotion);
}

TEST_CASE("Lien d'item : la base de données s'ouvre depuis le menu", "[ui][chat][database]") {
  constexpr std::uint32_t kRedPotion = 501;
  MenuBench bench;
  bench.kind = ChatLinkKind::kItem;
  bench.item_id = kRedPotion;
  bench.label = "[Red Potion]";
  bench.Open();
  CHECK(bench.Before("Rechercher sur le Marché", "Ouvrir dans la base de données"));
  REQUIRE(bench.Hover("Ouvrir dans la base de données"));
  bench.Click();
  CHECK(bench.answered.gesture == ChatLinkGesture::kGameDatabase);
  CHECK(bench.answered.item_id == kRedPotion);
}
