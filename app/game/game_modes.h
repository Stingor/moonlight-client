#pragma once

#include "ro/render/screen_tint.h"

#include "afk/afk_screen.h"
#include "game/survivors_mode_slot.h"
#include "game/survivors_ranking_mode.h"
#include "survivors/survivors_stage.h"

namespace moonclient {

// LES MODES QUI PRENNENT L'ÉCRAN À LA PARTIE, et ce qu'ils gardent d'une image
// à l'autre : la veille et le mode survivors. Un mode qui remplace l'image de
// la partie range son état ici, et se joue dans son propre fichier —
// `game_afk.cpp` pour la veille, `game_survivors.cpp` pour survivors.
//
// 🔴 UNE BASE DE `Game`, DONC CONSTRUITE AVANT TOUT MEMBRE ET DÉTRUITE APRÈS :
// rien de ce qu'un mode garde ne lit un membre de la partie pour se construire.
//
// ⚠ AUCUNE MÉTHODE : la partie les lit comme les siens, sous le même nom.
class GameModes {
 protected:
  // ── L'ÉCRAN DE VEILLE ──────────────────────────────────────────────────────
  //
  // ⚠ LES DEUX VONT ENSEMBLE et ne se séparent pas : le compteur décide QUAND
  // s'endormir et ce qu'il faut avaler, la veille ce que cela fait à l'image.
  AfkIdle afk_idle_;
  AfkScreen afk_screen_;
  // LA TEINTE DE LA VEILLE, sur l'image entière, après le post-traitement.
  ro::render::ScreenTint screen_tint_;
  // « ESSAYER MAINTENANT », armé par la page de réglages. ⚠ CONSOMMÉ À L'IMAGE
  // SUIVANTE : laissé en place, la veille se relancerait à chaque image.
  bool afk_requested_ = false;
  // Les deux lignes de son horloge vivent dans `GameUiBuffers`.

  // ── LE MODE SURVIVORS, dans `game_survivors.cpp` ───────────────────────────
  //
  // L'ÉTAGE NAÎT FERMÉ et ne lit rien pour se construire ; la route des
  // événements l'interroge (`owns_screen`). Le reste du mode — l'arène montée
  // à l'entrée derrière l'écran de chargement, gardée pour « Rejouer » et
  // libérée au « Retour », la veille du monde réel, les sons, la musique, les
  // vues — vit dans `SurvivorsMode`, que la partie nourrit image par image.
  SurvivorsStage survivors_;
  // LE CLASSEMENT DU MODE, qui vit hors d'une partie : il lit le flux et la
  // poignée de main à chaque image, et le mode lui emprunte le départ et la
  // fin des parties classées. ⚠ AVANT LE MODE, qui le désigne jusqu'à sa
  // destruction. Voir `SurvivorsRankingMode`.
  SurvivorsRankingMode survivors_ranking_;
  SurvivorsModeSlot survivors_mode_;
};

}  // namespace moonclient
