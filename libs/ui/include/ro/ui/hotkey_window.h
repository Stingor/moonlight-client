#pragma once

#include <optional>
#include <span>
#include <string_view>

#include "ro/core/i18n.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// LA CONFIGURATION DES RACCOURCIS.
//
// ⚠ ELLE NE CONNAÎT AUCUNE TOUCHE. Une ligne lui arrive avec son nom d'action et
// le LIBELLÉ de sa touche, déjà composé ; elle rend le rang de la ligne que le
// joueur veut changer. C'est la boucle qui écoute le clavier et qui écrit la
// table — `ui` ne sait pas ce qu'est un code de touche, et n'a pas à le savoir.
//
// ⚠ ET LA CAPTURE EST UN ÉTAT, PAS UN APPEL. Le joueur clique, puis appuie : les
// deux gestes tombent dans deux images différentes, et ce qui les relie doit
// survivre entre elles.

// Une ligne : une action, et la touche qui la déclenche.
struct HotkeyRowView {
  std::string_view action;
  // Vide quand aucune touche n'est posée — un raccourci peut se retirer.
  std::string_view key;
  // LA FAMILLE OÙ LA LIGNE SE RANGE — « Barre 3 » —, vide pour les actions qui
  // n'en ont pas.
  //
  // ⚠ LA LISTE COMPTE PLUS DE CENT LIGNES, dont quatre-vingt-seize cases qui se
  // ressemblent toutes. Sans un intertitre là où la famille change, le joueur
  // qui descend ne sait plus de quelle barre il lit les cases.
  std::string_view group;
  // Le rang de son onglet dans `HotkeyWindowView::tabs`.
  int tab = 0;
  // Vrai pour une action absente de cette construction : la ligne ne se
  // montre pas, et son rang reste celui de la table.
  bool hidden = false;
  // LA CONSIGNE pendant qu'elle attend son geste, déjà traduite ; nulle pour
  // la consigne ordinaire.
  const char* capture_hint = nullptr;
};

struct HotkeyWindowView {
  std::span<const HotkeyRowView> rows;
  // Les libellés des onglets, dans leur ordre ; « Tout » les précède.
  std::span<const std::string_view> tabs;
};

// L'onglet « Tout », qui montre toutes les lignes.
inline constexpr int kAllHotkeyTabs = -1;

struct HotkeyWindowState {
  bool open = false;
  char filter[64] = {};
  // Le rang de l'onglet montré dans `HotkeyWindowView::tabs`, ou `kAllHotkeyTabs`.
  int tab = kAllHotkeyTabs;
  // L'onglet à sélectionner à la prochaine image.
  std::optional<int> tab_request;
  // La ligne dont on attend une touche, ou moins de zéro. ⚠ LUE PAR LA BOUCLE :
  // c'est elle qui, la voyant posée, détourne la prochaine touche vers la table
  // au lieu de la laisser déclencher une action.
  int capturing = -1;
  // 🔴 À QUI LA DERNIÈRE TOUCHE POSÉE A ÉTÉ PRISE, déjà en phrase. Vide quand
  // rien n'a été repris.
  //
  // Poser une touche déjà prise la VOLE à son détenteur — c'est ce que fait le
  // jeu, et refuser laisserait le joueur devant un travail que l'écran sait
  // faire. Mais la liste compte plus de cent lignes : la victime est presque
  // toujours hors de l'écran, et sans cette phrase le joueur ne saura jamais ce
  // qu'il vient de perdre.
  //
  // ⚠ ÉCRITE PAR LA BOUCLE, qui seule sait nommer une action.
  char robbed[128] = {};
  // Posé par la boucle quand Échap annule une capture : la même frappe ne ferme
  // pas la fenêtre.
  bool escape_spent = false;
};

// Ce que le joueur a demandé.
struct HotkeyWindowAction {
  // La ligne dont il veut retirer la touche, ou moins de zéro.
  int cleared = -1;
  // Il demande à retrouver les raccourcis d'origine.
  bool restore_defaults = false;
  // L'ONGLET dont il veut poser le lien dans le chat — son rang, ou
  // `kAllHotkeyTabs` pour « Tout ».
  std::optional<int> link_tab;
};

inline constexpr const char* kHotkeyWindowTitle = core::TrText("Raccourcis clavier");
inline constexpr const char* kHotkeyWindowId = "hotkeys";

[[nodiscard]] HotkeyWindowAction DrawHotkeyWindow(HotkeyWindowState& state,
                                                  const HotkeyWindowView& view, const SkinArt& art);

}  // namespace ro::ui
