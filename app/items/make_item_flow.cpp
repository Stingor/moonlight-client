// LA FABRICATION : ce que la partie fait des listes du serveur. La vue se
// compose à part, dans `make_item_compose.cpp`.

#include "items/make_item_flow.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/net/inventory.h"
#include "ro/net/skill.h"
#include "ro/sim/inventory.h"
#include "ro/sim/world.h"
#include "ro/ui/skin_palette.h"
#include "ro/vfs/craft_words.h"

#include "items/make_item_odds.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::sim::MakeListKind;

// ── LES DÉLAIS ──────────────────────────────────────────────────────────────
// Un skill ou un objet envoyé depuis moins que ceci a ouvert la liste.
constexpr std::uint32_t kTriggerWindowMs = 3000;
// Un refus de skill arrivé moins que ceci après la demande la concerne.
constexpr std::uint32_t kRefusalWindowMs = 3000;
// L'anti-rafale d'une demande.
constexpr std::uint32_t kLeastSendIntervalMs = 300;
// Au-delà, la sonde du sac peut conclure.
constexpr std::uint32_t kProbeAfterMs = 300;
// Au-delà, plus rien n'est attendu.
constexpr std::uint32_t kAwaitTimeoutMs = 6000;
// Entre un résultat et la relance.
constexpr std::uint32_t kAutoRelaunchDelayMs = 300;
// Sans liste après une relance, celle-ci est tenue pour jetée.
constexpr std::uint32_t kRelaunchNoListMs = 1000;
constexpr int kMostRelaunchRetries = 3;
constexpr std::size_t kMostHistoryLines = 200;

constexpr std::size_t kTimeBytes = 16;
constexpr std::size_t kLineBytes = 512;

[[nodiscard]] bool Due(std::uint32_t at, std::uint32_t now_ms) {
  return at != 0 && static_cast<std::int32_t>(now_ms - at) >= 0;
}

[[nodiscard]] std::uint32_t Deadline(std::uint32_t now_ms, std::uint32_t delay_ms) {
  const std::uint32_t at = now_ms + delay_ms;
  return at == 0 ? 1 : at;
}

[[nodiscard]] bool Recent(const ro::sim::CraftTrigger& trigger, std::uint32_t now_ms) {
  return trigger.seq != 0 && static_cast<std::uint32_t>(now_ms - trigger.at_ms) <= kTriggerWindowMs;
}

[[nodiscard]] std::string ClockNow() {
  const std::time_t stamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::tm local{};
#ifdef _WIN32
  if (localtime_s(&local, &stamp) != 0) {
    return {};
  }
#else
  if (localtime_r(&stamp, &local) == nullptr) {
    return {};
  }
#endif
  std::array<char, kTimeBytes> text{};
  std::strftime(text.data(), text.size(), "%H:%M:%S", &local);
  return text.data();
}

[[nodiscard]] int Stock(const ro::sim::World& world, std::uint32_t item_id) {
  int stock = 0;
  for (const ro::sim::InventoryItem& item : world.inventory().items()) {
    if (item.item_id == item_id) {
      stock += item.amount;
    }
  }
  return stock;
}

}  // namespace

MakeItemFlow::MakeItemFlow(const ro::vfs::ItemTable* items, const ro::vfs::ItemWords* words,
                           ro::render::ItemIcons* icons)
    : items_(items), words_(words), icons_(icons) {}

// ── LA LECTURE DU SERVEUR ───────────────────────────────────────────────────

bool MakeItemFlow::Refresh(ro::sim::World& world, ro::ui::MakeItemWindowState& state,
                           const MakeItemSettings& settings, std::uint32_t now_ms) {
  const ro::sim::CraftSession& craft = world.craft();
  bool list_arrived = false;
  if (craft.cook_mastery_seq() != seen_mastery_) {
    seen_mastery_ = craft.cook_mastery_seq();
    TakeMastery(world);
  }
  if (craft.make_list_seq() != seen_list_) {
    seen_list_ = craft.make_list_seq();
    TakeList(world, state, settings, now_ms);
    list_arrived = true;
  }
  if (craft.make_result_seq() != seen_result_) {
    seen_result_ = craft.make_result_seq();
    TakeResult(world, settings, now_ms);
  }
  if (craft.refusal_seq() != seen_refusal_) {
    seen_refusal_ = craft.refusal_seq();
    TakeRefusal(now_ms);
  }
  Probe(world, settings, now_ms);
  if (state.open) {
    RecountOwned(world);
  }
  Compose(world, state, settings);
  return list_arrived;
}

