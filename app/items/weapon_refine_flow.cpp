#include "items/weapon_refine_flow.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <ctime>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/local_time.h"
#include "ro/core/log.h"
#include "ro/core/printed.h"
#include "ro/net/craft_session.h"
#include "ro/net/skill.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/inventory.h"
#include "ro/sim/world.h"
#include "ro/ui/skin_palette.h"
#include "ro/vfs/craft_words.h"
#include "ro/vfs/item_name.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::sim::RefineOutcome;
using ro::ui::WeaponRefineVerb;

// Upgrade Weapon.
constexpr std::uint16_t kWeaponRefineSkill = 477;
// Les trois minerais, dans l'ordre des niveaux d'arme : Phracon, Emveretarcon,
// Oridecon. Le quatrième niveau reprend l'Oridecon.
constexpr std::array<std::uint32_t, ro::ui::kRefineOreCount> kOres = {1010, 1011, 984};

// ── LES DÉLAIS ──────────────────────────────────────────────────────────────
// Entre un résultat et la relance du skill.
constexpr std::uint32_t kAutoRecastDelayMs = 400;
// Entre une liste et l'affinage automatique.
constexpr std::uint32_t kAutoRefineDelayMs = 350;
// Sans liste après une relance, celle-ci est tenue pour jetée.
constexpr std::uint32_t kRecastNoListMs = 1000;
constexpr int kMostRecastRetries = 3;
// L'anti-rafale d'une demande manuelle.
constexpr std::uint32_t kLeastSendIntervalMs = 400;
// Une tentative sans réponse cesse d'être attendue.
constexpr std::uint32_t kAwaitTimeoutMs = 10000;
constexpr std::size_t kMostHistoryLines = 200;

// Le type d'objet d'une arme sur le fil. La liste ne le porte pas : elle ne
// liste QUE des armes.
constexpr std::int32_t kWeaponItemType = 4;

// ── LES COULEURS DU FIL DE CONVERSATION, celles du client, en `0xRRGGBB` ──────
constexpr std::uint32_t kChatSucceededColor = 0x00FFFF;
constexpr std::uint32_t kChatFailedColor = 0x00CDCD;
constexpr std::uint32_t kChatRefusedColor = 0xFFC8C8;

constexpr std::size_t kTimeBytes = 16;

// ⚠ PAR SOUSTRACTION : l'horloge reboucle, et une échéance juste avant le
// rebouclage doit rester échue juste après.
[[nodiscard]] bool Due(std::uint32_t at, std::uint32_t now_ms) {
  return at != 0 && static_cast<std::int32_t>(now_ms - at) >= 0;
}

// Une échéance ; zéro voulant dire « rien d'armé », elle n'en vaut jamais zéro.
[[nodiscard]] std::uint32_t Deadline(std::uint32_t now_ms, std::uint32_t delay_ms) {
  const std::uint32_t at = now_ms + delay_ms;
  return at == 0 ? 1 : at;
}

[[nodiscard]] std::string ClockNow() {
  const std::time_t stamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::tm local{};
  if (!ro::core::ToLocalTime(stamp, local)) {
    return {};
  }
  std::array<char, kTimeBytes> text{};
  std::strftime(text.data(), text.size(), "%H:%M:%S", &local);
  return text.data();
}

// LE NOM D'UNE ARME DU SAC : ce que `vfs` sait composer, depuis ce que `sim`
// porte. ⚠ LE FORGERON N'EST PAS SUR LE FIL : `CreatorNames` le pose ensuite.
[[nodiscard]] ro::vfs::ItemNameFacts FactsOf(const ro::sim::InventoryItem& item) {
  static_assert(ro::sim::kItemCardSlots == ro::vfs::kItemNameCardSlots,
                "le fil porte quatre cartes des deux côtés de la frontière");
  ro::vfs::ItemNameFacts facts;
  facts.item_id = item.item_id;
  facts.type = item.type;
  facts.refine_level = item.refine_level;
  facts.grade = item.grade;
  facts.cards = item.cards;
  facts.identified = item.identified;
  return facts;
}

