// LE CARNET DE CHASSE MVP, vu de la boucle : l'étage avancé à chaque image, le
// nom d'un rang et les liens `<MVPL>` suivis, le lien posé dans la saisie, et
// le son de l'alerte.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <imgui.h>

#include "ro/ui/mvp_tracker_window.h"
#include "ro/ui/navigation_window.h"

#include "chat/chat_monster_link.h"
#include "chat/chat_mvp_link.h"
#include "game/game.h"
#include "game/mvp_alert_sound.h"
#include "game/mvp_tracker_bug_report.h"
#include "mvp/mvp_tracker_stage.h"

namespace moonclient {
namespace {

constexpr std::uint32_t kMsPerMinute = 60000;

// Le rang que la vue montre pour ce créneau, ou nul.
[[nodiscard]] const ro::ui::MvpSlotView* RowOf(const ro::ui::MvpTrackerView* view,
                                               std::uint32_t slot_id) {
  if (view == nullptr) {
    return nullptr;
  }
  const auto found = std::ranges::find_if(
      view->slots, [slot_id](const ro::ui::MvpSlotView& row) { return row.slot_id == slot_id; });
  return found != view->slots.end() ? &*found : nullptr;
}

}  // namespace

void Game::AdvanceMvpTracker(std::uint32_t now_ms) {
  MvpTrackerWindows& tracker = windows_.mvp_tracker;
  MvpTrackerOptions& page = settings_->options().pages().mvp_tracker;
  // ⚠ RELEVÉS AVANT L'AVANCÉE, qui consomme la réponse de la fenêtre ; JOUÉS
  // APRÈS, sur la vue qu'elle vient de composer : une trame de cette image a pu
  // refaire le catalogue sous la vue d'avant.
  const ro::ui::ChatLinkAsk named = tracker.answer.name_link;
  const bool open_settings = tracker.answer.open_settings;
  // ⚠ LE RAPPORT AUSSI, et sur la vue d'avant : c'est celle que le joueur a vue.
  if (tracker.answer.report_bug && tracker.view != nullptr) {
    AskBugReport(windows_.bug_report, MvpTrackerBugReport(tracker.window, *tracker.view));
  }
  const MvpTrackerContext context{
      .link = link_,
      .options = page,
      // ⚠ UNE LIAISON FERMÉE N'EST PLUS RECONNUE, même si sa poignée de main
      // garde l'état « vérifiée » jusqu'à la suivante.
      .verified =
          link_.is_open() && handshake_.state() == ro::net::MoonclientHandshakeState::kVerified,
      .local_now_s = LocalUnixSeconds(),
      .now_ms = now_ms,
      .sprite_box_px =
          static_cast<int>(std::lround(ro::ui::kMvpSpritePixels * ImGui::GetStyle().FontScaleMain)),
      // La propriété du groupe se déduit de notre nom parmi les membres.
      .own_name = world_.player_name()};
  // Le clic droit de l'en-tête montre « Délai » : le fichier de réglages doit
  // l'apprendre.
  if (pieces_.mvp_tracker.Advance(tracker, context)) {
    settings_->file().Touch();
  }
  // LE RÉSULTAT D'UNE INVITATION OU D'UNE SAISIE, carnet fermé : le bandeau
  // rouge ne se voit que carnet ouvert, le chat le dit.
  for (std::string& notice : pieces_.mvp_tracker.TakeNotices()) {
    world_.RecordChat(ro::sim::ChatKind::kServer, 0, std::move(notice), now_ms);
  }
  if (pieces_.mvp_tracker.TakeAlertSound()) {
    PlayMvpAlertSound({.audio = audio_,
                       .vfs = *vfs_,
                       .client_root = client_root_,
                       .tone = page.settings().alert_tone,
                       .custom_file = page.settings().alert_wav,
                       .reported = &page.alert_failure_reported(),
                       .custom_held = &page.alert_sound_held(),
                       .trouble = &page.alert_trouble()});
  }
  // ⚠ ICI, AVANT LE DESSIN QUI COMPOSE LA PAGE : les réglages ne voient pas
  // l'étage, et sans ce compte « Tout remettre au carnet » n'apparaîtrait jamais.
  // Ni le moteur : sans ce relevé, « bruitages coupés » survivrait à leur retour.
  page.NoteLineCount(pieces_.mvp_tracker.line_count());
  page.NoteSoundAudible(audio_.sound_audible());
  if (open_settings) {
    GameSettings::OpenOptionsAt(windows_, OptionSection::kMvpTracker);
  }

  // ── LE NOM D'UN RANG : les gestes d'un lien de monstre, et le respawn ──
  // Survolé, il donne la fiche compacte, comme un lien de monstre du chat, et
  // annonce ce que fait le Maj+clic : partager le respawn si le rang porte une
  // heure, le lien seul sinon.
  if (named.hovered_id != 0 && monster_sheet_.has_value()) {
    monster_sheet_->NoteHovered(named.hovered_id, /*by_view=*/false,
                                MvpNameHint(RowOf(tracker.view, named.hovered_token)));
  }
  if (named.gesture == ro::ui::ChatLinkGesture::kNothing) {
    return;
  }
  const ro::ui::MvpSlotView* row = RowOf(tracker.view, named.token);
  if (row == nullptr || row->link_mob_id == 0) {
    return;
  }
  const std::uint32_t monster_id = row->link_mob_id;
  const std::string label(row->label);
  // LE RESPAWN ET LA TOMBE SONT À LUI ; le reste est le geste de tout nom de
  // monstre, routé au même endroit que celui du fil.
  switch (named.gesture) {
    case ro::ui::ChatLinkGesture::kShareRespawn:
      // ⚠ SANS OBSERVATION, LA TOMBE EST INCONNUE : la vue la porte alors à -1.
      PostChatLink(MvpLink{.monster_id = monster_id,
                           .map = std::string(row->map),
                           .kill_time_s = row->kill_time_s,
                           .exact_respawn_s = row->exact_respawn_s,
                           .delay1_min = static_cast<std::uint16_t>(row->delay1_ms / kMsPerMinute),
                           .delay2_min = static_cast<std::uint16_t>(row->delay2_ms / kMsPerMinute),
                           .tomb_x = row->tomb_x,
                           .tomb_y = row->tomb_y,
                           .name = label});
      break;
    case ro::ui::ChatLinkGesture::kGuideToTomb:
      // Tombe inconnue (-1) : « Y aller », le guidage vers la carte entière.
      navi_.GuideToPlace(row->map, row->tomb_x, row->tomb_y);
      break;
    default:
      // ⚠ LE NOM DU CARNET EST CELUI D'UN MVP : son lien relayé le dit.
      FollowMonsterGesture(
          named.gesture,
          MonsterLink{.monster_id = monster_id, .rank = kMonsterRankMvp, .name = label},
          /*by_view=*/false, now_ms);
      break;
  }
}

void Game::FollowMvpLink(const ro::ui::ChatLinkAsk& asked, const MvpLink& link,
                         std::string_view sender, std::uint32_t now_ms) {
  switch (asked.gesture) {
    case ro::ui::ChatLinkGesture::kDescribe:
      // ⚠ LE CLIC N'IMPORTE RIEN : écrire dans le carnet touche celui de tout le
      // groupe, et cela ne se fait pas par mégarde. Il ouvre le carnet sur la
      // ligne — sans viser, si le créneau n'est pas au catalogue.
      pieces_.mvp_tracker.RequestOpenOn(pieces_.mvp_tracker.SlotOf(link));
      break;
    case ro::ui::ChatLinkGesture::kAddToMvpLog:
      pieces_.mvp_tracker.Import(link, sender, link_);
      break;
    case ro::ui::ChatLinkGesture::kGuideToTomb:
      // Tombe inconnue (-1) : « Y aller », le guidage vers la carte entière.
      navi_.GuideToPlace(link.map, link.tomb_x, link.tomb_y);
      break;
    case ro::ui::ChatLinkGesture::kMonsterSheet:
      if (monster_sheet_.has_value()) {
        monster_sheet_->Open(windows_.monster_sheet.state, link.monster_id, /*by_view=*/false);
      } else {
        FollowDescriptionLink(
            ro::ui::DescriptionWindowAction{.followed = MonsterBestiaryUrl(link.monster_id)},
            now_ms);
      }
      break;
    case ro::ui::ChatLinkGesture::kRepost:
      PostChatLink(link);
      break;
    case ro::ui::ChatLinkGesture::kNothing:
    case ro::ui::ChatLinkGesture::kMenu:
    case ro::ui::ChatLinkGesture::kOpenDatabase:
    case ro::ui::ChatLinkGesture::kItemInfo:
    case ro::ui::ChatLinkGesture::kWhoDrops:
    case ro::ui::ChatLinkGesture::kMarketSearch:
    case ro::ui::ChatLinkGesture::kGameDatabase:
    case ro::ui::ChatLinkGesture::kTryStyle:
    case ro::ui::ChatLinkGesture::kCopyCode:
    case ro::ui::ChatLinkGesture::kOpenBestiary:
    case ro::ui::ChatLinkGesture::kOpenInNavigation:
    case ro::ui::ChatLinkGesture::kPlaceContents:
    case ro::ui::ChatLinkGesture::kStaffWarp:
    case ro::ui::ChatLinkGesture::kVoteShop:
    case ro::ui::ChatLinkGesture::kWhisperWindow:
    case ro::ui::ChatLinkGesture::kInviteParty:
    case ro::ui::ChatLinkGesture::kInviteGuild:
    case ro::ui::ChatLinkGesture::kAddFriend:
    case ro::ui::ChatLinkGesture::kShareRespawn:
    case ro::ui::ChatLinkGesture::kWhereToFind:
    case ro::ui::ChatLinkGesture::kMobInfo:
    case ro::ui::ChatLinkGesture::kWhereIs:
    case ro::ui::ChatLinkGesture::kInviteMvp:
    case ro::ui::ChatLinkGesture::kShowImageOnce:
    case ro::ui::ChatLinkGesture::kAllowImageHost:
    case ro::ui::ChatLinkGesture::kViewImage:
    case ro::ui::ChatLinkGesture::kSeeInAlbum:
    case ro::ui::ChatLinkGesture::kPostWhereToFind:
    case ro::ui::ChatLinkGesture::kGoThere:
      break;
  }
}

void Game::PostChatLink(const MvpLink& link) {
  // 🔴 LE LIBELLÉ POSÉ EST CELUI QUE LE FIL RENDRA, sans « passé » : la saisie
  // n'a pas d'horloge, et ce que le joueur voit avant l'envoi est ce que tous
  // liront après.
  const std::string wire = BuildMvpLink(link);
  const std::string shown = MvpLinkLabel(link);
  if (wire.empty()) {
    return;
  }
  static_cast<void>(PlaceInChatInput(PendingChatLink{.shown = shown,
                                                     .wire = wire,
                                                     .kind = ro::ui::ChatLinkKind::kMvp,
                                                     .item_id = link.monster_id}));
}

}  // namespace moonclient