void MakeItemFlow::TakeMastery(const ro::sim::World& world) {
  const int told = world.craft().cook_mastery();
  const int before = std::exchange(cook_mastery_, told);
  // LE PREMIER ENVOI NE SE DIT PAS : c'est l'état, pas un changement.
  if (before < 0 || before == told) {
    return;
  }
  std::array<char, kLineBytes> line{};
  std::snprintf(line.data(), line.size(), Tr("Maîtrise culinaire : %d -> %d."), before, told);
  Log(line.data(), told > before ? ro::ui::SkinTone::kGood : ro::ui::SkinTone::kWarn);
}

void MakeItemFlow::TakeList(ro::sim::World& world, ro::ui::MakeItemWindowState& state,
                            const MakeItemSettings& settings, std::uint32_t now_ms) {
  static_cast<void>(settings);
  const ro::sim::MakeList& list = world.craft().make_list();
  const ro::sim::CraftSession& craft = world.craft();
  // 🔴 UNE LISTE DE CUISINE VIDE ARME QUAND MÊME le serveur.
  const bool arms_when_empty =
      list.kind == MakeListKind::kMaking && list.make_type == ro::sim::kCookingMakeType;

  // UNE LISTE VIDE SUR UNE SESSION ENCORE ARMÉE ne remplace rien : le serveur
  // n'en garde qu'une, et celle qu'on montre reste valable.
  if (list.item_ids.empty() && list_armed_ && !arms_when_empty) {
    std::array<char, kLineBytes> line{};
    if (Recent(craft.last_use(), now_ms)) {
      const std::string used = ItemName(craft.last_use_item());
      std::snprintf(line.data(), line.size(),
                    Tr("%s consommé sans effet : une session de fabrication est déjà ouverte, le "
                       "serveur n'en garde qu'une. Terminez-la ou fermez-la d'abord."),
                    used.empty() ? Tr("Item") : used.c_str());
    } else {
      std::snprintf(line.data(), line.size(), "%s",
                    Tr("Aucun produit proposé : une session de fabrication est déjà ouverte (le "
                       "serveur n'en garde qu'une). La liste affichée reste valable."));
    }
    Log(line.data(), ro::ui::SkinTone::kWarn);
    return;
  }

  if (list.item_ids.empty() && !entries_.empty()) {
    stale_entries_ = entries_;
  } else {
    stale_entries_.clear();
  }
  kind_ = list.kind;
  make_type_ = list.make_type;

  // LES DOUBLONS DU SERVEUR ne font qu'une ligne.
  entries_.clear();
  for (const std::uint32_t item_id : list.item_ids) {
    if (EntryOf(item_id) == nullptr) {
      entries_.push_back(Entry{.item_id = item_id});
    }
  }

  // ── CE QUI A OUVERT LA LISTE ────────────────────────────────────────────────
  const ro::sim::CraftTrigger& cast = craft.last_cast();
  bool cast_opened = false;
  if (Recent(cast, now_ms) && book_ != nullptr) {
    cast_opened =
        cast.id == kMakingArrowSkill || std::ranges::any_of(entries_, [&](const Entry& entry) {
          const CraftRecipe* recipe = book_->RecipeOf(entry.item_id);
          return recipe != nullptr && recipe->skill_id == cast.id;
        });
  } else if (Recent(cast, now_ms)) {
    cast_opened = true;
  }
  from_item_ = !cast_opened;
  skill_id_ = from_item_ ? 0 : cast.id;
  skill_level_ = from_item_ ? 0 : std::max<int>(1, cast.level);
  arrow_list_ = !from_item_ && skill_id_ == kMakingArrowSkill;

  for (Entry& entry : entries_) {
    entry.name = ItemName(entry.item_id);
    if (entry.name.empty()) {
      std::array<char, kLineBytes> numbered{};
      std::snprintf(numbered.data(), numbered.size(), "#%u", static_cast<unsigned>(entry.item_id));
      entry.name = numbered.data();
    }
    ResolveMaterials(entry);
  }
  RecountOwned(world);

  empty_list_ = list.item_ids.empty();
  awaiting_ = false;
  if (!auto_ours_) {
    last_result_.clear();
  }
  auto_recast_at_ = 0;
  relaunch_sent_at_ = 0;
  relaunch_retries_ = 0;
  reuse_owned_before_ = -1;

  // LES EMPLACEMENTS DE FORGE dont l'objet a quitté le sac se vident.
  for (std::size_t slot = 0; slot < forge_slots_.size(); ++slot) {
    const std::uint32_t item_id = forge_slots_[slot];
    if (item_id == 0 || Stock(world, item_id) > 0) {
      continue;
    }
    forge_slots_[slot] = 0;
    std::array<char, kLineBytes> line{};
    const std::string name = ItemName(item_id);
    if (name.empty()) {
      std::snprintf(line.data(), line.size(),
                    Tr("Emplacement %d vidé : plus d'item #%u en réserve."),
                    static_cast<int>(slot + 1), static_cast<unsigned>(item_id));
    } else {
      std::snprintf(line.data(), line.size(), Tr("Emplacement %d vidé : plus de %s en réserve."),
                    static_cast<int>(slot + 1), name.c_str());
    }
    Log(line.data(), ro::ui::SkinTone::kWarn);
  }

  if (from_item_ && Recent(craft.last_use(), now_ms) && craft.last_use_item() != 0) {
    source_item_id_ = craft.last_use_item();
    source_item_name_ = ItemName(source_item_id_);
    if (source_item_name_.empty()) {
      source_item_name_ = Tr("cet item");
    }
  } else {
    source_item_id_ = 0;
    source_item_name_.clear();
  }

  if (auto_ours_) {
    auto_ours_ = false;
    if (batch_left_ > 0 && !empty_list_) {
      batch_fire_ = true;
    }
  } else {
    // UNE LISTE OUVERTE À LA MAIN recommence tout.
    auto_chain_ = 0;
    auto_items_used_ = 0;
    batch_left_ = 0;
    batch_fire_ = false;
  }
  if (empty_list_) {
    batch_left_ = 0;
    if (auto_chain_ > 0) {
      stop_reason_ = Tr("Plus rien à fabriquer : relance arrêtée.");
    }
  } else {
    stop_reason_.clear();
  }

  // LA SÉLECTION : la première ligne d'une première liste ; ensuite le même
  // produit s'il y est encore, et aucun sinon.
  if (state.selected == 0) {
    state.selected = entries_.empty() ? 0 : entries_.front().item_id;
  } else if (EntryOf(state.selected) == nullptr) {
    state.selected = 0;
  }
  list_armed_ = !empty_list_ || arms_when_empty;
  state.open = true;
  state.raise = true;
}

