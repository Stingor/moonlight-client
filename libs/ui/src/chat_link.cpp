#include "ro/ui/chat_link.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>

#include "ro/core/i18n.h"
#include "ro/core/text.h"
#include "ro/ui/chat_rich_art.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_tooltip.h"

namespace ro::ui {

ChatLinkGesture HitChatLink(bool hovered) {
  if (!hovered) {
    return ChatLinkGesture::kNothing;
  }
  // ⚠ LA MAIN DIT QUE C'EST CLIQUABLE, et c'est tout ce qui le dit.
  ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  if (ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
    return ChatLinkGesture::kMenu;
  }
  if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
    // MAJ : on le repose au lieu de l'ouvrir. C'est le geste du client, et il ne
    // se distingue du premier que par cette touche.
    return ImGui::GetIO().KeyShift ? ChatLinkGesture::kRepost : ChatLinkGesture::kDescribe;
  }
  return ChatLinkGesture::kNothing;
}

namespace {

// Le libellé de l'entrée du vote shop : sa phrase et un prix.
constexpr std::size_t kVoteShopLabelBytes = 96;

// Les entrées d'un lien d'OBJET.
void ItemEntries(ChatLinkAsk& asked, const ItemLinkFacts& link_facts) {
  if (ImGui::MenuItem(core::Tr("Description"))) {
    asked.gesture = ChatLinkGesture::kDescribe;
  }
  if (ImGui::MenuItem(core::Tr("Base de données du site"))) {
    asked.gesture = ChatLinkGesture::kOpenDatabase;
  }
  if (ImGui::MenuItem(core::Tr("Lien dans le chat"))) {
    asked.gesture = ChatLinkGesture::kRepost;
  }
  if (ImGui::MenuItem(core::Tr("Rechercher sur le Marché"))) {
    asked.gesture = ChatLinkGesture::kMarketSearch;
  }
  if (ImGui::MenuItem(core::Tr("Ouvrir dans la base de données"))) {
    asked.gesture = ChatLinkGesture::kGameDatabase;
  }
  // UNE CARTE : sa pochette dans l'album.
  if (SeeInAlbumMenuItem(CardAlbumEntryOf(link_facts.album, asked.item_id))) {
    asked.gesture = ChatLinkGesture::kSeeInAlbum;
  }
  // LE VOTE SHOP : l'entrée n'existe QUE si l'item y est vendu — sa présence
  // EST l'information. Le prix y figure parce que la question d'après est
  // toujours « combien ? ».
  if (const std::optional<std::int32_t> price =
          VoteShopPriceOf(link_facts.vote_shop, asked.item_id);
      price.has_value()) {
    ImGui::Separator();
    std::array<char, kVoteShopLabelBytes> text{};
    std::snprintf(text.data(), text.size(), core::Tr("Vote Shop : ajouter au panier (%d pts)"),
                  *price);
    if (ImGui::MenuItem(text.data())) {
      asked.gesture = ChatLinkGesture::kVoteShop;
    }
  }
  ImGui::Separator();
  // ⚠ DEUX COMMANDES DU SERVEUR, envoyées comme une ligne tapée : leur réponse
  // revient dans le fil. Elles ne sont pas traduites — ce sont des commandes, pas
  // des libellés, et le joueur les retape parfois à la main.
  if (ImGui::MenuItem("@iteminfo")) {
    asked.gesture = ChatLinkGesture::kItemInfo;
  }
  if (ImGui::MenuItem("@whodrops")) {
    asked.gesture = ChatLinkGesture::kWhoDrops;
  }
}

// Les entrées d'un lien de STYLE.
//
// ⛔ NI DESCRIPTION NI BASE DE DONNÉES : un style n'existe dans aucune table,
// et il n'y a rien à en dire que le pantin ne montre mieux. « Voir ce style »
// est donc le premier geste, et c'est aussi celui du clic gauche.
void StyleEntries(ChatLinkAsk& asked) {
  if (ImGui::MenuItem(core::Tr("Voir ce style"))) {
    asked.gesture = ChatLinkGesture::kDescribe;
  }
  if (ImGui::MenuItem(core::Tr("Essayer dans l'éditeur"))) {
    asked.gesture = ChatLinkGesture::kTryStyle;
  }
  if (ImGui::MenuItem(core::Tr("Copier le code"))) {
    asked.gesture = ChatLinkGesture::kCopyCode;
  }
  if (ImGui::MenuItem(core::Tr("Lien dans le chat"))) {
    asked.gesture = ChatLinkGesture::kRepost;
  }
}

// LE GUIDAGE D'UN MVP : vers sa tombe quand elle est connue, sinon vers sa
// carte quand elle est nommée, sinon rien. Les deux posent le même geste :
// `app` guide vers la carte entière quand la cellule manque.
void GuideEntry(ChatLinkAsk& asked, const MvpLinkMenuFacts& facts) {
  const char* label = nullptr;
  if (facts.tomb_known) {
    label = core::Tr("Guider vers la tombe");
  } else if (facts.map_known) {
    label = core::Tr("Y aller");
  }
  if (label != nullptr && ImGui::MenuItem(label)) {
    asked.gesture = ChatLinkGesture::kGuideToTomb;
  }
}

// Les entrées d'un lien de MONSTRE.
//
// ⚠ LE NOM D'UN MVP DU CARNET EN PROPOSE DAVANTAGE, et lui seul : ses faits
// disent quand il revient et où est sa tombe. Un lien de monstre du chat n'en
// sait rien, et proposer de partager son respawn serait proposer du vide.
//
// La partie basse, elle, vaut pour TOUT nom de monstre : la recherche part sur
// le NOM, et un nom vide n'a rien à chercher.
void MonsterEntries(ChatLinkAsk& asked, const MvpLinkMenuFacts* facts, std::string_view label) {
  const bool database_id = facts == nullptr || !facts->sprite_class_only;
  if (ImGui::MenuItem(core::Tr("Fiche du monstre"))) {
    asked.gesture = ChatLinkGesture::kDescribe;
  }
  if (database_id && ImGui::MenuItem(core::Tr("Bestiaire du site"))) {
    asked.gesture = ChatLinkGesture::kOpenBestiary;
  }
  // La fiche du monstre dans la base de données du jeu (ADR 0116).
  if (database_id && ImGui::MenuItem(core::Tr("Ouvrir dans la base de données"))) {
    asked.gesture = ChatLinkGesture::kGameDatabase;
  }
  // Un lien de monstre porte un identifiant de la BASE : une classe de sprite
  // relayée ouvrirait, chez le lecteur, la fiche d'un autre monstre.
  if (database_id && ImGui::MenuItem(core::Tr("Lien dans le chat"))) {
    asked.gesture = ChatLinkGesture::kRepost;
  }
  if (facts != nullptr) {
    // LE RESPAWN SE PARTAGE EN `<MVPL>` : ce qu'on partage n'est pas le
    // monstre, que tout le monde connaît, c'est QUAND il revient.
    if (facts->carries_respawn && ImGui::MenuItem(core::Tr("Partager le respawn dans le chat"))) {
      asked.gesture = ChatLinkGesture::kShareRespawn;
    }
    GuideEntry(asked, *facts);
  }
  // OÙ LE TROUVER : la fiche dit ce que c'est, la navigation dit où. Deux
  // entrées et pas une : ouvrir chez soi n'est pas poser la question à
  // quelqu'un d'autre.
  const bool named = facts != nullptr ? facts->named : !label.empty();
  if (named || database_id) {
    ImGui::Separator();
  }
  if (named) {
    if (ImGui::MenuItem(core::Tr("Où le trouver"))) {
      asked.gesture = ChatLinkGesture::kWhereToFind;
    }
    if (ImGui::MenuItem(core::Tr("Lien « où le trouver » dans le chat"))) {
      asked.gesture = ChatLinkGesture::kPostWhereToFind;
    }
  }
  if (!database_id) {
    return;
  }
  if (named) {
    ImGui::Separator();
  }
  // ⚠ DEUX COMMANDES DU SERVEUR, non traduites, comme celles d'un objet.
  if (ImGui::MenuItem("@mobinfo")) {
    asked.gesture = ChatLinkGesture::kMobInfo;
  }
  if (ImGui::MenuItem("@whereis")) {
    asked.gesture = ChatLinkGesture::kWhereIs;
  }
}

// Les entrées d'un lien de RECETTE.
//
// ⚠ TOUT LIEN SE RELAIE, et tout menu mène là où va le clic — l'Atlas en
// tête, puisque c'est le geste du clic gauche.
void RecipeEntries(ChatLinkAsk& asked) {
  if (ImGui::MenuItem(core::Tr("Ouvrir dans l'Atlas"))) {
    asked.gesture = ChatLinkGesture::kDescribe;
  }
  if (ImGui::MenuItem(core::Tr("Lien dans le chat"))) {
    asked.gesture = ChatLinkGesture::kRepost;
  }
}

// Les entrées d'un lien de LIEU : le guidage, que le clic gauche lance aussi, la
// fenêtre de navigation, ce qu'il y a là-bas, le nom interne, et le relais.
void PlaceEntries(ChatLinkAsk& asked, bool staff_warp) {
  if (ImGui::MenuItem(core::Tr("Y aller"))) {
    asked.gesture = ChatLinkGesture::kDescribe;
  }
  if (ImGui::MenuItem(core::Tr("Ouvrir la navigation"))) {
    asked.gesture = ChatLinkGesture::kOpenInNavigation;
  }
  if (ImGui::MenuItem(core::Tr("Voir ce qu'il y a là-bas"))) {
    asked.gesture = ChatLinkGesture::kPlaceContents;
  }
  if (ImGui::MenuItem(core::Tr("Copier le nom de la carte"))) {
    asked.gesture = ChatLinkGesture::kCopyCode;
  }
  if (staff_warp && ImGui::MenuItem(core::Tr("Téléporter à cet endroit"))) {
    asked.gesture = ChatLinkGesture::kStaffWarp;
  }
  ImGui::Separator();
  if (ImGui::MenuItem(core::Tr("Lien dans le chat"))) {
    asked.gesture = ChatLinkGesture::kRepost;
  }
}

// Les entrées d'une RECHERCHE DE NAVIGATION.
//
// ⚠ « Y ALLER » DEMANDE UN LIEU : une carte est le sien, un PNJ ou un monstre
// n'en a qu'avec un contexte. Sans lui, c'est toute la raison d'être de ce lien
// — une question, pas un point.
void NaviSearchEntries(ChatLinkAsk& asked, NaviSearchMenuFacts navi) {
  constexpr std::uint8_t kMapFamily = 0;
  if (ImGui::MenuItem(core::Tr("Chercher dans la navigation"))) {
    asked.gesture = ChatLinkGesture::kDescribe;
  }
  if (navi.family == kMapFamily) {
    if (ImGui::MenuItem(core::Tr("Y aller"))) {
      asked.gesture = ChatLinkGesture::kGoThere;
    }
  } else if (navi.bounded) {
    if (ImGui::MenuItem(core::Tr("Y aller"))) {
      asked.gesture = ChatLinkGesture::kGoThere;
    }
    if (ImGui::MenuItem(core::Tr("Voir ce qu'il y a là-bas"))) {
      asked.gesture = ChatLinkGesture::kPlaceContents;
    }
  }
  if (ImGui::MenuItem(core::Tr("Copier le nom"))) {
    asked.gesture = ChatLinkGesture::kCopyCode;
  }
  if (ImGui::MenuItem(core::Tr("Lien dans le chat"))) {
    asked.gesture = ChatLinkGesture::kRepost;
  }
}

// UNE ENTRÉE QUI PEUT ÊTRE REFUSÉE D'AVANCE : grisée, avec sa raison au survol.
void OfferedEntry(ChatLinkAsk& asked, const char* label, const char* refusal,
                  ChatLinkGesture gesture) {
  if (ImGui::MenuItem(label, nullptr, false, refusal == nullptr)) {
    asked.gesture = gesture;
  }
  if (refusal != nullptr && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    SkinTooltipFormat("%s", refusal);
  }
}

// Les entrées d'un RESPAWN DE MVP, dans cet ordre.
//
// 🔴 LE CLIC GAUCHE N'IMPORTE RIEN : il ouvre le carnet. Écrire dans le carnet
// touche celui de tout le groupe, et cela ne se fait que par cette entrée —
// qui, sans faits, reste GRISÉE : jamais offerte à l'aveugle.
void MvpEntries(ChatLinkAsk& asked, const MvpLinkMenuFacts* facts) {
  OfferedEntry(asked, core::Tr("Ajouter à mon carnet"),
               facts != nullptr ? facts->import_refusal
                                : core::Tr("Créez un groupe de chasse pour tenir un carnet."),
               ChatLinkGesture::kAddToMvpLog);
  OfferedEntry(asked, core::Tr("Ouvrir le carnet"),
               facts != nullptr ? facts->open_refusal : nullptr, ChatLinkGesture::kDescribe);
  // LA TOMBE REDEVIENT UN LIEU, ou à défaut sa carte.
  if (facts != nullptr) {
    GuideEntry(asked, *facts);
  }
  // ⚠ UN LIEN SANS MONSTRE n'a pas de fiche à promettre.
  if (asked.item_id != 0 && ImGui::MenuItem(core::Tr("Fiche du monstre"))) {
    asked.gesture = ChatLinkGesture::kMonsterSheet;
  }
  if (ImGui::MenuItem(core::Tr("Lien dans le chat"))) {
    asked.gesture = ChatLinkGesture::kRepost;
  }
}

// Les entrées d'un PSEUDO, copie du nom comprise : aucun autre geste ne la
// permet sur une ligne du fil.
void PlayerEntries(ChatLinkAsk& asked, const PlayerLinkRefusals* refusals) {
  const PlayerLinkRefusals none;
  const PlayerLinkRefusals& refused = refusals != nullptr ? *refusals : none;
  if (ImGui::MenuItem(core::Tr("Chuchoter…"))) {
    asked.gesture = ChatLinkGesture::kDescribe;
  }
  if (ImGui::MenuItem(core::Tr("Chuchoter dans une fenêtre…"))) {
    asked.gesture = ChatLinkGesture::kWhisperWindow;
  }
  ImGui::Separator();
  OfferedEntry(asked, core::Tr("Inviter dans le groupe"), refused.party,
               ChatLinkGesture::kInviteParty);
  // LE CARNET DE CHASSE, AVEC LES AUTRES INVITATIONS : c'en est une.
  if (!refused.mvp_withdrawn && MvpInviteMenuItem(refused.mvp)) {
    asked.gesture = ChatLinkGesture::kInviteMvp;
  }
  OfferedEntry(asked, core::Tr("Inviter dans la guilde"), refused.guild,
               ChatLinkGesture::kInviteGuild);
  OfferedEntry(asked, core::Tr("Ajouter en ami"), refused.befriend, ChatLinkGesture::kAddFriend);
  ImGui::Separator();
  if (ImGui::MenuItem(core::Tr("Copier le nom"))) {
    asked.gesture = ChatLinkGesture::kCopyCode;
  }
}

// Le libellé de « Toujours afficher <hôte>… » : sa phrase et un nom d'hôte,
// borné à la longueur d'un nom de domaine pour que la phrase reste entière.
constexpr std::size_t kAllowHostLabelBytes = 320;
constexpr std::size_t kMostHostBytes = 253;

// Les entrées d'une ADRESSE WEB.
//
// ⚠ LES DEUX AUTORISATIONS D'IMAGE NE SE VALENT PAS. « Cette image » ne
// retient rien : le risque est celui d'un clic sur le lien, que le joueur peut
// déjà faire. « Toujours » accorde un accès AUTOMATIQUE et PERMANENT au survol
// — d'où les points de suspension, et la confirmation qui suit.
//
// ⚠ ELLES NE DÉPENDENT PAS DU RÉGLAGE « aperçu au survol » : ce sont des gestes
// explicites, et les cacher tant que le réglage est éteint rendrait l'aperçu
// introuvable.
//
// 🔴 « COPIER L'ADRESSE » EST TOUJOURS LÀ : l'adresse est écrite par un tiers,
// et la copier plutôt que l'ouvrir est le geste prudent — personne ne juge un
// lien sur ce qui tient dans une ligne de chat.
void UrlEntries(ChatLinkAsk& asked, const ChatUrlFacts* url) {
  const bool allowed = url != nullptr && url->allowed;
  if (!allowed) {
    if (ImGui::MenuItem(core::Tr("Afficher cette image"))) {
      asked.gesture = ChatLinkGesture::kShowImageOnce;
    }
    // Sans hôte, il n'y a rien à admettre durablement.
    if (url != nullptr && !url->host.empty()) {
      std::array<char, kAllowHostLabelBytes> text{};
      // ⚠ COUPÉ ENTRE DEUX CARACTÈRES, avant `snprintf` qui couperait n'importe
      // où : un hôte accentué finirait sinon en losange.
      const std::string host(core::WithoutDanglingUtf8(url->host.substr(0, kMostHostBytes)));
      std::snprintf(text.data(), text.size(), core::Tr("Toujours afficher %s…"), host.c_str());
      if (ImGui::MenuItem(text.data())) {
        asked.gesture = ChatLinkGesture::kAllowImageHost;
      }
    }
    ImGui::Separator();
  }
  if (url != nullptr && url->preview == ChatPictureState::kReady &&
      ImGui::MenuItem(core::Tr("Agrandir l'image"))) {
    asked.gesture = ChatLinkGesture::kViewImage;
  }
  if (ImGui::MenuItem(core::Tr("Ouvrir dans le navigateur"))) {
    asked.gesture = ChatLinkGesture::kDescribe;
  }
  if (ImGui::MenuItem(core::Tr("Copier l'adresse"))) {
    asked.gesture = ChatLinkGesture::kCopyCode;
  }
}

// Les entrées d'un PSEUDO DISCORD : il n'existe pas dans le jeu, on ne peut ni
// lui chuchoter ni l'inviter. Le mentionner pose « @nom » dans la saisie.
void DiscordNameEntries(ChatLinkAsk& asked) {
  if (ImGui::MenuItem(core::Tr("Mentionner"))) {
    asked.gesture = ChatLinkGesture::kDescribe;
  }
  if (ImGui::MenuItem(core::Tr("Copier le nom"))) {
    asked.gesture = ChatLinkGesture::kCopyCode;
  }
}

// Les entrées d'un lien de RÉGLAGE : l'ouvrir, ou le reposer.
void SettingEntries(ChatLinkAsk& asked) {
  if (ImGui::MenuItem(core::Tr("Ouvrir"))) {
    asked.gesture = ChatLinkGesture::kDescribe;
  }
  if (ImGui::MenuItem(core::Tr("Lien dans le chat"))) {
    asked.gesture = ChatLinkGesture::kRepost;
  }
}

// LA TÊTE D'UN MENU D'ADRESSE, coupée à ce nombre de caractères : une adresse
// d'image de Discord en compte deux cents, et le menu prendrait la largeur de
// l'écran pour la montrer.
constexpr std::size_t kUrlMenuHeadChars = 60;
constexpr std::string_view kCutMark = "…";

// Un octet de suite d'UTF-8, qui ne commence pas de caractère.
[[nodiscard]] bool ContinuesUtf8(char byte) {
  constexpr unsigned kContinuationMask = 0xC0U;
  constexpr unsigned kContinuationBits = 0x80U;
  return (static_cast<unsigned char>(byte) & kContinuationMask) == kContinuationBits;
}

// Le libellé tel que la tête le montre : entier, ou coupé ENTRE deux
// caractères avec sa marque.
[[nodiscard]] std::string MenuHead(ChatLinkKind kind, std::string_view label) {
  if (kind != ChatLinkKind::kUrl) {
    return std::string(label);
  }
  std::size_t chars = 0;
  for (std::size_t at = 0; at < label.size(); ++at) {
    if (ContinuesUtf8(label[at])) {
      continue;
    }
    if (chars == kUrlMenuHeadChars) {
      return std::string(label.substr(0, at)) + std::string(kCutMark);
    }
    ++chars;
  }
  return std::string(label);
}

}  // namespace

