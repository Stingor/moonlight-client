#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <utility>
#include <vector>

#include "ro/sim/actor.h"
#include "ro/sim/skill_book.h"
#include "ro/sim/world_fwd.h"

namespace moonclient {

// ── LE MODE CIBLAGE ─────────────────────────────────────────────────────────
//
// Un mode de jeu optionnel, éteint par défaut : la cible est la dernière entité
// DÉSIGNÉE, et elle le reste quand on cesse de l'attaquer. Un HUD la montre, une
// flèche la surmonte, des raccourcis la font passer d'un monstre à l'autre, et
// quelques réglages changent ce que le clic veut dire.
//
// La désignation vit dans le monde (`World::Select`) : ce module décide QUAND
// elle change. Un GESTE la pose — un clic, un cyclage, un sort qui part —, jamais
// un sondage : une cible perdue reste perdue tant qu'on ne l'a pas redésignée.
//
// Éteint, le mode est un vrai maître : aucun geste ne désigne, aucun raccourci
// n'agit, aucun clic ne change de sens.

struct TargetingSettings {
  bool enabled = false;
  // La flèche au-dessus de la cible.
  bool native_marker = true;
  // Le clic désigne sans engager ; le double-clic attaque.
  bool click_no_attack = false;
  // Une compétence ciblée armée au clavier part sur la cible.
  bool cast_on_target = false;
  // Cliquer un cadre du HUD revient à cliquer l'entité.
  bool proxy_click = false;
  bool cycle_wrap = true;
  // Passé ce délai sans cycler, le cycle repart du plus proche. Zéro : jamais.
  int cycle_reset_ms = 1000;
  // Échap lâche la cible avant d'ouvrir le menu du jeu.
  bool escape_clears = false;
  // Le portrait : orientation 0 à 7 (0 = de face), rang d'animation, et s'il
  // défile.
  int portrait_direction = 0;
  int portrait_animation = 0;
  bool portrait_animated = true;
};

// Un monstre qu'un cyclage peut prendre : vivant, visible, à l'écran.
struct TargetCandidate {
  std::uint32_t id = 0;
  // Au carré : la racine ne servirait qu'à ralentir un tri.
  float distance_squared = 0.0F;
};

// Les actions d'une demande d'attaque, telles que le serveur les lit : zéro
// frappe une fois, sept sans fin.
inline constexpr int kAttackActionOnce = 0;
inline constexpr int kAttackActionRepeat = 7;
// La demande d'attaque est jetée.
inline constexpr int kAttackDropped = -1;

// Un sort qui vise une entité : offensif, ou de soutien.
enum class SkillAimKind : std::uint8_t { kOffensive, kSupport };
[[nodiscard]] SkillAimKind SkillAimKindOf(const ro::sim::KnownSkill& skill);

// Les redemandes que la cible courante appelle cette image.
struct TargetRequests {
  std::uint32_t info_id = 0;
  std::uint32_t statuses_id = 0;
  std::uint32_t name_id = 0;
};

// Une entité qu'on ne suit plus : cachée, ou refusée par le serveur.
[[nodiscard]] bool TargetIsConcealed(const ro::sim::Actor& actor);

// Cet acteur reçoit-il une flèche ? Jamais un portail, jamais une entité cachée.
[[nodiscard]] bool TargetTakesMarker(const ro::sim::Actor& actor);

// Les monstres qu'un cyclage parcourt, triés du plus proche au plus loin. L'ordre
// ne dépend pas de celui, arbitraire, du monde : deux passages donnent la même
// suite.
void SortByDistance(std::vector<TargetCandidate>& candidates);

class Targeting {
 public:
  explicit Targeting(const TargetingSettings& settings) : settings_(&settings) {}

  [[nodiscard]] const TargetingSettings& settings() const noexcept { return *settings_; }
  [[nodiscard]] bool enabled() const noexcept { return settings_->enabled; }

  // ── LES GESTES QUI DÉSIGNENT ──────────────────────────────────────────────

  // Un clic sur une entité, ou la demande d'une autre surface. Rend faux si le
  // mode est éteint ou que personne ne porte cet identifiant.
  bool Designate(ro::sim::World& world, std::uint32_t id);