void MakeItemFlow::TakeResult(ro::sim::World& world, const MakeItemSettings& settings,
                              std::uint32_t now_ms) {
  const ro::sim::MakeResult& result = world.craft().make_result();
  const bool ours = awaiting_;
  awaiting_ = false;
  sent_owned_ = -1;
  sent_material_ = 0;
  sent_material_owned_ = -1;
  if (ours && batch_left_ > 0) {
    // UN ÉCHEC EST AUSSI UN TOUR de la série.
    --batch_left_;
  }
  // 🔴 PAIR : RÉUSSITE, les codes allant par paires de métier.
  const bool succeeded = result.succeeded();
  std::string name = ItemName(result.item_id);
  if (name.empty() && result.item_id == last_sent_id_) {
    name = last_sent_name_;
  }
  if (name.empty()) {
    std::array<char, kLineBytes> numbered{};
    std::snprintf(numbered.data(), numbered.size(), "#%u", static_cast<unsigned>(result.item_id));
    name = numbered.data();
  }
  const std::string_view pattern =
      words_ != nullptr ? ro::vfs::MakeResultTemplate(*words_, succeeded) : std::string_view();
  std::string said;
  if (!ro::vfs::FillFirstString(pattern, name, &said)) {
    if (pattern.empty()) {
      said = std::string(succeeded ? Tr("Succès : ") : Tr("Échec : ")) + name;
    } else {
      said = std::string(pattern) + " (" + name + ")";
    }
  }
  last_result_ = said;
  last_result_tone_ = succeeded ? ro::ui::SkinTone::kGood : ro::ui::SkinTone::kBad;
  Log(said, last_result_tone_);
  RecountOwned(world);
  if (ours) {
    ScheduleAutoRecast(settings, world, now_ms);
  }
}