[[nodiscard]] int LearnedLevel(const ro::sim::World& world) {
  const ro::sim::KnownSkill* skill = world.skill_book().Find(kWeaponRefineSkill);
  return skill != nullptr ? skill->level : 0;
}

}  // namespace

WeaponRefineFlow::WeaponRefineFlow(const ro::vfs::ItemTable* items, const ro::vfs::ItemWords* words,
                                   ro::render::ItemIcons* icons)
    : items_(items), words_(words), icons_(icons) {}

// ── LA LECTURE DU SERVEUR ───────────────────────────────────────────────────

void WeaponRefineFlow::Refresh(ro::sim::World& world, ro::ui::WeaponRefineWindowState& state,
                               const WeaponRefineSettings& settings, std::uint32_t now_ms) {
  const ro::sim::CraftSession& craft = world.craft();
  if (craft.refine_list_seq() != seen_list_) {
    seen_list_ = craft.refine_list_seq();
    TakeList(world, state, settings, now_ms);
  }
  if (craft.refine_result_seq() != seen_result_) {
    seen_result_ = craft.refine_result_seq();
    TakeResult(world, settings, state.open, now_ms);
  }
  if (craft.refusal_seq() != seen_refusal_) {
    seen_refusal_ = craft.refusal_seq();
    if (craft.refused_skill() == kWeaponRefineSkill) {
      TakeRefusal(settings, state.open, now_ms);
    }
  }
  Compose(world, state, settings);
}

void WeaponRefineFlow::TakeList(ro::sim::World& world, ro::ui::WeaponRefineWindowState& state,
                                const WeaponRefineSettings& settings, std::uint32_t now_ms) {
  entries_ = world.craft().refine_list();
  empty_list_ = entries_.empty();
  session_armed_ = !entries_.empty();
  awaiting_ = false;
  consumed_ = false;
  // LE RÉSULTAT D'AVANT ne survit qu'aux listes que la chaîne a provoquées.
  if (auto_chain_ == 0) {
    last_result_.clear();
  }
  auto_recast_at_ = 0;
  recast_sent_at_ = 0;
  recast_retries_ = 0;
  if (empty_list_ && (auto_chain_ > 0 || auto_refine_count_ > 0)) {
    stop_reason_ = Tr("Plus aucune arme à refine : chaîne arrêtée.");
  }
  // LA SÉLECTION : la première arme d'une première liste ; ensuite, la même
  // place si elle y est encore — un succès garde la place, un échec l'efface —,
  // et aucune sinon.
  const bool first_list = state.selected == ro::ui::WeaponRefineWindowAction::kNoRefineRow;
  if (first_list) {
    state.selected =
        entries_.empty() ? ro::ui::WeaponRefineWindowAction::kNoRefineRow : entries_.front().index;
  } else if (EntryAt(state.selected) == nullptr) {
    state.selected = ro::ui::WeaponRefineWindowAction::kNoRefineRow;
  }
  state.open = true;
  state.raise = true;
  ScheduleAutoRefine(settings, world, true, now_ms);
}