  // « Cibler » depuis une surface — un menu, une fenêtre : la désignation, et
  // la sélection du jeu avec elle.
  bool DesignateByProxy(ro::sim::World& world, std::uint32_t id);

  // LA SÉLECTION DU JEU QUE LE MODE VIENT DE RÉÉCRIRE, et qu'on lit une fois :
  // le ciblage au clavier et « Cibler » la posent, le déciblage la remet à zéro.
  // Aucun ne lève l'engagement de la souris : sous un engagement, la flèche
  // saute sur la nouvelle sélection ; sans lui, c'est la cible du HUD qui la
  // reçoit de toute façon.
  [[nodiscard]] std::optional<std::uint32_t> TakeSelectionWrite() noexcept {
    return std::exchange(selection_write_, std::nullopt);
  }

  // Le monstre suivant (`forward`) ou précédent parmi ceux de l'écran. Rend faux
  // quand il n'y a rien à cibler.
  bool CycleTarget(ro::sim::World& world, bool forward, std::uint32_t now_ms);

  // Le monstre le plus proche à l'écran, sans rien parcourir.
  bool TargetNearest(ro::sim::World& world, std::uint32_t now_ms);

  // Lâche la cible. Rend faux quand il n'y avait rien à lâcher : c'est ce qui
  // rend la touche Échap au jeu.
  bool ClearTarget(ro::sim::World& world);

  // Un sort part sur cette entité : le dernier geste gagne, y compris sur un
  // cadavre ou un allié que le clic ne désigne pas. Un sort sur soi ne change
  // rien.
  void NoteSkillTarget(ro::sim::World& world, std::uint32_t id);

  // Les monstres ciblables de l'image, relevés par qui dessine la scène.
  void NoteOnScreen(std::vector<TargetCandidate> candidates);

  // Perd la cible qui s'est cachée, ou que le serveur dit hors de portée.
  void Revalidate(ro::sim::World& world) const;

  // ── LE CLIC QUI N'ENGAGE PAS ──────────────────────────────────────────────

  // Vrai si ce clic sur un monstre ou un joueur ne doit rien armer : la cible,
  // elle, est déjà désignée. Consomme le passage qu'un ordre explicite a ouvert.
  bool SuppressClickEngage(bool skill_armed);

  // L'action à laisser partir pour cette demande d'attaque, ou `kAttackDropped`.
  // Une dispense d'un seul coup se consomme ici, et réécrit l'action à un coup.
  int FilterBasicAttack(std::uint32_t target_id, int action);

  // Un ordre d'attaque explicite sur `id` : il ouvre UN passage dans le chemin du
  // clic, et une dispense du coup pour ce GID — durable, ou d'un seul coup.
  void NoteExplicitAttack(std::uint32_t id, bool once);

  // Hors siège, CTRL tenu ou `/noctrl` allumé : l'attaque continue. Le siège
  // est celui que la carte a annoncé — château ou champ de bataille, heure de
  // guerre ou non : voir `SiegeMap`.
  [[nodiscard]] static bool WantsContinuousAttack(bool siege_map, bool ctrl_held, bool no_ctrl);

  // ── LE DOUBLE-CLIC DANS LE MONDE ──────────────────────────────────────────

  // Un appui du bouton gauche sur le monde, second d'un double-clic ou non —
  // voir `DoubleClickClock`. Le second attend d'être rejoué.
  void NoteWorldPress(bool second_of_double);

  // L'acteur qui a pris cet appui, zéro pour le sol. Le clic le dit une fois
  // qu'il l'a trouvé ; un appui qui n'en dit rien compte comme le sol.
  void NotePressedOn(std::uint32_t id) { pressed_on_id_ = id; }

  // L'identifiant à attaquer au titre du double-clic, ou zéro. Ne rend quelque
  // chose que sous « le clic cible sans attaquer », et que si le second appui
  // est tombé SUR la cible : un double-clic au sol n'attaque rien, puisque
  // seul compte ce que l'appui vient de désigner.
  [[nodiscard]] std::uint32_t TakeDoubleClickTarget(const ro::sim::World& world, bool skill_armed);

  // ── LES CADRES QUI AGISSENT COMME LA CIBLE ────────────────────────────────

