#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>

#include "survivors/survivors_stage.h"

namespace moonclient {

// LA PORTE DU MODE, sans écran : ce que le mode mène quand la liaison tombe,
// quand le bilan répond, et quand la salle des Mini-jeux se rouvre. `app`
// exécute les pas ; ces décisions s'essaient sans fenêtre ni son.

// LA LIAISON PERDUE PENDANT QUE LE MODE TIENT L'ÉCRAN (Q3 A) : une partie
// commencée se fige au pas de la coupure et s'écrit comme interrompue, puis le
// mode sort — musique de la carte, bruits tus, arène libérée — pour que la
// boîte de liaison perdue paraisse. Le bilan attend la réouverture de la salle.
struct LinkLossSteps {
  // Figer la partie et écrire son journal et sa fin interrompue.
  bool freeze = false;
  // Sortir du mode.
  bool exit = false;
  [[nodiscard]] bool operator==(const LinkLossSteps&) const = default;
};
[[nodiscard]] LinkLossSteps LinkLossStepsOf(SurvivorsScreen screen, bool link_open) noexcept;

// Fige l'étage (`SurvivorsStage::Freeze`) et écrit la partie interrompue.
// Vrai quand une partie s'est close là ; un échec d'écriture se dit au journal
// du client.
bool FreezeAndSave(SurvivorsStage& stage, const std::filesystem::path& client_root);

// CE QUE DEMANDE LE BILAN. « Rejouer » rejuge la garde d'entrée : un refus
// rend la préparation avec sa raison.
enum class SummaryStep : std::uint8_t {
  kStay,
  kBack,
  kReplay,
  kBackToPrepare,
};
[[nodiscard]] SummaryStep SummaryStepOf(bool back, bool replay, bool entry_refused) noexcept;

// LES OPTIONS DU JEU OUVERTES DEPUIS LA PAUSE : « Options » les ouvre
// par-dessus elle, Échap les ferme d'abord et rend la pause, et elles se
// ferment dès que la pause cesse — reprise, abandon, fin. La partie reste en
// pause tant qu'elles sont ouvertes : rien ici ne la relance.
//
// `window_open` est la case de la fenêtre d'options du jeu, que la porte ouvre
// et ferme. ⚠ LA PORTE REND LA CASE TELLE QU'ELLE L'A TROUVÉE : des options
// laissées ouvertes dans le jeu avant la partie s'y retrouvent après, même
// montrées par « Options » sur la pause.
class PauseOptionsDoor {
 public:
  // AVANT L'AVANCÉE DE L'ÉTAGE. Vrai quand Échap a fermé la fenêtre : il ne va
  // pas à la partie.
  [[nodiscard]] bool Follow(bool& window_open, bool open_asked, bool escape,
                            SurvivorsScreen screen) noexcept;
  // APRÈS L'AVANCÉE : la pause finie, la fenêtre qu'elle a ouverte se ferme.
  void Settle(bool& window_open, SurvivorsScreen screen) noexcept;
  // La fenêtre se dessine par-dessus la pause.
  [[nodiscard]] bool shown(bool window_open, SurvivorsScreen screen) const noexcept {
    return opened_here_ && window_open && screen == SurvivorsScreen::kPaused;
  }

 private:
  // La pause cesse de montrer la fenêtre : la case reprend son état d'avant.
  void Release(bool& window_open) noexcept;

  bool opened_here_ = false;
  // La fenêtre était déjà ouverte quand la pause l'a demandée.
  bool open_before_ = false;
};

// LA SALLE ROUVERTE MONTRE LE BILAN D'UNE PARTIE INTERROMPUE (Q3), une fois :
// à la seule image où elle s'ouvre, l'étage fermé, la fin écrite se lit et
// s'efface.
class HubDoor {
 public:
  [[nodiscard]] std::optional<SurvivorsEnding> EndingToShow(
      bool hub_open, SurvivorsScreen screen, const std::filesystem::path& client_root);

 private:
  bool hub_was_open_ = false;
};

}  // namespace moonclient