void WeaponRefineFlow::TakeResult(ro::sim::World& world, const WeaponRefineSettings& settings,
                                  bool window_open, std::uint32_t now_ms) {
  const ro::sim::RefineResult& result = world.craft().refine_result();
  const bool ours = awaiting_;
  awaiting_ = false;

  // ── LE SUJET DU MESSAGE ─────────────────────────────────────────────────────
  // 🔴 POUR UN MINERAI MANQUANT, L'OBJET DE LA RÉPONSE EST LE MINERAI.
  std::string subject;
  if (result.outcome == RefineOutcome::kMissingOre) {
    subject = ItemName(result.item_id);
  } else {
    const ro::sim::InventoryItem* item =
        sent_index_ >= 0 ? world.inventory().Find(static_cast<std::uint16_t>(sent_index_))
                         : nullptr;
    std::string name;
    if (item != nullptr) {
      ro::vfs::ItemNameFacts facts = FactsOf(*item);
      facts.refine_level = 0;
      creators_.Fill(facts);
      if (items_ != nullptr && words_ != nullptr) {
        name = ro::vfs::ItemNameText(ro::vfs::ComposeItemName(facts, *items_, *words_));
      }
    }
    if (name.empty()) {
      name = sent_name_;
    }
    if (name.empty()) {
      name = ro::core::Printed("id %u", static_cast<unsigned>(result.item_id));
    }
    if (result.outcome == RefineOutcome::kSucceeded && sent_refine_ >= 0) {
      subject = ro::core::Printed("%s +%d -> +%d", name.c_str(), sent_refine_, sent_refine_ + 1);
    } else {
      subject = std::move(name);
    }
  }

  // ── LE TEXTE DU CLIENT, et sa couleur ──────────────────────────────────────
  const int outcome = static_cast<int>(result.outcome);
  const std::string_view pattern =
      words_ != nullptr ? ro::vfs::RefineResultTemplate(*words_, outcome) : std::string_view();
  std::string body;
  if (!ro::vfs::FillFirstString(pattern, subject, &body)) {
    body = pattern.empty() ? subject : std::string(pattern) + " [" + subject + "]";
  }
  ro::ui::SkinTone tone = ro::ui::SkinTone::kWarn;
  std::uint32_t chat_color = kChatRefusedColor;
  std::string prefix;
  switch (result.outcome) {
    case RefineOutcome::kSucceeded:
      prefix = Tr("Succès — ");
      tone = ro::ui::SkinTone::kGood;
      chat_color = kChatSucceededColor;
      break;
    case RefineOutcome::kFailed:
      prefix = Tr("ÉCHEC — arme détruite — ");
      tone = ro::ui::SkinTone::kBad;
      chat_color = kChatFailedColor;
      break;
    case RefineOutcome::kSkillTooLow:
    case RefineOutcome::kMissingOre:
      break;
  }
  // LE FIL DE CONVERSATION REÇOIT LA PHRASE DU CLIENT, telle quelle : c'est ce
  // qu'il écrit, avec l'allure des « objets » mais dans la case générale.
  if (!pattern.empty()) {
    world.RecordChat(ro::sim::ChatKind::kItem, ro::sim::ChatType::kPublic, 0, body, now_ms,
                     chat_color);
  }
  last_result_ = prefix + body;
  last_result_tone_ = tone;
  Log(last_result_, tone);
  if (ours) {
    ScheduleAutoRecast(result.outcome, settings, world, window_open, now_ms);
  }
}

void WeaponRefineFlow::TakeRefusal(const WeaponRefineSettings& settings, bool window_open,
                                   std::uint32_t now_ms) {
  if (awaiting_) {
    // C'EST LA TENTATIVE QUI EST REFUSÉE : arme non affinable, coût absent de la
    // table du serveur, échange en cours. Rien n'a été consommé.
    awaiting_ = false;
    Log(Tr("Tentative refusée par le serveur (aucun minerai consommé)."), ro::ui::SkinTone::kWarn);
    auto_recast_at_ = 0;
    stop_reason_ = Tr("Le serveur a refusé la tentative : relance arrêtée.");
    return;
  }
  // SINON C'EST NOTRE RELANCE qui est tombée, jetée par le délai de lancement.
  RetryRecast(settings, window_open, now_ms);
}

// ── LA CHAÎNE ───────────────────────────────────────────────────────────────

const char* WeaponRefineFlow::AutoStopCause(const ro::sim::World& world) const {
  const ro::sim::Inventory& bag = world.inventory();
  const bool no_ore =
      std::ranges::all_of(kOres, [&](std::uint32_t ore) { return bag.AmountOf(ore) <= 0; });
  if (no_ore) {
    return Tr("Plus aucun minerai : chaîne arrêtée.");
  }
  // UN COÛT INCONNU LAISSE PASSER : mieux vaut une relance que le serveur
  // refuse qu'une chaîne qui s'arrête sur une supposition.
  const ro::sim::KnownSkill* skill = world.skill_book().Find(kWeaponRefineSkill);
  const std::int64_t sp = world.player_status().sp;
  if (skill != nullptr && skill->sp_cost > 0 && sp != ro::sim::kUnknownStat &&
      sp < skill->sp_cost) {
    return Tr("Plus assez de SP pour relancer le skill : chaîne arrêtée.");
  }
  return nullptr;
}