void MakeItemFlow::TakeRefusal(std::uint32_t now_ms) {
  if (!awaiting_ || sent_at_ == 0 ||
      static_cast<std::uint32_t>(now_ms - sent_at_) > kRefusalWindowMs) {
    return;
  }
  awaiting_ = false;
  Log(Tr("Demande refusée par le serveur."), ro::ui::SkinTone::kWarn);
  auto_recast_at_ = 0;
  batch_left_ = 0;
  batch_fire_ = false;
  if (auto_chain_ > 0) {
    stop_reason_ = Tr("Refus serveur : relance arrêtée.");
  }
}

// 🔴 LE SERVEUR SE TAIT pour les flèches, les convertisseurs, la forge et la
// pharmacie : le sac seul dit ce qui s'est passé.
void MakeItemFlow::Probe(const ro::sim::World& world, const MakeItemSettings& settings,
                         std::uint32_t now_ms) {
  if (!awaiting_ || sent_owned_ < 0 || last_sent_id_ == 0) {
    return;
  }
  const std::uint32_t elapsed = now_ms - sent_at_;
  if (elapsed > kProbeAfterMs) {
    const int now_owned = Stock(world, last_sent_id_);
    // ⚠ DANS UN SENS OU DANS L'AUTRE : pour les flèches, l'objet envoyé est le
    // MATÉRIAU, qui baisse.
    if (now_owned != sent_owned_) {
      const int gained = now_owned - sent_owned_;
      awaiting_ = false;
      sent_owned_ = -1;
      sent_material_ = 0;
      sent_material_owned_ = -1;
      std::array<char, kLineBytes> line{};
      if (gained > 1) {
        std::snprintf(line.data(), line.size(), Tr("%s créé ×%d"), last_sent_name_.c_str(), gained);
      } else if (gained == 1) {
        std::snprintf(line.data(), line.size(), Tr("%s créé."), last_sent_name_.c_str());
      } else {
        std::snprintf(line.data(), line.size(), Tr("%s transformé."), last_sent_name_.c_str());
      }
      last_result_ = line.data();
      last_result_tone_ = ro::ui::SkinTone::kGood;
      Log(last_result_, last_result_tone_);
      if (batch_left_ > 0) {
        --batch_left_;
      }
      ScheduleAutoRecast(settings, world, now_ms);
      return;
    }
    // LE PRODUIT N'A PAS BOUGÉ MAIS LE MATÉRIAU A BAISSÉ : le serveur a traité
    // la demande, et le tirage a raté.
    if (sent_material_ != 0 && sent_material_owned_ >= 0 &&
        Stock(world, sent_material_) < sent_material_owned_) {
      awaiting_ = false;
      sent_owned_ = -1;
      sent_material_ = 0;
      sent_material_owned_ = -1;
      std::array<char, kLineBytes> line{};
      std::snprintf(line.data(), line.size(), Tr("%s : échec, matériaux perdus."),
                    last_sent_name_.c_str());
      last_result_ = line.data();
      last_result_tone_ = ro::ui::SkinTone::kWarn;
      Log(last_result_, last_result_tone_);
      if (batch_left_ > 0) {
        --batch_left_;
      }
      ScheduleAutoRecast(settings, world, now_ms);
      return;
    }
  }
  if (elapsed > kAwaitTimeoutMs) {
    awaiting_ = false;
    sent_owned_ = -1;
    sent_material_ = 0;
    sent_material_owned_ = -1;
    auto_recast_at_ = 0;
    batch_left_ = 0;
    batch_fire_ = false;
    const CraftRecipe* recipe = book_ != nullptr ? book_->RecipeOf(last_sent_id_) : nullptr;
    const bool dish = recipe != nullptr && IsDishItemLevel(recipe->item_level);
    stop_reason_ =
        dish ? Tr("Pas de réponse : la fabrication a probablement ÉCHOUÉ. La cuisine ne renvoie "
                  "aucun message quand elle rate - seulement un effet visuel. Les matériaux sont "
                  "consommés et la maîtrise culinaire baisse. Regarde les chances annoncées avant "
                  "de recommencer. (Une demande refusée à la revalidation aurait le même silence, "
                  "mais n'aurait rien consommé.)")
             : Tr("Pas de réponse : la fabrication a probablement ÉCHOUÉ - ces skills ne "
                  "renvoient aucun message, ni en cas de succès ni en cas d'échec. Autre "
                  "possibilité : une demande refusée à la revalidation des matériaux, qui elle "
                  "n'aurait rien consommé.");
    Log(dish ? Tr("Échec probable — la cuisine ne dit rien quand elle rate.")
             : Tr("Aucune réponse du serveur — échec probable."),
        ro::ui::SkinTone::kWarn);
  }
}

