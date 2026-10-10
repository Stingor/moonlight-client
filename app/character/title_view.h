#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "ro/net/connection.h"
#include "ro/render/title_table.h"
#include "ro/sim/title_book.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/character_window.h"
#include "ro/vfs/achievement_titles.h"

namespace moonclient {

// L'ONGLET DES TITRES, VU DE LA PARTIE.
//
// ⚠ IL CROISE TROIS SOURCES QUE RIEN NE RELIE : les succès réclamés, que le
// serveur dit ; le titre que chacun récompense, que dit une table ; et le
// libellé de ce titre, que dit une autre. `ui` ne voit que le résultat.
//
// ⚠ LES DEUX TABLES PEUVENT MANQUER : sans la première, aucun titre ne se
// possède ; sans la seconde, chacun s'écrit par son numéro.
class TitleView {
 public:
  TitleView(const ro::vfs::AchievementTitles* rewards, const ro::render::TitleTable* labels);

  // Refait la liste. ⚠ LES LIBELLÉS SONT GARDÉS D'UNE IMAGE À L'AUTRE : la vue
  // rendue POINTE dessus.
  void Refresh(const ro::sim::TitleBook& book);

  [[nodiscard]] const ro::ui::CharacterTitlesView& view() const noexcept { return view_; }

  // Demande le titre choisi.
  //
  // 🔴 ET DEMANDE NOTRE PROPRE CARTE DE NOM, une fois, quand l'onglet est
  // montré sans elle. Le serveur ne dit le titre porté que dans cette carte —
  // qu'il ne pousse pas à l'entrée en jeu — ou dans l'accusé d'un changement.
  // Sans elle, l'onglet dirait « aucun » à qui porte un titre depuis la veille.
  void Apply(const ro::ui::CharacterWindowAction& asked, bool tab_shown, ro::net::Connection& link,
             const ro::sim::World& world);

 private:
  const ro::vfs::AchievementTitles* rewards_ = nullptr;
  const ro::render::TitleTable* labels_ = nullptr;

  std::vector<std::string> owned_labels_;
  std::vector<ro::ui::CharacterTitleView> owned_;
  std::string equipped_label_;
  ro::ui::CharacterTitlesView view_;
  bool own_card_asked_ = false;
};

// UN TITRE, NOMMÉ ET RANGÉ.
struct NamedTitle {
  std::int32_t title_id = 0;
  std::string label;
};

// LES TITRES QU'ON POSSÈDE, dans l'ordre où la liste les montre.
//
// ⚠ SANS DOUBLON : deux succès peuvent récompenser le même titre.
//
// ⚠ RANGÉS PAR LIBELLÉ, OCTET PAR OCTET, et c'est le libellé de la TABLE qui
// range — vide pour un titre qu'elle ne nomme pas, donc en tête —, pas le repli
// numéroté qu'on affiche à sa place.
[[nodiscard]] std::vector<NamedTitle> OwnedTitles(std::span<const std::uint32_t> rewarded,
                                                  const ro::vfs::AchievementTitles* rewards,
                                                  const ro::render::TitleTable* labels);

// Le libellé à montrer : celui de la table, ou le numéro.
[[nodiscard]] std::string ShownTitle(std::int32_t title_id, const ro::render::TitleTable* labels);

}  // namespace moonclient