void WeaponRefineFlow::ScheduleAutoRecast(RefineOutcome outcome,
                                          const WeaponRefineSettings& settings,
                                          const ro::sim::World& world, bool window_open,
                                          std::uint32_t now_ms) {
  auto_recast_at_ = 0;
  if (!settings.chains() || paused_ || !window_open) {
    return;
  }
  if (outcome != RefineOutcome::kSucceeded && outcome != RefineOutcome::kFailed) {
    stop_reason_ = Tr("Le serveur a refusé la condition : chaîne arrêtée.");
    return;
  }
  if (const char* cause = AutoStopCause(world); cause != nullptr) {
    stop_reason_ = cause;
    return;
  }
  stop_reason_ = nullptr;
  ++auto_chain_;
  auto_recast_at_ = Deadline(now_ms, kAutoRecastDelayMs);
}

void WeaponRefineFlow::ScheduleAutoRefine(const WeaponRefineSettings& settings,
                                          const ro::sim::World& world, bool window_open,
                                          std::uint32_t now_ms) {
  auto_refine_at_ = 0;
  if (!settings.auto_refine || !chain_consent_ || paused_ || !window_open || entries_.empty() ||
      consumed_ || awaiting_) {
    return;
  }
  if (const char* cause = AutoStopCause(world); cause != nullptr) {
    stop_reason_ = cause;
    return;
  }
  auto_refine_at_ = Deadline(now_ms, kAutoRefineDelayMs);
}

void WeaponRefineFlow::RetryRecast(const WeaponRefineSettings& settings, bool window_open,
                                   std::uint32_t now_ms) {
  recast_sent_at_ = 0;
  if (!settings.chains() || paused_ || !window_open || !consumed_ || pending_ != Pending::kNone) {
    return;
  }
  if (++recast_retries_ > kMostRecastRetries) {
    stop_reason_ = Tr("Le skill ne repart pas (délai de lancement) : relance arrêtée.");
    Log(stop_reason_, ro::ui::SkinTone::kWarn);
    return;
  }
  auto_recast_at_ = Deadline(now_ms, kAutoRecastDelayMs);
  stop_reason_ = nullptr;
  Log(ro::core::Printed(Tr("Relance jetée par le délai de lancement : nouvel essai (%d/%d)."),
                        recast_retries_, kMostRecastRetries),
      ro::ui::SkinTone::kWarn);
}

void WeaponRefineFlow::Tick(const ro::ui::WeaponRefineWindowAction& asked,
                            ro::ui::WeaponRefineWindowState& state,
                            const WeaponRefineSettings& settings, const ro::sim::World& world,
                            std::uint32_t now_ms) {
  if (awaiting_ && static_cast<std::uint32_t>(now_ms - awaiting_since_) > kAwaitTimeoutMs) {
    awaiting_ = false;
  }
  if (Due(auto_recast_at_, now_ms)) {
    auto_recast_at_ = 0;
    if (settings.chains() && !paused_ && state.open && consumed_ && pending_ == Pending::kNone) {
      if (const char* cause = AutoStopCause(world); cause != nullptr) {
        stop_reason_ = cause;
      } else {
        pending_ = Pending::kRecast;
      }
    }
  }
  if (Due(auto_refine_at_, now_ms)) {
    const bool cancelled = !settings.auto_refine || !chain_consent_ || paused_ || !state.open ||
                           consumed_ || awaiting_ || pending_ != Pending::kNone ||
                           state.confirm_index != ro::ui::WeaponRefineWindowAction::kNoRefineRow ||
                           entries_.empty();
    if (cancelled) {
      auto_refine_at_ = 0;
    } else if (asked.list_drawn) {
      // ⚠ RIEN NE PART AVANT QUE LA LISTE N'AIT ÉTÉ MONTRÉE : la cible est ce que
      // le joueur voit, filtre et tri compris.
      auto_refine_at_ = 0;
      if (const char* cause = AutoStopCause(world); cause != nullptr) {
        stop_reason_ = cause;
      } else {
        const std::int32_t target = asked.selection_visible ? state.selected : asked.first_visible;
        if (target < 0) {
          stop_reason_ = Tr("Aucune arme ne correspond au filtre : chaîne arrêtée.");
        } else {
          state.selected = target;
          pending_ = Pending::kRefine;
          pending_index_ = target;
          last_send_ms_ = now_ms;
          stop_reason_ = nullptr;
          ++auto_refine_count_;
        }
      }
    }
  }
  if (recast_sent_at_ != 0 &&
      static_cast<std::uint32_t>(now_ms - recast_sent_at_) > kRecastNoListMs) {
    RetryRecast(settings, state.open, now_ms);
  }
}

