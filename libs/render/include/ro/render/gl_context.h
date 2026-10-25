#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ro/core/error.h"

// Déclarations anticipées : SDL n'apparaît pas dans cette interface. Les
// couches supérieures pilotent un contexte sans jamais inclure SDL, ce qui
// laisse la porte ouverte à un autre back-end sans réécrire leurs en-têtes.
struct SDL_Window;
struct SDL_GLContextState;

namespace ro::render {

// ── CE QUE LA FENÊTRE OFFRE AU JOUEUR ───────────────────────────────────────

// LES TROIS FAÇONS D'OCCUPER L'ÉCRAN.
//
// ⚠ LES DEUX PLEIN-ÉCRAN NE SE VALENT PAS, et la différence se paie à chaque
// bascule vers une autre fenêtre : le plein écran EXCLUSIF prend le mode vidéo
// de l'écran — donc l'écran se resynchronise à chaque Alt+Tab, une seconde de
// noir —, là où le SANS BORDURE n'est qu'une fenêtre de la taille du bureau, qui
// se quitte et se retrouve instantanément.
//
// L'exclusif garde un avantage : il peut imposer une résolution PLUS PETITE que
// le bureau et la laisser étirer par l'écran, ce que le sans bordure ne sait pas
// faire.
enum class DisplayMode : std::uint8_t {
  kWindowed,
  // Plein écran de BUREAU : une fenêtre sans bordure à la taille de l'écran.
  kBorderless,
  // Plein écran EXCLUSIF : le mode vidéo de l'écran change.
  kFullscreen,
};

// LE COIN HAUT-GAUCHE DE LA FENÊTRE, en pixels du bureau.
//
// ⚠ IL PEUT ÊTRE NÉGATIF : un second écran posé à gauche du principal a des
// abscisses négatives, et c'est une position parfaitement valable.
struct WindowPlace {
  int x = 0;
  int y = 0;

  [[nodiscard]] bool operator==(const WindowPlace& other) const noexcept = default;
};

// Une taille d'image, en pixels.
struct Resolution {
  int width = 0;
  int height = 0;

  [[nodiscard]] bool operator==(const Resolution& other) const noexcept = default;
};

// UN ÉCRAN BRANCHÉ, tel que le système le décrit.
//
// ⚠ IL SE DÉSIGNE PAR SON RANG dans la liste que rend
// `GlContext::ConnectedScreens`, et ce rang ne vaut que le temps d'une lecture :
// brancher ou débrancher un moniteur réordonne la liste.
struct ScreenInfo {
  // Le nom que le système donne au moniteur. Deux moniteurs du même modèle
  // portent le même : seul le rang les sépare alors.
  std::string name;
  // La taille du bureau sur cet écran. Nulle si l'écran ne répond pas.
  Resolution desktop;
  bool primary = false;
};

// CHOISIT L'ÉCRAN OÙ LA FENÊTRE NAÎT : reçoit les écrans branchés, rend le rang
// de celui qu'on veut, ou moins un pour laisser le système choisir.
using ScreenPicker = std::function<int(std::span<const ScreenInfo>)>;

// Taille en pixels du tampon de dessin.
//
// Ce n'est PAS forcément la taille demandée à la création : sur un écran à
// facteur d'échelle, le système en donne davantage. Dessiner en se fiant à la
// taille demandée laisse un quart de la fenêtre vide.
struct DrawableSize {
  int width = 0;
  int height = 0;
};

// Contexte OpenGL 3.3 core.
//
// La fenêtre peut rester cachée : c'est ce qui permet à roc-cli de produire une
// image sans rien afficher, donc de comparer des rendus automatiquement.
class GlContext {
 public:
  GlContext(const GlContext&) = delete;
  GlContext& operator=(const GlContext&) = delete;
  GlContext(GlContext&&) = delete;
  GlContext& operator=(GlContext&&) = delete;
  ~GlContext();

