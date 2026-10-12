#pragma once

#include <cstddef>
#include <deque>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/ui/nav_panel.h"
#include "ro/ui/options_window.h"

namespace ro::ui {

// ── LE FILTRE DU PANNEAU DE RÉGLAGES, ET L'ORDRE DE SA NAVIGATION ──────────
//
// Ce que la fenêtre garde d'une section quand le joueur tape un mot, et dans
// quel ordre elle range les sections. ⚠ À PART DE LA FENÊTRE : ce sont des
// règles, et elles s'essaient sans rien dessiner.
//
// 🔴 CE QUI SE CHERCHE : le titre d'une section, ses onglets, ses titres de
// bloc et ses intertitres, le libellé d'une rangée, sa commande et l'objet d'un
// bouton. PAS L'AIDE : une phrase d'aide parle de tout, et un mot court la
// trouverait partout.
//
// 🔴 UN TITRE TROUVÉ GARDE TOUT CE QU'IL COIFFE. « Bulles » montre toute la
// section, « Barre 5 » tout son onglet, « Les effets » tout son bloc : on a tapé
// le nom de ce qu'on cherche, pas celui d'un réglage.
//
// ⚠ LES ONGLETS FERMÉS SE CHERCHENT DANS UNE COPIE : l'appelant ne compose que
// l'onglet ouvert, et recopie les autres dans un `SearchedTabs` le temps d'une
// recherche. Une page dessinée par elle-même se cherche dans les libellés
// qu'elle donne (`OptionSectionView::body_terms`) : trouvée, elle se montre
// entière, la fenêtre ne sachant pas en masquer une partie.

// LE TEXTE TAPÉ, réduit une fois à sa clé de recherche.
class OptionsSearch {
 public:
  explicit OptionsSearch(std::string_view typed);
  // Vrai quand il y a quelque chose à chercher.
  [[nodiscard]] bool active() const noexcept { return !needle_.empty(); }
  // `text` contient-il ce qu'on cherche ? Nul ne contient rien ; ce qui suit
  // « ## » — une identité ImGui — ne compte pas.
  [[nodiscard]] bool Finds(const char* text) const;

 private:
  std::string needle_;
};

// CE QUE LA RECHERCHE GARDE D'UNE SUITE DE RANGÉES : une case par rangée, vraie
// pour celle qui se montre. Tout, quand rien n'est cherché ou que `whole` le
// demande.
//
// ⚠ UNE RANGÉE TROUVÉE EMMÈNE SA MAÎTRESSE, quand elle est décalée sous elle,
// et ses sœurs posées sur la même ligne : seule, elle ne se lirait pas.
[[nodiscard]] std::vector<bool> KeptRows(std::span<const OptionRowView> rows,
                                         const OptionsSearch& search, bool whole);

// Combien de RÉGLAGES se montrent parmi les rangées gardées : ni une note ni un
// cadre, qui ne règlent rien.
[[nodiscard]] int CountKeptSettings(std::span<const OptionRowView> rows,
                                    const std::vector<bool>& kept);

// UNE SECTION SOUS LA RECHERCHE.
struct SectionFinding {
  // Elle se montre dans la navigation.
  bool shown = true;
  // Son titre est trouvé : tout se montre.
  bool whole = true;
  // Les réglages trouvés, page dessinée comprise : un libellé trouvé compte
  // pour un.
  int settings = 0;
  // PAR ONGLET de la section : son nom est trouvé — tout l'onglet se montre —,
  // et les réglages qu'il porte. Vides pour une section sans onglets ; le
  // compte reste à zéro sans `searched_tabs`.
  std::vector<bool> tab_whole;
  std::vector<int> tab_settings;
  // Le premier onglet qui répond, ou moins un.
  int first_tab = -1;
  // La page dessinée par elle-même a un libellé trouvé, et l'onglet du premier
  // — moins un s'il vaut sous tous.
  bool body_found = false;
  int body_tab = -1;

  // L'onglet `tab` se montre-t-il entier ?
  [[nodiscard]] bool TabWhole(int tab) const;
};

[[nodiscard]] SectionFinding FindInSection(const OptionSectionView& section,
                                           const OptionsSearch& search);

// LES RANGÉES DES ONGLETS FERMÉS, RECOPIÉES pour la recherche : la section ne
// garde de rangées que pour l'onglet ouvert.
//
// 🔴 UNE COPIE QUI POSSÈDE SES TEXTES. Un libellé composé pointe souvent dans
// un tampon que l'onglet suivant réécrit — « Barre 3 » devient « Barre 4 » — :
// garder son adresse ferait chercher dans le dernier onglet composé. Seul ce
// que la recherche lit survit à la copie : identité, libellés, titres, forme et
// mise en page ; ni valeur, ni aperçu, ni panneau.
class SearchedTabs {
 public:
  void Clear();
  // Les rangées d'un onglet de plus, au rang qui suit les précédents.
  void Add(std::span<const OptionRowView> rows);
  [[nodiscard]] std::span<const std::span<const OptionRowView>> tabs() const noexcept {
    return views_;
  }

 private:
  [[nodiscard]] const char* Keep(const char* text);

  // ⚠ UNE DEQUE : ses chaînes ne bougent pas quand elle grandit, et les rangées
  // pointent dedans.
  std::deque<std::string> texts_;
  std::vector<std::vector<OptionRowView>> rows_;
  std::vector<std::span<const OptionRowView>> views_;
};

// LES ENTRÉES DE LA NAVIGATION, dans l'ordre où elles se lisent : chaque
// famille garde sa place, et ses sections s'y rangent par ordre alphabétique
// de leur titre, accents et casse ignorés. Une section qu'on ne montre pas
// — `shown` faux — n'a pas d'entrée ; une famille sans entrée perd son titre.
//
// ⚠ LA FAMILLE D'UNE SECTION EST CELLE DU DERNIER TITRE DE FAMILLE QUI LA
// PRÉCÈDE dans `sections` : c'est l'ordre reçu qui la dit, le tri ne la change
// pas.
[[nodiscard]] std::vector<NavEntry> SortedNavEntries(std::span<const OptionSectionView> sections,
                                                     const std::vector<bool>& shown);

}  // namespace ro::ui
