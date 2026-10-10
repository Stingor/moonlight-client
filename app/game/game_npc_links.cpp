// CE QU'UN LIEN D'UN DIALOGUE DE PNJ OUVRE.
//
// 🔴 À PART DE LA BOUCLE, comme les liens de la conversation : ce sont des
// décisions, pas des étapes. Et c'est ici que se décide ce qu'on N'OUVRE PAS —
// une adresse vient d'une donnée du serveur, et ouvrir un programme du système
// ne se décide pas au fond d'une couche de dessin.
//
// ⚠ LES GESTES SONT CEUX DE LA CONVERSATION, et ils passent par le même
// guichet : un lien d'objet se décrit, se repose dans la saisie et s'interroge
// exactement comme celui qu'un joueur a écrit. Une seconde convention par
// surface est précisément ce que `chat_link.h` raconte avoir coûté.

#include <cstdint>
#include <cstdlib>
#include <string>

#include "ro/vfs/item_link.h"

#include "chat/chat_setting_link.h"
#include "game/game.h"
#include "game/setting_destination_open.h"
#include "options/setting_links.h"

namespace moonclient {

void Game::FollowNpcDialogLink(const ro::ui::NpcDialogAnswer& answer, std::uint32_t now_ms) {
  // UN MONSTRE SURVOLÉ S'APERÇOIT, comme dans la conversation.
  if (answer.hovered.kind == ro::ui::DialogTargetKind::kMonster && monster_sheet_.has_value()) {
    monster_sheet_->NoteHovered(answer.hovered.id, /*by_view=*/false);
  }
  // UNE RECETTE AUSSI, par son skill et ses composants.
  if (answer.hovered.kind == ro::ui::DialogTargetKind::kRecipe && craft_atlas_.has_value()) {
    craft_atlas_->NoteHoveredRecipe(answer.hovered.id);
  }
  // ── LE VOCABULAIRE DU CLIENT ──────────────────────────────────────────────
  switch (answer.followed) {
    case ro::ui::DialogLink::kItem: {
      // La charge est l'identifiant, en chiffres. ⚠ SANS PLACE NI CONTENANT : le
      // script parle d'un objet, pas d'un exemplaire qu'on posséderait.
      const auto item_id = static_cast<std::uint32_t>(std::atoi(answer.followed_payload.c_str()));
      if (item_id != 0) {
        windows_.description.item_id = item_id;
        windows_.description.from_slot = 0;
        windows_.description.from_holder = ro::ui::ItemHolder::kInventory;
        windows_.description.open = true;
        windows_.description.place_at_mouse = true;
        windows_.description.raise = true;
      }
      break;
    }
    case ro::ui::DialogLink::kAddress:
      // ⚠ PAR LE MÊME GUICHET QUE LE LIEN D'UNE DESCRIPTION : lui seul décide
      // qu'une adresse s'ouvre, et il en garde les gardes — « http » ou
      // « https » en toutes lettres, et rien d'autre.
      FollowDescriptionLink(ro::ui::DescriptionWindowAction{.followed = answer.followed_payload},
                            now_ms);
      break;
    case ro::ui::DialogLink::kPlace:
      FollowDescriptionLink(
          ro::ui::DescriptionWindowAction{.followed = answer.followed_payload,
                                          .followed_kind = ro::ui::RichLinkKind::kPlace},
          now_ms);
      break;
    // ⛔ CE QUE LE CLIENT D'ORIGINE OUVRE ET QUE NOUS N'AVONS PAS : sa fenêtre de
    // quêtes, et ses bulles d'astuce. Le balisage est reconnu — il ne s'affiche
    // donc pas en toutes lettres —, et son clic ne promet rien.
    case ro::ui::DialogLink::kQuest:
    case ro::ui::DialogLink::kNotice:
    case ro::ui::DialogLink::kNone:
      break;
  }

  // ── LE VOCABULAIRE DE LA MAISON ───────────────────────────────────────────
  if (answer.gesture == ro::ui::ChatLinkGesture::kNothing || !answer.target.valid()) {
    return;
  }
  switch (answer.target.kind) {
    case ro::ui::DialogTargetKind::kItem:
      // 🔴 REPOSER UN LIEN DE SCRIPT NE PASSE PAS PAR LA TABLE DU FIL. Celle-ci
      // est indexée par un jeton, et le jeton zéro y désigne un AUTRE objet —
      // celui du premier lien reçu. Un lien de script porte l'objet de
      // catalogue, sans raffinement ni cartes : il se recompose ici.
      if (answer.gesture == ro::ui::ChatLinkGesture::kRepost) {
        PostChatLink(ro::vfs::ItemLink{.item_id = answer.target.id});
        return;
      }
      FollowChatLink(ro::ui::ChatLinkAsk{.gesture = answer.gesture,
                                         .item_id = answer.target.id,
                                         .token = answer.target.id,
                                         .from_input = false},
                     now_ms);
      return;
    // L'ATLAS DES RECETTES, ouvert sur le produit — ou le lien reposé tel que
    // le script l'a écrit, nom compris.
    case ro::ui::DialogTargetKind::kRecipe:
      if (answer.gesture == ro::ui::ChatLinkGesture::kDescribe) {
        ro::ui::OpenCraftAtlasOn(windows_.craft_atlas.state, answer.target.id);
      } else if (answer.gesture == ro::ui::ChatLinkGesture::kRepost) {
        PostChatLink(RecipeLink{.item_id = answer.target.id, .name = answer.target.label});
      }
      return;
    case ro::ui::DialogTargetKind::kSetting:
      // CE QUE LA CLÉ DÉSIGNE S'OUVRE — une page du panneau, un onglet d'une
      // fenêtre —, par le même guichet que les liens de la conversation. ⚠ LA
      // CLÉ A DÉJÀ ÉTÉ RECONNUE À L'AFFICHAGE — sans quoi il n'y aurait pas de
      // lien —, et c'est la même table qui répond ici : deux tables diraient un
      // jour deux choses. Le Maj+clic la repose dans la saisie, comme ailleurs.
      if (const SettingDestination* const destination = FindSettingDestination(answer.target.key);
          destination != nullptr) {
        if (answer.gesture == ro::ui::ChatLinkGesture::kRepost) {
          static_cast<void>(PlaceInChatInput(SettingPendingLink(*destination)));
        } else if (answer.gesture == ro::ui::ChatLinkGesture::kDescribe) {
          OpenSettingDestination(*destination, windows_);
        }
      }
      return;
    // LA FICHE DU MONSTRE. ⚠ L'IDENTIFIANT DU SCRIPT EST CELUI DE LA BASE, et
    // non une classe de sprite : c'est ce que la balise transporte.
    case ro::ui::DialogTargetKind::kMonster:
      if (answer.gesture == ro::ui::ChatLinkGesture::kDescribe && monster_sheet_.has_value()) {
        monster_sheet_->Open(windows_.monster_sheet.state, answer.target.id, /*by_view=*/false);
      } else if (answer.gesture == ro::ui::ChatLinkGesture::kRepost) {
        // Le lien REPOSÉ tel que le script l'a écrit : identifiant, rang, nom.
        PostChatLink(MonsterLink{.monster_id = answer.target.id,
                                 .rank = static_cast<std::uint8_t>(answer.target.rank),
                                 .name = answer.target.label});
      }
      return;
    // ⛔ UN GENRE SANS DESTINATION DANS CE CLIENT : l'état, qui n'a pas de
    // fenêtre à lui. Son libellé reste lisible et son clic ne fait rien — ce
    // qui vaut mieux qu'un clic qui ouvre autre chose.
    case ro::ui::DialogTargetKind::kStatus:
    case ro::ui::DialogTargetKind::kNone:
      return;
  }
}

}  // namespace moonclient