  // Fenêtre cachée, dimensionnée pour le rendu hors écran.
  [[nodiscard]] static core::Result<std::unique_ptr<GlContext>> CreateHeadless(int width,
                                                                               int height);

  // Fenêtre visible et redimensionnable, pour l'affichage interactif, CENTRÉE
  // sur l'écran que `pick` désigne — l'écran principal s'il est vide ou s'il
  // rend un rang hors liste.
  [[nodiscard]] static core::Result<std::unique_ptr<GlContext>> CreateWindowed(
      const std::string& title, int width, int height, const ScreenPicker& pick = {});

  // LES ÉCRANS BRANCHÉS, dans l'ordre du système.
  //
  // ⚠ VIDE TANT QU'AUCUNE FENÊTRE N'EXISTE : la lecture ne démarre pas la vidéo
  // d'elle-même, si bien qu'un essai qui compose les réglages sans écran
  // n'ouvre rien.
  [[nodiscard]] static std::vector<ScreenInfo> ConnectedScreens();

  [[nodiscard]] SDL_Window* window() const noexcept { return window_; }

  // Renomme la fenêtre. Le titre porte la carte, et une carte qu'on quitte
  // laisserait sinon son nom au-dessus d'une autre.
  void SetTitle(const std::string& title) const;

  [[nodiscard]] DrawableSize drawable_size() const;

  // Dirige le dessin vers la fenêtre et le cadre sur toute sa surface.
  //
  // L'équivalent de OffscreenTarget::Bind pour l'affichage : sans lui, le
  // dessin partirait dans le dernier tampon hors écran lié, et la fenêtre
  // resterait vide sans la moindre erreur.
  void BindForDisplay() const;

  // EFFACE LA CIBLE LIÉE : la couleur et la profondeur.
  //
  // 🔴 D'ORDINAIRE C'EST LE TERRAIN QUI EFFACE, en tête de son dessin, et cela
  // suffisait tant qu'une carte était toujours peinte. Un écran qui choisit de
  // n'en peindre AUCUNE — l'écran de connexion, quand le joueur décoche sa
  // ville de fond — ne laissait alors pas un fond vide : il laissait la
  // DERNIÈRE image, indéfiniment, et le réglage paraissait sans effet.
  //
  // ⚠ ELLE EFFACE CE QUI EST LIÉ, pas forcément la fenêtre : à l'appelant
  // d'avoir dirigé le dessin là où il veut, comme pour tout le reste.
  void ClearBoundTarget(const std::array<float, 4>& color) const;

  // Présente l'image dessinée.
  void SwapBuffers() const;

  // ── L'AFFICHAGE, TEL QUE LE JOUEUR LE RÈGLE ───────────────────────────────

  // Les tailles que l'écran de cette fenêtre sait prendre, de la plus petite à
  // la plus grande, sans doublon.
  //
  // ⚠ CE SONT LES MODES VIDÉO, et ils ne disent rien du mode FENÊTRÉ : une
  // fenêtre peut faire n'importe quelle taille. Mais offrir une liste libre
  // n'aiderait personne — ces tailles-là sont celles que l'écran rend sans
  // déformer, et ce sont celles qu'un joueur reconnaît.
  //
  // Vide si l'écran ne répond pas : l'appelant garde alors la taille courante.
  [[nodiscard]] std::vector<Resolution> AvailableResolutions() const;

  [[nodiscard]] Resolution size() const;

  // La taille du BUREAU sur l'écran de cette fenêtre. Nulle si l'écran ne
  // répond pas.
  [[nodiscard]] Resolution desktop_size() const;

  [[nodiscard]] WindowPlace position() const;
  void SetPosition(WindowPlace where) const;

  // Le rang, dans `ConnectedScreens`, de l'écran où se tient la fenêtre ; moins
  // un s'il ne répond pas.
  [[nodiscard]] int screen_rank() const;

  // Le rang de l'écran qui contient ce point du bureau ; moins un s'il tombe
  // hors de tout écran — celui qui le portait a été débranché.
  [[nodiscard]] static int ScreenRankAt(WindowPlace where);

