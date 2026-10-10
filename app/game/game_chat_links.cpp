// LES LIENS DE LA CONVERSATION, vus de la boucle : ce qu'un geste sur un lien
// demande, et comment on en pose un dans la saisie.
//
// ⚠ À CÔTÉ DE LA BOUCLE, et pas dedans : `game.cpp` tenait déjà tout le reste
// d'une image, et ceci forme un sujet entier — la seule partie du client où un
// texte reçu d'un autre joueur devient un geste.
//
// 🔴 DEUX ESPÈCES, ET ELLES NE PARLENT PAS DE LA MÊME CHOSE. Un lien d'OBJET
// DÉSIGNE : l'objet existe chez le lecteur, sa table le nomme, et son menu
// mène à des fenêtres et à des commandes du serveur. Un lien de STYLE
// TRANSPORTE : la recette entière voyage, parce qu'elle n'existe nulle part
// hors de son porteur — et rien de ce qu'il propose ne touche au personnage.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <imgui.h>

#include "ro/sim/navi_search.h"
#include "ro/sim/style_code.h"
#include "ro/ui/chat_input.h"
#include "ro/vfs/item_link.h"
#include "ro/vfs/item_name.h"
#include "ro/vfs/item_table.h"

#include "chat/chat_item_link.h"
#include "chat/chat_mention_piece.h"
#include "chat/chat_setting_link.h"
#include "game/game.h"
#include "game/setting_destination_open.h"
#include "items/inventory_view.h"

