// Les classements et les avis d'étoile dans la conversation. Voir l'en-tête.

#include "world/rank_notices.h"

#include <cstddef>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/printed.h"
#include "ro/net/item_facts.h"
#include "ro/sim/chat_line.h"
#include "ro/vfs/item_link.h"

#include "chat/chat_monster_link.h"

namespace moonclient {
namespace {

using ro::core::Printed;
using ro::core::Tr;
using ro::net::RankKind;
using ro::net::StarNoticeCode;

// LE STYLE D'UN CLASSEMENT : son en-tête, la couleur de l'en-tête et celle des
// lignes. Le killer n'a pas de phrase de points ; il a son classement.
struct RankStyle {
  const char* header = nullptr;
  std::uint32_t header_color = 0;
  std::uint32_t line_color = 0;
};

constexpr std::uint32_t kGreyHeader = 0x9B9B9B;
constexpr std::uint32_t kGreyLine = 0xCDCDCD;
constexpr std::uint32_t kGreenHeader = 0x9BCD9B;
constexpr std::uint32_t kGreenLine = 0xCDFFCD;
constexpr std::uint32_t kBlueHeader = 0x9B9BCD;
constexpr std::uint32_t kBlueLine = 0xCDCDFF;

std::optional<RankStyle> StyleOf(RankKind kind) {
  switch (kind) {
    case RankKind::kBlacksmith:
      return RankStyle{Tr("========== BLACKSMITH RANK ============"), kGreyHeader, kGreyLine};
    case RankKind::kAlchemist:
      return RankStyle{Tr("=========== ALCHEMIST RANK ============="), kGreenHeader, kGreenLine};
    case RankKind::kTaekwon:
      return RankStyle{Tr("============ TAEKWON RANK ============="), kBlueHeader, kBlueLine};
    case RankKind::kKiller:
      return RankStyle{Tr("=========== KILLER RANK =============="), kGreyHeader, kGreyLine};
  }
  return std::nullopt;
}

constexpr std::string_view kRankingRule = "===========================================";

// LES AVIS D'ÉTOILE s'écrivent en bleu pâle.
constexpr std::uint32_t kStarNoticeColor = 0xC8C8FF;
// L'effet que posent les avis qui désignent ou rappellent, et celui de la
// remise à zéro.
constexpr std::uint32_t kStarNoticeEffect = 543;
constexpr std::uint32_t kStarsResetEffect = 432;
// Le son d'un rappel de Feeling ou de Hatred.
constexpr std::string_view kStarRecallSound = "effect\\T_피리.wav";

// LE CLASSEMENT ATTEND SES NOMS au plus trois secondes, en regardant chaque
// seconde.
constexpr std::uint32_t kNameLookMs = 1000;
constexpr std::uint32_t kNameLooks = 3;
// UNE DEMANDE DU TABLEAU attend sa réponse au plus ce temps ; au-delà, elle est
// tenue pour perdue et la réponse suivante s'écrit.
constexpr std::uint32_t kQuietAnswerPatienceMs = 5000;

// Soleil, lune, étoile : trois rangs, et un quatrième n'a pas de phrase.
constexpr std::uint8_t kStarCount = 3;

std::string PlaceDesignated(std::uint8_t star, const char* own, const char* subject) {
  switch (star) {
    case 0:
      return Printed(Tr("%s désigne %s comme Solar Space."), own, subject);
    case 1:
      return Printed(Tr("%s désigne %s comme Luna Space."), own, subject);
    default:
      return Printed(Tr("%s désigne %s comme Stellar Space."), own, subject);
  }
}

std::string PlaceRecalled(std::uint8_t star, const char* own, const char* subject) {
  switch (star) {
    case 0:
      return Printed(Tr("Solar Space de %s : %s"), own, subject);
    case 1:
      return Printed(Tr("Luna Space de %s : %s"), own, subject);
    default:
      return Printed(Tr("Stellar Space de %s : %s"), own, subject);
  }
}

std::string TargetDesignated(std::uint8_t star, const char* own, const char* subject) {
  switch (star) {
    case 0:
      return Printed(Tr("%s désigne %s comme Solar Monster."), own, subject);
    case 1:
      return Printed(Tr("%s désigne %s comme Luna Monster."), own, subject);
    default:
      return Printed(Tr("%s désigne %s comme Stellar Monster."), own, subject);
  }
}

std::string TargetRecalled(std::uint8_t star, const char* own, const char* subject) {
  switch (star) {
    case 0:
      return Printed(Tr("Solar Monster de %s : %s"), own, subject);
    case 1:
      return Printed(Tr("Luna Monster de %s : %s"), own, subject);
    default:
      return Printed(Tr("Stellar Monster de %s : %s"), own, subject);
  }
}

// Le lien cliquable d'un monstre, ou son nom seul quand il n'en a pas.
std::string MonsterNamed(const ro::net::StarNotice& notice) {
  std::string link = BuildMonsterLink(MonsterLink{
      .monster_id = notice.subject_id, .rank = kMonsterRankOrdinary, .name = notice.name});
  return link.empty() ? notice.name : link;
}

}  // namespace

const char* StarPlaceQuestion(std::uint8_t subject) {
  if (subject == ro::net::kCostlySpellSubject) {
    return Tr("Lancer le sort qui consomme 1 Black Gemstone et 1 000 000 zeny ?");
  }
  return Tr(
      "Une carte désignée ne peut plus être changée. Voulez-vous vraiment désigner cette "
      "carte ?");
}

std::vector<RankNoticeLine> RankingLines(
    const ro::net::RankingList& list, const std::array<std::string, ro::net::kRankingSize>& names) {
  const std::optional<RankStyle> style = StyleOf(list.kind);
  // UN CLASSEMENT INCONNU S'ÉCRIT QUAND MÊME, sous un simple trait.
  const RankStyle shown = style.value_or(RankStyle{kRankingRule.data(), 0, 0});
  std::vector<RankNoticeLine> lines;
  lines.push_back({shown.header, shown.header_color});
  for (std::size_t rank = 0; rank < ro::net::kRankingSize; ++rank) {
    const std::string& name = names[rank];
    lines.push_back(
        {Printed(Tr("[ %2d ] %s  :  %4d POINT"), static_cast<int>(rank + 1),
                 name.empty() ? Tr("Sans nom") : name.c_str(), static_cast<int>(list.points[rank])),
         shown.line_color});
  }
  lines.push_back({std::string(kRankingRule), shown.header_color});
  lines.push_back({Printed(Tr("MY POINT    :  %4d POINT "), static_cast<int>(list.own_points)),
                   shown.line_color});
  lines.push_back({std::string(kRankingRule), shown.header_color});
  return lines;
}

std::optional<RankNoticeLine> RankingPointsLine(const ro::net::RankingPoints& points) {
  const int gained = static_cast<int>(points.gained);
  const int total = static_cast<int>(points.total);
  switch (points.kind) {
    case RankKind::kBlacksmith:
      return RankNoticeLine{
          Printed(Tr("[Point] %d points de rang Blacksmith gagnés. Total : %d."), gained, total),
          kGreyLine};
    case RankKind::kAlchemist:
      return RankNoticeLine{
          Printed(Tr("[Point] %d points de rang Alchemist gagnés. Total : %d."), gained, total),
          kGreenLine};
    case RankKind::kTaekwon:
      return RankNoticeLine{
          Printed(Tr("[POINT] %d points de rang Taekwon Mission gagnés. Total : %d."), gained,
                  total),
          kBlueLine};
    case RankKind::kKiller:
      return RankNoticeLine{
          Printed(Tr("[POINT] %d points de rang Slayer gagnés. Total : %d."), gained, total),
          kGreyLine};
  }
  return std::nullopt;
}

std::optional<StarNoticeOutcome> StarNoticeOutcomeOf(const ro::net::StarNotice& notice,
                                                     std::string_view own_name) {
  const std::string own(own_name);
  StarNoticeOutcome outcome;
  outcome.line.color = kStarNoticeColor;
  const bool known_star = notice.star < kStarCount;
  switch (notice.code) {
    case StarNoticeCode::kPlaceDesignated:
      if (!known_star) {
        return std::nullopt;
      }
      outcome.line.text = PlaceDesignated(notice.star, own.c_str(), notice.name.c_str());
      return outcome;
    case StarNoticeCode::kPlaceRecalled:
      if (!known_star) {
        return std::nullopt;
      }
      outcome.line.text = PlaceRecalled(notice.star, own.c_str(), notice.name.c_str());
      outcome.effect_id = kStarNoticeEffect;
      outcome.sound = std::string(kStarRecallSound);
      return outcome;
    case StarNoticeCode::kTargetDesignated:
      if (!known_star) {
        return std::nullopt;
      }
      outcome.line.text = TargetDesignated(notice.star, own.c_str(), notice.name.c_str());
      return outcome;
    case StarNoticeCode::kTargetRecalled:
      if (!known_star) {
        return std::nullopt;
      }
      outcome.line.text = TargetRecalled(notice.star, own.c_str(), notice.name.c_str());
      outcome.effect_id = kStarNoticeEffect;
      outcome.sound = std::string(kStarRecallSound);
      return outcome;
    case StarNoticeCode::kTaekwonMission:
      // LA CIBLE EST UN LIEN : un clic ouvre la fiche du monstre.
      // ET SON AVANCEMENT : le serveur donne le compte des tués, sur cent.
      outcome.line.text =
          Printed(Tr("[Taekwon Mission] Monstre cible : %s — %d / %d tués, il en reste %d"),
                  MonsterNamed(notice).c_str(), static_cast<int>(notice.star),
                  static_cast<int>(kTaekwonMissionGoal),
                  static_cast<int>(kTaekwonMissionGoal) - static_cast<int>(notice.star));
      outcome.effect_id = kStarNoticeEffect;
      return outcome;
    case StarNoticeCode::kTamingMission:
      outcome.line.text =
          Printed(Tr("[Taming Mission] Monstre cible : %s"), MonsterNamed(notice).c_str());
      outcome.effect_id = kStarNoticeEffect;
      return outcome;
    case StarNoticeCode::kCollectorItem: {
      const std::string item =
          ro::vfs::BuildItemLink(ro::vfs::ItemLink{.item_id = notice.subject_id}, 0);
      outcome.line.text = Printed(Tr("[Collector Rank] Item cible : %s"), item.c_str());
      outcome.effect_id = kStarNoticeEffect;
      return outcome;
    }
    case StarNoticeCode::kStarsReset:
      outcome.line.text =
          Tr("[Solar, Lunar and Stellar Angel] Les lieux et les monstres désignés sont remis à "
             "zéro.");
      outcome.effect_id = kStarsResetEffect;
      return outcome;
    case StarNoticeCode::kTargetHp:
      outcome.line.text = Printed(Tr("Target HP : %u"), notice.subject_id);
      return outcome;
  }
  return std::nullopt;
}

ro::net::FrameOutcome RankNotices::ApplyFrame(const ro::net::Frame& frame, ro::sim::World& world,
                                              ro::net::Connection& link, std::uint32_t now_ms) {
  if (std::optional<ro::net::CharacterNameAnswer> answer = ro::net::ReadCharacterName(frame)) {
    names_.insert_or_assign(answer->character_id, std::move(answer->name));
    // ⚠ PAS « APPLIQUÉE » : la description d'un objet lit la même réponse pour
    // le nom de son forgeron.
    return ro::net::FrameOutcome::kIgnored;
  }
  if (const std::optional<ro::net::RankingList> list = ro::net::ReadRankingList(frame)) {
    for (const std::uint32_t character_id : list->character_ids) {
      if (character_id != 0 && !names_.contains(character_id)) {
        static_cast<void>(ro::net::RequestCreatorName(link, character_id));
      }
    }
    const bool quiet = quiet_.has_value() && quiet_->kind == list->kind &&
                       now_ms - quiet_->asked_ms < kQuietAnswerPatienceMs;
    quiet_.reset();
    latest_ = *list;
    ++list_serial_;
    pending_ = Pending{.list = *list, .received_ms = now_ms, .looks_done = 0, .quiet = quiet};
    // DÉJÀ CONNUS : le classement s'écrit tout de suite.
    if (AllNamesKnown(*list)) {
      if (!quiet) {
        Print(world, *list, now_ms);
      }
      pending_.reset();
    }
    return ro::net::FrameOutcome::kApplied;
  }
  if (const std::optional<ro::net::RankingPoints> points = ro::net::ReadRankingPoints(frame)) {
    if (const std::optional<RankNoticeLine> line = RankingPointsLine(*points)) {
      world.RecordChat(ro::sim::ChatKind::kServer, 0, line->text, now_ms, line->color);
    }
    return ro::net::FrameOutcome::kApplied;
  }
  if (const std::optional<std::uint8_t> subject = ro::net::ReadStarPlaceQuestion(frame)) {
    star_place_subject_ = *subject;
    ++star_place_serial_;
    return ro::net::FrameOutcome::kApplied;
  }
  if (const std::optional<ro::net::StarNotice> notice = ro::net::ReadStarNotice(frame)) {
    if (notice->code == StarNoticeCode::kTaekwonMission) {
      mission_.Start(notice->subject_id, notice->name, notice->star);
    }
    const ro::sim::Actor* player = world.player();
    const std::optional<StarNoticeOutcome> outcome =
        StarNoticeOutcomeOf(*notice, player != nullptr ? player->name : std::string());
    if (outcome.has_value()) {
      // L'AVIS SE POSE AUSSI EN HAUT DE L'ÉCRAN, dans un corps plus grand que
      // celui des annonces.
      ro::sim::BalloonNotice balloon{.family = ro::sim::BalloonFamily::kAnnouncement,
                                     .font_px = ro::sim::kStarBalloonFontPixels,
                                     .heard_ms = now_ms};
      balloon.lines.push_back(
          ro::sim::BalloonLine{.text = outcome->line.text, .color = outcome->line.color});
      world.balloon_news().Post(std::move(balloon));
      world.RecordChat(ro::sim::ChatKind::kServer, 0, outcome->line.text, now_ms,
                       outcome->line.color);
      if (outcome->effect_id != 0 && player != nullptr) {
        static_cast<void>(world.RecordEffect(world.player_id(), outcome->effect_id, now_ms));
      }
      if (!outcome->sound.empty()) {
        sounds_.push_back(outcome->sound);
      }
    }
    return ro::net::FrameOutcome::kApplied;
  }
  return ro::net::FrameOutcome::kUnhandled;
}

void RankNotices::Advance(ro::sim::World& world, std::uint32_t now_ms) {
  mission_.Advance(world, now_ms);
  if (!pending_.has_value()) {
    return;
  }
  const std::uint32_t looks_due = (now_ms - pending_->received_ms) / kNameLookMs;
  while (pending_->looks_done < looks_due) {
    ++pending_->looks_done;
    if (AllNamesKnown(pending_->list) || pending_->looks_done >= kNameLooks) {
      if (!pending_->quiet) {
        Print(world, pending_->list, now_ms);
      }
      pending_.reset();
      return;
    }
    if (!pending_->quiet) {
      world.RecordChat(ro::sim::ChatKind::kServer, 0, Tr("Récupération du classement…"), now_ms);
    }
  }
}

std::vector<std::string> RankNotices::TakeSounds() { return std::exchange(sounds_, {}); }

void RankNotices::AdvanceStarPlace(StarPlaceBox& box, ro::net::Connection& link) {
  // ⚠ LA RÉPONSE VISE LA QUESTION QUE LA BOÎTE MONTRAIT, qui est la dernière
  // reçue.
  if (const std::optional<bool> answer = std::exchange(box.answer, std::nullopt);
      answer.has_value() && star_place_subject_.has_value()) {
    if (*answer) {
      ro::net::AgreeStarPlace(link, *star_place_subject_);
    }
    star_place_subject_.reset();
    box.question.clear();
  }
  if (star_place_serial_ != star_place_asked_serial_ && star_place_subject_.has_value()) {
    star_place_asked_serial_ = star_place_serial_;
    box.ask = true;
    box.question = StarPlaceQuestion(*star_place_subject_);
  }
}

bool RankNotices::AllNamesKnown(const ro::net::RankingList& list) const {
  for (const std::uint32_t character_id : list.character_ids) {
    if (character_id != 0 && !names_.contains(character_id)) {
      return false;
    }
  }
  return true;
}

std::array<std::string, ro::net::kRankingSize> RankNotices::NamesOf(
    const ro::net::RankingList& list) const {
  std::array<std::string, ro::net::kRankingSize> names;
  for (std::size_t rank = 0; rank < ro::net::kRankingSize; ++rank) {
    if (const auto found = names_.find(list.character_ids[rank]); found != names_.end()) {
      names[rank] = found->second;
    }
  }
  return names;
}

void RankNotices::Print(ro::sim::World& world, const ro::net::RankingList& list,
                        std::uint32_t now_ms) const {
  for (RankNoticeLine& line : RankingLines(list, NamesOf(list))) {
    world.RecordChat(ro::sim::ChatKind::kServer, 0, std::move(line.text), now_ms, line.color);
  }
}

}  // namespace moonclient
