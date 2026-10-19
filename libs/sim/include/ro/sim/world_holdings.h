#pragma once

#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include "ro/sim/bank.h"
#include "ro/sim/card_composition.h"
#include "ro/sim/companion.h"
#include "ro/sim/craft_session.h"
#include "ro/sim/inventory.h"
#include "ro/sim/skill_book.h"
#include "ro/sim/skill_facts.h"
#include "ro/sim/storage_facts.h"
#include "ro/sim/storage_shelf.h"
#include "ro/sim/warp_menu.h"

namespace ro::sim {

// CE QUE LE PERSONNAGE TIENT, ET QUE LA CARTE NE TOUCHE PAS : son grimoire, ses
// contenants, ses coffres, sa banque, ce qu'il peut appeler, et ce que le
// serveur seul sait de ses objets et de ses compétences.
//
// 🔴 C'EST UNE BASE DE `World`, ET CE QUI LES TIENT ENSEMBLE, C'EST QU'ILS
// TRAVERSENT LA CARTE : `World::Clear` n'y touche pas, et `World::EnterMap` n'y
// fait qu'une chose — refermer le coffre (`CloseStorage`), la session et non les
// contenants. Un état qui suit la carte n'a rien à y faire.
//
// ⚠ ELLE NE VOIT AUCUN ACTEUR : ses méthodes ne touchent que ses propres champs.
// C'est ro_net qui remplit ces contenants, à partir de paquets dont le monde ne
// sait rien — d'où les accès en écriture.
class WorldHoldings {
 public:
  // Le GRIMOIRE du joueur : ce qu'il sait lancer. Il ne suit pas la carte, et
  // Clear ne l'efface pas : ce que l'on sait ne s'oublie pas en changeant de
  // carte. Le serveur le redit à chaque entrée, mais plus tard que la carte
  // elle-même, et le vider laisserait un grimoire blanc entre les deux annonces.
  //
  // Ouvert en écriture, comme la conversation ne l'est pas : c'est ro_net qui
  // le remplit, à partir de paquets dont le monde ne sait rien.
  [[nodiscard]] const SkillBook& skill_book() const noexcept { return skill_book_; }
  [[nodiscard]] SkillBook& skill_book() noexcept { return skill_book_; }

  // LE SAC, LA CHARRETTE ET LE COFFRE, pour les mêmes raisons que le grimoire :
  // ouverts en écriture parce que c'est ro_net qui les remplit, et non effacés
  // par Clear parce qu'ils ne suivent pas la carte.
  //
  // ⚠ TROIS CONTENANTS SÉPARÉS, et pas un seul indexé : le serveur les envoie
  // par les mêmes paquets, et les mêmes numéros de case s'y répètent. Les mêler
  // ferait qu'une potion du coffre écrase celle du sac.
  //
  // Le coffre est une SESSION : il se vide quand le serveur le ferme, alors que
  // les deux autres durent autant que le personnage.
  [[nodiscard]] const Inventory& inventory() const noexcept { return inventory_; }
  [[nodiscard]] Inventory& inventory() noexcept { return inventory_; }
  [[nodiscard]] const Inventory& cart() const noexcept { return cart_; }
  [[nodiscard]] Inventory& cart() noexcept { return cart_; }
  [[nodiscard]] const Inventory& storage() const noexcept { return storage_; }
  [[nodiscard]] Inventory& storage() noexcept { return storage_; }
  [[nodiscard]] const CardComposition& composition() const noexcept { return composition_; }
  [[nodiscard]] CardComposition& composition() noexcept { return composition_; }
  [[nodiscard]] WarpMenu& warp_menu() noexcept { return warp_menu_; }
  // L'ATELIER : listes de fabrication et d'affinage — voir `CraftSession`.
  [[nodiscard]] const CraftSession& craft() const noexcept { return craft_; }
  [[nodiscard]] CraftSession& craft() noexcept { return craft_; }

  // ── LE COFFRE EST UNE SESSION, PAS UN CONTENANT QUI DORT ──────────────────
  //
  // 🔴 ET CET ÉTAT DÉBORDE DE SA PROPRE FENÊTRE : tant qu'un coffre est ouvert,
  // le serveur REFUSE les échanges entre le sac et la charrette
  // (`sd->state.storage_flag`), et il les refuse SANS RIEN DIRE. Qui ne suit pas
  // l'ouverture propose des gestes qui échouent en silence.
  //
  // Le contenu ne suffit pas à la deviner : un coffre ouvert et vide et un
  // coffre fermé ont tous deux zéro objet.
  [[nodiscard]] bool storage_open() const noexcept { return storage_open_; }

  // Le serveur vient d'ouvrir un coffre. Le nom qu'il en donne est déjà dans le
  // contenant : c'est la seule chose qui sépare le stockage personnel de celui
  // de la guilde.
  void OpenStorage() noexcept { storage_open_ = true; }

