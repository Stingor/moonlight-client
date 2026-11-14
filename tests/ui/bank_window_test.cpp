// LES BORNES DE LA BANQUE : ce que « tout déposer » et « tout retirer »
// calculent, et ce qu'une saisie hors bornes devient.
//
// 🔴 LE PLAFOND S'APPLIQUE À LA DESTINATION, JAMAIS À LA SOURCE, et c'est la
// seule chose difficile de ce calcul. Déposer est borné par ce que la BANQUE
// peut encore recevoir ; retirer, par ce que la POCHE peut encore contenir. Les
// intervertir donne un bouton qui propose un transfert que le serveur refuse en
// bloc — et le refus, lui, arrive une seconde plus tard, sans que rien ne relie
// les deux.

#include "ro/ui/bank_window.h"

#include <cstddef>
#include <cstdint>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/skin_pieces.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/ui_context.h"

using ro::render::SkinPiece;
using ro::ui::BankClampAmount;
using ro::ui::BankMaxDeposit;
using ro::ui::BankMaxWithdraw;
using ro::ui::BankVerb;
using ro::ui::BankWindowAction;
using ro::ui::BankWindowState;
using ro::ui::BankWindowView;
using ro::ui::kBankRequestMostZeny;
using ro::ui::kBankVaultMaxUnannounced;
using ro::ui::kPocketZenyMax;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;

// LE FOND, À SA TAILLE RÉELLE : c'est lui qui commande la largeur de la
// fenêtre, et l'essai ne vaut que s'il la porte.
constexpr float kBackgroundWidthPixels = 345.0F;
constexpr float kBackgroundHeightPixels = 187.0F;
// La bande grise du bitmap, relevée ligne par ligne : `[86, 134[`.
constexpr float kBandHeightPixels = 48.0F;
constexpr float kPlainPiecePixels = 8.0F;

[[nodiscard]] ro::ui::SkinArt ArtWithBackground() {
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<SkinPiece>(rank),
            ro::ui::SkinImage{.id = 1, .width = kPlainPiecePixels, .height = kPlainPiecePixels});
  }
  art.Set(SkinPiece::kBankBackground,
          ro::ui::SkinImage{
              .id = 1, .width = kBackgroundWidthPixels, .height = kBackgroundHeightPixels});
  return art;
}

// Une banque ouverte, dessinée image par image.
struct BankProbe {
  ro::ui::UiContext ui;
  ro::ui::SkinArt art = ArtWithBackground();
  BankWindowState state;
  BankWindowView view{.vault = 1000, .zeny = 2000, .zeny_known = true};

  BankProbe() { state.open = true; }

  BankWindowAction Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    BankWindowAction action = ro::ui::DrawBankWindow(state, view, art);
    ui.EndFrame();
    return action;
  }

  void Settle() {
    for (int frame = 0; frame < 4; ++frame) {
      static_cast<void>(Frame());
    }
  }
};

}  // namespace

TEST_CASE("Banque : la fenêtre prend la largeur de son fond", "[ui][bank]") {
  // 🔴 LE BITMAP COMMANDE, et ce n'est le cas d'aucune autre fenêtre. Le sac
  // de zeny et la bande du montant y sont peints aux pixels près : une
  // largeur qui ne serait pas la sienne décalerait le champ hors de sa bande.
  BankProbe probe;
  probe.Settle();
  const ImGuiWindow* window = ImGui::FindWindowByName("###bank");
  REQUIRE(window != nullptr);
  CHECK(window->Size.x == kBackgroundWidthPixels);
}

TEST_CASE("Banque : la bande du fond fait les quarante-huit lignes du bitmap", "[ui][bank]") {
  // ⚠ CE BANC NE REGARDE PAS L'IMAGE — rien ici ne sait lire un pixel. Il
  // tient le PLANCHER dont elle découle : la bande ne se rétrécit jamais sous
  // la hauteur qu'elle a dans le bitmap, sans quoi le sac de zeny qu'elle
  // porte se retrouverait écrasé.
  //
  // La règle qui la POSE, elle, est celle du toolkit : un cadre vaut la barre
  // de titre — voir le banc de `SkinFramePaddingY`. C'est elle qui décide de
  // la hauteur des trois soldes écrits au-dessus, donc de l'endroit où la
  // bande s'ouvre.
  CHECK(ro::ui::kBankBandPixels == kBandHeightPixels);
}

TEST_CASE("Banque : fermée, elle ne dessine rien", "[ui][bank]") {
  BankProbe probe;
  probe.state.open = false;
  const BankWindowAction action = probe.Frame();
  CHECK(action.verb == BankVerb::kNothing);
  CHECK(ImGui::FindWindowByName("###bank") == nullptr);
}

TEST_CASE("Banque : rouvrir remet le montant et le message à zéro", "[ui][bank]") {
  // ⚠ SINON LE MONTANT DE LA DERNIÈRE VISITE RESTERAIT LÀ, armé, sur une
  // fenêtre qui déplace de l'argent.
  BankProbe probe;
  probe.Settle();
  probe.state.amount = 777;
  ro::ui::SetBankStatus(probe.state, "quelque chose", true);

  probe.state.open = false;
  static_cast<void>(probe.Frame());
  probe.state.open = true;
  static_cast<void>(probe.Frame());

  CHECK(probe.state.amount == 0);
  CHECK(probe.state.status[0] == '\0');
}

