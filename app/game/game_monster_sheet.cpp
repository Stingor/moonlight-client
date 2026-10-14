// LA FICHE D'UN MONSTRE, vue de la boucle : ce que la fenêtre a rendu à
// l'image précédente, le relevé de Sense qui l'ouvre, ce qu'il faut demander au
// serveur, puis la vue qu'elle dessinera.
//
// ⚠ À CÔTÉ DE LA BOUCLE : la fenêtre forme un sujet entier — une fiche reçue,
// ses gestes, son sprite qui réagit et qui parle.

#include <string>
#include <utility>

#include "ro/core/log.h"
#include "ro/net/monster_sheet.h"
#include "ro/render/skill_sound_table.h"
#include "ro/sim/bank.h"
#include "ro/sim/card_composition.h"
#include "ro/sim/inventory.h"
#include "ro/vfs/item_table.h"

#include "chat/chat_view.h"
#include "game/game.h"
#include "game/map_plan_tip.h"

namespace moonclient {
namespace {

// Le volume d'un son de la fiche : plein, et au centre — il ne vient d'aucun
// point du monde.
constexpr float kSheetSoundGain = 1.0F;
constexpr float kSheetSoundPan = 0.0F;

}  // namespace

void Game::AdvanceMonsterSheet(std::uint32_t now_ms) {
  // LA BASE DE DONNÉES D'ABORD : elle peut héberger la fiche, et les gestes
  // qu'on y a faits sur elle sont ceux de cette image.
  AdvanceDatabase(now_ms);
  if (!monster_sheet_.has_value()) {
    windows_.monster_sheet.view = nullptr;
    return;
  }
  MonsterSheet& sheet = *monster_sheet_;
  ro::ui::MonsterSheetWindowState& state = windows_.monster_sheet.state;
  const ro::ui::MonsterSheetAnswer answer = std::exchange(windows_.monster_sheet.answer, {});
  const MonsterSheetOptions& options = settings_->options().monster_sheet();

  // 🔴 UNE AUTRE CARTE, UNE FICHE REFERMÉE ET OUBLIÉE : les ressources du
  // sprite ont pu partir, et une fiche d'une autre session ne s'affiche pas.
  if (sheet.ForgetOnMapEntry(world_.map_entries())) {
    state.open = false;
  }

  // ── UN RELEVÉ DE SENSE L'OUVRE ────────────────────────────────────────────
  //
  // 🔴 L'IDENTIFIANT EST UNE CLASSE DE SPRITE : le paquet ne porte que
  // l'apparence du monstre visé, et c'est le serveur qui remonte à la base.
  if (const std::optional<std::uint32_t> sensed = sheet.TakeSense(options.show_guardians());
      sensed.has_value()) {
    sheet.Open(state, *sensed, /*by_view=*/true);
  }

  // ── CE QUE LA FENÊTRE A DEMANDÉ ───────────────────────────────────────────
  if (answer.poked) {
    if (MonsterSprite* const sprite = sheet.sprite(); sprite != nullptr) {
      sprite->Poke(state.sprite_direction, now_ms);
    }
  }
  if (answer.open_monster != 0) {
    sheet.Open(state, answer.open_monster, /*by_view=*/false);
  }
  if (answer.open_database && sheet.shown_id() != 0) {
    database_.AskOpen(
        ro::ui::DatabaseTarget{.kind = ro::ui::DatabaseKind::kMonster, .id = sheet.shown_id()});
  }
  ro::ui::AskSkillDescription(windows_.skill_description, answer.describe_skill);
  // ── UNE CARTE D'APPARITION, avec les gestes de tout lieu ──────────────────
  //
  // Le clic guide vers la carte ENTIÈRE — la fiche ne dit pas où, sur elle, le
  // monstre se tient ; le trajet se lit sur la minimap et au sol. Le Maj+clic
  // pose le `<NAVIL>` de la carte dans la saisie, et le survol emprunte
  // l'infobulle d'un lien de lieu : nom lisible et plan.
  if (!answer.guide_map.empty()) {
    navi_.GuideToPlace(answer.guide_map, 0, 0);
  }
  if (!answer.link_map.empty()) {
    PostChatLink(PlaceLink{.map = answer.link_map});
  }
  if (!answer.hovered_map.empty()) {
    windows_.navigation.place_tip = PlaceTipOf(PlaceLink{.map = answer.hovered_map}, pieces_);
  }

  // ── LES GESTES SUR UN BUTIN, les mêmes que partout où un objet se montre ──
  //
  // ⚠ UN BUTIN EST UN OBJET DE CATALOGUE : ni raffinement, ni cartes. Il se
  // décrit, se relaie et s'interroge par son seul identifiant.
  FollowCatalogItem(answer.item, now_ms);

  // ── POSER LE MONSTRE DANS LA BARRE DE CHAT ────────────────────────────────
  //
  // 🔴 C'EST ICI QUE LE NOM EXISTE : le lien emporte celui que la fiche montre,
  // et le rang qu'elle connaît.
  if (answer.post_link) {
    if (const ro::sim::MonsterSheet* const shown = sheet.shown(); shown != nullptr) {
      PostChatLink(MonsterLink{
          .monster_id = sheet.shown_id(), .rank = shown->boss_rank, .name = shown->name});
    }
  }

  // L'APERÇU D'UN LIEN SURVOLÉ, que les surfaces ont noté à l'image précédente.
  windows_.monster_sheet.preview =
      sheet.Preview(windows_.monster_sheet.preview_state, options.animate(), now_ms);

  // ── LA DEMANDE AU SERVEUR ─────────────────────────────────────────────────
  //
  // ⚠ LA FENÊTRE SEULEMENT OUVERTE : c'est l'ouverture qui pose la question, et
  // une fiche qu'on ne regarde pas n'a pas à se rafraîchir. L'aperçu, lui,
  // demande ce qu'il survole.
  // ── LA BASE DE DONNÉES L'HÉBERGE, quand elle est ouverte ──────────────────
  const bool hosted = HostMonsterSheet(sheet);
  if (link_.is_open()) {
    if (const std::optional<MonsterSheet::Ask> ask =
            sheet.NextAsk(world_.player_status(), state.open || hosted, now_ms);
        ask.has_value()) {
      ReportRefusal(ro::net::RequestMonsterSheet(link_, ask->monster_id, ask->by_view));
    }
  }

  if (!state.open && !hosted) {
    windows_.monster_sheet.view = nullptr;
    database_.NoteMonsterView(windows_.monster_sheet.database, nullptr);
    return;
  }
  const ro::ui::MonsterSheetView& view =
      sheet.Refresh(state, world_.player_status(), options.animate(), true, now_ms);
  sheet.NoteVoteShop(pieces_.cash_shop.offers());
  // « Base de données » dans la fiche seule, que la base reprendrait.
  sheet.NoteDatabaseLink(state.open && !sheet.shown_by_view());
  windows_.monster_sheet.view = state.open ? &view : nullptr;
  database_.NoteMonsterView(windows_.monster_sheet.database, hosted ? &view : nullptr);

  // ── CE QUE LE SPRITE FAIT ENTENDRE ────────────────────────────────────────
  if (MonsterSprite* const sprite = sheet.sprite(); sprite != nullptr) {
    for (const std::string& name : sprite->TakeSounds()) {
      if (!audio_.sound_audible()) {
        continue;
      }
      if (!audio_.KnowsSound(name)) {
        auto bytes = vfs_->Read(std::string(ro::render::kSoundFolder) + name);
        if (!bytes) {
          ro::core::LogWarning("son : {} absent", name);
          continue;
        }
        if (auto held = audio_.LoadSound(name, *bytes); !held) {
          ro::core::LogWarning("son : {} illisible — {}", name, ro::core::Describe(held.error()));
          continue;
        }
      }
      audio_.PlaySound(name, kSheetSoundGain, kSheetSoundPan, ro::audio::SoundChannel::kSprites);
    }
  }
}

void Game::FollowCatalogItem(const ro::ui::ChatLinkAsk& asked, std::uint32_t now_ms) {
  if (asked.hovered_id != 0) {
    hovered_item_ = asked.hovered_id;
    hovered_slot_ = 0;
    hovered_holder_ = ro::ui::ItemHolder::kInventory;
  }
  const std::uint32_t item_id = asked.item_id;
  switch (item_id == 0 ? ro::ui::ChatLinkGesture::kNothing : asked.gesture) {
    case ro::ui::ChatLinkGesture::kDescribe:
      windows_.description.item_id = item_id;
      windows_.description.from_slot = 0;
      windows_.description.from_holder = ro::ui::ItemHolder::kInventory;
      windows_.description.open = true;
      windows_.description.place_at_mouse = true;
      windows_.description.raise = true;
      break;
    case ro::ui::ChatLinkGesture::kRepost:
      PostChatLink(ro::vfs::ItemLink{.item_id = item_id});
      break;
    case ro::ui::ChatLinkGesture::kOpenDatabase:
      FollowDescriptionLink(
          ro::ui::DescriptionWindowAction{.followed = ro::vfs::ItemDatabaseUrl(item_id)}, now_ms);
      break;
    case ro::ui::ChatLinkGesture::kMarketSearch:
      pieces_.trade.market().AskOpen(item_id);
      break;
    case ro::ui::ChatLinkGesture::kGameDatabase:
      database_.AskOpen(ro::ui::DatabaseTarget{.kind = ro::ui::DatabaseKind::kItem, .id = item_id});
      break;
    case ro::ui::ChatLinkGesture::kItemInfo:
    case ro::ui::ChatLinkGesture::kWhoDrops: {
      const char* const verb =
          asked.gesture == ro::ui::ChatLinkGesture::kItemInfo ? "@iteminfo " : "@whodrops ";
      if (chat_dispatch_.has_value()) {
        SubmitChat(*chat_dispatch_, verb + std::to_string(item_id), now_ms);
      }
      break;
    }
    case ro::ui::ChatLinkGesture::kVoteShop:
      // Le vote shop : la boutique s'ouvre, l'item au panier.
      static_cast<void>(pieces_.cash_shop.OpenWithItem(item_id, windows_.cash_shop.state, link_));
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

}  // namespace moonclient
