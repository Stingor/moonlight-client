#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "ro/core/error.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_news.h"
#include "ro/sim/survivors_play.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"
#include "ro/sim/survivors_week.h"
#include "ro/sim/walk_map.h"

#include "survivors/presentation_clock.h"
#include "survivors/survivors_keys.h"

namespace moonclient {

// L'ÉTAGE DU MODE SURVIVORS, sans GPU ni SDL : la machine d'états de l'écran,
// la partie et son journal, l'horloge de présentation. app l'avance à chaque
// image avec l'intention traduite des événements ; les fenêtres le lisent.
//
//   kClosed → kPreparing → kLoading → kArriving → kPlaying
//   kPlaying ⇄ kPaused / kChoosing / kChest
//   kPlaying → kDying / kWon → kSummary → kLoading (Rejouer) ou kClosed
//
// La partie n'avance qu'en kPlaying. Pause, choix, coffre et écrans de fin
// cessent simplement de l'appeler : rien ne s'inscrit au journal, et la
// reprise ne rattrape pas le temps passé. Le coffre fige le monde jusqu'au
// clic qui ferme sa liste, comme le choix de niveau (Q1). Une pause, même
// automatique, tient jusqu'à « Reprendre » ou Échap, au-delà de la garde (Q2).
// Prise pendant l'arrivée, elle y reprend, au temps de descente où elle l'a
// laissée. Une alerte ou une perte de focus pendant un choix ou un coffre
// attend leur fin : la pause tombe au retour au jeu, avec sa cause.
enum class SurvivorsScreen : std::uint8_t {
  kClosed,
  kPreparing,
  kLoading,
  kArriving,
  kPlaying,
  kPaused,
  kChoosing,
  kChest,
  kDying,
  kWon,
  kSummary,
};
// Ce qui a mis la partie en pause : le bandeau de la pause le dit.
enum class PauseCause : std::uint8_t { kPlayer, kFocusLost, kWorldAlert };

// L'arrivée, que la descente de la caméra suit (`arrival_ms`), la roulette
// d'un coffre, le voile de la mort (U15) et le bandeau « Victoire ! » (Q7), en
// temps d'écran. Les trois derniers se passent d'un clic, au-delà de la garde.
inline constexpr std::uint32_t kArrivalMs = 1200;
inline constexpr std::uint32_t kChestRouletteMs = 1500;
inline constexpr std::uint32_t kDeathVeilMs = 3000;
inline constexpr std::uint32_t kVictoryBannerMs = 3000;

// L'INTENTION D'UNE IMAGE, traduite des événements par app.
struct SurvivorsIntent {
  std::uint8_t heading = ro::sim::kNoHeading;  // le cap tenu
  bool escape = false;                         // pause, ou reprise depuis la pause
  bool resume = false;                         // le bouton « Reprendre »
  bool focus_lost = false;                     // pause automatique (U6)
  bool world_alert = false;                    // pause automatique (U5)
  bool proceed = false;  // un clic qui passe la roulette, le voile, le bandeau
  bool abandon = false;  // confirmé dans la pause
  std::optional<ro::sim::SurvivorsAnswer> answer;  // touche 1 à 4, R, P ou B
};

// LE TABLEAU OÙ UNE PARTIE SE JOUE : la partie libre, au départ que donne le
// carnet, le défi d'une semaine, au départ neutre (U22), ou la partie classée,
// au départ neutre sur la graine que le serveur a tirée (Q2).
enum class SurvivorsBoardKind : std::uint8_t { kFree, kChallenge, kRanked };
struct SurvivorsRunBoard {
  SurvivorsBoardKind kind = SurvivorsBoardKind::kFree;
  ro::sim::IsoWeek week;  // la semaine du défi ; {0, 0} hors du défi
  [[nodiscard]] bool operator==(const SurvivorsRunBoard&) const = default;
};

// CE QUE L'ÉCRAN DE FIN MONTRE d'une partie, et ce qu'une partie interrompue
// garde jusqu'à la prochaine ouverture de la salle (Q3). Les cartes portées
// (`cards`) sont celles de la partie ; les cartes TOMBÉES, ramassées pour
// l'Album, sont dans `recap.cards_found`.
struct SurvivorsEnding {
  std::uint16_t class_id = 0;
  ro::sim::SurvivorsTally tally;
  ro::sim::SurvivorsRecap recap;
  std::vector<ro::sim::OwnedCard> cards;
  std::uint32_t zeny = 0;
  SurvivorsRunBoard board;
  [[nodiscard]] bool operator==(const SurvivorsEnding&) const = default;
};
[[nodiscard]] SurvivorsEnding EndingOf(const ro::sim::SurvivorsField& field,
                                       const SurvivorsRunBoard& board);

class SurvivorsStage {
 public:
  // kClosed → kPreparing ; sans effet ailleurs.
  void Open();
  // kPreparing, kLoading ou kSummary → kLoading, sur une partie neuve : un
  // départ qui attend son jeton derrière l'écran de chargement s'y refait sur
  // la graine du serveur, ou en partie libre. kInvalidArgument ailleurs ; les
  // refus de SurvivorsRun::Start tels quels, l'étage restant où il était. La
  // fin de la partie porte son tableau.
  [[nodiscard]] ro::core::Result<void> Begin(const ro::sim::SurvivorsRules& rules,
                                             ro::sim::WalkMap arena,
                                             const ro::sim::SurvivorsStart& start,
                                             const SurvivorsRunBoard& board = {});
  // La partie garde l'adresse de ses règles.
  ro::core::Result<void> Begin(const ro::sim::SurvivorsRules&& rules, ro::sim::WalkMap arena,
                               const ro::sim::SurvivorsStart& start,
                               const SurvivorsRunBoard& board = {}) = delete;
  // L'arène est montée : kLoading → kArriving.
  void Loaded();
  // Une image de `elapsed_ms` de temps d'écran. Les nouvelles de la partie
  // sont celles de cette image seulement.
  void Advance(std::uint32_t elapsed_ms, const SurvivorsIntent& intent);
  // LA LIAISON EST PERDUE (Q3) : une partie en cours s'abandonne au pas de la
  // coupure (kAbandoned, inscrit au journal) et l'étage passe au bilan. Une
  // partie finie dont le bilan n'a pas encore paru (voile de la mort, bandeau
  // de victoire) y passe telle quelle. Vrai dans ces deux cas : app écrit
  // alors son journal et sa fin, que la salle montrera à sa réouverture.
  bool Freeze();
  // Le bilan d'une partie interrompue, relu à l'ouverture de la salle.
  void ShowEnding(SurvivorsEnding ending);
  // Retour au jeu : la partie est oubliée.
  void Close();

