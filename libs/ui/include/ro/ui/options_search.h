#pragma once

#include <cstddef>
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
// section, « Les effets » tout son bloc : on a tapé le nom de ce qu'on cherche,
// pas celui d'un réglage.
//
// ⚠ CE QUI NE SE CHERCHE PAS ENCORE : les rangées des onglets fermés — seul
// l'onglet ouvert est composé — et ce que dessine une page elle-même (voir
// `OptionSectionView::body`), dont seul le titre se trouve.

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
  // Son titre ou un de ses onglets est trouvé : tout se montre.
  bool whole = true;
  // Les réglages trouvés.
  int settings = 0;
};

[[nodiscard]] SectionFinding FindInSection(const OptionSectionView& section,
                                           const OptionsSearch& search);

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
