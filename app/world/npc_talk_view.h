#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "ro/render/actor_vignette.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/render/game_images.h"
#include "ro/render/item_icons.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/npc_dialog_window.h"
#include "ro/vfs/status_table.h"

#include "options/setting_links.h"

namespace moonclient {

// LE DIALOGUE D'UN PNJ, VU DE LA PARTIE.
//
// ⚠ ELLE CROISE CE QUE `ui` NE PEUT PAS RAPPROCHER : le dialogue vit dans le
// monde, le NOM du PNJ vit sur son acteur, et les images que son texte appelle
// — icônes d'objets, sprites de monstres, bitmaps de l'installation — vivent
// dans les accessoires. Aucune de ces sources n'est visible depuis une couche de
// dessin.
//
// 🔴 LE NOM N'EST PAS DANS LE DIALOGUE, ET NE PEUT PAS Y ÊTRE. Le serveur
// n'envoie jamais le nom avec la ligne : il l'a dit une fois, à l'apparition du
// PNJ, dans un paquet d'enseigne. Le dialogue ne porte qu'un identifiant, et
// c'est l'acteur qui répond — quand il est encore en vue.
class NpcTalkView {
 public:
  // La table des états sert à NOMMER ce qu'un `<STAL>` désigne : sans elle, le
  // lien ne se forme pas et le libellé du script reste en texte simple.
  NpcTalkView(ro::render::ItemIcons* icons, ro::render::GameImages* images,
              ro::render::ActorWardrobe* wardrobe, const ro::vfs::StatusTable* statuses)
      : icons_(icons), images_(images), wardrobe_(wardrobe), statuses_(statuses) {}

  // Refait la vue. À appeler une fois par image, avant le dessin. Rend nul tant
  // qu'aucune conversation n'est ouverte.
  //
  // `now_ms` fait vivre les sprites : un monstre montré dans une page respire,
  // comme dans le monde.
  //
  // `menu_search` est le réglage du joueur : la couche `ui` ne lit aucun
  // fichier, et ce qu'elle montre lui est donc passé.
  [[nodiscard]] const ro::ui::NpcDialogView* Refresh(const ro::sim::World& world,
                                                     std::uint32_t now_ms, bool menu_search);

  // Le catalogue du vote shop, pour l'entrée du menu d'un item. ⚠ À POSER APRÈS
  // `Refresh`, qui remet la vue à zéro.
  void NoteVoteShop(std::span<const ro::ui::VoteShopOffer> offers) noexcept {
    view_.vote_shop = offers;
  }

  // CE QUE LE LECTEUR A, qui décide qu'un `<SETL>` forme un lien : l'onglet d'un
  // compagnon absent reste en texte simple, comme dans la conversation.
  void SeesSettingReach(SettingReach reach) noexcept { setting_reach_ = reach; }

  // De quoi peindre ce que le texte porte.
  [[nodiscard]] ro::ui::NpcDialogArt art();

  // LA VIGNETTE D'UN MONSTRE, dans la boîte demandée. Rend une texture
  // inutilisable quand l'installation n'a pas son sprite — ce qui arrive, et
  // n'est pas une panne.
  //
  // ⚠ UNE VIGNETTE PAR MONSTRE ET PAR TAILLE DE BOÎTE : chacune tient sa propre
  // texture, et deux tailles demandées au même monstre la feraient se recomposer
  // à chaque image, l'une chassant l'autre.
  [[nodiscard]] const ro::render::IconTexture* MonsterVignette(std::uint32_t monster_id, int width,
                                                               int height);

 private:
  ro::render::ItemIcons* icons_ = nullptr;
  ro::render::GameImages* images_ = nullptr;
  ro::render::ActorWardrobe* wardrobe_ = nullptr;
  const ro::vfs::StatusTable* statuses_ = nullptr;
  SettingReach setting_reach_;

  // ⚠ LES TEXTES SE GARDENT ICI : la vue rendue POINTE dessus, et elle est lue
  // après le retour de cette fonction.
  std::string name_;
  // 🔴 LES VUES SE PRENNENT APRÈS LE REMPLISSAGE. Un `span` pris sur un vecteur
  // qu'on remplit ensuite survit au premier cadrage et meurt au premier menu
  // plus long que le précédent.
  std::vector<std::string_view> options_;
  ro::ui::NpcDialogView view_;

  // La clé d'une vignette : le monstre et la boîte où il tombe.
  struct VignetteKey {
    std::uint32_t monster_id = 0;
    int width = 0;
    int height = 0;

    [[nodiscard]] bool operator==(const VignetteKey& other) const noexcept = default;
  };
  struct VignetteHash {
    [[nodiscard]] std::size_t operator()(const VignetteKey& key) const noexcept;
  };
  std::unordered_map<VignetteKey, ro::render::ActorVignette, VignetteHash> monsters_;
  std::uint32_t now_ms_ = 0;
};

}  // namespace moonclient