// ── LES GESTES ──────────────────────────────────────────────────────────────

void WeaponRefineFlow::RequestRefine(std::int32_t index, ro::ui::WeaponRefineWindowState& state,
                                     const WeaponRefineSettings& settings, std::uint32_t now_ms) {
  if (index < 0 || awaiting_ || consumed_ || pending_ != Pending::kNone ||
      state.confirm_index != ro::ui::WeaponRefineWindowAction::kNoRefineRow ||
      static_cast<std::uint32_t>(now_ms - last_send_ms_) <= kLeastSendIntervalMs) {
    return;
  }
  last_send_ms_ = now_ms;
  // UN GESTE DU JOUEUR LÈVE L'ARRÊT de la chaîne.
  paused_ = false;
  if (settings.confirm) {
    state.confirm_index = index;
    state.confirm_opened = true;
    return;
  }
  pending_ = Pending::kRefine;
  pending_index_ = index;
}

void WeaponRefineFlow::RequestClose(ro::ui::WeaponRefineWindowState& state) {
  pending_ = Pending::kCancel;
  state.open = false;
  state.selected = ro::ui::WeaponRefineWindowAction::kNoRefineRow;
  state.confirm_index = ro::ui::WeaponRefineWindowAction::kNoRefineRow;
  auto_refine_at_ = 0;
  auto_recast_at_ = 0;
}

void WeaponRefineFlow::Apply(const ro::ui::WeaponRefineWindowAction& asked,
                             ro::ui::WeaponRefineWindowState& state,
                             const WeaponRefineSettings& settings, ro::net::Connection& link,
                             ro::sim::World& world, std::uint32_t now_ms) {
  switch (asked.verb) {
    case WeaponRefineVerb::kNothing:
      break;
    case WeaponRefineVerb::kRefine:
      RequestRefine(asked.index, state, settings, now_ms);
      break;
    case WeaponRefineVerb::kConfirm:
      if (state.confirm_index != ro::ui::WeaponRefineWindowAction::kNoRefineRow) {
        pending_ = Pending::kRefine;
        pending_index_ = state.confirm_index;
      }
      state.confirm_index = ro::ui::WeaponRefineWindowAction::kNoRefineRow;
      break;
    case WeaponRefineVerb::kDismissConfirm:
      state.confirm_index = ro::ui::WeaponRefineWindowAction::kNoRefineRow;
      break;
    case WeaponRefineVerb::kRecast:
      pending_ = Pending::kRecast;
      auto_chain_ = 0;
      auto_recast_at_ = 0;
      stop_reason_ = nullptr;
      recast_retries_ = 0;
      paused_ = false;
      break;
    case WeaponRefineVerb::kStartChain: {
      chain_consent_ = true;
      paused_ = false;
      stop_reason_ = nullptr;
      // ⚠ LE RÉGLAGE NE SE COCHE QU'APRÈS CETTE IMAGE, par la partie : la chaîne
      // se programme donc comme s'il l'était déjà.
      WeaponRefineSettings chained = settings;
      chained.auto_refine = true;
      ScheduleAutoRefine(chained, world, state.open, now_ms);
      Log(Tr("Chaîne automatique lancée."), ro::ui::SkinTone::kWarn);
      break;
    }
    case WeaponRefineVerb::kStop:
      chain_consent_ = false;
      paused_ = true;
      auto_refine_at_ = 0;
      auto_recast_at_ = 0;
      if (pending_ == Pending::kRefine || pending_ == Pending::kRecast) {
        pending_ = Pending::kNone;
      }
      stop_reason_ = nullptr;
      Log(Tr("Chaîne automatique arrêtée à la demande."), ro::ui::SkinTone::kWarn);
      break;
    case WeaponRefineVerb::kClose:
      RequestClose(state);
      break;
  }
  // ⚠ LA CIBLE DE LA CONFIRMATION PEUT AVOIR QUITTÉ LA LISTE entre-temps.
  if (state.confirm_index != ro::ui::WeaponRefineWindowAction::kNoRefineRow &&
      EntryAt(state.confirm_index) == nullptr) {
    state.confirm_index = ro::ui::WeaponRefineWindowAction::kNoRefineRow;
  }
  Tick(asked, state, settings, world, now_ms);
  Flush(state, link, world, now_ms);
}