// ── LA RELANCE ──────────────────────────────────────────────────────────────

void MakeItemFlow::ScheduleAutoRecast(const MakeItemSettings& settings, const ro::sim::World& world,
                                      std::uint32_t now_ms) {
  static_cast<void>(world);
  if (from_item_) {
    if (!settings.auto_reuse_item) {
      batch_left_ = 0;
      return;
    }
    if (source_item_id_ == 0) {
      stop_reason_ =
          Tr("Item d'origine inconnu : relance impossible. (La liste a été ouverte "
             "avant que le client ne puisse l'observer.)");
      batch_left_ = 0;
      return;
    }
    if (settings.auto_reuse_max > 0 && auto_items_used_ >= settings.auto_reuse_max) {
      std::array<char, kLineBytes> line{};
      std::snprintf(line.data(), line.size(),
                    Tr("Plafond atteint : %d %s consommés. Relance "
                       "arrêtée."),
                    auto_items_used_, source_item_name_.c_str());
      stop_reason_ = line.data();
      batch_left_ = 0;
      return;
    }
  } else {
    if (!settings.auto_recast) {
      batch_left_ = 0;
      return;
    }
    if (skill_id_ <= 0) {
      stop_reason_ = Tr("Skill inconnu : relance impossible.");
      batch_left_ = 0;
      return;
    }
  }
  ++auto_chain_;
  auto_recast_at_ = Deadline(now_ms, kAutoRelaunchDelayMs);
}

void MakeItemFlow::RetryRelaunch(const MakeItemSettings& settings, const ro::sim::World& world,
                                 bool window_open, std::uint32_t now_ms) {
  relaunch_sent_at_ = 0;
  if (!window_open || pending_ != Pending::kNone) {
    return;
  }
  if (from_item_) {
    if (!settings.auto_reuse_item || source_item_id_ == 0) {
      return;
    }
    // L'EXEMPLAIRE EST PARTI SANS LISTE EN RETOUR : on n'entame pas davantage.
    if (reuse_owned_before_ >= 0 && Stock(world, source_item_id_) < reuse_owned_before_) {
      std::array<char, kLineBytes> line{};
      std::snprintf(line.data(), line.size(),
                    Tr("%s consommé mais aucune liste n'est revenue : relance arrêtée (le stock ne "
                       "sera pas entamé davantage)."),
                    source_item_name_.c_str());
      stop_reason_ = line.data();
      batch_left_ = 0;
      return;
    }
  } else if (!settings.auto_recast) {
    return;
  }
  if (++relaunch_retries_ > kMostRelaunchRetries) {
    stop_reason_ = Tr("La liste ne revient pas (délai de lancement) : relance arrêtée.");
    Log(stop_reason_, ro::ui::SkinTone::kWarn);
    batch_left_ = 0;
    return;
  }
  stop_reason_.clear();
  auto_recast_at_ = Deadline(now_ms, kAutoRelaunchDelayMs);
  std::array<char, kLineBytes> line{};
  std::snprintf(line.data(), line.size(), Tr("Relance sans réponse : nouvel essai (%d/%d)."),
                relaunch_retries_, kMostRelaunchRetries);
  Log(line.data(), ro::ui::SkinTone::kWarn);
}

void MakeItemFlow::Tick(const MakeItemSettings& settings, const ro::sim::World& world,
                        ro::ui::MakeItemWindowState& state, std::uint32_t now_ms) {
  if (Due(auto_recast_at_, now_ms)) {
    auto_recast_at_ = 0;
    pending_ = from_item_ ? Pending::kReuseItem : Pending::kRecast;
  }
  if (relaunch_sent_at_ != 0 &&
      static_cast<std::uint32_t>(now_ms - relaunch_sent_at_) > kRelaunchNoListMs) {
    RetryRelaunch(settings, world, state.open, now_ms);
  }
}

// ── LES GESTES ──────────────────────────────────────────────────────────────

