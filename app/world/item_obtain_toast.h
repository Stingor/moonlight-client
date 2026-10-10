#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "ro/render/item_icons.h"
#include "ro/sim/inventory.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/item_obtain_toast.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/item_words.h"

#include "items/creator_names.h"
#include "world/item_obtain_rules.h"

namespace moonclient {

// UNE ANNONCE QUI N'EST PAS UN RAMASSAGE — un compagnon qui a faim : même
// bandeau, même place, mêmes réglages, mais rien à cumuler. L'icône est celle
// d'un item, zéro pour aucune.
struct ToastNotice {
  std::uint32_t icon_item_id = 0;
  std::string name;
  std::string suffix;
};

// LE BANDEAU « <objet> - N obtained. », CÔTÉ ÉTAT VIVANT.
//
// C'est ici que le fait du monde devient ce qui se voit : le sac tient un
// journal des ramassages, ce module en relève les neufs par leur RANG, compose
// le nom et l'icône, et tient la pile jusqu'à son extinction.
//
// 🔴 TROIS SOURCES SE CROISENT ICI, ET NULLE PART AILLEURS. `sim` dit ce qui est
// entré et quand, `vfs` sait le nommer, `render` sait l'illustrer — et `ui` ne
// reçoit que du texte déjà composé et une image. C'est ce qui permet au dessin
// de vivre dans une bibliothèque qui ne connaît ni sac ni table d'objets.
//
// Les règles de la pile — fusionner, expirer, écrêter — vivent dans
// `item_obtain_rules.h`, et ce module ne fait que les appliquer.
class ItemObtainToast {
 public:
  // ⚠ PAR RÉFÉRENCE AU CONSTRUCTEUR : les sources vivent ailleurs et lui
  // survivent. `ItemIcons` n'est pas constant — il mémorise ce qu'il charge —
  // et il est FACULTATIF : une installation sans catalogue d'icônes garde son
  // bandeau, qui ne montre alors que le texte.
  ItemObtainToast(const ro::vfs::ItemTable& items, const ro::vfs::ItemWords& words,
                  ro::render::ItemIcons* icons)
      : items_(&items), words_(&words), icons_(icons) {}

  // Relève les ramassages que le sac porte depuis le dernier passage, et ouvre
  // leur ligne.
  //
  // ⚠ À APPELER DANS LE POMPAGE RÉSEAU, pas dans le dessin : c'est là que les
  // faits du monde se relèvent, et une image sautée ne doit pas perdre un
  // ramassage.
  //
  // L'instant d'une ligne est celui de la RÉCEPTION, que le journal porte : un
  // ramassage relevé en retard n'en devient pas plus jeune.
  //
  // 🔴 UN ITEM FORGÉ ATTEND SON FORGERON : tant que le serveur ne l'a pas nommé,
  // et trois secondes au plus, rien ne s'ouvre — sinon le bandeau dirait
  // « Sans nom » pour de bon, alors que la réponse arrive d'ordinaire dans la
  // milliseconde. Tout ce qui suit attend avec lui, pour garder l'ordre.
  void Notice(const ro::sim::World& world, const ro::ui::ItemObtainToastLook& look,
              const CreatorNames& creators, std::uint32_t now_ms);

  // Ouvre une ligne qui n'est pas un ramassage. ⚠ ELLE NE SE FUSIONNE AVEC
  // RIEN : deux annonces sont deux nouvelles.
  void Announce(ToastNotice notice, std::uint32_t now_ms);

  // Purge, écrête, et rend ce qu'il y a à peindre, dans l'ORDRE d'affichage.
  //
  // ⚠ LES VUES SONT PRISES APRÈS LA COMPOSITION, sur une pile qui ne bouge
  // plus : une vue désignée pendant le remplissage meurt à la première
  // réallocation du vecteur.
  [[nodiscard]] std::span<const ro::ui::ItemObtainToastEntry> Shown(
      std::uint32_t now_ms, const ro::ui::ItemObtainToastLook& look);

  // Oublie ce qui est à l'écran : un changement de carte ne doit rien laisser
  // traîner, les lignes d'avant ne veulent plus rien dire.
  //
  // 🔴 LE TÉMOIN DE RANG, LUI, NE RECULE PAS. Le journal du sac SURVIT au
  // changement de carte : un témoin remis à zéro ferait reparaître d'un coup,
  // au premier pas sur la carte suivante, les derniers ramassages de la
  // précédente. Ce qu'on oublie, c'est l'affichage ; ce qu'on a déjà vu reste
  // vu.
  //
  // Une VRAIE nouvelle session se reconnaît autrement : le rang du sac y repart
  // de zéro, donc en dessous du témoin, et c'est `Notice` qui le remet alors.
  void Forget();

 private:
  // Compose le nom et le suffixe d'une ligne à partir de ce que le sac porte.
  [[nodiscard]] ObtainedLine LineOf(const ro::sim::InventoryItem& picked, std::uint32_t obtained_ms,
                                    const CreatorNames& creators) const;
  // Refait le suffixe d'une ligne dont la quantité vient de changer.
  void RefreshSuffix(ObtainedLine& line) const;
  // La ligne d'exemple, montrée quand on place l'ancre sans rien avoir ramassé.
  void ComposePreview();

  const ro::vfs::ItemTable* items_;
  const ro::vfs::ItemWords* words_;
  ro::render::ItemIcons* icons_;

  std::vector<ObtainedLine> lines_;
  std::vector<ro::ui::ItemObtainToastEntry> shown_;
  ObtainedLine preview_;

  // Le rang du dernier ramassage relevé.
  //
  // ⚠ IL AVANCE MÊME QUAND LE RÉGLAGE TAIT LA LIGNE : un ramassage qu'on a
  // choisi de ne pas montrer n'est pas un ramassage à rejouer quand on rallume.
  std::uint64_t seen_seq_ = 0;
};

}  // namespace moonclient
