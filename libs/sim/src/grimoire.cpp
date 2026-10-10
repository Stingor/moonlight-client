#include "ro/sim/grimoire.h"

#include <utility>

namespace ro::sim {
namespace {

// Profondeur de remontée des prérequis. Aucun arbre de métier n'en compte
// autant ; le plafond n'est pas là pour eux mais pour un cycle dans les tables,
// qui ferait tourner la remontée sans fin.
constexpr int kDeepestPrerequisiteChain = 8;

// Un prérequis rencontré pendant la remontée.
struct ChainLink {
  std::uint16_t skill_id = 0;
  // Le rang le plus HAUT réclamé : deux branches peuvent réclamer la même
  // compétence à des rangs différents, et c'est le plus exigeant qui compte.
  std::uint16_t level = 0;
  // La profondeur la plus GRANDE : c'est elle qui décide de l'ordre, et une
  // compétence atteinte par deux chemins doit se payer avant le plus profond des
  // deux.
  int depth = 0;
};

void PushLink(std::vector<ChainLink>& chain, std::uint16_t skill_id, std::uint16_t level,
              int depth) {
  for (ChainLink& link : chain) {
    if (link.skill_id != skill_id) {
      continue;
    }
    link.level = std::max(link.level, level);
    link.depth = std::max(link.depth, depth);
    return;
  }
  chain.push_back(ChainLink{.skill_id = skill_id, .level = level, .depth = depth});
}

}  // namespace

const GrimoireEntry* Grimoire::Find(std::uint16_t skill_id) const {
  const auto found = row_by_skill_.find(skill_id);
  return found == row_by_skill_.end() ? nullptr : &entries_[found->second];
}

std::uint16_t Grimoire::LearnedLevelOf(std::uint16_t skill_id) const {
  const GrimoireEntry* entry = Find(skill_id);
  return entry == nullptr ? 0 : entry->learned;
}

std::uint16_t Grimoire::ReservedLevelOf(std::uint16_t skill_id) const {
  for (const auto& [reserved_id, level] : reservations_) {
    if (reserved_id == skill_id) {
      return level;
    }
  }
  return 0;
}

int Grimoire::reserved_points() const noexcept {
  int total = 0;
  for (const auto& [skill_id, level] : reservations_) {
    total += level - LearnedLevelOf(skill_id);
  }
  return total;
}

int Grimoire::points_left() const noexcept {
  return static_cast<int>(skill_points_) - reserved_points();
}

void Grimoire::CancelReservations() {
  reservations_.clear();
  for (GrimoireEntry& entry : entries_) {
    entry.reserved = 0;
  }
}

void Grimoire::Refresh(std::uint32_t job, const SkillBook& book, std::int64_t skill_points) {
  skill_points_ = skill_points > 0 ? skill_points : 0;
  entries_.clear();
  row_by_skill_.clear();

  // Les grilles de la chaîne, du plus ancien métier au plus récent.
  for (const std::uint32_t ancestor : tree_->InheritanceOfJob(job)) {
    for (const SkillSlot& slot : tree_->SlotsOfJob(ancestor)) {
      (void)RowFor(slot.skill_id, ancestor, slot.position, true);
    }
  }

  // Ce que le serveur a annoncé. Une compétence qu'aucune grille ne place entre
  // quand même : le serveur en accorde par quête, par objet ou par groupe, et le
  // client natif les range lui aussi dans une liste à part.
  for (const KnownSkill& known : book.skills()) {
    GrimoireEntry& entry = RowFor(known.id, 0, 0, false);
    entry.learned = known.level;
    entry.sp_cost = known.sp_cost;
    entry.range_cells = known.range_cells;
    entry.targeting = known.targeting;
  }

  // ── Le tri des réservations ──────────────────────────────────────────────
  // Une réservation sans objet — compétence absente de l'arbre, ou rang déjà
  // atteint par le serveur — continuerait de compter des points pour rien.
  std::vector<std::pair<std::uint16_t, std::uint16_t>> kept;
  kept.reserve(reservations_.size());
  for (const auto& [skill_id, level] : reservations_) {
    if (tree_->Find(skill_id) == nullptr) {
      continue;
    }
    if (level <= LearnedLevelOf(skill_id)) {
      continue;
    }
    kept.emplace_back(skill_id, level);
  }
  reservations_ = std::move(kept);
  MarkReservedRows();
}

GrimoireEntry& Grimoire::RowFor(std::uint16_t skill_id, std::uint32_t job, std::uint16_t position,
                                bool in_grid) {
  // ⚠ UNE COMPÉTENCE PEUT ÊTRE DANS DEUX GRILLES DE LA CHAÎNE. On garde la
  // PREMIÈRE, celle du tronc : deux lignes pour la même compétence lui
  // donneraient deux cases, et deux fois le même rang à monter.
  const auto seen = row_by_skill_.find(skill_id);
  if (seen != row_by_skill_.end()) {
    return entries_[seen->second];
  }
  row_by_skill_.emplace(skill_id, entries_.size());
  entries_.push_back(GrimoireEntry{.skill_id = skill_id,
                                   .job = job,
                                   .position = position,
                                   .in_grid = in_grid,
                                   .node = tree_->Find(skill_id)});
  return entries_.back();
}

void Grimoire::MarkReservedRows() {
  for (const auto& [skill_id, level] : reservations_) {
    // Une compétence seulement RÉSERVÉE peut n'être ni dans une grille de la
    // chaîne ni dans le livre : un prérequis d'un autre métier. Elle prend sa
    // ligne tout de suite — sans quoi le joueur ne la verrait qu'au prochain
    // paquet du serveur, c'est-à-dire jamais s'il ne se passe rien.
    RowFor(skill_id, 0, 0, false).reserved = level;
  }
}

int Grimoire::ReserveUpTo(const SkillNode& node, int wanted_level, int& points_left) {
  const int current = std::max<int>(LearnedLevelOf(node.id), ReservedLevelOf(node.id));
  const int reachable = std::min<int>(wanted_level, node.max_level);
  if (reachable <= current || points_left <= 0) {
    return 0;
  }
  const int taken = std::min(reachable - current, points_left);
  if (taken <= 0) {
    return 0;
  }
  const auto level = static_cast<std::uint16_t>(current + taken);
  for (auto& [skill_id, reserved] : reservations_) {
    if (skill_id == node.id) {
      reserved = level;
      points_left -= taken;
      return taken;
    }
  }
  reservations_.emplace_back(node.id, level);
  points_left -= taken;
  return taken;
}

ReserveOutcome Grimoire::Reserve(std::uint16_t skill_id, bool up_to_max) {
  // ⚠ CHERCHÉE DANS TOUT L'ARBRE, pas dans les lignes : un prérequis vit souvent
  // dans la grille d'un autre métier que celui qu'on regarde.
  const SkillNode* target = tree_->Find(skill_id);
  if (target == nullptr) {
    return ReserveOutcome::kUnknownSkill;
  }
  int points_left = this->points_left();
  if (points_left <= 0) {
    return ReserveOutcome::kNoPointLeft;
  }

  // ── Toute la chaîne de prérequis, en largeur ─────────────────────────────
  std::vector<ChainLink> chain;
  for (const SkillRequirement& need : target->needs) {
    PushLink(chain, need.skill_id, need.level, 1);
  }
  // `chain` grandit pendant la boucle : c'est le parcours lui-même.
  for (std::size_t rank = 0; rank < chain.size(); ++rank) {
    if (chain[rank].depth >= kDeepestPrerequisiteChain) {
      continue;
    }
    const SkillNode* node = tree_->Find(chain[rank].skill_id);
    if (node == nullptr) {
      continue;
    }
    for (const SkillRequirement& need : node->needs) {
      PushLink(chain, need.skill_id, need.level, chain[rank].depth + 1);
    }
  }

  int deepest = 0;
  for (const ChainLink& link : chain) {
    deepest = std::max(deepest, link.depth);
  }
  for (int depth = deepest; depth >= 1; --depth) {
    for (const ChainLink& link : chain) {
      if (link.depth != depth) {
        continue;
      }
      const SkillNode* node = tree_->Find(link.skill_id);
      if (node != nullptr) {
        (void)ReserveUpTo(*node, link.level, points_left);
      }
    }
  }
  // ⚠ MARQUÉES TOUT DE SUITE, AVANT DE SAVOIR SI LE SOMMET TIENDRA. Une chaîne
  // à moitié payée reste posée : le joueur voit la base préparée et ce qui lui
  // manque encore, au lieu d'un refus sec qui n'aurait rien avancé.
  MarkReservedRows();

  // ── La compétence demandée ───────────────────────────────────────────────
  const int current = std::max<int>(LearnedLevelOf(skill_id), ReservedLevelOf(skill_id));
  if (target->max_level == 0 || current >= target->max_level) {
    return ReserveOutcome::kAlreadyAtMax;
  }
  // `ReserveUpTo` borne déjà au rang maximal ET aux points restants : viser le
  // maximum revient à demander tout ce qui reste, sans boucler.
  const int wanted = up_to_max ? target->max_level : current + 1;
  if (ReserveUpTo(*target, wanted, points_left) == 0) {
    return ReserveOutcome::kNotEnoughForPrerequisites;
  }

  // Les lignes portent le rang visé, pour que le dessin n'ait rien à recalculer.
  MarkReservedRows();
  return ReserveOutcome::kReserved;
}

std::vector<std::uint16_t> Grimoire::PendingUpgrades() const {
  std::vector<std::uint16_t> upgrades;
  for (const auto& [skill_id, level] : reservations_) {
    for (int rank = LearnedLevelOf(skill_id) + 1; rank <= level; ++rank) {
      upgrades.push_back(skill_id);
    }
  }
  return upgrades;
}

}  // namespace ro::sim