void MakeItemFlow::RequestMake(ro::ui::MakeItemWindowState& state, bool selection_visible,
                               const ro::sim::World& world, std::uint32_t now_ms) {
  if (!state.open || !list_armed_ || entries_.empty() || awaiting_ || state.selected == 0 ||
      !selection_visible || pending_ != Pending::kNone ||
      (sent_at_ != 0 && static_cast<std::uint32_t>(now_ms - sent_at_) < kLeastSendIntervalMs)) {
    return;
  }
  const Entry* chosen = EntryOf(state.selected);
  if (chosen == nullptr) {
    return;
  }
  if (batch_left_ == 0) {
    batch_left_ = state.batch_target;
  }
  sent_at_ = now_ms == 0 ? 1 : now_ms;
  last_sent_id_ = chosen->item_id;
  last_sent_name_ = chosen->name;
  sent_owned_ = Stock(world, chosen->item_id);
  // LA SONDE DU MATÉRIAU : le premier qui est vraiment consommé.
  sent_material_ = 0;
  sent_material_owned_ = -1;
  for (const Material& material : chosen->materials) {
    if (material.item_id != 0 && material.quantity > 0 && !material.kept && !material.output) {
      sent_material_ = material.item_id;
      sent_material_owned_ = Stock(world, material.item_id);
      break;
    }
  }
  awaiting_ = true;
  pending_ = Pending::kConfirm;
  pending_id_ = chosen->item_id;
}

void MakeItemFlow::CloseAndCancel(ro::ui::MakeItemWindowState& state) {
  auto_recast_at_ = 0;
  auto_ours_ = false;
  batch_left_ = 0;
  batch_fire_ = false;
  relaunch_sent_at_ = 0;
  relaunch_retries_ = 0;
  if (pending_ == Pending::kRecast || pending_ == Pending::kReuseItem) {
    pending_ = Pending::kNone;
  }
  // 🔴 L'ANNULATION PART TOUJOURS, liste armée ou fenêtre ouverte : une de trop
  // est inoffensive, une qui manque bloque le personnage.
  if (state.open || list_armed_) {
    pending_ = Pending::kCancel;
  } else {
    entries_.clear();
  }
  state.open = false;
}

void MakeItemFlow::Apply(const ro::ui::MakeItemWindowAction& asked,
                         ro::ui::MakeItemWindowState& state, const MakeItemSettings& settings,
                         ro::net::Connection& link, ro::sim::World& world, std::uint32_t now_ms) {
  if (asked.forge_slot >= 0 && static_cast<std::size_t>(asked.forge_slot) < forge_slots_.size()) {
    forge_slots_[static_cast<std::size_t>(asked.forge_slot)] = asked.forge_item_id;
  }
  // UNE SÉRIE CACHÉE NE COMPTE PAS : sans la relance, on fabrique une fois.
  if (!BatchPossible(settings)) {
    state.batch_target = ro::ui::kLeastBatch;
  }
  switch (asked.verb) {
    case ro::ui::MakeItemVerb::kNothing:
    case ro::ui::MakeItemVerb::kOpenAtlas:
      break;
    case ro::ui::MakeItemVerb::kMake:
      RequestMake(state, asked.selection_visible, world, now_ms);
      break;
    case ro::ui::MakeItemVerb::kRelaunch:
      auto_chain_ = 0;
      auto_items_used_ = 0;
      batch_left_ = 0;
      batch_fire_ = false;
      stop_reason_.clear();
      auto_ours_ = true;
      pending_ = from_item_ ? Pending::kReuseItem : Pending::kRecast;
      break;
    case ro::ui::MakeItemVerb::kClose:
      CloseAndCancel(state);
      break;
  }
  Tick(settings, world, state, now_ms);
  // LA SÉRIE : une liste provoquée par nous vient d'arriver, la demande repart.
  if (batch_fire_ && !awaiting_ && pending_ == Pending::kNone) {
    batch_fire_ = false;
    RequestMake(state, asked.selection_visible, world, now_ms);
  }
  Flush(state, settings, link, world, now_ms);
}

