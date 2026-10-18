#include "character/status_view.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <span>

#include "ro/sim/world.h"

#include "game/skin_image_of.h"

namespace moonclient {
namespace {

// Combien de lettres une abréviation tirée d'un nom retient. ⚠ TROIS : à quinze
// pixels de haut, la quatrième ne se lit plus.
constexpr std::size_t kAbbreviationLetters = 3;

// Au-delà, la table des états ne décrit plus rien : le plus grand identifiant
// est 1688, et la borne évite de balayer au hasard.
constexpr std::uint32_t kHighestStatusEffect = 1700;

// Les durées de l'aperçu, en millisecondes. ⚠ LA DERNIÈRE EST NULLE : c'est
// l'état PERMANENT, celui qui n'a ni voile ni compte à rebours — et le cas qu'on
// oublie de regarder en réglant.
constexpr std::array<std::uint32_t, 6> kPreviewDurationsMs = {12'000,  45'000, 150'000,
                                                              600'000, 30'000, 0};

// En combien de crans la part écoulée de l'aperçu s'étage.
constexpr std::uint32_t kPreviewSpentSteps = 5;

// Cet octet est-il une lettre latine ou un chiffre ?
//
// ⚠ TESTÉ SUR L'OCTET, ET C'EST VOLONTAIRE : les noms de la table sont en UTF-8,
// et soixante-sept d'entre eux sont en coréen. Couper trois OCTETS d'un nom
// coréen rendrait une demi-syllabe illisible ; on ne garde donc que ce qui se
// lit seul.
[[nodiscard]] bool IsLatinLetterOrDigit(char byte) {
  const auto value = static_cast<unsigned char>(byte);
  return std::isalnum(static_cast<int>(value)) != 0 && value < 0x80;
}

}  // namespace

std::string StatusAbbreviation(std::string_view name) {
  // 🔴 LA TABLE EST LA SEULE SOURCE : un état qu'elle ne nomme pas n'a pas de
  // lettres, et sa case ne se dessine pas. Aucun texte de repli ne s'écrit ici.
  std::string letters;
  for (const char byte : name) {
    if (!IsLatinLetterOrDigit(byte)) {
      continue;
    }
    letters.push_back(
        static_cast<char>(std::toupper(static_cast<int>(static_cast<unsigned char>(byte)))));
    if (letters.size() == kAbbreviationLetters) {
      break;
    }
  }
  return letters;
}

void StatusView::ScanDrawable(int want) {
  if (want <= scanned_for_ || table_ == nullptr || icons_ == nullptr) {
    return;
  }
  scanned_for_ = want;
  drawable_.clear();
  // ⚠ ORDONNÉ PAR NUMÉRO, et non dans l'ordre de la table : celle-ci est une
  // table de hachage, et son parcours change d'une exécution à l'autre. Un
  // aperçu qui montre six icônes différentes à chaque lancement ne sert plus à
  // comparer deux réglages.
  std::vector<ro::sim::StatusEffectId> known;
  known.reserve(table_->by_id.size());
  for (const auto& [effect, info] : table_->by_id) {
    if (!info.icon_file.empty() && effect <= kHighestStatusEffect) {
      known.push_back(static_cast<ro::sim::StatusEffectId>(effect));
    }
  }
  std::sort(known.begin(), known.end());
  for (const ro::sim::StatusEffectId effect : known) {
    if (static_cast<int>(drawable_.size()) >= want) {
      break;
    }
    if (icons_->Get(effect).usable()) {
      drawable_.push_back(effect);
    }
  }
}

void StatusView::Collect(std::span<const ro::sim::ActiveStatus> statuses, std::uint32_t now_ms) {
  pending_.clear();
  abbreviations_.clear();
  description_lines_.clear();
  cells_.clear();

  // ── CE QUE LE FIL A DIT, AVEC SA DURÉE ────────────────────────────────────
  for (const ro::sim::ActiveStatus& status : statuses) {
    Pending held;
    held.effect = status.effect;
    // ⚠ ZÉRO DES DEUX CÔTÉS VEUT DIRE LA MÊME CHOSE — « sans échéance ». Un état
    // dont le terme est passé compte pour permanent le temps d'une image ; c'est
    // le balayage de la boucle qui le retire, pas cette vue.
    held.remaining_ms = status.expires_ms > now_ms ? status.expires_ms - now_ms : 0;
    held.total_ms = status.total_ms;
    pending_.push_back(held);
  }
}

void StatusView::RefreshList(std::span<const ro::sim::ActiveStatus> statuses, std::uint32_t now_ms,
                             int preview) {
  Collect(statuses, now_ms);
  AppendPreview(preview);
  BuildCells();
}

void StatusView::Refresh(const ro::sim::World& world, std::uint32_t now_ms, int preview) {
  Collect(world.player_statuses(), now_ms);

  // ── CE QUI ARRIVE EN BITS, ET QUE PERSONNE N'ANNONCE ──────────────────────
  //
  // ⚠ SANS DURÉE, ET CE N'EST PAS UN MANQUE DE NOTRE PART : `ZC_STATE_CHANGE` ne
  // porte que des bits. Le serveur ne dit pas combien de temps on dort.
  const ro::sim::Actor* player = world.player();
  if (player != nullptr) {
    // ⚠ LE MÉTIER AVANT TOUTE LECTURE D'ICÔNE : six états en tirent une autre
    // image, et le chargeur vide son cache quand il change.
    if (icons_ != nullptr) {
      icons_->SetJob(player->look.job);
    }
    ailments_.clear();
    ro::sim::AilmentStatuses(player->body_state, player->health_state, ailments_);
    for (const ro::sim::StatusEffectId effect : ailments_) {
      // ⚠ PAS DEUX FOIS LA MÊME ALTÉRATION. Certaines voyagent par les DEUX
      // voies — un bit d'`opt2` et un EFST de son cru —, et le joueur verrait
      // alors deux icônes pour un seul poison.
      const bool already =
          std::any_of(pending_.begin(), pending_.end(),
                      [effect](const Pending& held) { return held.effect == effect; });
      if (!already) {
        pending_.push_back(Pending{.effect = effect, .remaining_ms = 0, .total_ms = 0});
      }
    }
  }
  AppendPreview(preview);
  BuildCells();
}

void StatusView::AppendPreview(int preview) {
  // ── L'APERÇU, POUR RÉGLER SANS ATTENDRE D'AVOIR DES BUFFS ─────────────────
  //
  // ⚠ DES DURÉES ÉTAGÉES, et c'est ce qui le rend utile : un voile, un compte à
  // rebours et un tri ne se jugent que sur des états qui ne finissent pas
  // ensemble. Le dernier est PERMANENT — c'est le cas qu'on oublie de vérifier.
  genuine_pending_ = pending_.size();
  if (preview > 0) {
    ScanDrawable(preview);
    for (std::size_t rank = 0;
         rank < drawable_.size() && static_cast<int>(pending_.size()) < preview; ++rank) {
      const std::uint32_t total_ms = kPreviewDurationsMs[rank % kPreviewDurationsMs.size()];
      pending_.push_back(Pending{
          .effect = drawable_[rank],
          // La part écoulée s'étage aussi : une case à
          // peine voilée à côté d'une presque pleine.
          .remaining_ms =
              total_ms == 0
                  ? 0
                  : total_ms - total_ms * static_cast<std::uint32_t>(rank % kPreviewSpentSteps) /
                                   kPreviewSpentSteps,
          .total_ms = total_ms});
    }
  }
}

void StatusView::BuildCells() {
  // ── LES ABRÉVIATIONS, TOUTES AVANT LA PREMIÈRE VUE ────────────────────────
  abbreviations_.reserve(pending_.size());
  for (const Pending& held : pending_) {
    const bool drawn = icons_ != nullptr && icons_->Get(held.effect).usable();
    abbreviations_.push_back(drawn ? std::string() : StatusAbbreviation(NameOf(held.effect)));
  }

  // ── LES LIGNES DE DESCRIPTION, TOUTES AVANT LA PREMIÈRE TRANCHE ───────────
  //
  // ⚠ DEUX PASSES, ET C'EST LA MÊME DISCIPLINE QUE LES ABRÉVIATIONS : une
  // tranche prise pendant que le vecteur grandit pendrait à sa première
  // réallocation, et cela ne se verrait qu'à l'écran, plus tard.
  std::size_t lines_needed = 0;
  for (const Pending& held : pending_) {
    lines_needed += DescriptionOf(held.effect).size();
  }
  description_lines_.reserve(lines_needed);
  for (const Pending& held : pending_) {
    for (const std::string& line : DescriptionOf(held.effect)) {
      description_lines_.emplace_back(line);
    }
  }

  // ── LES CASES ─────────────────────────────────────────────────────────────
  cells_.reserve(pending_.size());
  genuine_cells_ = 0;
  std::size_t taken = 0;
  for (std::size_t rank = 0; rank < pending_.size(); ++rank) {
    const Pending& held = pending_[rank];
    // ⚠ LE CURSEUR AVANCE MÊME POUR UNE CASE ÉCARTÉE : le tampon a été rempli
    // pour TOUTES les entrées, et sauter son avance décalerait la description
    // de toutes les suivantes — chacune lirait celle de sa voisine.
    const std::size_t mine = taken;
    taken += DescriptionOf(held.effect).size();
    ro::ui::StatusCellView cell;
    if (icons_ != nullptr) {
      const ro::render::IconTexture& icon = icons_->Get(held.effect);
      cell.icon = SizedSkinImageOf(icon);
    }
    cell.abbreviation = abbreviations_[rank];
    // ⚠ NI IMAGE NI MOT : la case ne se dessinerait pas, et lui garder sa place
    // creuserait un TROU dans la rangée. Voir `DrawStatusCell`, qui rend faux.
    if (!cell.icon.usable() && cell.abbreviation.empty()) {
      continue;
    }
    cell.name = NameOf(held.effect);
    cell.description = std::span<const std::string_view>(description_lines_)
                           .subspan(mine, DescriptionOf(held.effect).size());
    cell.time_line = TimeLineOf(held.effect);
    cell.remaining_ms = held.remaining_ms;
    cell.total_ms = held.total_ms;
    cells_.push_back(cell);
    if (rank < genuine_pending_) {
      genuine_cells_ = cells_.size();
    }
  }
}

const ro::ui::StatusPreviewView& StatusView::Preview(ro::sim::StatusEffectId effect) {
  preview_ = ro::ui::StatusPreviewView{};
  preview_lines_.clear();
  if (effect == 0) {
    return preview_;
  }
  const std::vector<std::string>& lines = DescriptionOf(effect);
  preview_lines_.reserve(lines.size());
  for (const std::string& line : lines) {
    // ⚠ LA PLACE DE LA DURÉE SE SAUTE : la table réserve une ligne qui ne porte
    // que son gabarit, et un aperçu d'ÉTAT n'a pas de durée à y mettre. La
    // laisser écrirait son gabarit en toutes lettres.
    if (line == ro::vfs::kStatusTimePlaceholder) {
      continue;
    }
    preview_lines_.push_back(line);
  }
  preview_.effect_id = effect;
  preview_.name = NameOf(effect);
  // ⚠ LE SPAN SE PREND APRÈS LE REMPLISSAGE : pris avant, il meurt à la première
  // réallocation du vecteur.
  preview_.description = preview_lines_;
  if (icons_ != nullptr) {
    const ro::render::IconTexture& icon = icons_->Get(effect);
    if (icon.usable()) {
      preview_.icon = SkinImageOf(icon);
    }
  }
  return preview_;
}

}  // namespace moonclient
