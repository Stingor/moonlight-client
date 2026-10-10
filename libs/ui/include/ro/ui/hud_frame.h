#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>

#include <imgui.h>

#include "ro/ui/align_grid.h"

namespace ro::ui {

// ── LE CADRE DE HUD LIBRE ───────────────────────────────────────────────────
//
// La brique des éléments d'interface que le joueur POSE lui-même : une zone
// rectangulaire sans barre de titre ni bordure, qu'on saisit n'importe où pour
// la déplacer et par ses BORDS pour la retailler.
//
// 🔴 CE N'EST PAS UNE FENÊTRE, ET LA DIFFÉRENCE EST DANS LE GESTE. On ne
// « range » pas un HUD : on le compose, et il n'a donc ni titre, ni bouton de
// fermeture, ni mémoire de pile. Verrouillé, il se fige ET laisse passer les
// clics au jeu — c'est la différence entre un HUD qu'on met en place et un HUD
// dont on se sert.
//
// 🔴 ET LA POIGNÉE SE VOIT, parce que le curseur du jeu n'est pas celui du
// système : un client Ragnarok dessine son propre curseur, donc la flèche de
// redimensionnement que l'OS afficherait ailleurs ne paraît JAMAIS ici. Sans un
// repère DANS le cadre, rien ne dirait qu'il se retaille.

// Les bords, en bitmask. Un coin en allume deux.
enum HudEdge : int {
  kHudEdgeNone = 0,
  kHudEdgeLeft = 1,
  kHudEdgeRight = 2,
  kHudEdgeTop = 4,
  kHudEdgeBottom = 8,
};

// L'épaisseur de prise d'un bord, et celle du coin bas-droit — plus généreuse,
// parce que c'est la prise que l'on cherche en premier.
inline constexpr float kHudEdgeGrab = 5.0F;
inline constexpr float kHudCornerGrab = 12.0F;

// Le rayon d'aimantation entre cadres frères.
inline constexpr float kHudSnapReach = 10.0F;

// À quelle distance deux cadres « se touchent », pour le déplacement en bloc.
inline constexpr float kHudTouchSlack = 2.0F;

// Le plus grand nombre de frères qu'un groupe porte, tenu par le masque de bits
// du bloc. ⚠ CE N'EST PAS UNE LIMITE D'INTERFACE mais celle du masque : un
// groupe plus large ne se déplacerait plus d'un seul tenant, en silence.
inline constexpr int kHudMostSiblings = 32;

// La géométrie mémorisée d'un cadre, en pixels. ⚠ C'EST L'APPELANT QUI LA
// PERSISTE : `ui` ne possède rien.
//
// 🔴 LA PLACE S'ANCRE AU BORD LE PLUS PROCHE, comme celle des fenêtres — voir
// `WindowPlace`. `x` et `y` sont la distance au bord d'ancrage de chaque axe :
// un cadre posé dans le tiers droit de l'écran suit le bord droit quand la
// résolution change. Sans ancre, `x` et `y` se lisent depuis le coin haut-gauche.
struct HudRect {
  float x = 0.0F;
  float y = 0.0F;
  float width = 120.0F;
  float height = 20.0F;
  // Le rang d'`AnchorSide` en largeur, puis en hauteur. ⚠ UN ENTIER : c'est ce
  // que le fichier de réglages sait ranger.
  int across = 0;
  int down = 0;
};

// LE CADRE À L'ÉCRAN, coin haut-gauche en pixels, tel que l'ancre le donne sur
// cet écran, et ramené dedans.
[[nodiscard]] HudRect HudOnScreen(const HudRect& stored, ImVec2 screen);
// LA PLACE À RETENIR pour un cadre posé là sur cet écran : ancrée au bord le
// plus proche de chaque axe.
[[nodiscard]] HudRect HudAnchored(const HudRect& on_screen, ImVec2 screen);

// Les cadres FRÈRES : ceux avec qui celui-ci s'aimante, et qui le suivent en
// déplacement CTRL.
//
// ⚠ `shown` PEUT ÊTRE VIDE — tous visibles. Sinon elle s'indexe comme `frames` :
// un cadre masqué n'aimante rien et ne part pas en bloc, sans quoi on
// s'alignerait sur ce qu'on ne voit pas.
struct HudSiblings {
  std::span<HudRect> frames;
  std::span<const bool> shown;
  // La place de CE cadre dans le groupe. -1 quand il n'en a pas.
  int mine = -1;

  [[nodiscard]] bool Shown(int rank) const;
};

// Ce que le joueur est en train de faire à UN cadre.
//
// 🔴 UN SEUL POUR TOUS LES CADRES, ET C'EST VOULU : il n'y a qu'une souris. Un
// état par cadre autoriserait le défaut « deux cadres se croient attrapés », que
// rien ne signalerait. L'appelant en garde donc un et le passe à chacun.
struct HudGrip {
  // L'identité du cadre saisi ; vide quand aucun ne l'est.
  std::string held;
  int edges = kHudEdgeNone;
  bool resizing = false;
  // L'écart du curseur à ce qu'il tient, figé au début du geste.
  ImVec2 offset{0.0F, 0.0F};
  // Les frères emportés par le bloc CTRL.
  std::uint32_t block = 0;