namespace moonclient {
namespace {

constexpr std::string_view kMapPlanFolder = "map\\";
constexpr std::string_view kMapPlanExtension = ".bmp";
constexpr std::string_view kMapGridExtension = ".gat";

// LE PLAN D'UNE CARTE, celui que le radar montre ; vide quand il manque.
[[nodiscard]] ro::ui::SkinImage MapPlanOf(std::string_view map, GamePieces& pieces) {
  if (!pieces.game_images.has_value() || map.empty()) {
    return {};
  }
  std::string path(ro::render::kSkinRoot);
  path += kMapPlanFolder;
  path += map;
  path += kMapPlanExtension;
  const ro::render::IconTexture& plan = pieces.game_images->GetOurs(path);
  if (!plan.usable()) {
    return {};
  }
  return ro::ui::SkinImage{.id = static_cast<ImTextureID>(plan.texture.name()),
                           .width = static_cast<float>(plan.width_px),
                           .height = static_cast<float>(plan.height_px)};
}

// L'INFOBULLE D'UN LIEU : son nom lisible, et le plan que le radar montre.
[[nodiscard]] ro::ui::NavigationPlaceTip PlaceTipOf(const PlaceLink& place, GamePieces& pieces) {
  ro::ui::NavigationPlaceTip tip{.map_name = place.map, .x = place.x, .y = place.y};
  if (pieces.map_display_names.has_value()) {
    tip.label = std::string(
        pieces.map_display_names->DisplayNameOf(place.map + std::string(kMapGridExtension))
            .value_or(std::string_view()));
  }
  tip.map_image = MapPlanOf(place.map, pieces);
  return tip;
}

// LA CLASSE DE SPRITE D'UN PNJ que les données de navigation placent sur cette
// carte sous ce nom ; zéro quand elles ne le connaissent pas. ⚠ LA PARTIE
// CACHÉE DU NOM NE COMPTE PAS : les tables ne la portent pas, un lien parfois.
[[nodiscard]] std::uint32_t NpcSpriteOf(const ro::formats::NaviData& data, std::string_view map,
                                        std::string_view name) {
  const ro::formats::NaviMapIndex at = data.FindMap(map);
  if (at == ro::formats::kNoNaviMap) {
    return 0;
  }
  const std::string_view visible = ro::sim::VisibleNpcName(name);
  for (const std::uint32_t rank : data.NpcsOn(at)) {
    if (rank < data.npcs.size() && data.npcs[rank].name == visible &&
        data.npcs[rank].sprite_class > 0)
      return static_cast<std::uint32_t>(data.npcs[rank].sprite_class);
  }
  return 0;
}

// L'INFOBULLE D'UNE RECHERCHE : le terme lisible, la carte de contexte, et ce
// qui répond à « lequel ? ». Une carte cherchée montre son plan ; un PNJ ou un
// monstre borné, le plan de sa carte et, pour un PNJ connu, sa silhouette. Sans
// contexte, aucun plan : c'est faute de lieu unique qu'on partage une recherche.
[[nodiscard]] ro::ui::NavigationSearchTip SearchTipOf(const NaviSearchLink& link,
                                                      GamePieces& pieces, NaviSession& navi,
                                                      std::uint32_t map_entries,
                                                      std::uint32_t now_ms) {
  const ro::vfs::MapDisplayNames* names =
      pieces.map_display_names.has_value() ? &*pieces.map_display_names : nullptr;
  ro::ui::NavigationSearchTip tip;
  const bool is_map = link.family == NaviSearchFamily::kMap;
  tip.term = is_map ? PlaceShownName(names, link.term) : link.term;
  // Un PNJ se montre sans la partie cachée de son nom, comme sur sa plaque.
  if (link.family == NaviSearchFamily::kNpc) {
    tip.term = std::string(ro::sim::VisibleNpcName(link.term));
  }
  if (tip.term.empty()) {
    tip.term = link.term;
  }
  if (!link.map.empty()) {
    tip.context_label = PlaceShownName(names, link.map);
    if (tip.context_label.empty()) {
      tip.context_label = link.map;
    }
  }
  tip.plan_map_name = is_map ? link.term : link.map;
  tip.map_image = MapPlanOf(tip.plan_map_name, pieces);
  const ro::formats::NaviData* data = navi.data();
  if (link.family != NaviSearchFamily::kNpc || link.map.empty() || data == nullptr ||
      !pieces.wardrobe.has_value()) {
    return tip;
  }
  const std::uint32_t sprite_class = NpcSpriteOf(*data, link.map, link.term);
  if (sprite_class == 0) {
    return tip;
  }
  if (!navi.tip_sprite.has_value()) {
    navi.tip_sprite.emplace(*pieces.wardrobe);
  }
  // La garde-robe se vide à chaque entrée de carte : la silhouette oublie tout.
  if (navi.tip_map_entries != map_entries) {
    navi.tip_map_entries = map_entries;
    navi.tip_sprite->Forget();
  }
  const MonsterSprite::Shown shown = navi.tip_sprite->Compose(
      MonsterSprite::Framing{.sprite_class = sprite_class,
                             .direction = 0,
                             .animate = true,
                             .box_width = ro::ui::kNavigationSearchTipNpcSidePx,
                             .box_height = ro::ui::kNavigationSearchTipNpcSidePx},
      now_ms);
  // ⚠ MUETTE : une infobulle ne crie pas.
  static_cast<void>(navi.tip_sprite->TakeSounds());
  if (!shown.missing) {
    tip.npc_image = shown.image;
  }
  return tip;
}

// CE QUE LE JETON DÉSIGNE, quelle que soit l'espèce du lien. Nul quand il ne
// désigne plus rien, ou que la balise posée ne se relit pas.
//
// ⚠ UNE SEULE TABLE PAR SURFACE : le fil refait la sienne à chaque image en
// triant ses onglets, chaque saisie a la liste de ses liens posés — nulle pour
// une saisie qui n'existe plus. Toutes se numérotent à partir de zéro, et les
// confondre relaierait autre chose.
[[nodiscard]] const ChatLinkTarget* AimChatLink(const std::vector<PendingChatLink>* pending,
                                                const std::optional<ChatTabsView>& tabs,
                                                std::uint32_t token, bool from_input,
                                                ChatLinkTarget& scratch) {
  if (!from_input) {
    return tabs.has_value() ? tabs->LinkOf(token) : nullptr;
  }
  if (pending == nullptr || token >= pending->size()) {
    return nullptr;
  }
  const PendingChatLink& posed = (*pending)[token];
  scratch = ChatLinkTarget{};
  scratch.kind = posed.kind;
  scratch.code = posed.code;
  scratch.owner = posed.owner;
  // 🔴 LA BALISE EST RELUE plutôt que recopiée depuis l'objet : c'est bien le
  // MÊME objet qu'on relaie — raffinement, cartes, grade, forgeron.
  if (posed.kind == ro::ui::ChatLinkKind::kItem &&
      !ro::vfs::ParseItemLink(posed.wire, &scratch.item, nullptr)) {
    return nullptr;
  }
  if (posed.kind == ro::ui::ChatLinkKind::kMonster &&
      !ParseMonsterLink(posed.wire, &scratch.monster, nullptr)) {
    return nullptr;
  }
  if (posed.kind == ro::ui::ChatLinkKind::kRecipe &&
      !ParseRecipeLink(posed.wire, &scratch.recipe, nullptr)) {
    return nullptr;
  }
  if (posed.kind == ro::ui::ChatLinkKind::kPlace &&
      !ParsePlaceLink(posed.wire, &scratch.place, nullptr)) {
    return nullptr;
  }
  // ⚠ UN RESPAWN POSÉ N'A PAS D'EXPÉDITEUR : c'est le nôtre, pas encore parti.
  if (posed.kind == ro::ui::ChatLinkKind::kMvp &&
      ParseMvpLink(posed.wire, &scratch.mvp, nullptr) != MvpLinkRead::kRead) {
    return nullptr;
  }
  if (posed.kind == ro::ui::ChatLinkKind::kNaviSearch &&
      ParseNaviSearchLink(posed.wire, &scratch.navi_search, nullptr) != NaviSearchLinkRead::kRead) {
    return nullptr;
  }
  if (posed.kind == ro::ui::ChatLinkKind::kSetting) {
    SettingLink setting;
    if (ParseSettingLink(posed.wire, &setting, nullptr) != SettingLinkRead::kRead) {
      return nullptr;
    }
    scratch.setting_key = std::move(setting.key);
  }
  return &scratch;
}

// LA MENTION DANS LA SAISIE : voir `MentionPiece`, et pourquoi elle ne commence
// jamais une ligne par « @ ».
void MentionInInput(ro::ui::ChatInputState& input, std::string_view name) {
  const std::string_view typed(input.text.data());
  const std::string_view before = input.pending_insert.empty() ? typed : input.pending_insert;
  if (!ro::ui::AppendPendingInsert(input, MentionPiece(before, name))) {
    return;
  }
  input.unfolded = true;
  input.focus_on_whisper = false;
  input.wants_focus = true;
}

}  // namespace

// CE QU'UN GESTE SUR UN LIEN DEMANDE, joué ici et nulle part ailleurs.
//
// ⚠ LE JETON DIT DE QUELLE TABLE IL SORT : celle du fil, refaite à chaque image
// par le tri des onglets, ou celle de la saisie, qui est la liste des liens
// posés. Les deux se numérotent à partir de zéro, et les confondre ferait
// relayer un autre objet.
void Game::FollowChatLink(const ro::ui::ChatLinkAsk& asked, std::uint32_t now_ms) {
  // LA SAISIE D'OÙ VIENT UNE PASTILLE : la barre de la conversation, ou celle
  // d'une conversation 1:1, qui a ses propres liens.
  const std::vector<PendingChatLink>* posed =
      asked.input_peer.empty() ? &pending_links_ : pieces_.whispers.PendingOf(asked.input_peer);
  // UNE ADRESSE S'APERÇOIT APRÈS UN SURVOL CONTINU : voir `ChatWebStage`.
  if (asked.hovering && asked.hovered_kind == ro::ui::ChatLinkKind::kUrl) {
    ChatLinkTarget scratch;
    const ChatLinkTarget* hovered =
        AimChatLink(posed, chat_tabs_, asked.hovered_token, asked.hovered_from_input, scratch);
    if (hovered != nullptr && hovered->kind == ro::ui::ChatLinkKind::kUrl) {
      pieces_.chat_web.NoteHovered(hovered->url, now_ms);
    }
  }
  // L'APERÇU AU SURVOL, comme sur une case. ⚠ IL NE SE FILTRE PAS PAR FENÊTRE :
  // un lien est déjà un geste volontaire — on l'a écrit ou on le lit —, et le
  // réglage qui vaut pour une grille de cent cases n'a pas de sens ici.
  if (asked.hovering && asked.hovered_kind == ro::ui::ChatLinkKind::kMvp) {
    // UN RESPAWN S'APERÇOIT PAR CE QU'IL DIT : la carte, le retour, la mort et
    // la tombe, à l'image suivante. ⚠ JAMAIS COMME UN OBJET : son identifiant
    // est celui d'un monstre.
    ChatLinkTarget scratch;
    const ChatLinkTarget* hovered =
        AimChatLink(posed, chat_tabs_, asked.hovered_token, asked.hovered_from_input, scratch);
    if (hovered != nullptr && hovered->kind == ro::ui::ChatLinkKind::kMvp) {
      windows_.mvp_tracker.link_tip = pieces_.mvp_tracker.TipOf(hovered->mvp, LocalUnixSeconds());
    }
  } else if (asked.hovered_id != 0 && asked.hovered_kind == ro::ui::ChatLinkKind::kMonster) {
    // UN MONSTRE S'APERÇOIT PAR SA FICHE : sprite, niveau, race, élément,
    // taille, PV. ⚠ L'IDENTIFIANT DU LIEN EST CELUI DE LA BASE.
    if (monster_sheet_.has_value()) {
      monster_sheet_->NoteHovered(asked.hovered_id, /*by_view=*/false);
    }
  } else if (asked.hovered_id != 0 && asked.hovered_kind == ro::ui::ChatLinkKind::kRecipe) {
    // UNE RECETTE S'APERÇOIT PAR SON SKILL ET SES COMPOSANTS, pas par les
    // stats du produit : c'est de sa fabrication qu'on parle.
    if (craft_atlas_.has_value()) {
      craft_atlas_->NoteHoveredRecipe(asked.hovered_id);
    }
  } else if (asked.hovered_id != 0) {
    hovered_item_ = asked.hovered_id;
    hovered_slot_ = 0;
    hovered_holder_ = ro::ui::ItemHolder::kInventory;
    // L'APERÇU MONTRE L'OBJET DU LIEN, et pas l'entrée de catalogue qui porte
    // son nom : raffinement, cartes et options voyagent dans la balise.
    ChatLinkTarget scratch;
    const ChatLinkTarget* hovered =
        AimChatLink(posed, chat_tabs_, asked.hovered_token, asked.hovered_from_input, scratch);
    if (hovered != nullptr && hovered->kind == ro::ui::ChatLinkKind::kItem &&
        hovered->item.item_id == asked.hovered_id) {
      description_preview_->NoteLinked(hovered->item);
      hovered_holder_ = ro::ui::ItemHolder::kChatLink;
    }
  }
  // UN LIEU S'APERÇOIT PAR SON PLAN, à l'image suivante.
  if (asked.hovering && asked.hovered_kind == ro::ui::ChatLinkKind::kPlace) {
    ChatLinkTarget scratch;
    const ChatLinkTarget* hovered =
        AimChatLink(posed, chat_tabs_, asked.hovered_token, asked.hovered_from_input, scratch);
    if (hovered != nullptr && hovered->kind == ro::ui::ChatLinkKind::kPlace)
      windows_.navigation.place_tip = PlaceTipOf(hovered->place, pieces_);
  }
  // UNE RECHERCHE S'APERÇOIT PAR CE QU'ELLE CHERCHE, et où.
  if (asked.hovering && asked.hovered_kind == ro::ui::ChatLinkKind::kNaviSearch) {
    ChatLinkTarget scratch;
    const ChatLinkTarget* hovered =
        AimChatLink(posed, chat_tabs_, asked.hovered_token, asked.hovered_from_input, scratch);
    if (hovered != nullptr && hovered->kind == ro::ui::ChatLinkKind::kNaviSearch)
      windows_.navigation.search_tip =
          SearchTipOf(hovered->navi_search, pieces_, navi_, world_.map_entries(), now_ms);
  }
  // UN RÉGLAGE S'APERÇOIT PAR SON CHEMIN : où il mène, et ses deux gestes.
  if (asked.hovering && asked.hovered_kind == ro::ui::ChatLinkKind::kSetting) {
    ChatLinkTarget scratch;
    const ChatLinkTarget* hovered =
        AimChatLink(posed, chat_tabs_, asked.hovered_token, asked.hovered_from_input, scratch);
    const SettingDestination* const destination =
        hovered != nullptr && hovered->kind == ro::ui::ChatLinkKind::kSetting
            ? FindSettingDestination(hovered->setting_key)
            : nullptr;
    if (destination != nullptr) {
      windows_.chat.setting_tip = SettingDestinationTip(*destination);
    }
  }
  if (asked.gesture == ro::ui::ChatLinkGesture::kNothing) {
    return;
  }
  ChatLinkTarget from_input;
  const ChatLinkTarget* aimed =
      AimChatLink(posed, chat_tabs_, asked.token, asked.from_input, from_input);
  if (aimed == nullptr) {
    return;
  }
  if (aimed->kind == ro::ui::ChatLinkKind::kStyle) {
    FollowStyleLink(*aimed, asked.gesture, now_ms);
    return;
  }
  // ── UNE ADRESSE WEB : la confirmation, l'aperçu, la visionneuse ───────────
  if (aimed->kind == ro::ui::ChatLinkKind::kUrl) {
    pieces_.chat_web.Follow(asked, aimed->url, windows_.chat);
    return;
  }
  // ── UN PSEUDO DISCORD : il n'existe pas dans le jeu — on le nomme ─────────
  if (aimed->kind == ro::ui::ChatLinkKind::kDiscordName) {
    if (asked.gesture == ro::ui::ChatLinkGesture::kDescribe) {
      MentionInInput(windows_.chat.main.input, aimed->discord_name);
    } else if (asked.gesture == ro::ui::ChatLinkGesture::kCopyCode) {
      ImGui::SetClipboardText(aimed->discord_name.c_str());
    }
    return;
  }
  // ── UN RÉGLAGE : ce qu'il désigne s'ouvre, ou le lien se repose ───────────
  //
  // ⚠ LA CLÉ A ÉTÉ RECONNUE ET JUGÉE OFFERTE À L'ANALYSE, sans quoi il n'y aurait
  // pas de lien : le clic ne revient pas sur ce jugement.
  if (aimed->kind == ro::ui::ChatLinkKind::kSetting) {
    const SettingDestination* const destination = FindSettingDestination(aimed->setting_key);
    if (destination == nullptr) {
      return;
    }
    if (asked.gesture == ro::ui::ChatLinkGesture::kDescribe) {
      OpenSettingDestination(*destination, windows_);
    } else if (asked.gesture == ro::ui::ChatLinkGesture::kRepost) {
      static_cast<void>(PlaceInChatInput(SettingPendingLink(*destination)));
    }
    return;
  }
  // ── UN RESPAWN DE MVP : le carnet, l'import, la tombe, la fiche, le relais ─
  if (aimed->kind == ro::ui::ChatLinkKind::kMvp) {
    FollowMvpLink(asked, aimed->mvp, aimed->sender, now_ms);
    return;
  }
  // ── UN MONSTRE : le routage de tout nom de monstre ────────────────────────
  if (aimed->kind == ro::ui::ChatLinkKind::kMonster) {
    FollowMonsterGesture(asked.gesture, aimed->monster, /*by_view=*/false, now_ms);
    return;
  }
  // ── UNE RECHERCHE : la navigation, le guidage, le terme, le relais ────────
  if (aimed->kind == ro::ui::ChatLinkKind::kNaviSearch) {
    FollowNaviSearchLink(asked, aimed->navi_search, now_ms);
    return;
  }
  // ── UNE RECETTE : l'Atlas ouvert sur son produit, ou le lien relayé ───────
  if (aimed->kind == ro::ui::ChatLinkKind::kRecipe) {
    if (asked.gesture == ro::ui::ChatLinkGesture::kDescribe) {
      ro::ui::OpenCraftAtlasOn(windows_.craft_atlas.state, aimed->recipe.item_id);
    } else if (asked.gesture == ro::ui::ChatLinkGesture::kRepost) {
      PostChatLink(aimed->recipe);
    }
    return;
  }
  // ── UN LIEU : le guidage, ou le lien relayé ───────────────────────────────
  if (aimed->kind == ro::ui::ChatLinkKind::kPlace) {
    const PlaceLink& place = aimed->place;
    if (asked.gesture == ro::ui::ChatLinkGesture::kDescribe) {
      // Le trajet se lit sur la minimap et au sol, la destination reste écrite.
      navi_.GuideToPlace(place.map, place.x, place.y);
      world_.RecordChat(
          ro::sim::ChatKind::kServer, 0,
          PlaceDestinationLine(
              place, pieces_.map_display_names.has_value() ? &*pieces_.map_display_names : nullptr),
          now_ms);
    } else if (asked.gesture == ro::ui::ChatLinkGesture::kRepost) {
      PostChatLink(place);
    } else if (asked.gesture == ro::ui::ChatLinkGesture::kCopyCode) {
      ImGui::SetClipboardText(place.map.c_str());
    } else if (asked.gesture == ro::ui::ChatLinkGesture::kStaffWarp) {
      SendStaffWarp(place, Rights(), link_, world_);
    } else if (asked.gesture == ro::ui::ChatLinkGesture::kOpenInNavigation) {
      // 🔴 LA RECHERCHE PART SUR LE NOM INTERNE : le nom lisible porte une
      // ponctuation qui casse les morceaux de la recherche. Le guidage part
      // aussi, pour que la fenêtre montre l'itinéraire.
      ro::ui::OpenNavigationSearch(windows_.navigation.state, place.map, /*monsters_only=*/false);
      navi_.Search(place.map, /*only_map=*/{}, world_.map_name());
      navi_.GuideToPlace(place.map, place.x, place.y);
    } else if (asked.gesture == ro::ui::ChatLinkGesture::kPlaceContents) {
      windows_.navigation.state.open = true;
      windows_.navigation.state.raise = true;
      if (const ro::formats::NaviData* data = navi_.data(); data != nullptr)
        navi_.navigation_view.ShowContents(*data, data->FindMap(place.map));
    }
    return;
  }
  // ── UN PSEUDO : le chuchotement préparé, la conversation, les invitations ──
  if (aimed->kind == ro::ui::ChatLinkKind::kPlayer) {
    // L'INVITATION AU CARNET DE CHASSE n'est pas aux chuchotements : elle part
    // du carnet.
    if (asked.gesture == ro::ui::ChatLinkGesture::kInviteMvp) {
      pieces_.mvp_tracker.Invite(aimed->player, link_);
      return;
    }
    pieces_.whispers.FollowPlayer(aimed->player, asked.gesture, link_, windows_.chat.main.input);
    return;
  }
  if (asked.item_id == 0) {
    return;
  }
  const ro::vfs::ItemLink* carried = &aimed->item;

  switch (asked.gesture) {
    case ro::ui::ChatLinkGesture::kDescribe:
      // ⚠ SANS PLACE : on ne possède pas forcément ce qu'un autre joueur a lié.
      // Le nom, les cartes et les options se composent de ce que la balise porte.
      item_description_->NoteLinked(*carried);
      windows_.description.item_id = asked.item_id;
      windows_.description.from_slot = 0;
      windows_.description.from_holder = ro::ui::ItemHolder::kChatLink;
      windows_.description.open = true;
      windows_.description.place_at_mouse = true;
      windows_.description.raise = true;
      break;
    case ro::ui::ChatLinkGesture::kRepost:
      // 🔴 LA BALISE EST RÉ-ENCODÉE DEPUIS CE QUE LE LIEN PORTAIT : raffinement,
      // cartes, grade et options survivent au relais. C'est bien le MÊME objet
      // qu'on repasse, pas l'entrée de catalogue qui porte son nom.
      if (carried != nullptr) {
        PostChatLink(*carried);
      }
      break;
    case ro::ui::ChatLinkGesture::kOpenDatabase:
      // ⚠ PAR LE MÊME GUICHET QUE LE LIEN DE LA DESCRIPTION : lui seul décide
      // qu'une adresse s'ouvre, et il en garde les gardes.
      FollowDescriptionLink(
          ro::ui::DescriptionWindowAction{.followed = ro::vfs::ItemDatabaseUrl(asked.item_id)},
          now_ms);
      break;
    case ro::ui::ChatLinkGesture::kItemInfo:
    case ro::ui::ChatLinkGesture::kWhoDrops: {
      // ⚠ ENVOYÉES COMME UNE LIGNE TAPÉE : leur réponse revient dans le fil, et
      // le serveur applique les mêmes droits qu'à une commande écrite à la main.
      const char* const verb =
          asked.gesture == ro::ui::ChatLinkGesture::kItemInfo ? "@iteminfo " : "@whodrops ";
      SubmitChat(*chat_dispatch_, verb + std::to_string(asked.item_id), now_ms);
      break;
    }
    case ro::ui::ChatLinkGesture::kVoteShop:
      // Le vote shop : la boutique s'ouvre, l'item au panier.
      static_cast<void>(
          pieces_.cash_shop.OpenWithItem(asked.item_id, windows_.cash_shop.state, link_));
      break;
    case ro::ui::ChatLinkGesture::kNothing:
    case ro::ui::ChatLinkGesture::kMenu:
    case ro::ui::ChatLinkGesture::kTryStyle:
    case ro::ui::ChatLinkGesture::kCopyCode:
    case ro::ui::ChatLinkGesture::kOpenBestiary:
    case ro::ui::ChatLinkGesture::kOpenInNavigation:
    case ro::ui::ChatLinkGesture::kPlaceContents:
    case ro::ui::ChatLinkGesture::kStaffWarp:
    case ro::ui::ChatLinkGesture::kWhisperWindow:
    case ro::ui::ChatLinkGesture::kInviteParty:
    case ro::ui::ChatLinkGesture::kInviteGuild:
    case ro::ui::ChatLinkGesture::kAddFriend:
    case ro::ui::ChatLinkGesture::kMonsterSheet:
    case ro::ui::ChatLinkGesture::kShareRespawn:
    case ro::ui::ChatLinkGesture::kGuideToTomb:
    case ro::ui::ChatLinkGesture::kWhereToFind:
    case ro::ui::ChatLinkGesture::kMobInfo:
    case ro::ui::ChatLinkGesture::kWhereIs:
    case ro::ui::ChatLinkGesture::kAddToMvpLog:
    case ro::ui::ChatLinkGesture::kInviteMvp:
    case ro::ui::ChatLinkGesture::kShowImageOnce:
    case ro::ui::ChatLinkGesture::kAllowImageHost:
    case ro::ui::ChatLinkGesture::kViewImage:
    case ro::ui::ChatLinkGesture::kPostWhereToFind:
    case ro::ui::ChatLinkGesture::kGoThere:
      break;
  }
}

// CE QU'UN GESTE SUR UN LIEN DE STYLE DEMANDE.
//
// 🔴 AUCUN DE CES TROIS-LÀ NE TOUCHE AU PERSONNAGE, et c'est la règle de tout
// le chantier des palettes : un code vient d'un AUTRE JOUEUR, et il ne doit
// pas pouvoir changer une apparence d'un simple clic. Le clic montre, le menu
// charge dans l'éditeur, et c'est la validation qui engage.
void Game::FollowStyleLink(const ChatLinkTarget& aimed, ro::ui::ChatLinkGesture gesture,
                           std::uint32_t now_ms) {
  switch (gesture) {
    case ro::ui::ChatLinkGesture::kDescribe:
      // « Voir ce style » : le seul aperçu honnête d'une recette, qui ne veut
      // rien dire hors d'un sprite — le NÔTRE.
      if (style_preview_.has_value()) {
        static_cast<void>(style_preview_->Open(aimed.code, aimed.owner, windows_.style_preview));
      }
      break;
    case ro::ui::ChatLinkGesture::kTryStyle:
      // ⚠ LA FENÊTRE S'OUVRE AVEC : charger un code dans un éditeur fermé
      // serait un geste sans réponse visible.
      windows_.style.open = true;
      static_cast<void>(style_view_->TryCode(aimed.code, windows_.style, now_ms));
      break;
    case ro::ui::ChatLinkGesture::kCopyCode:
      // ⚠ LE CODE TEL QU'IL A VOYAGÉ : ré-encoder perdrait ce qu'une version
      // future y ajouterait, et ce qu'on colle ne serait plus ce qu'on a reçu.
      ImGui::SetClipboardText(aimed.code.c_str());
      break;
    case ro::ui::ChatLinkGesture::kRepost:
      PostStyleLink(aimed.code, aimed.owner);
      break;
    case ro::ui::ChatLinkGesture::kNothing:
    case ro::ui::ChatLinkGesture::kMenu:
    case ro::ui::ChatLinkGesture::kOpenDatabase:
    case ro::ui::ChatLinkGesture::kItemInfo:
    case ro::ui::ChatLinkGesture::kWhoDrops:
    case ro::ui::ChatLinkGesture::kVoteShop:
    case ro::ui::ChatLinkGesture::kOpenBestiary:
    case ro::ui::ChatLinkGesture::kOpenInNavigation:
    case ro::ui::ChatLinkGesture::kPlaceContents:
    case ro::ui::ChatLinkGesture::kStaffWarp:
    case ro::ui::ChatLinkGesture::kWhisperWindow:
    case ro::ui::ChatLinkGesture::kInviteParty:
    case ro::ui::ChatLinkGesture::kInviteGuild:
    case ro::ui::ChatLinkGesture::kAddFriend:
    case ro::ui::ChatLinkGesture::kMonsterSheet:
    case ro::ui::ChatLinkGesture::kShareRespawn:
    case ro::ui::ChatLinkGesture::kGuideToTomb:
    case ro::ui::ChatLinkGesture::kWhereToFind:
    case ro::ui::ChatLinkGesture::kMobInfo:
    case ro::ui::ChatLinkGesture::kWhereIs:
    case ro::ui::ChatLinkGesture::kAddToMvpLog:
    case ro::ui::ChatLinkGesture::kInviteMvp:
    case ro::ui::ChatLinkGesture::kShowImageOnce:
    case ro::ui::ChatLinkGesture::kAllowImageHost:
    case ro::ui::ChatLinkGesture::kViewImage:
    case ro::ui::ChatLinkGesture::kPostWhereToFind:
    case ro::ui::ChatLinkGesture::kGoThere:
      break;
  }
}

void Game::PostChatLink(const MonsterLink& link) {
  // 🔴 LE NOM VOYAGE AVEC LA BALISE : le lecteur ne saurait pas le composer.
  // Et le libellé posé est celui que le fil rendra, pour que ce que le joueur
  // voit AVANT l'envoi soit ce que tous liront APRÈS.
  const std::string wire = BuildMonsterLink(link);
  const std::string shown = MonsterLinkLabel(link);
  if (wire.empty()) {
    return;
  }
  static_cast<void>(PlaceInChatInput(PendingChatLink{.shown = shown,
                                                     .wire = wire,
                                                     .kind = ro::ui::ChatLinkKind::kMonster,
                                                     .item_id = link.monster_id}));
}

void Game::PostChatLink(const PlaceLink& link) {
  // 🔴 LE LIBELLÉ POSÉ EST CELUI QUE LE FIL RENDRA : le nom affiché de la carte,
  // résolu chez nous comme il le sera chez le lecteur.
  const std::string wire = BuildPlaceLink(link);
  const std::string shown = PlaceLinkLabel(
      link, pieces_.map_display_names.has_value() ? &*pieces_.map_display_names : nullptr);
  if (wire.empty()) {
    return;
  }
  static_cast<void>(PlaceInChatInput(
      PendingChatLink{.shown = shown, .wire = wire, .kind = ro::ui::ChatLinkKind::kPlace}));
}

void Game::PostChatLink(const NaviSearchLink& link) {
  // 🔴 LE LIBELLÉ POSÉ EST CELUI QUE LE FIL RENDRA : composé depuis la balise
  // RELUE — un contexte mal formé s'y perd —, avec les noms lisibles de chez
  // nous, comme le lecteur avec les siens.
  const std::string wire = BuildNaviSearchLink(link);
  NaviSearchLink back;
  if (wire.empty() || ParseNaviSearchLink(wire, &back, nullptr) != NaviSearchLinkRead::kRead) {
    return;
  }
  const std::string shown = NaviSearchLinkLabel(
      back, pieces_.map_display_names.has_value() ? &*pieces_.map_display_names : nullptr);
  if (shown.empty()) {
    return;
  }
  static_cast<void>(PlaceInChatInput(
      PendingChatLink{.shown = shown, .wire = wire, .kind = ro::ui::ChatLinkKind::kNaviSearch}));
}

void Game::PostChatLink(const RecipeLink& link) {
  // 🔴 LE LIBELLÉ POSÉ EST CELUI QUE LE FIL RENDRA, comme pour un monstre : ce
  // que le joueur voit avant l'envoi est ce que tous liront après.
  const std::string wire = BuildRecipeLink(link);
  const std::string shown = RecipeLinkLabel(link.name);
  if (wire.empty()) {
    return;
  }
  static_cast<void>(PlaceInChatInput(PendingChatLink{.shown = shown,
                                                     .wire = wire,
                                                     .kind = ro::ui::ChatLinkKind::kRecipe,
                                                     .item_id = link.item_id}));
}

void Game::PostItemLink(const ro::sim::Inventory& holder, std::uint16_t client_slot) {
  if (const ro::sim::InventoryItem* item = holder.Find(client_slot); item != nullptr) {
    PostChatLink(ItemLinkOf(*item));
  }
}

void Game::PostChatLink(const ro::vfs::ItemLink& link) {
  const ro::vfs::ItemTable* items =
      pieces_.wardrobe.has_value() ? &pieces_.wardrobe->catalog().items : nullptr;
  if (items == nullptr || link.item_id == 0) {
    return;
  }
  const ro::vfs::ItemEntry* entry = items->Find(link.item_id);
  const std::string wire = ro::vfs::BuildItemLink(link, entry != nullptr ? entry->card_slots : 0);
  // 🔴 LE LIBELLÉ SE COMPOSE DEPUIS LA BALISE RELUE, pas depuis l'objet : on rend
  // exactement ce que le fil rendra une fois la ligne partie. C'est la seule
  // façon que ce que le joueur voit AVANT l'envoi soit ce que tous verront APRÈS.
  ro::vfs::ItemLink back;
  if (wire.empty() || !ro::vfs::ParseItemLink(wire, &back, nullptr)) {
    return;
  }
  ro::vfs::ItemNameFacts facts = ro::vfs::ItemLinkNameFacts(back);
  if (item_description_.has_value()) {
    item_description_->creator_names().Fill(facts);
  }
  const std::string name =
      ro::vfs::ItemNameText(ro::vfs::ComposeItemName(facts, *items, pieces_.item_words));
  if (name.empty()) {
    return;
  }
  static_cast<void>(PlaceInChatInput(PendingChatLink{.shown = ChatLinkLabel(name),
                                                     .wire = wire,
                                                     .kind = ro::ui::ChatLinkKind::kItem,
                                                     .item_id = link.item_id}));
}

void Game::PostStyleLink(const std::string& code, const std::string& label) {
  if (code.empty()) {
    return;
  }
  // 🔴 CE QUE LA BALISE TRANSPORTE EST LE CODE EN ENTIER, et ce n'est pas un
  // raccourci. Un lien d'objet ne porte qu'une DÉSIGNATION : l'objet existe
  // chez le lecteur, sa table le nomme. Un style n'existe NULLE PART hors de
  // son porteur — la recette d'un joueur en zone disparaît avec lui —, donc un
  // lien qui ne dirait qu'un pseudo mourrait au premier changement de carte.
  //
  // ⚠ ET LA BALISE RESTE LISIBLE CHEZ QUI NE LA REND PAS : un client sans
  // greffon en montre le texte brut, l'étiquette en tête. C'est la raison
  // pour laquelle celle-ci vient AVANT le code, et non l'inverse.
  //
  // ⚠ L'ÉTIQUETTE PAR DÉFAUT EST NOTRE PSEUDO, et elle se résout ICI : le nom
  // du personnage appartient au monde, pas à l'éditeur de style.
  const std::string owner = label.empty() ? std::string(world_.player_name()) : label;
  const std::string wire = ro::sim::BuildStyleTag(owner, code);
  // ⚠ LE LIBELLÉ SE COMPOSE DE L'ÉTIQUETTE NETTOYÉE, celle que la balise
  // portera vraiment : ce que le joueur voit AVANT l'envoi doit être ce que
  // tous liront APRÈS.
  const std::string shown = StyleLinkLabel(ro::sim::StyleTagLabel(owner));
  if (wire.empty()) {
    return;
  }
  static_cast<void>(PlaceInChatInput(PendingChatLink{.shown = shown,
                                                     .wire = wire,
                                                     .kind = ro::ui::ChatLinkKind::kStyle,
                                                     .code = code,
                                                     .owner = owner}));
}

bool Game::PlaceInChatInput(PendingChatLink link) {
  // 🔴 DANS LA DERNIÈRE SAISIE ACTIVE, et non dans celle qui a le focus : au
  // moment du Maj+clic, c'est la fenêtre cliquée qui l'a. Une conversation 1:1
  // tapée en dernier reçoit donc le lien, jusqu'à ce que la barre reprenne le
  // clavier. ⚠ UNE POSE QUI ÉCHOUE LÀ — fenêtre fermée, place qui manque — SE
  // RABAT SUR LA BARRE : le geste du joueur ne doit pas se perdre en silence.
  if (pieces_.whispers.TakesLinks() && pieces_.whispers.PlaceLink(link, windows_.whispers)) {
    return true;
  }
  // ⚠ AU BOUT DE CE QUE LA SAISIE PORTERA, dépôt en attente compris : voir
  // `PlaceChatLinkLabel`.
  if (!PlaceChatLinkLabel(link.shown, windows_.chat.main.input, &pending_links_)) {
    return false;
  }
  pending_links_.push_back(std::move(link));
  // La main revient au clavier : on vient de poser un mot dans une phrase.
  //
  // 🔴 ET LA BARRE S'OUVRE SI LE BATTLE MODE L'AVAIT REPLIÉE. Sans cela le
  // lien atterrit dans un champ que rien ne montre : le joueur ne voit pas ce
  // qu'il vient de poser, et la phrase part au premier Entrée sans qu'il l'ait
  // relue. Tout ce qui écrit dans la saisie doit la déplier.
  windows_.chat.main.input.unfolded = true;
  windows_.chat.main.input.focus_on_whisper = false;
  windows_.chat.main.input.wants_focus = true;
  return true;
}

void Game::PostClickedItemLink(const UiStage::Answers& answers) {
  // ── MAJ + CLIC GAUCHE : LE LIEN DE L'OBJET DANS LA SAISIE ──────────────────
  //
  // ⚠ PLUSIEURS FENÊTRES LE DEMANDENT, une seule l'a demandé : le joueur n'a
  // qu'une souris. Le coffre n'en est pas — le client ne l'y propose pas non plus.
  // ⚠ ET LA FICHE AUSSI : une pièce PORTÉE vit dans le sac, avec un drapeau, donc
  // sa place est une place de sac — le mannequin n'est pas un contenant.
  if (answers.inventory.link_slot != 0) {
    PostItemLink(world_.inventory(), answers.inventory.link_slot);
  } else if (answers.cart.link_slot != 0) {
    PostItemLink(world_.cart(), answers.cart.link_slot);
  } else if (answers.character.link_slot != 0) {
    PostItemLink(world_.inventory(), answers.character.link_slot);
  } else if (answers.shortcuts.link >= 0) {
    // ⚠ UNE CASE NE CONNAÎT QU'UN IDENTIFIANT, et un lien a besoin d'un
    // EXEMPLAIRE : c'est lui qui porte le raffinement, les cartes et la forge.
    // On cherche donc la place au moment de poser ; sans exemplaire sous la
    // main, rien ne part — un lien qui annoncerait un objet nu mentirait sur ce
    // que le joueur montre.
    std::uint16_t at = 0;
    if (shortcuts_->DescribedItem(static_cast<std::size_t>(answers.shortcuts.link), world_, &at) !=
            0 &&
        at != 0) {
      PostItemLink(world_.inventory(), at);
    }
  }
}

}  // namespace moonclient