  void NoteFrameClicks(const ro::sim::World& world, bool left, bool right);
  // L'identifiant cliqué au bouton gauche, ou zéro ; puis au bouton droit.
  [[nodiscard]] std::uint32_t TakeFrameClick();
  [[nodiscard]] std::uint32_t TakeFrameMenu();
  // La souris était sur un cadre qui agit, à l'image précédente : un appui y
  // vaut un clic sur l'entité, et ne désarme pas la visée.
  void NoteFrameHover(bool hovered) noexcept { frame_hovered_ = hovered; }
  [[nodiscard]] bool frame_hovered() const noexcept { return frame_hovered_; }

  // Les cadres reprennent-ils la souris cette image ? Il faut une cible, des
  // cadres verrouillés, et que cliquer l'entité veuille dire quelque chose.
  [[nodiscard]] bool FramesAct(const ro::sim::World& world, bool locked,
                               bool ground_skill_armed) const;

  // ── MASQUER LA FENÊTRE POUR CETTE ENTITÉ ──────────────────────────────────
  //
  // « Pas celle-ci », et non un interrupteur : la cible reste désignée, mais ni
  // le HUD, ni la flèche, ni le sort au clavier ne la servent plus. N'importe
  // quelle désignation — la même entité comprise — la rallume.

  // Sans effet si `id` n'est pas la cible.
  void HideFor(const ro::sim::World& world, std::uint32_t id);
  [[nodiscard]] bool Hidden(const ro::sim::World& world) const;
  // La fenêtre de cible montre-t-elle CETTE entité en ce moment ?
  [[nodiscard]] bool ShownFor(const ro::sim::World& world, std::uint32_t id) const;

  // ── CE QUE LES AUTRES SURFACES LISENT ─────────────────────────────────────

  // L'acteur sur qui poser la flèche, ou nul.
  [[nodiscard]] const ro::sim::Actor* MarkedActor(const ro::sim::World& world) const;

  // Le mode répond-il à « sur qui ? » pour un sort armé au clavier ? C'est Quick
  // cast qui lance, relance et retire le cercle : voir `world/quick_cast.h`.
  [[nodiscard]] bool CastsOnTarget() const noexcept {
    return enabled() && settings_->cast_on_target;
  }

  // L'identifiant visé par une compétence armée au clavier, ou zéro : jamais
  // soi-même, jamais une entité cachée, et jamais un cadavre pour un sort
  // offensif.
  [[nodiscard]] std::uint32_t SkillTargetId(const ro::sim::World& world, SkillAimKind aim) const;

  // Ce qu'il faut redemander au serveur à cette image, selon les cadences.
  [[nodiscard]] TargetRequests Poll(const ro::sim::World& world, std::uint32_t now_ms,
                                    bool statuses_shown);

  // Le changement de carte purge la cible ; les gestes en attente partent avec.
  void Forget();

 private:
  bool ApplyKeyboardTarget(ro::sim::World& world, std::uint32_t id, std::uint32_t now_ms);

  const TargetingSettings* settings_;
  std::vector<TargetCandidate> on_screen_;
  std::uint32_t last_cycle_ms_ = 0;

  std::uint32_t explicit_attack_id_ = 0;
  bool explicit_engage_pending_ = false;
  bool explicit_attack_once_ = false;

  bool double_click_pending_ = false;
  std::uint32_t pressed_on_id_ = 0;

  std::uint32_t frame_click_id_ = 0;
  std::uint32_t frame_menu_id_ = 0;
  bool frame_hovered_ = false;
  // L'entité dont la fenêtre est masquée à la main, ou zéro.
  std::uint32_t hidden_id_ = 0;
  std::optional<std::uint32_t> selection_write_;

  // Les cadences de redemande, et la cible qu'elles servent : une nouvelle cible
  // les remet à zéro.
  std::uint32_t polled_id_ = 0;
  std::uint32_t info_polled_ms_ = 0;
  std::uint32_t statuses_polled_ms_ = 0;
  std::uint32_t name_polled_ms_ = 0;
  std::uint32_t refusals_seen_ = 0;
  // L'instant du dernier refus de la liste des états, par entité.
  std::map<std::uint32_t, std::uint32_t> statuses_refused_ms_;
};

}  // namespace moonclient