  // Ce que `EndHudFrame` doit peindre PAR-DESSUS le contenu.
  //
  // 🔴 LA POIGNÉE ET LES BORDS NE PEUVENT PAS SE PEINDRE DANS `Begin` : le
  // contenu viendrait après et les recouvrirait. Un seul cadre est ouvert à la
  // fois, donc une seule attente suffit — ce n'est pas une pile.
  struct Pending {
    bool open = false;
    bool locked = false;
    bool resizable = true;
    int lit = kHudEdgeNone;
    ImVec2 top_left{0.0F, 0.0F};
    ImVec2 bottom_right{0.0F, 0.0F};
  };
  Pending pending;

  [[nodiscard]] bool Holds(const char* id) const;
};

struct HudFrameLook {
  // Fige la géométrie ET rend le cadre transparent aux clics.
  bool locked = false;
  bool border = false;
  float rounding = 4.0F;
  // Vide = aucun fond ; le contenu se pose alors à même le monde.
  std::optional<ImU32> background;
  // La grille partagée. ⚠ PASSÉE EN PARAMÈTRE et non lue ici : `ui` ne sait rien
  // de qui la possède.
  const AlignGrid* grid = nullptr;
  bool sticky = true;
  float least_width = 8.0F;
  float least_height = 8.0F;

  // 🔴 CLIQUABLE BIEN QUE VERROUILLÉ : la géométrie reste figée, mais le cadre
  // REPREND la souris au jeu au lieu de la laisser passer.
  //
  // ⚠ ET LA REPRISE EST TOTALE : tant que le curseur est dessus, le clic droit
  // et la molette ne vont plus au jeu non plus. À n'allumer que quand le cadre
  // sert vraiment — l'appelant en décide à chaque image.
  bool clickable = false;

  // 🔴 LE REPÈRE DE CENTRE : centrer un HUD à l'œil est faux d'une poignée de
  // pixels, et l'erreur ne se voit qu'une fois reverrouillé — trop tard pour la
  // corriger sans tout rouvrir. La grille aimante les BORDS ; ce repère donne le
  // point qu'on veut réellement poser sur une de ses lignes. Il disparaît avec
  // le déverrouillage : c'est un outil de pose, pas une décoration.
  bool center_mark = false;

  // SE RETAILLE-T-IL ? Faux pour ce dont la taille est celle d'un dessin — une
  // icône : ses bords ne se saisissent plus, tout le cadre déplace, et la
  // poignée du coin disparaît, puisqu'elle promettrait un geste qui n'existe pas.
  bool resizable = true;

  // Le survol d'un cadre cliquable allume ses bords. ⚠ ÉTEINT POUR UN BOUTON
  // QUI DESSINE LUI-MÊME SON ÉTAT, comme une icône et son bitmap enfoncé : deux
  // retours pour un seul survol se contrediraient.
  bool lit_on_hover = true;
};

// Ce qu'un cadre CLIQUABLE a reçu cette image.
struct HudFrameClicks {
  bool hovered = false;
  bool left = false;
  bool right = false;
  // Le bouton gauche est TENU sur le cadre : c'est l'état enfoncé d'un bouton.
  bool held = false;
};

// ── LA GÉOMÉTRIE, SÉPARÉE DU DESSIN ────────────────────────────────────────
//
// ⚠ SÉPARÉE POUR ÊTRE ESSAYÉE, comme le placement d'une rangée d'états. Ce sont
// ces quatre règles qui se trompent en silence — un bord qui s'aimante du
// mauvais côté, un bloc qui oublie un frère — et aucune ne demande d'ImGui.

// Quels bords le curseur tient. Le coin bas-droit en rend DEUX.
[[nodiscard]] int HudEdgesAt(const HudRect& rect, ImVec2 mouse);

// Une coordonnée ramenée sur un cadre frère : même bord, bord opposé, juste
// après, juste avant. Rend `at` quand rien n'est assez proche.
[[nodiscard]] float SnappedToSiblings(float at, float extent, const HudSiblings& siblings,
                                      bool vertical);

// Le bloc de cadres qui se TOUCHENT, en partant de `seed`, et transitivement :
// A touche B, B touche C, les trois partent ensemble.
[[nodiscard]] std::uint32_t TouchingFrames(const HudSiblings& siblings, int seed);

// La géométrie que le tirage des bords `edges` donne. Les bords NON saisis
// restent épinglés, et chacun s'aimante à la grille avant d'être borné.
[[nodiscard]] HudRect ResizedTo(const HudRect& rect, int edges, ImVec2 mouse, ImVec2 offset,
                                const HudFrameLook& look, ImVec2 screen);

// ── LE CADRE ────────────────────────────────────────────────────────────────

// Ouvre le cadre `id` — une identité ImGui, donc stable et unique. Met à jour
// `rect` (et les frères, en bloc CTRL) quand le joueur agit, et pose `*moved`
// dans ce cas : l'appelant s'en sert pour ne persister qu'une fois.
//
// Rend vrai quand le contenu doit être dessiné. ⚠ `EndHudFrame` SE RAPPELLE
// TOUJOURS, y compris sur faux — c'est la règle Begin/End d'ImGui.
bool BeginHudFrame(const char* id, HudRect& rect, const HudFrameLook& look,
                   const HudSiblings& siblings, HudGrip& grip, bool* moved = nullptr,
                   HudFrameClicks* clicks = nullptr);
void EndHudFrame(HudGrip& grip);

}  // namespace ro::ui
