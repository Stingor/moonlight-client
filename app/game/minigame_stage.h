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
#include "ro/ui/escape_stack.h"
#include "ro/ui/minigames_window.h"
#include "ro/ui/roggle_window.h"
#include "ro/ui/rojeweled_window.h"

#include "game/interface_sound.h"
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
  // Les vues ont été composées pour l'image : sans elles, une fenêtre
  // désignerait ce que l'étage d'une autre partie a rangé.
  bool composed = false;
};

// DESSINE LES TROIS FENÊTRES, quand leurs vues sont composées, et les déclare
// à Échap.
void DrawMinigameWindows(MinigameWindows& windows, const ro::ui::SkinArt& art,
                         ro::ui::EscapeStack& escape);

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

// L'ÉTAGE DES MINI-JEUX : les parties en cours, les trophées, et ce qu'il faut
// dessiner et faire entendre.
//
// ⚠ UNE PARTIE SE FIGE FENÊTRE FERMÉE, et reprend à sa réouverture : le temps
// ne court que pendant qu'on regarde.
//
// ⚠ L'ENREGISTREMENT SE LIT À LA PREMIÈRE AVANCÉE, et s'écrit à chaque
// trophée ou meilleur score : rien ne se perd si le client se ferme mal.
class MinigameStage {
 public:
  void Advance(MinigameWindows& windows, const MinigamePieces& pieces,
               const std::filesystem::path& client_root, std::uint32_t now_ms);

  // LES SONS À JOUER. Consommés.
  [[nodiscard]] std::vector<InterfaceSoundFile> TakeSounds();

 private:
  void ApplyAnswers(MinigameWindows& windows, std::uint32_t now_ms);
  void PlayRojeweled(std::uint32_t now_ms);
  void PlayRoggle(std::uint32_t now_ms);
  void ComposeHub(MinigameWindows& windows);
  void ComposeRojeweled(MinigameWindows& windows, const MinigamePieces& pieces,
                        std::uint32_t now_ms);
  void ComposeRoggle(MinigameWindows& windows, const MinigamePieces& pieces, std::uint32_t now_ms);
  [[nodiscard]] const char* Toast(std::uint32_t now_ms, float& alpha);
  [[nodiscard]] ro::ui::SkinImage SpriteOf(std::size_t slot, std::uint32_t sprite_class, int box_px,
                                           ro::render::ActorWardrobe* wardrobe,
                                           std::uint32_t now_ms);

  std::optional<ro::sim::MinigameRecord> record_;
  ro::sim::Match3Game rojeweled_{0};
  ro::sim::PegGame roggle_{0};
  bool seeded_ = false;
  // La dernière avancée de Roggle, pour le temps écoulé ; vide fenêtre fermée.
  std::optional<std::uint32_t> roggle_last_ms_;
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