std::optional<std::int32_t> VoteShopPriceOf(std::span<const VoteShopOffer> offers,
                                            std::uint32_t item_id) {
  if (item_id == 0) {
    return std::nullopt;
  }
  const auto found = std::ranges::lower_bound(offers, item_id, {}, &VoteShopOffer::item_id);
  if (found == offers.end() || found->item_id != item_id) {
    return std::nullopt;
  }
  return found->price;
}

ChatLinkAsk DrawChatLinkMenu(const char* popup_id, ChatLinkKind kind, std::uint32_t item_id,
                             std::uint32_t token, std::string_view label, ItemLinkFacts link_facts,
                             const PlayerLinkRefusals* refusals, const MvpLinkMenuFacts* mvp,
                             NaviSearchMenuFacts navi, const ChatUrlFacts* url, bool staff_warp) {
  ChatLinkAsk asked;
  if (!BeginSkinPopup(popup_id)) {
    return asked;
  }
  asked.kind = kind;
  asked.item_id = item_id;
  asked.token = token;
  // ⚠ LE NOM EN TÊTE, GRISÉ : un menu ouvert sur un mot d'une phrase dense doit
  // dire de QUEL mot il parle.
  if (!label.empty()) {
    ImGui::TextDisabled(" %s ", MenuHead(kind, label).c_str());
    ImGui::Separator();
  }
  switch (kind) {
    case ChatLinkKind::kStyle:
      StyleEntries(asked);
      break;
    case ChatLinkKind::kMonster:
      MonsterEntries(asked, mvp, label);
      break;
    case ChatLinkKind::kRecipe:
      RecipeEntries(asked);
      break;
    case ChatLinkKind::kPlace:
      PlaceEntries(asked, staff_warp);
      break;
    case ChatLinkKind::kPlayer:
      PlayerEntries(asked, refusals);
      break;
    case ChatLinkKind::kItem:
      ItemEntries(asked, link_facts);
      break;
    case ChatLinkKind::kMvp:
      MvpEntries(asked, mvp);
      break;
    case ChatLinkKind::kNaviSearch:
      NaviSearchEntries(asked, navi);
      break;
    case ChatLinkKind::kUrl:
      UrlEntries(asked, url);
      break;
    case ChatLinkKind::kDiscordName:
      DiscordNameEntries(asked);
      break;
    case ChatLinkKind::kSetting:
      SettingEntries(asked);
      break;
  }
  EndSkinPopup();
  return asked;
}

bool MvpInviteMenuItem(const char* refusal) {
  const bool chosen =
      ImGui::MenuItem(core::Tr("Inviter dans mon carnet MVP"), nullptr, false, refusal == nullptr);
  if (refusal != nullptr) {
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      SkinTooltipFormat("%s", refusal);
    }
  } else if (ImGui::IsItemHovered()) {
    SkinTooltipFormat("%s", core::Tr("Guilde et liste d'amis uniquement.\nInviter quelqu'un, "
                                     "c'est inviter tous ses personnages et tous ses comptes de "
                                     "jeu."));
  }
  return chosen;
}

void DrawSettingLinkTip(std::string_view path) {
  if (!BeginSkinTooltip()) {
    return;
  }
  ImGui::TextUnformatted(path.data(), path.data() + path.size());
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
  ImGui::TextUnformatted(core::Tr("Clic : ouvrir · Maj+clic : reposer"));
  ImGui::PopStyleColor();
  EndSkinTooltip();
}

}  // namespace ro::ui