  // La session est finie : le contenu s'en va avec elle.
  //
  // ⚠ ON VIDE, et ce n'est pas ce qu'on fait d'une charrette : le coffre existe
  // toujours, mais son contenu ne nous est plus annoncé. Le garder afficherait
  // une liste qui ne bouge plus et que rien ne corrige — pire qu'un coffre vide.
  void CloseStorage();

  // ── LA BANQUE : un coffre à zeny, au compte — voir `BankSession` ──────────
  [[nodiscard]] BankSession& bank() noexcept { return bank_; }
  [[nodiscard]] const BankSession& bank() const noexcept { return bank_; }

  // CE QUE LE PERSONNAGE PEUT APPELER : chariot, monture, faucon.
  //
  // ⚠ IL NE SUIT PAS LA CARTE. Le serveur le pousse au login vérifié, puis à
  // chaque changement — c'est-à-dire aussi quand une Kafra pose le chariot, les
  // scripts passant par les mêmes poseurs. Le vider en changeant de carte
  // laisserait la fiche sans ses cases jusqu'au prochain geste.
  void TellCompanions(CompanionState told) { companions_ = told; }
  [[nodiscard]] const CompanionState& companions() const noexcept { return companions_; }

  // CE QUE LE SERVEUR SEUL SAIT D'UNE COMPÉTENCE : son cast, ses dégâts.
  //
  // ⚠ ILS SE DEMANDENT, ILS N'ARRIVENT PAS SEULS — voir `ro/net/skill_facts.h`.
  // Une fenêtre qui les montre doit savoir se dessiner sans eux.
  void TellSkillCastFacts(std::uint32_t skill_id, std::vector<SkillCastFacts> levels) {
    skill_facts_.TellCast(skill_id, std::move(levels));
  }
  void TellSkillDamageFacts(SkillDamageFacts told) { skill_facts_.TellDamage(std::move(told)); }
  void NoteSkillDamageTooSoon(SkillDamageQuestion refused) {
    skill_facts_.NoteDamageTooSoon(refused);
  }
  [[nodiscard]] const SkillFacts& skill_facts() const noexcept { return skill_facts_; }

  // CE QUE LE SERVEUR DIT DES OBJETS DU COFFRE — prix de revente, sous-type,
  // emplacement. ⚠ IL EST SEUL À LES CONNAÎTRE : ils vivent dans sa base, et la
  // table de l'installation n'en porte aucun. Voir `ro/sim/storage_facts.h`.
  void TellStorageFacts(std::vector<StorageItemFacts> told) {
    storage_facts_.Tell(std::move(told));
  }
  [[nodiscard]] const StorageFacts& storage_facts() const noexcept { return storage_facts_; }

  // ── LES COFFRES QUE CE PERSONNAGE PEUT OUVRIR ──────────────────────────────
  //
  // 🔴 LA LISTE ET L'OUVERT ARRIVENT ENSEMBLE, dans la même trame, et c'est ce qui les garde
  // d'accord. Le serveur la renvoie À CHAQUE OUVERTURE, avec le numéro du coffre qu'il vient
  // d'ouvrir : les poser séparément laisserait un onglet marqué sur un autre coffre.
  //
  // ⚠ `open_storage_id()` NE REMPLACE PAS `storage_open()` : le premier dit LEQUEL, le
  // second QU'IL Y EN A UN, et c'est lui qui décide de ce que le serveur refuse en silence.
  // Sans cette liste, le client sait donc quand même refermer.
  void TellStorages(std::vector<StorageShelf> shelves, std::uint8_t open_id);
  [[nodiscard]] std::span<const StorageShelf> storage_shelves() const noexcept {
    return storage_shelves_;
  }
  [[nodiscard]] std::uint8_t open_storage_id() const noexcept { return open_storage_id_; }

  // Le contenant que désigne un tenant, pour du code qui n'en connaît pas le nom
  // — typiquement le décodeur, qui lit lequel dans le paquet.
  [[nodiscard]] Inventory& HolderFor(ItemHolder holder) noexcept;

 private:
  SkillBook skill_book_;
  Inventory inventory_;
  Inventory cart_;
  Inventory storage_;
  CardComposition composition_;
  WarpMenu warp_menu_;
  CraftSession craft_;
  CompanionState companions_;
  // ⚠ ELLE SURVIT À LA FERMETURE D'UN COFFRE ET AU CHANGEMENT DE CARTE : le serveur ne la
  // repousse qu'au login et à chaque ouverture, et la vider priverait la fenêtre de ses onglets.
  std::vector<StorageShelf> storage_shelves_;
  std::uint8_t open_storage_id_ = kNoStorageOpen;
  bool storage_open_ = false;
  StorageFacts storage_facts_;
  SkillFacts skill_facts_;
  // ⚠ LE SOLDE SURVIT À LA FERMETURE DE LA FENÊTRE : d'autres écrans le lisent, et il n'est
  // redit qu'à la réouverture.
  BankSession bank_;
};

}  // namespace ro::sim
