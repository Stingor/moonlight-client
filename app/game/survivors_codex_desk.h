#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ro/sim/survivors_rules.h"
#include "ro/ui/survivors_codex_window.h"

#include "survivors/survivors_save.h"
#include "survivors/survivors_sightings.h"

namespace moonclient {

// ── LES FICHES DU CODEX, tirées des règles ──────────────────────────────────
//
// La vue entière, composée depuis `SurvivorsRules`, le carnet et le relevé de
// la partie en cours, en nombres, clés et identifiants : les noms, icônes et
// sprites se posent ensuite, comme pour l'Album. Aucun chiffre n'est recopié :
// chacun se lit dans le catalogue, et une entrée qui y entre a sa fiche sans
// une ligne de plus ici.

// LES LIGNES QUE LES SPANS DE LA VUE DÉSIGNENT, gardées d'une image à l'autre.
// Chaque fiche a ses vecteurs : un vecteur déplacé garde son tampon, donc les
// spans restent valides quand la liste grandit.
struct SurvivorsCodexRowsHeld {
  std::array<std::vector<ro::ui::SurvivorsCodexEntry>, ro::ui::kSurvivorsCodexTabCount> entries;
  std::vector<std::vector<ro::ui::SurvivorsCodexFigure>> figures;
  std::vector<std::vector<ro::ui::SurvivorsCodexSource>> sources;
  std::vector<std::vector<ro::ui::SurvivorsCodexAffinity>> affinities;
  std::vector<std::vector<ro::ui::SurvivorsCodexMove>> moves;
  // Les clés de Codex que les entrées désignent par `key` : une file à deux
  // bouts ne déplace pas ses chaînes quand elle grandit, si bien que le texte
  // d'une clé courte, logé dans la chaîne même, reste en place.
  std::deque<std::string> keys;

  void Clear();
  // Garde des lignes, et rend la span qui les désigne.
  std::span<const ro::ui::SurvivorsCodexFigure> Hold(
      std::vector<ro::ui::SurvivorsCodexFigure> rows);
  std::span<const ro::ui::SurvivorsCodexSource> Hold(
      std::vector<ro::ui::SurvivorsCodexSource> rows);
  std::span<const ro::ui::SurvivorsCodexAffinity> Hold(
      std::vector<ro::ui::SurvivorsCodexAffinity> rows);
  std::span<const ro::ui::SurvivorsCodexMove> Hold(std::vector<ro::ui::SurvivorsCodexMove> rows);
  // Une clé gardée, et son texte stable.
  const char* HoldKey(std::string key);
};

// ── LES SORTS, LES OBJETS ET LES PLANTES (survivors_codex_desk.cpp) ─────────

// Les lignes d'un sort qui ont une valeur non nulle à un niveau au moins, dans
// l'ordre fixe de la fiche : dégâts, recharge, quantité, perce, portée, aire,
// longueur, durée, touches, ralentissement, gel, recul, rebonds, éclats. Les
// mesures en dixièmes de case, les durées en ms. Les champs qui disent
// comment le sort se joue (vitesse, période, rayon d'impact…) ne se montrent
// pas.
[[nodiscard]] std::vector<ro::ui::SurvivorsCodexFigure> SkillFiguresOf(
    std::span<const ro::sim::SkillLevel> levels);

// Une mesure en sous-unités, en dixièmes de case arrondis.
[[nodiscard]] std::int64_t CellTenthsOf(std::int64_t units) noexcept;

// Les sorts, dans l'ordre des règles (la réserve, puis les évolués) ; un sort
// de base nomme son évolution, un évolué ce dont il vient. Un sort que
// verrouille un exploit porte la clé de cet exploit.
void CodexSkillsOf(const ro::sim::SurvivorsRules& rules, const SurvivorsCodexSeen& seen,
                   SurvivorsCodexRowsHeld& held);
// Les objets au sol dans l'ordre des règles, le coffre compris, puis les
// gemmes par seuil croissant, puis la carte au sol.
void CodexItemsOf(const ro::sim::SurvivorsRules& rules, const SurvivorsCodexSeen& seen,
                  SurvivorsCodexRowsHeld& held);
// Les plantes dans l'ordre des règles, avec l'objet qu'elles lâchent et leur
// poids au tirage sur la somme des poids.
void CodexPlantsOf(const ro::sim::SurvivorsRules& rules, const SurvivorsCodexSeen& seen,
                   SurvivorsCodexRowsHeld& held);

// ── LES MONSTRES, LES BOSS ET LES ÉVÉNEMENTS (survivors_codex_foes.cpp) ─────

// QUAND ET PAR OÙ UN MONSTRE ARRIVE : le plus tôt de quatre sources — les
// vagues, les événements, les escortes et les invocations des boss. Une vague
// à plancher et lot nuls ne fait venir personne : elle est écartée. Chaque
// source une fois, à sa première heure, par heure croissante.
struct FoeArrival {
  std::optional<std::uint32_t> arrives_ms;
  std::vector<ro::ui::SurvivorsCodexSource> sources;
};
[[nodiscard]] FoeArrival FoeArrivalOf(const ro::sim::SurvivorsRules& rules, std::uint16_t foe_id);

// CE QUE CHAQUE ÉLÉMENT D'ATTAQUE FAIT À UNE DÉFENSE : au-dessus de 100 % une
// faiblesse, en dessous une résistance (0 % : une immunité), chacune par écart
// à 100 décroissant.
struct FoeAffinities {
  std::vector<ro::ui::SurvivorsCodexAffinity> weaknesses;
  std::vector<ro::ui::SurvivorsCodexAffinity> resistances;
};
[[nodiscard]] FoeAffinities AffinitiesOf(const ro::sim::SurvivorsRules& rules,
                                         ro::sim::SurvivorsElement defence);

// Les monstres communs, par heure d'arrivée puis dans l'ordre des règles.
void CodexFoesOf(const ro::sim::SurvivorsRules& rules, const SurvivorsCodexSeen& seen,
                 SurvivorsCodexRowsHeld& held);
// LA CHRONOLOGIE : les boss et les événements dans l'ordre de l'heure, les
// élites à leur première heure, la nuit à sa tombée. À la même heure, l'ordre
// des règles : boss, événements, élites, nuit.
void CodexBossesOf(const ro::sim::SurvivorsRules& rules, const SurvivorsCodexSeen& seen,
                   SurvivorsCodexRowsHeld& held);

// ── LA VUE ──────────────────────────────────────────────────────────────────

// CE QUI COMPTE COMME VU : le carnet, plus le relevé de la partie en cours
// quand il y en a une (`live`), plus ce que le carnet permet de déduire sans
// rien écrire (Q2) — les boss, événements, élites, la nuit et les monstres
// des vagues atteints avant le meilleur temps survécu, le monstre de chaque
// carte trouvée, le premier sort de chaque record.
[[nodiscard]] SurvivorsCodexSeen CodexSeenOf(const ro::sim::SurvivorsRules& rules,
                                             const SurvivorsSave& save,
                                             const SurvivorsCodexSeen* live);

// La vue entière ; `held` est vidé puis rempli, et doit survivre à la vue.
[[nodiscard]] ro::ui::SurvivorsCodexView ComposeSurvivorsCodex(const ro::sim::SurvivorsRules& rules,
                                                               const SurvivorsCodexSeen& seen,
                                                               SurvivorsCodexRowsHeld& held);

}  // namespace moonclient