void MakeItemFlow::Flush(ro::ui::MakeItemWindowState& state, const MakeItemSettings& settings,
                         ro::net::Connection& link, ro::sim::World& world, std::uint32_t now_ms) {
  const Pending pending = std::exchange(pending_, Pending::kNone);
  switch (pending) {
    case Pending::kNone:
      return;
    case Pending::kConfirm: {
      ro::core::Result<void> sent = {};
      switch (kind_) {
        case MakeListKind::kArrow:
          sent = ro::net::RequestMakeArrow(link, pending_id_);
          break;
        case MakeListKind::kMaking:
          sent = ro::net::RequestMakeItem(link, make_type_, pending_id_);
          break;
        case MakeListKind::kProduce:
          sent = ro::net::RequestProduce(link, pending_id_, forge_slots_);
          break;
      }
      if (!sent) {
        ro::core::LogWarning("fabrication : demande non émise — {}",
                             ro::core::Describe(sent.error()));
        awaiting_ = false;
        return;
      }
      // LA LISTE RESTE AFFICHÉE, grisée : le serveur ne l'honore plus.
      list_armed_ = false;
      empty_list_ = false;
      return;
    }
    case Pending::kCancel: {
      ro::core::Result<void> sent = {};
      switch (kind_) {
        case MakeListKind::kArrow:
          sent = ro::net::RequestMakeArrow(link, ro::net::kMakeCancelItem);
          break;
        case MakeListKind::kMaking:
          sent = ro::net::RequestMakeItem(link, make_type_, ro::net::kMakeCancelItem);
          break;
        case MakeListKind::kProduce:
          // ⚠ ZÉRO SUR CE CHEMIN, pas moins un.
          sent = ro::net::RequestProduce(link, 0, {});
          break;
      }
      if (!sent) {
        ro::core::LogWarning("fabrication : annulation non émise — {}",
                             ro::core::Describe(sent.error()));
      }
      list_armed_ = false;
      entries_.clear();
      state.open = false;
      return;
    }
    case Pending::kRecast: {
      const std::uint32_t self = world.player_id();
      if (skill_id_ <= 0 || self == 0) {
        return;
      }
      auto_ours_ = true;
      if (auto sent = ro::net::RequestSkillOnTarget(link, static_cast<std::uint16_t>(skill_id_),
                                                    static_cast<std::uint16_t>(skill_level_), self);
          !sent) {
        ro::core::LogWarning("fabrication : relance non émise — {}",
                             ro::core::Describe(sent.error()));
        return;
      }
      relaunch_sent_at_ = now_ms == 0 ? 1 : now_ms;
      return;
    }
    case Pending::kReuseItem:
      auto_ours_ = true;
      reuse_owned_before_ = Stock(world, source_item_id_);
      SendReuseItem(settings, link, world);
      if (auto_ours_) {
        relaunch_sent_at_ = now_ms == 0 ? 1 : now_ms;
      }
      return;
  }
}

// 🔴 L'OBJET SE RETROUVE PAR SON IDENTIFIANT, jamais par la place d'origine :
// l'exemplaire d'avant est consommé, et le sac a pu se tasser.
void MakeItemFlow::SendReuseItem(const MakeItemSettings& settings, ro::net::Connection& link,
                                 ro::sim::World& world) {
  const ro::sim::InventoryItem* held = nullptr;
  for (const ro::sim::InventoryItem& item : world.inventory().items()) {
    if (item.item_id == source_item_id_ && item.amount > 0) {
      held = &item;
      break;
    }
  }
  const std::uint32_t self = world.player_id();
  bool sent = false;
  if (held != nullptr && self != 0) {
    sent = static_cast<bool>(ro::net::RequestUseItem(link, held->client_slot, self));
  }
  if (!sent) {
    std::array<char, kLineBytes> line{};
    std::snprintf(line.data(), line.size(),
                  Tr("Plus de %s dans l'inventory : relance arrêtée (%d consommés)."),
                  source_item_name_.c_str(), auto_items_used_);
    stop_reason_ = line.data();
    auto_ours_ = false;
    return;
  }
  ++auto_items_used_;
  std::array<char, kLineBytes> line{};
  if (settings.auto_reuse_max > 0) {
    std::snprintf(line.data(), line.size(), Tr("Relance : %s consommé (%d/%d)."),
                  source_item_name_.c_str(), auto_items_used_, settings.auto_reuse_max);
  } else {
    std::snprintf(line.data(), line.size(), Tr("Relance : %s consommé (%d au total)."),
                  source_item_name_.c_str(), auto_items_used_);
  }
  Log(line.data(), ro::ui::SkinTone::kWarn);
}

void MakeItemFlow::Log(std::string text, ro::ui::SkinTone tone) {
  history_.push_back(LogLine{.text = std::move(text), .tone = tone, .time = ClockNow()});
  while (history_.size() > kMostHistoryLines) {
    history_.pop_front();
  }
}

}  // namespace moonclient