void WeaponRefineFlow::Flush(ro::ui::WeaponRefineWindowState& state, ro::net::Connection& link,
                             ro::sim::World& world, std::uint32_t now_ms) {
  const Pending pending = std::exchange(pending_, Pending::kNone);
  switch (pending) {
    case Pending::kNone:
      return;
    case Pending::kRefine: {
      const ro::sim::RefinableWeapon* weapon = EntryAt(pending_index_);
      sent_index_ = pending_index_;
      sent_name_ = weapon != nullptr ? WeaponName(world, *weapon, false) : std::string();
      sent_refine_ = weapon != nullptr ? weapon->refine : -1;
      if (auto sent = ro::net::RequestWeaponRefine(link, pending_index_); !sent) {
        ro::core::LogWarning("affinage : demande non émise — {}", ro::core::Describe(sent.error()));
        return;
      }
      awaiting_ = true;
      awaiting_since_ = now_ms;
      consumed_ = true;
      session_armed_ = false;
      return;
    }
    case Pending::kCancel:
      // 🔴 L'ANNULATION SEULEMENT SI LE SERVEUR ATTEND : un moins un en trop est
      // ignoré, un moins un manquant bloque le personnage.
      if (session_armed_) {
        if (auto sent = ro::net::RequestWeaponRefine(link, ro::net::kWeaponRefineCancel); !sent) {
          ro::core::LogWarning("affinage : annulation non émise — {}",
                               ro::core::Describe(sent.error()));
        }
        session_armed_ = false;
      }
      ResetSession(state);
      return;
    case Pending::kRecast: {
      const std::uint32_t self = world.player_id();
      if (self == 0) {
        return;
      }
      const auto level = static_cast<std::uint16_t>(std::max(1, LearnedLevel(world)));
      if (auto sent = ro::net::RequestSkillOnTarget(link, kWeaponRefineSkill, level, self); !sent) {
        ro::core::LogWarning("affinage : relance non émise — {}", ro::core::Describe(sent.error()));
        return;
      }
      recast_sent_at_ = now_ms == 0 ? 1 : now_ms;
      return;
    }
  }
}

void WeaponRefineFlow::CloseForOtherCraft(ro::ui::WeaponRefineWindowState& state,
                                          ro::net::Connection& link) {
  if (!state.open && !session_armed_) {
    return;
  }
  if (session_armed_) {
    if (auto sent = ro::net::RequestWeaponRefine(link, ro::net::kWeaponRefineCancel); !sent) {
      ro::core::LogWarning("affinage : annulation non émise — {}",
                           ro::core::Describe(sent.error()));
    }
    session_armed_ = false;
  }
  Log(Tr("Session de refine abandonnée : une fabrication a été lancée (le serveur n'en garde "
         "qu'une)."),
      ro::ui::SkinTone::kWarn);
  ResetSession(state);
}