  // PORTE LA FENÊTRE SUR UN AUTRE ÉCRAN, centrée. Sans effet sur un rang hors
  // liste.
  //
  // 🔴 ELLE QUITTE D'ABORD LE PLEIN ÉCRAN : une fenêtre en plein écran exclusif
  // est rivée à son écran, et le système ignore qu'on la déplace. À l'appelant
  // de reposer ensuite le mode d'affichage — sur le nouvel écran, puisque
  // `SetDisplayMode` prend toujours celui où se tient la fenêtre.
  void MoveToScreen(int screen_rank) const;

  // Ramène la fenêtre dans la partie UTILISABLE de son écran — celle qui exclut
  // la barre des tâches.
  //
  // 🔴 CE N'EST PAS UN LUXE : `SDL_SetWindowSize` garde le coin HAUT-GAUCHE et
  // fait grandir la fenêtre vers le bas et la droite. Une fenêtre centrée à
  // 640 sur 480 qu'on passe à 1920 sur 1080 sort donc de l'écran par deux côtés,
  // et sa barre de titre avec.
  //
  // ⚠ ET LE COIN HAUT-GAUCHE EST PRIORITAIRE sur le bas-droit : une fenêtre plus
  // grande que l'écran ne peut pas tenir, et c'est sa BARRE DE TITRE qu'il faut
  // sauver — sans elle on ne peut plus ni la déplacer ni la refermer.
  void KeepOnScreen() const;

  // Pose la taille. ⚠ SANS EFFET HORS DU MODE FENÊTRÉ : le sans bordure suit le
  // bureau, et l'exclusif a déjà choisi son mode vidéo.
  void SetSize(Resolution wanted) const;

  // La poignée du système répond-elle encore ?
  //
  // ⚠ CELA NE TOUCHE PAS AUX FENÊTRES DE L'INTERFACE : c'est le bord de la
  // fenêtre du SYSTÈME qu'on fige, pour qui a trouvé sa taille et ne veut plus
  // la perdre d'un glissement.
  void SetResizable(bool resizable) const;

  // 🔴 LE MODE CHANGE LA TAILLE SOUS LES PIEDS DE L'APPELANT. Passer en plein
  // écran redimensionne la fenêtre ; tout ce qui a mémorisé une taille doit la
  // relire après. C'est `drawable_size` qui fait foi, jamais ce qu'on a demandé.
  [[nodiscard]] core::Result<void> SetDisplayMode(DisplayMode mode, Resolution wanted) const;

  // Attendre le balayage de l'écran avant de présenter.
  //
  // ⚠ C'EST LE SEUL FREIN D'IMAGES QUI NE COÛTE RIEN : sans lui la carte dessine
  // aussi vite qu'elle peut, chauffe, et l'écran n'en montre qu'une sur trois.
  // Faux quand le pilote refuse — certains l'imposent ou l'interdisent.
  [[nodiscard]] bool SetVerticalSync(bool on) const;

  // LA MÉMOIRE VIDÉO LIBRE, en kibioctets, quand le pilote la dit (extensions
  // NVIDIA ou AMD) ; rien sinon. Une mesure, pas une décision : deux lectures
  // encadrent un montage pour dire ce qu'il a pris.
  //
  // ⚠ LE CONTEXTE DOIT ÊTRE COURANT, et la lecture attend la fin de ce qui a
  // été demandé à la carte : elle ne se fait pas à chaque image.
  [[nodiscard]] static std::optional<std::int64_t> FreeVideoMemoryKib();

 private:
  GlContext() = default;

  [[nodiscard]] static core::Result<std::unique_ptr<GlContext>> Create(const std::string& title,
                                                                       int width, int height,
                                                                       bool visible,
                                                                       const ScreenPicker& pick);

  SDL_Window* window_ = nullptr;
  SDL_GLContextState* context_ = nullptr;
};

}  // namespace ro::render