  [[nodiscard]] SurvivorsScreen screen() const noexcept { return screen_; }
  // L'écran est au mode : tout sauf la fermeture et la préparation, qui se
  // dessine dans l'image du jeu.
  [[nodiscard]] bool owns_screen() const noexcept;
  // L'écran où la dernière image est entrée, s'il a changé.
  [[nodiscard]] std::optional<SurvivorsScreen> entered() const noexcept { return entered_; }
  [[nodiscard]] PauseCause pause_cause() const noexcept { return pause_cause_; }
  // Le temps d'écran passé dans l'écran courant.
  [[nodiscard]] std::uint64_t screen_elapsed_ms() const noexcept {
    return clock_ms_ - entered_at_ms_;
  }
  // Le temps d'arrivée passé, borné à `kArrivalMs` : il conduit la descente
  // de la caméra, s'arrête avec la pause et reprend avec elle.
  [[nodiscard]] std::uint32_t arrival_ms() const noexcept { return arrival_ms_; }
  // Vrai quand la garde laisse passer une réponse ou un clic.
  [[nodiscard]] bool accepts_input() const noexcept { return guard_.Accepts(clock_ms_); }
  // En kChest : la roulette est passée, la liste des gains est montrée.
  [[nodiscard]] bool chest_list_shown() const noexcept { return chest_list_shown_; }
  // La partie, de kLoading au bilan ; rien pour un bilan relu.
  [[nodiscard]] const ro::sim::SurvivorsPlay* play() const noexcept;
  [[nodiscard]] const ro::sim::SurvivorsNews& news() const noexcept { return news_; }
  // Le coffre ouvert que la fenêtre montre, en kChest.
  [[nodiscard]] const std::vector<ro::sim::SurvivorsChestNews>& chests() const noexcept {
    return chests_;
  }
  [[nodiscard]] std::uint64_t presentation_ms() const noexcept {
    return presentation_.presentation_ms();
  }
  // La fin de la partie, dès kDying ou kWon.
  [[nodiscard]] const SurvivorsEnding* ending() const noexcept;
  // Le tableau de la partie commencée.
  [[nodiscard]] const SurvivorsRunBoard& board() const noexcept { return board_; }

 private:
  void Enter(SurvivorsScreen screen);
  void AdvancePlaying(std::uint32_t elapsed_ms, const SurvivorsIntent& intent);
  void AdvanceChoosing(const SurvivorsIntent& intent);
  void AdvanceChest(const SurvivorsIntent& intent);
  void AdvanceArriving(std::uint32_t elapsed_ms, const SurvivorsIntent& intent);
  void AdvancePaused(const SurvivorsIntent& intent);
  // Une pause automatique demandée pendant un choix ou un coffre : gardée
  // jusqu'au retour au jeu.
  void HoldPause(const SurvivorsIntent& intent);
  void AdvanceEnd(std::uint32_t elapsed_ms, const SurvivorsIntent& intent, std::uint32_t lasts_ms);
  void PauseFor(PauseCause cause);
  // L'écran que la partie impose après un pas ou une réponse : la fin, un
  // coffre ouvert, un choix, sinon le jeu.
  void FollowRun();
  void TakeNews();

  SurvivorsScreen screen_ = SurvivorsScreen::kClosed;
  std::optional<SurvivorsScreen> entered_;
  std::optional<ro::sim::SurvivorsPlay> play_;
  std::optional<SurvivorsEnding> ending_;
  SurvivorsRunBoard board_;
  ro::sim::SurvivorsNews news_;
  std::vector<ro::sim::SurvivorsChestNews> chests_;
  PresentationClock presentation_;
  InputGuard guard_;
  std::uint64_t clock_ms_ = 0;  // temps d'écran depuis l'ouverture
  std::uint64_t entered_at_ms_ = 0;
  PauseCause pause_cause_ = PauseCause::kPlayer;
  std::optional<PauseCause> held_pause_;
  std::uint32_t arrival_ms_ = 0;
  bool chest_list_shown_ = false;
};

}  // namespace moonclient