void WeaponRefineFlow::ResetSession(ro::ui::WeaponRefineWindowState& state) {
  entries_.clear();
  last_result_.clear();
  state.open = false;
  state.selected = ro::ui::WeaponRefineWindowAction::kNoRefineRow;
  state.confirm_index = ro::ui::WeaponRefineWindowAction::kNoRefineRow;
  state.filter[0] = '\0';
  consumed_ = false;
  awaiting_ = false;
  empty_list_ = false;
  pending_ = Pending::kNone;
  auto_recast_at_ = 0;
  auto_chain_ = 0;
  stop_reason_ = nullptr;
  auto_refine_at_ = 0;
  auto_refine_count_ = 0;
  paused_ = false;
  chain_consent_ = false;
  recast_sent_at_ = 0;
  recast_retries_ = 0;
}

void WeaponRefineFlow::Log(std::string text, ro::ui::SkinTone tone) {
  history_.push_back(LogLine{.text = std::move(text), .tone = tone, .time = ClockNow()});
  while (history_.size() > kMostHistoryLines) {
    history_.pop_front();
  }
}

// ── LES NOMS ────────────────────────────────────────────────────────────────

const ro::sim::RefinableWeapon* WeaponRefineFlow::EntryAt(std::int32_t index) const {
  const auto found = std::ranges::find_if(
      entries_, [&](const ro::sim::RefinableWeapon& weapon) { return weapon.index == index; });
  return found == entries_.end() ? nullptr : &*found;
}

std::string WeaponRefineFlow::WeaponName(const ro::sim::World& world,
                                         const ro::sim::RefinableWeapon& weapon, bool bare) const {
  if (items_ == nullptr || words_ == nullptr) {
    return ItemName(weapon.item_id);
  }
  // LE SAC D'ABORD : il connaît le type et l'identification. La liste sinon —
  // elle ne liste que des armes.
  ro::vfs::ItemNameFacts facts;
  if (const ro::sim::InventoryItem* item =
          world.inventory().Find(static_cast<std::uint16_t>(weapon.index));
      item != nullptr && item->item_id == weapon.item_id) {
    facts = FactsOf(*item);
  } else {
    facts.item_id = weapon.item_id;
    facts.type = kWeaponItemType;
    facts.cards = weapon.cards;
  }
  facts.refine_level = bare ? 0 : weapon.refine;
  creators_.Fill(facts);
  std::string name = ro::vfs::ItemNameText(ro::vfs::ComposeItemName(facts, *items_, *words_));
  return name.empty() ? ItemName(weapon.item_id) : name;
}

std::string WeaponRefineFlow::ItemName(std::uint32_t item_id) const {
  const ro::vfs::ItemEntry* entry = items_ != nullptr ? items_->Find(item_id) : nullptr;
  if (entry != nullptr && !entry->name.empty()) {
    return entry->name;
  }
  return {};
}

ro::ui::SkinImage WeaponRefineFlow::IconOf(std::uint32_t item_id) const {
  if (icons_ == nullptr) {
    return {};
  }
  const ro::render::IconTexture& icon = icons_->Get(item_id);
  return ro::ui::SkinImage{
      .id = icon.usable() ? static_cast<ImTextureID>(icon.texture.name()) : ImTextureID{0},
      .width = static_cast<float>(icon.width_px),
      .height = static_cast<float>(icon.height_px)};
}

int WeaponRefineFlow::SlotsOf(std::uint32_t item_id) const {
  const ro::vfs::ItemEntry* entry = items_ != nullptr ? items_->Find(item_id) : nullptr;
  return entry != nullptr ? entry->card_slots : 0;
}

// ── LA VUE ──────────────────────────────────────────────────────────────────

