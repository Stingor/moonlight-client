#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>

#include "ro/render/actor_vignette.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/render/item_icons.h"
#include "ro/sim/viewed_equipment.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/view_equip_window.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/item_words.h"

#include "items/creator_names.h"
#include "world/actor_styles.h"

namespace moonclient {

// « VOIR L'ÉQUIPEMENT », VU DE LA PARTIE : la fiche qu'un autre joueur a
// montrée, croisée avec l'installation — noms composés, icônes, pantin — et avec
// ce que JE porte, pour la comparaison.
//
// 🔴 LA FENÊTRE S'OUVRE À LA RÉPONSE, jamais à la demande, et c'est ce que fait
// le client d'origine. La plupart des refus du serveur sont silencieux — cible
// partie, autre carte — : ouverte « en attente », elle resterait vide sans que
// rien ne vienne jamais la remplir.
class ViewedEquipmentView {
 public:
  // LES NOMS DES FORGERONS, confiés une fois la description ouverte.
  void SeeCreators(CreatorNames creators) noexcept { creators_ = creators; }

  // Tout peut manquer : une installation sans table d'objets rend des noms nus,
  // sans garde-robe le pantin se tait.
  ViewedEquipmentView(const ro::vfs::ItemTable* items, const ro::vfs::ItemWords* words,
                      ro::render::ItemIcons* icons, ro::render::ActorWardrobe* wardrobe);

  // L'AID du joueur qu'on vient de demander.
  //
  // 🔴 IL N'EST PAS DANS LA RÉPONSE, qui ne porte que le NOM. Sans lui, « Actualiser »
  // n'aurait personne à qui redemander, et deux homonymes seraient
  // indiscernables. ⚠ UNE DEMANDE PEUT RESTER SANS RÉPONSE : l'AID n'est promu
  // « joueur affiché » qu'à la réception.
  void NotePending(std::uint32_t account_id) noexcept { pending_id_ = account_id; }

  // Suit le livre, ouvre la fenêtre à une réponse neuve, et compose la vue.
  // `staff` : le compte est du staff, et ses trois outils se montrent.
  // ⚠ UN REFUS ne la concerne pas : le décodeur des messages du serveur l'écrit
  // dans la conversation.
  void Refresh(const ro::sim::ViewedEquipmentBook& book, const ro::sim::World& world,
               ActorStyles& styles, bool staff, ro::ui::ViewEquipWindowState& state,
               std::uint32_t now_ms);

  [[nodiscard]] const ro::ui::ViewEquipView& view() const noexcept { return view_; }

  // Ce que le menu d'un item sait d'avance : vote shop, album. ⚠ À POSER APRÈS
  // `Refresh`, qui remet la vue à zéro.
  void NoteLinkFacts(ro::ui::ItemLinkFacts facts) noexcept { view_.link_facts = facts; }

  // Le joueur affiché : zéro quand la fiche n'a pas été demandée par nous.
  [[nodiscard]] std::uint32_t shown_id() const noexcept { return shown_id_; }

  // La pièce qu'un jeton désigne — la sienne, ou la mienne —, et d'où elle vient.
  struct Designated {
    const ro::sim::InventoryItem* item = nullptr;
    // Vrai pour une pièce du joueur affiché, faux pour la mienne.
    bool theirs = false;
  };
  // ⚠ RELUE À LA DEMANDE, pas gardée : entre le dessin et le geste, la fiche a
  // pu être remplacée et ma pièce ôtée.
  [[nodiscard]] static Designated PieceOf(const ro::sim::ViewedEquipmentBook& book,
                                          const ro::sim::World& world, std::uint32_t token);

  // Au changement de carte : la cible n'y est plus forcément, et le serveur
  // exige la même carte. Garder l'affichage donnerait une fiche périmée sans
  // rien qui le dise.
  void Forget(ro::sim::ViewedEquipmentBook& book, ro::ui::ViewEquipWindowState& state) noexcept;

 private:
  void ComposeRows(const ro::sim::ViewedEquipment& sheet, const ro::sim::Inventory& mine);
  void ComposeDoll(const ro::sim::ViewedEquipment& sheet, const ro::sim::World& world,
                   ActorStyles& styles, const ro::ui::ViewEquipWindowState& state,
                   std::uint32_t now_ms);
  [[nodiscard]] ro::ui::ViewEquipPieceView PieceView(const ro::sim::InventoryItem& piece,
                                                     std::uint32_t token);

  const ro::vfs::ItemTable* items_;
  const ro::vfs::ItemWords* words_;
  CreatorNames creators_;
  ro::render::ItemIcons* icons_;
  ro::render::ActorWardrobe* wardrobe_;
  std::optional<ro::render::ActorVignette> doll_;

  std::uint32_t pending_id_ = 0;
  std::uint32_t shown_id_ = 0;
  std::uint32_t received_seen_ = 0;
  std::uint32_t received_ms_ = 0;

  std::string title_;
  ro::ui::ViewEquipView view_;
};

}  // namespace moonclient
