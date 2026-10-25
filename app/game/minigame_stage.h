#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <optional>
#include <vector>

#include "ro/render/actor_wardrobe.h"
#include "ro/render/item_icons.h"
#include "ro/sim/match3_game.h"
#include "ro/sim/minigame_trophies.h"
#include "ro/sim/peg_game.h"
#include "ro/sim/survivors_run.h"
#include "ro/ui/escape_stack.h"
#include "ro/ui/minigames_window.h"
#include "ro/ui/roggle_window.h"
#include "ro/ui/rojeweled_window.h"
#include "ro/ui/survivors_prepare_window.h"

#include "game/interface_sound.h"
#include "game/minigame_file.h"
#include "game/minigame_trophy_announcer.h"
#include "game/ui_stage_survivors.h"
#include "world/monster_sprite.h"

namespace moonclient {

// LES MINI-JEUX dans la trame de l'interface : la salle, les deux jeux, leurs
// vues, et ce qu'ils ont demandé.
struct MinigameWindows {
  ro::ui::MinigamesState hub;
  ro::ui::MinigamesView hub_view;
  ro::ui::MinigamesAsk hub_answer;
  ro::ui::RojeweledState rojeweled;
  ro::ui::RojeweledView rojeweled_view;
  ro::ui::RojeweledAsk rojeweled_answer;
  ro::ui::RoggleState roggle;
  ro::ui::RoggleView roggle_view;
  ro::ui::RoggleAsk roggle_answer;
  // LA PRÉPARATION D'UNE PARTIE DE SURVIVORS, que la troisième ligne de la
  // salle ouvre : dessinée dans l'image du jeu, le jeu restant visible. Sa vue
  // est composée par la partie (`SurvivorsMode`), qui lit sa réponse.
  ro::ui::SurvivorsPrepareState survivors_prepare;
  ro::ui::SurvivorsPrepareView survivors_prepare_view;
  ro::ui::SurvivorsPrepareAsk survivors_prepare_answer;
  bool survivors_prepare_composed = false;
  // Les fenêtres de la méta que l'accueil ouvre, dessinées avec lui.
  SurvivorsMetaWindows survivors_meta;
  // Les vues ont été composées pour l'image : sans elles, une fenêtre
  // désignerait ce que l'étage d'une autre partie a rangé.
  bool composed = false;
};

// DESSINE LES FENÊTRES, quand leurs vues sont composées, et les déclare à
// Échap : la salle, les deux jeux, la préparation de survivors et les
// fenêtres de méta qu'elle ouvre.
void DrawMinigameWindows(MinigameWindows& windows, const ro::ui::SkinArt& art,
                         ro::ui::EscapeStack& escape);

// ENTRÉE EST-ELLE À LA MÉTA SURVIVORS ? L'accueil, l'Église, la Kafra ou
// l'Album qui a le focus en fait son geste — partir, acheter, déplier une
// carte —, et la touche ne va pas alors à la barre de conversation. Le focus
// est celui de l'image précédente : voir `ro::ui::WindowHasFocus`.
[[nodiscard]] bool SurvivorsClaimsEnter(const MinigameWindows& windows);

// CE QUE L'ÉTAGE EMPRUNTE À LA PARTIE. ⚠ TOUT PEUT MANQUER : sans art, les
// jeux se jouent avec leurs formes de repli.
struct MinigamePieces {
  // Nul : aucun monstre, des pastilles seules.
  ro::render::ActorWardrobe* wardrobe = nullptr;
  // Nul : des plots ronds au lieu des gemmes.
  ro::render::ItemIcons* item_icons = nullptr;

  [[nodiscard]] static MinigamePieces Of(std::optional<ro::render::ActorWardrobe>& wardrobe,
                                         std::optional<ro::render::ItemIcons>& item_icons) {
    return MinigamePieces{.wardrobe = wardrobe.has_value() ? &*wardrobe : nullptr,
                          .item_icons = item_icons.has_value() ? &*item_icons : nullptr};
  }
};

// LE PLUS LONG PAS D'UNE IMAGE : un gel de l'affichage, comme le chargement
// d'une carte derrière son écran, ne se rattrape qu'à hauteur d'un quart de
// seconde.
inline constexpr std::uint32_t kMostWatchedStepMs = 250;

// UNE HORLOGE QUI NE COURT QUE FENÊTRE OUVERTE.
//
// ⚠ LA PREMIÈRE IMAGE APRÈS UNE OUVERTURE N'AVANCE DE RIEN, plutôt que de
// rattraper la fermeture ; aucun pas ne dépasse `kMostWatchedStepMs`.
class WatchedClock {
 public:
  // À CHAQUE IMAGE, fenêtre ouverte ou non. Rend le temps couru depuis l'image
  // précédente : nul fenêtre fermée.
  std::uint32_t Follow(bool watched, std::uint32_t now_ms);
  // Le temps couru fenêtre ouverte, en tout.
  [[nodiscard]] std::uint32_t watched_ms() const noexcept { return watched_ms_; }

