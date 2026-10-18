#include "character/skill_view.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/net/chat_target.h"
#include "ro/net/skill.h"
#include "ro/sim/world.h"

#include "game/homunculus_staff_words.h"
#include "game/skin_image_of.h"

namespace moonclient {
namespace {

// LE LIBELLÉ D'UN PALIER, quand le client n'en nomme aucun pour ce métier.
//
// ⚠ UN SWITCH DE LITTÉRAUX, ET NON UNE TABLE INDEXÉE. `Tr` n'accepte qu'un
// littéral : une chaîne prise dans un tableau n'est dans aucun catalogue, et le
// contrôle des traductions le refuse — à raison, puisqu'elle ne serait jamais
// traduite. Même raison que les gravités du journal.
[[nodiscard]] const char* FallbackTabName(std::size_t tab) {
  switch (tab) {
    case 0:
      return ro::core::Tr("1re classe");
    case 1:
      return ro::core::Tr("2e classe");
    case 2:
      return ro::core::Tr("3e classe");
    default:
      return ro::core::Tr("4e classe");
  }
}

// Le libellé d'un onglet : celui du client quand il en donne un, sinon le nôtre.
//
// ⚠ ON TRADUIT CE QU'ON RECONNAÎT, ET RIEN D'AUTRE. Le client écrit « 1st »,
// « 2nd »… pour les paliers ordinaires ; ce qui n'en est pas est un nom PROPRE à
// la classe — « Ninja », « Taekwon » — qui vient déjà de la langue du client, et
// qu'on montre tel quel.
[[nodiscard]] const char* TabName(std::span<const std::string> from_client, std::size_t tab) {
  if (tab >= from_client.size() || from_client[tab].empty()) {
    return FallbackTabName(tab);
  }
  const std::string& written = from_client[tab];
  if (written == "1st") {
    return ro::core::Tr("1re classe");
  }
  if (written == "2nd") {
    return ro::core::Tr("2e classe");
  }
  if (written == "3rd") {
    return ro::core::Tr("3e classe");
  }
  if (written == "4th") {
    return ro::core::Tr("4e classe");
  }
  return written.c_str();
}

// Les paliers que le client fusionne dans un même onglet : le premier et le
// deuxième. Les deux autres restent seuls — ce sont des arbres entiers.
constexpr std::array<std::array<std::size_t, 2>, 3> kGroupedTabs = {
    std::array<std::size_t, 2>{0, 1}, std::array<std::size_t, 2>{2, 2},
    std::array<std::size_t, 2>{3, 3}};

}  // namespace

void SkillView::Rebuild(const ro::sim::SkillTree& tree, const ro::sim::Grimoire& grimoire,
                        std::uint32_t job, ro::render::SkillIcons& icons,
                        const ro::sim::SkillBook& book, const SkillUseLevels& levels,
                        std::uint32_t now_ms) {
  cells_.clear();
  needs_.clear();
  splits_.clear();
  groups_.clear();
  labels_.clear();

  const std::vector<ro::sim::SkillJobTab> chain = tree.TabsOfJob(job);
  const std::span<const std::string> from_client = tree.TabNamesOfJob(job);

  // ── Une case par compétence, dans l'ordre des groupes ────────────────────
  // Les prérequis se rangent à part : on ne pointe dessus qu'une fois le tampon
  // rempli, sinon une réallocation ferait pendre les vues déjà posées.
  struct Placed {
    std::uint16_t skill_id = 0;
    int position = 0;
    std::size_t needs_at = 0;
    std::size_t needs_count = 0;
  };
  std::vector<Placed> placed;
  // Par groupe : où commencent ses cases et ses traits, et combien il en a.
  struct Bounds {
    std::size_t cells_at = 0;
    std::size_t cells_count = 0;
    std::size_t splits_at = 0;
    std::size_t splits_count = 0;
  };
  std::vector<Bounds> bounds;

  // Les prérequis que le serveur vérifie pour le métier du personnage.
  const auto add_needs = [&](std::uint16_t skill_id) -> std::pair<std::size_t, std::size_t> {
    const std::size_t at = needs_.size();
    for (const ro::sim::SkillRequirement& need : tree.NeedsOf(job, skill_id)) {
      needs_.push_back(ro::ui::SkillNeedView{.skill_id = need.skill_id, .level = need.level});
    }
    return {at, needs_.size() - at};
  };

  for (const std::array<std::size_t, 2>& tabs : kGroupedTabs) {
    Bounds group;
    group.cells_at = placed.size();
    group.splits_at = splits_.size();
    // Le prochain rang de case libre, en lignes entieres.
    int base = 0;
    std::size_t sources = 0;

    for (std::size_t tab = tabs[0]; tab <= tabs[1]; ++tab) {
      // ⚠ UNE SOURCE EST UN ONGLET, PAS UN MÉTIER, et c'est TOUT le placement.
      // Les métiers d'un même palier PARTAGENT une grille : l'Épéiste laisse la
      // colonne zéro libre pour le Novice, le Seigneur Chevalier occupe les
      // colonnes que le Chevalier laisse. Leurs index de case sont faits pour
      // cohabiter, et les décaler l'un sous l'autre étale l'arbre sur trois fois
      // sa hauteur — avec une colonne d'icônes seules là où le jeu en montre
      // sept.
      const std::size_t before = placed.size();
      int highest = 0;
      for (const ro::sim::SkillJobTab& step : chain) {
        if (static_cast<std::size_t>(step.tab) != tab) {
          continue;
        }
        for (const ro::sim::SkillSlot& slot : tree.SlotsOfJob(step.job)) {
          // ⚠ LE GRIMOIRE DÉCIDE DE CE QUI SE MONTRE : une case que le serveur
          // ne donne pas au métier n'y a pas de ligne de grille.
          const ro::sim::GrimoireEntry* row = grimoire.Find(slot.skill_id);
          if (row == nullptr || !row->in_grid) {
            continue;
          }
          // Une compétence peut être dans deux grilles de la chaîne : on garde
          // la PREMIÈRE, celle du tronc. Deux cases pour la même compétence lui
          // donneraient deux places et deux fois le même identifiant.
          const bool seen = std::any_of(placed.begin(), placed.end(), [&slot](const Placed& other) {
            return other.skill_id == slot.skill_id;
          });
          if (seen) {
            continue;
          }
          const auto [needs_at, needs_count] = add_needs(slot.skill_id);
          placed.push_back(Placed{.skill_id = slot.skill_id,
                                  .position = base + slot.position,
                                  .needs_at = needs_at,
                                  .needs_count = needs_count});
          highest = std::max(highest, static_cast<int>(slot.position));
        }
      }
      if (placed.size() == before) {
        continue;
      }
      // Entre deux PALIERS, en revanche, les index se recouvrent : le suivant se
      // décale d'un nombre entier de lignes, et un trait dit où il commence.
      if (sources > 0) {
        splits_.push_back(ro::ui::SkillSplitView{.row = base / ro::sim::kSkillGridColumns,
                                                 .label = TabName(from_client, tab)});
      }
      base += ((highest / ro::sim::kSkillGridColumns) + 1) * ro::sim::kSkillGridColumns;
      ++sources;
    }
    if (sources == 0) {
      continue;
    }
    group.cells_count = placed.size() - group.cells_at;
    group.splits_count = splits_.size() - group.splits_at;
    bounds.push_back(group);

    if (tabs[0] == tabs[1]) {
      labels_.emplace_back(TabName(from_client, tabs[0]));
    } else {
      labels_.emplace_back(std::string(TabName(from_client, tabs[0])) + " / " +
                           TabName(from_client, tabs[1]));
    }
  }

  // ── Les compétences hors grille ──────────────────────────────────────────
  // Le serveur en accorde par quête, par objet ou par groupe : les jeter
  // laisserait le joueur sans moyen de voir ce qu'il sait.
  {
    Bounds group;
    group.cells_at = placed.size();
    group.splits_at = splits_.size();
    int position = 0;
    for (const ro::sim::GrimoireEntry& entry : grimoire.entries()) {
      if (entry.in_grid) {
        continue;
      }
      const auto [needs_at, needs_count] = add_needs(entry.skill_id);
      placed.push_back(Placed{.skill_id = entry.skill_id,
                              .position = position++,
                              .needs_at = needs_at,
                              .needs_count = needs_count});
    }
    group.cells_count = placed.size() - group.cells_at;
    if (group.cells_count > 0) {
      bounds.push_back(group);
      labels_.emplace_back(ro::core::Tr("Divers"));
    }
  }

  // ── Les vues, maintenant que les tampons ne bougent plus ─────────────────
  cells_.reserve(placed.size());
  for (const Placed& one : placed) {
    const ro::sim::GrimoireEntry* entry = grimoire.Find(one.skill_id);
    const ro::sim::SkillNode* node = tree.Find(one.skill_id);
    const int learned = entry != nullptr ? entry->learned : 0;
    const int reserved = entry != nullptr ? entry->reserved : 0;
    const ro::sim::SkillRule* rule = tree.RuleOf(job, one.skill_id);
    const int max_level = rule != nullptr ? rule->max_level : 0;
    // Une compétence que le serveur n'accorde pas contre des points ne se
    // monte qu'une fois sue.
    const bool by_points = rule != nullptr && (rule->by_points || learned > 0);
    const ro::sim::KnownSkill* known = book.Find(one.skill_id);
    const ro::render::IconTexture& icon =
        icons.Get(one.skill_id, node != nullptr ? node->idname : std::string_view());
    cells_.push_back(ro::ui::SkillCellView{
        .skill_id = one.skill_id,
        .position = one.position,
        .name = node != nullptr ? std::string_view(node->display_name) : std::string_view(),
        .idname = node != nullptr ? std::string_view(node->idname) : std::string_view(),
        .icon = SizedSkinImageOf(icon),
        .learned = learned,
        .reserved = reserved,
        .max_level = max_level,
        .raisable =
            by_points && std::max(learned, reserved) < max_level && grimoire.points_left() > 0,
        .needs = std::span(needs_).subspan(one.needs_at, one.needs_count),
        // ⚠ LE TEMPS D'ATTENTE SE LIT DANS LE LIVRE DU SERVEUR, jamais dans
        // l'arbre : c'est un fait de la partie, pas une propriété de la
        // compétence. Il s'y note même pour ce que le grimoire ne porte pas.
        .cooldown = book.CooldownFraction(one.skill_id, now_ms),
        .cooldown_ms = book.CooldownRemainingMs(one.skill_id, now_ms),
        // 🔴 ZÉRO VEUT DIRE « CELUI QU'ON A APPRIS », et c'est le dessin qui le
        // traduit : lui descendre le rang appris ferait une valeur écrite pour
        // chaque compétence du grimoire, dont aucune n'a été choisie.
        .use_level = levels.Of(one.skill_id),
        .level_matters = node != nullptr && node->level_matters,
        // ⚠ `ui` NE SAIT PAS CE QU'EST UNE PASSIVE : c'est ici qu'on lit les
        // drapeaux de ciblage du serveur, et une compétence qu'on n'a pas encore
        // apprise ne se lance pas non plus.
        .castable = known != nullptr && !ro::sim::SkillIsPassive(*known),
        // ⚠ LE COÛT ET LA PORTÉE VIENNENT DU SERVEUR, au rang appris : la table du
        // client ne porte qu'un barème, et c'est le serveur qui fait foi.
        .sp_cost = known != nullptr ? known->sp_cost : 0,
        .range_cells = known != nullptr ? known->range_cells : 0,
        // 🔴 LE PASSIF SE LIT SUR LES DRAPEAUX DE CIBLAGE, et il faut l'avoir
        // appris pour les connaître : une compétence qu'on n'a pas encore n'est
        // pas « passive », elle est INCONNUE — deux silences différents.
        .passive = known != nullptr && ro::sim::SkillIsPassive(*known)});
  }

  groups_.reserve(bounds.size());
  for (std::size_t rank = 0; rank < bounds.size(); ++rank) {
    groups_.push_back(ro::ui::SkillGroupView{
        .label = labels_[rank],
        .cells = std::span(cells_).subspan(bounds[rank].cells_at, bounds[rank].cells_count),
        .splits = std::span(splits_).subspan(bounds[rank].splits_at, bounds[rank].splits_count)});
  }
}

int SkillUseLevels::Of(std::uint16_t skill_id) const {
  const auto found = chosen_.find(skill_id);
  return found == chosen_.end() ? 0 : found->second;
}

void SkillUseLevels::Set(std::uint16_t skill_id, int level, int learned) {
  // ⚠ HORS DES BORNES, ON N'ÉCRIT RIEN : le dessin peut se tromper d'une unité
  // au bord de la plage, et cela ne doit pas devenir un paquet que le serveur
  // refuse. Un rang égal à l'appris efface le choix — c'est le défaut.
  if (level <= 0 || level > learned) {
    return;
  }
  if (level == learned) {
    chosen_.erase(skill_id);
    return;
  }
  chosen_[skill_id] = level;
}

// LA COMMANDE QUE LA LIGNE DU STAFF A DEMANDÉE, composée puis envoyée COMME SI
// LE JOUEUR LA TAPAIT.
//
// 🔴 LE VOCABULAIRE DU SERVEUR EST ICI, PAS DANS LA FENÊTRE. Celle-ci émet une
// intention ; c'est `app` qui sait que « oublier tout » s'écrit « @resetskill ».
// Le même partage que pour l'ouverture du coffre.
//
// ⚠ ET RIEN N'EST VALIDÉ ICI : le serveur revalide selon le groupe du compte, et
// son refus arrive dans la conversation comme d'habitude. Le droit qui masque la
// ligne ne décide que de ce qui se MONTRE.
void SendStaffOrder(const ro::ui::SkillWindowAction& action, ro::net::Connection& link,
                    const ro::sim::World& world) {
  std::array<char, 32> line{};
  switch (action.staff_order) {
    case ro::ui::StaffSkillOrder::kNone:
      return;
    case ro::ui::StaffSkillOrder::kForgetAll:
      std::snprintf(line.data(), line.size(), "@resetskill");
      break;
    case ro::ui::StaffSkillOrder::kLearnAll:
      std::snprintf(line.data(), line.size(), "@allskills");
      break;
    case ro::ui::StaffSkillOrder::kClearCooldowns:
      std::snprintf(line.data(), line.size(), "@resetcooltime");
      break;
    case ro::ui::StaffSkillOrder::kGivePoints:
      std::snprintf(line.data(), line.size(), "@skpoint %d", action.staff_points);
      break;
    case ro::ui::StaffSkillOrder::kMakeHomunculus:
      std::snprintf(line.data(), line.size(), "%s",
                    MakeHomunculusCommand(action.staff_homunculus_job).c_str());
      break;
  }
  if (auto sent = ro::net::SendServerCommand(link, world.player_name(), line.data()); !sent) {
    // ⚠ LA COMMANDE COMPOSÉE, ET PAS SEULEMENT L'ERREUR : c'est le seul contenu
    // que ce site apporte, et le même libellé sert dans `staff_view.cpp` pour
    // une AUTRE fenêtre — sans elle, on ne sait ni laquelle a échoué, ni sur
    // quoi.
    ro::core::LogWarning("commande de staff « {} » non émise : {}", line.data(),
                         ro::core::Describe(sent.error()));
  }
}

void ApplySkillAction(const ro::ui::SkillWindowAction& action, ro::sim::Grimoire& grimoire,
                      ro::net::Connection& link) {
  if (action.cancel) {
    grimoire.CancelReservations();
    return;
  }
  if (action.reserve != 0) {
    // ⚠ LE REFUS NE SE DIT PAS ENCORE À L'ÉCRAN. Le code est là — plus de point,
    // déjà au maximum, pas assez pour les prérequis —, et la phrase qui le montre
    // viendra avec le reste de l'en-tête. Le taire vaut mieux que l'inventer.
    static_cast<void>(grimoire.Reserve(action.reserve, action.to_max));
    return;
  }
  if (!action.apply) {
    return;
  }
  // ⚠ UNE MONTÉE PAR RANG, DANS L'ORDRE DES PRÉREQUIS : le paquet ne porte pas de
  // niveau, et le serveur refuserait un sommet dont la base n'est pas montée.
  const std::vector<std::uint16_t> upgrades = grimoire.PendingUpgrades();
  for (const std::uint16_t skill_id : upgrades) {
    if (auto sent = ro::net::RequestSkillUp(link, skill_id); !sent) {
      ro::core::LogWarning("montée de compétence refusée : {}", ro::core::Describe(sent.error()));
      return;
    }
  }
  // Les réservations tombent d'elles-mêmes au prochain paquet du serveur — il
  // annoncera les rangs montés, et le tri du grimoire les jettera. Les effacer
  // ici laisserait la grille revenir en arrière le temps de l'aller-retour.
  ro::core::LogInfo("grimoire : {} montée(s) envoyée(s)", upgrades.size());
}

SkillSheet::SkillSheet(const ro::sim::SkillTree* tree, ro::render::SkillIcons* icons)
    : tree_(tree), icons_(icons) {
  if (tree_ != nullptr) {
    grimoire_.emplace(*tree_);
  }
}

ro::ui::SkillPointTally SkillSheet::Refresh(bool open, const ro::sim::World& world,
                                            std::uint32_t now_ms) {
  if (!open || !grimoire_.has_value() || icons_ == nullptr) {
    return {};
  }
  const ro::sim::Actor* self = world.player();
  const std::uint32_t job = self != nullptr ? self->look.job : 0;
  grimoire_->Refresh(job, world.skill_book(), world.player_status().skill_point);
  view_.Rebuild(*tree_, *grimoire_, job, *icons_, world.skill_book(), use_levels_, now_ms);
  return ro::ui::SkillPointTally{.left = grimoire_->points_left(),
                                 .reserved = grimoire_->reserved_points()};
}

void SkillSheet::Apply(const ro::ui::SkillWindowAction& action, SkillBookPanel& panel,
                       ro::net::Connection& link, ro::sim::World& world, std::uint32_t now_ms) {
  // ── LE RANG AUQUEL ON LANCE ───────────────────────────────────────────────
  if (action.use_level_for != 0) {
    const ro::sim::KnownSkill* known = world.skill_book().Find(action.use_level_for);
    if (known != nullptr) {
      use_levels_.Set(action.use_level_for, action.use_level, known->level);
    }
  }
  // ── LANCER DEPUIS LA GRILLE ───────────────────────────────────────────────
  //
  // 🔴 AU RANG D'UTILISATION, ET C'EST TOUT L'OBJET DU RÉGLAGE. Une COPIE du
  // livre, pas un pointeur : `ChooseSkill` garde ce qu'on lui donne jusqu'au clic
  // sur la cible, et le rang qu'on veut n'est pas celui que le serveur a envoyé.
  if (action.cast != 0) {
    const ro::sim::KnownSkill* known = world.skill_book().Find(action.cast);
    if (known != nullptr) {
      ro::sim::KnownSkill chosen = *known;
      const int wanted = use_levels_.Of(action.cast);
      if (wanted > 0 && wanted <= chosen.level) {
        chosen.level = static_cast<std::uint16_t>(wanted);
      }
      panel.ChooseSkill(link, world, chosen, now_ms);
    }
  }
  SendStaffOrder(action, link, world);
  if (grimoire_.has_value()) {
    ApplySkillAction(action, *grimoire_, link);
  }
  // 🔴 LA RÉSERVATION SE VIDE AVEC L'ORDRE, et APRÈS lui : les deux commandes
  // qui refont l'arbre court-circuitent la mécanique de réservation, et un point
  // réservé sur une compétence qu'« oublier tout » vient d'effacer se ferait
  // jeter au tri de l'image suivante — mais après avoir compté pour rien dans
  // l'en-tête. Le temps de recharge, lui, ne touche pas l'arbre : il ne vide rien.
  const bool remakes_tree = action.staff_order == ro::ui::StaffSkillOrder::kForgetAll ||
                            action.staff_order == ro::ui::StaffSkillOrder::kLearnAll;
  if (remakes_tree && grimoire_.has_value()) {
    grimoire_->CancelReservations();
  }
}

}  // namespace moonclient