void WeaponRefineFlow::Compose(const ro::sim::World& world,
                               const ro::ui::WeaponRefineWindowState& state,
                               const WeaponRefineSettings& settings) {
  view_ = ro::ui::WeaponRefineView{};
  const int cap = LearnedLevel(world);
  bare_names_.clear();
  full_names_.clear();
  rows_.clear();
  for (const ro::sim::RefinableWeapon& weapon : entries_) {
    bare_names_.push_back(WeaponName(world, weapon, true));
    full_names_.push_back(WeaponName(world, weapon, false));
    ro::sim::InventoryItem probe;
    probe.item_id = weapon.item_id;
    probe.cards = weapon.cards;
    const int slots = SlotsOf(weapon.item_id);
    rows_.push_back(
        ro::ui::WeaponRefineRowView{.index = weapon.index,
                                    .item_id = weapon.item_id,
                                    .refine = weapon.refine,
                                    .icon = IconOf(weapon.item_id),
                                    .slots = slots,
                                    .cards = static_cast<int>(ro::sim::ItemCardsInSlots(
                                        probe, static_cast<std::size_t>(std::max(slots, 0)))),
                                    .at_cap = cap > 0 && weapon.refine + 1 >= cap});
  }
  for (std::size_t rank = 0; rank < rows_.size(); ++rank) {
    rows_[rank].bare_name = bare_names_[rank];
    rows_[rank].full_name = full_names_[rank];
  }
  view_.rows = rows_;

  for (std::size_t rank = 0; rank < kOres.size(); ++rank) {
    ore_names_[rank] = ItemName(kOres[rank]);
    view_.ores[rank] = ro::ui::RefineOreView{.item_id = kOres[rank],
                                             .name = ore_names_[rank],
                                             .icon = IconOf(kOres[rank]),
                                             .owned = world.inventory().AmountOf(kOres[rank])};
  }
  view_.cap = cap;

  // LA CHANCE EXACTE de l'arme choisie, cherchée dans la liste entière.
  if (const ro::sim::RefinableWeapon* aimed = EntryAt(state.selected);
      aimed != nullptr && book_ != nullptr) {
    const int rate = book_->RefineRate(book_->WeaponLevel(aimed->item_id), aimed->refine + 1);
    const std::int64_t job_level = world.player_status().job_level;
    if (rate >= 0 && job_level != ro::sim::kUnknownStat) {
      view_.chance = RefineChancePercent(rate, static_cast<int>(job_level));
    }
  }

  view_.empty_list = empty_list_;
  view_.consumed = consumed_;
  view_.awaiting = awaiting_;
  view_.relaunch_coming = auto_recast_at_ != 0 || pending_ == Pending::kRecast;
  view_.chain_armed = settings.auto_refine && chain_consent_;
  view_.chain_running = view_.chain_armed && !paused_ &&
                        (auto_refine_at_ != 0 || auto_recast_at_ != 0 || awaiting_ ||
                         pending_ == Pending::kRefine || pending_ == Pending::kRecast);
  if (auto_refine_at_ != 0) {
    view_.chain_note = ro::ui::RefineChainNote::kRefineSoon;
    view_.chain_count = auto_refine_count_ + 1;
  } else if (auto_recast_at_ != 0) {
    view_.chain_note = settings.auto_refine ? ro::ui::RefineChainNote::kChaining
                                            : ro::ui::RefineChainNote::kRecasting;
    view_.chain_count = auto_chain_;
  } else if (paused_) {
    view_.chain_note = ro::ui::RefineChainNote::kPaused;
  } else if (settings.auto_refine && !chain_consent_ && !entries_.empty() && !consumed_) {
    view_.chain_note = ro::ui::RefineChainNote::kReady;
  } else if (stop_reason_ != nullptr && settings.chains()) {
    view_.chain_note = ro::ui::RefineChainNote::kStopped;
    view_.stop_reason = stop_reason_;
  }
  view_.last_result = last_result_;
  view_.last_result_tone = last_result_tone_;

  history_view_.clear();
  for (const LogLine& line : history_) {
    history_view_.push_back(
        ro::ui::CraftLogLineView{.text = line.text, .tone = line.tone, .time = line.time});
  }
  view_.history = history_view_;

  if (const ro::sim::RefinableWeapon* aimed = EntryAt(state.confirm_index); aimed != nullptr) {
    confirm_name_ = WeaponName(world, *aimed, false);
    view_.confirm_name = confirm_name_;
    view_.confirm_refine = aimed->refine;
  }

  view_.show_cards = settings.show_cards;
  view_.show_filter = settings.show_filter;
  view_.show_history = settings.show_history;
  view_.log_time = settings.log_time;
  view_.enter_key = settings.enter_key;
  view_.hover_preview = settings.hover_preview;
}

}  // namespace moonclient