 private:
  std::uint32_t watched_ms_ = 0;
  // L'image précédente ; vide quand la fenêtre y était fermée.
  std::optional<std::uint32_t> last_ms_;
};

// L'ÉTAGE DES MINI-JEUX : les parties en cours, les trophées, et ce qu'il faut
// dessiner et faire entendre.
//
// ⚠ UNE PARTIE SE FIGE FENÊTRE FERMÉE, et reprend à sa réouverture : le temps
// ne court que pendant qu'on regarde. Chaque jeu a sa `WatchedClock` ; l'avis
// d'un trophée et l'animation des monstres suivent l'heure du client.
//
// ⚠ L'ENREGISTREMENT SE LIT À LA PREMIÈRE AVANCÉE, et s'écrit à chaque
// trophée ou meilleur score : rien ne se perd si le client se ferme mal. Une
// écriture ratée se retente au changement suivant, à la fermeture des fenêtres
// et à la fin de la partie, jamais à chaque image.
class MinigameStage {
 public:
  void Advance(MinigameWindows& windows, const MinigamePieces& pieces,
               const std::filesystem::path& client_root, std::uint32_t now_ms);

  // À LA FIN DE LA PARTIE : une écriture ratée se retente une dernière fois.
  void Leave(const std::filesystem::path& client_root);

  // UNE PARTIE DE SURVIVORS FINIE, ou figée par une liaison perdue : ses
  // trophées s'inscrivent et s'écrivent aussitôt, et se rendent : leur avis
  // est une ligne de l'écran de fin du mode (Q13), jamais la file des avis des
  // autres jeux. `Announce` les dit au serveur. Voir `ro::sim::RecordSurvivors`.
  [[nodiscard]] std::vector<ro::sim::MinigameTrophy> RecordSurvivorsRun(
      const ro::sim::SurvivorsRun& run, const std::filesystem::path& client_root);

  // LES SONS À JOUER. Consommés.
  [[nodiscard]] std::vector<InterfaceSoundFile> TakeSounds();

  // À CHAQUE IMAGE, liaison ouverte : dit au serveur les trophées qu'il
  // n'a pas encore appris. `map_entries` est le compte d'entrées en carte du
  // monde. Voir `MinigameTrophyAnnouncer`.
  //
  // ⚠ APRÈS `PumpNetwork`, qui envoie l'avis de chargement dans l'image même où
  // `map_entries` change : redits avant lui, les trophées seraient jetés une
  // seconde fois.
  void Announce(ro::net::Connection& link, bool verified, std::uint32_t map_entries,
                const std::filesystem::path& client_root);

 private:
  [[nodiscard]] ro::sim::MinigameRecord& Record(const std::filesystem::path& client_root);
  void Save(const std::filesystem::path& client_root);
  void ApplyAnswers(MinigameWindows& windows);
  void PlayRojeweled();
  void PlayRoggle(std::uint32_t step_ms);
  void ComposeHub(MinigameWindows& windows);
  void ComposeRojeweled(MinigameWindows& windows, const MinigamePieces& pieces,
                        std::uint32_t now_ms);
  void ComposeRoggle(MinigameWindows& windows, const MinigamePieces& pieces, std::uint32_t now_ms);
  [[nodiscard]] const char* Toast(std::uint32_t now_ms, float& alpha);
  [[nodiscard]] ro::ui::SkinImage SpriteOf(std::size_t slot, std::uint32_t sprite_class, int box_px,
                                           ro::render::ActorWardrobe* wardrobe,
                                           std::uint32_t now_ms);

  std::optional<MinigameRecordHeld> record_;
  // La dernière écriture de `record_` a échoué.
  bool record_unsaved_ = false;
  MinigameTrophyAnnouncer announcer_;
  ro::sim::Match3Game rojeweled_{0};
  ro::sim::PegGame roggle_{0};
  bool seeded_ = false;
  WatchedClock rojeweled_clock_;
  WatchedClock roggle_clock_;
  std::vector<InterfaceSoundFile> sounds_;
  // Les trophées à annoncer, le premier à l'écran depuis `toast_started_ms_`.
  std::deque<ro::sim::MinigameTrophy> toasts_;
  std::optional<std::uint32_t> toast_started_ms_;
  // Un monstre par espèce de gemme, puis le Poring de la bille.
  std::array<std::optional<MonsterSprite>, ro::ui::kRojeweledKinds + 1> sprites_;
  // LA COMPOSITION : ce que les vues désignent.
  std::vector<ro::ui::MinigameTrophyRow> trophy_rows_;
  std::vector<ro::ui::RojeweledBurst> bursts_;
  std::vector<ro::ui::RogglePeg> pegs_;
  std::vector<ro::ui::RogglePoint> guide_;
};

}  // namespace moonclient
