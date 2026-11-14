#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "ro/sim/character_ledger.h"
#include "ro/ui/escape_stack.h"
#include "ro/ui/reputation_window.h"
#include "ro/ui/skin_art.h"

#include "world/reputation_book.h"

namespace moonclient {

// ── LA FENÊTRE DE RÉPUTATION ────────────────────────────────────────────────
//
// Elle montre ce que `sim::CharacterLedger` garde des points, sous les noms et
// les bornes du livre. Rien ne se demande au serveur : il envoie la liste
// entière à chaque entrée sur une carte, puis la faction qui change.
//
// Elle s'ouvre par son icône, ou par un script (`reputationui`) : celui-ci
// choisit un groupe et peut désigner une faction, dont le fond se colore
// jusqu'à ce que la fenêtre se ferme ou change de groupe.
//
// « Toutes les factions » montre chaque faction du livre, même hors de tout
// groupe ; une faction qui ne se montre qu'avec des points attend d'en avoir.

// LES PIÈCES DE LA FENÊTRE dans les fenêtres de l'interface.
struct ReputationWindows {
  ro::ui::ReputationWindowState window;
  // La vue de l'image ; nulle avant la première avancée.
  const ro::ui::ReputationView* view = nullptr;
  // UNE OUVERTURE DEMANDÉE par l'icône, lue à l'avancée suivante.
  bool open_asked = false;
};

// L'ICÔNE DU MENU : ferme la fenêtre ouverte, en demande l'ouverture sinon.
void ToggleReputationWindow(ReputationWindows& windows);

// Dessine la fenêtre quand sa vue est composée, et la déclare à Échap.
void DrawReputationWindows(ReputationWindows& windows, const ro::ui::SkinArt& art,
                           ro::ui::EscapeStack& escape);

class ReputationBoard {
 public:
  // À CHAQUE IMAGE : l'ouverture demandée par un script ou par l'icône, puis la
  // vue, composée tant que la fenêtre est ouverte.
  void Advance(ReputationWindows& windows, ro::sim::CharacterLedger& ledger,
               const ReputationBook& book);

 private:
  void Compose(const ReputationWindows& windows, const ro::sim::CharacterLedger& ledger,
               const ReputationBook& book);

  // La faction qu'un script a désignée, et le groupe sous lequel il l'a fait.
  std::uint64_t highlighted_faction_id_ = 0;
  int highlighted_group_ = ro::ui::kReputationAllGroups;
  // LA COMPOSITION : ce que la vue pointe.
  std::vector<std::string_view> group_names_;
  std::vector<ro::ui::ReputationRowView> rows_;
  ro::ui::ReputationView view_;
};

}  // namespace moonclient