TEST_CASE("Banque : déposer est borné par la poche quand la banque a de la place", "[ui][bank]") {
  CHECK(BankMaxDeposit(0, 500, kBankVaultMaxUnannounced) == 500);
  CHECK(BankMaxDeposit(1000, 500, kBankVaultMaxUnannounced) == 500);
}

TEST_CASE("Banque : déposer est borné par ce qui reste à la BANQUE", "[ui][bank]") {
  // La banque est à cent de son plafond, la poche en a mille : on ne peut
  // déposer que cent.
  CHECK(BankMaxDeposit(kBankVaultMaxUnannounced - 100, 1000, kBankVaultMaxUnannounced) == 100);
  // Pleine, elle ne reçoit plus rien — et le résultat est ZÉRO, jamais négatif.
  CHECK(BankMaxDeposit(kBankVaultMaxUnannounced, 1000, kBankVaultMaxUnannounced) == 0);
  // Un solde au-delà d'un plafond qu'on croyait plus bas : zéro, pas un négatif.
  CHECK(BankMaxDeposit(kBankVaultMaxUnannounced + 1, 1000, kBankVaultMaxUnannounced) == 0);
}

TEST_CASE("Banque : un plafond annoncé au-delà de 2^31 laisse déposer sur un gros solde",
          "[ui][bank]") {
  // 🔴 LE CAS QUE LE PLAFOND EN TRENTE-DEUX BITS BLOQUAIT : le solde dépasse
  // déjà `INT32_MAX`, et le serveur tient plus. Seule la poche borne.
  constexpr std::int64_t kAnnouncedMax = 9'999'999'999'999'999;
  constexpr std::int64_t kBigVault = 5'000'000'000;
  CHECK(BankMaxDeposit(kBigVault, 1000, kAnnouncedMax) == 1000);
  // La poche pleine se dépose en entier : elle tient dans une demande.
  CHECK(BankMaxDeposit(kBigVault, kPocketZenyMax, kAnnouncedMax) == kPocketZenyMax);
  // Près du plafond annoncé, c'est lui qui borne.
  CHECK(BankMaxDeposit(kAnnouncedMax - 100, 1000, kAnnouncedMax) == 100);
}

TEST_CASE("Banque : retirer est borné par la POCHE et par ce qu'une demande porte", "[ui][bank]") {
  CHECK(BankMaxWithdraw(1000, 0) == 1000);
  // La poche est à cent de son plafond, la banque en a mille : on ne peut
  // retirer que cent.
  CHECK(BankMaxWithdraw(1000, kPocketZenyMax - 100) == 100);
  CHECK(BankMaxWithdraw(1000, kPocketZenyMax) == 0);
  // Un solde au-delà de 2^31 ne se retire qu'à hauteur d'une demande — et la
  // poche vide l'accepte en entier.
  constexpr std::int64_t kBigVault = 5'000'000'000;
  CHECK(BankMaxWithdraw(kBigVault, 0) == kBankRequestMostZeny);
  CHECK(BankMaxWithdraw(kBigVault, kPocketZenyMax - 100) == 100);
}

TEST_CASE("Banque : une banque vide ne rend rien, une poche vide ne donne rien", "[ui][bank]") {
  CHECK(BankMaxWithdraw(0, 0) == 0);
  CHECK(BankMaxDeposit(0, 0, kBankVaultMaxUnannounced) == 0);
}

TEST_CASE("Banque : la saisie se borne à zéro et à ce qu'une demande porte", "[ui][bank]") {
  CHECK(BankClampAmount(-1) == 0);
  CHECK(BankClampAmount(0) == 0);
  CHECK(BankClampAmount(42) == 42);
  CHECK(BankClampAmount(kBankRequestMostZeny) == kBankRequestMostZeny);
  // ⚠ LE MONTANT EST SUR SOIXANTE-QUATRE BITS POUR ABSORBER CELA sans déborder :
  // un palier ajouté à une saisie déjà au plafond dépasse un `int32`.
  CHECK(BankClampAmount(kBankRequestMostZeny + 100000000) == kBankRequestMostZeny);
}

TEST_CASE("Banque : sans plafond annoncé, la vue garde celui d'un serveur d'usine", "[ui][bank]") {
  const BankWindowView view;
  CHECK(view.vault_max == kBankVaultMaxUnannounced);
  CHECK(kBankVaultMaxUnannounced == kPocketZenyMax);
}

TEST_CASE("Banque : un solde au-delà de 2^31 se dessine et laisse déposer", "[ui][bank]") {
  // La fenêtre entière, sur un solde que le plafond en trente-deux bits
  // rendait indéposable : elle ne doit ni planter ni griser le dépôt.
  BankProbe probe;
  probe.view = BankWindowView{
      .vault = 5'000'000'000, .zeny = 2000, .zeny_known = true, .vault_max = 9'999'999'999'999'999};
  probe.Settle();
  CHECK(BankMaxDeposit(probe.view.vault, probe.view.zeny, probe.view.vault_max) == 2000);
  // Et le même solde sous un serveur muet : rien à déposer, sans négatif.
  probe.view.vault_max = kBankVaultMaxUnannounced;
  probe.Settle();
  CHECK(BankMaxDeposit(probe.view.vault, probe.view.zeny, probe.view.vault_max) == 0);
}
