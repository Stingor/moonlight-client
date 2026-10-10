#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "ro/ui/skin_art.h"
#include "ro/ui/staff_homunculus.h"

namespace ro::ui {

// ── L'ONGLET HOMONCULE DE LA FICHE ──────────────────────────────────────────
//
// Les deux fenêtres de l'homoncule du client d'origine en une : sa fiche
// d'état, et la liste de ses compétences, avec ce
// qu'elle comble : l'expérience courante ET requise, l'état
// « déjà renommé », un « + » seulement là où le serveur accepterait de monter,
// et l'abandon confirmé par le nom retapé.
//
// ⚠ ELLE NE DÉCIDE DE RIEN : `app` dit ce que le serveur a envoyé, et les
// gestes partent en intentions.

// Le nom tient en vingt-trois octets et son terminateur : c'est la borne du
// serveur, imposée par la saisie elle-même.
inline constexpr std::size_t kHomunculusNameBytes = 24;

// ── LA SECTION « STAFF » ────────────────────────────────────────────────────
//
// Les bornes des commandes du serveur : la faim se règle sur cent, l'intimité
// sur mille — l'échelle même que le serveur envoie au client.
inline constexpr int kHomunculusStaffHungerMost = 100;
inline constexpr int kHomunculusStaffIntimacyMost = 1000;
// Le serveur ne mute qu'un homoncule ÉVOLUÉ d'au moins ce niveau.
inline constexpr int kHomunculusMutateLevelLeast = 99;
// La phrase de `@homtalk` : le serveur en lit quatre-vingt-dix-neuf octets.
inline constexpr std::size_t kHomunculusTalkBytes = 100;
// LE TEMPS PENDANT LEQUEL UNE VALEUR ENVOYÉE EST ATTENDUE, en secondes. Le
// serveur renvoie la fiche en une fraction de seconde ; un refus, lui, ne
// renvoie rien, et le curseur doit bien finir par revenir à la valeur servie.
inline constexpr double kHomunculusStaffAwaitSeconds = 5.0;

// UNE VALEUR QU'UN CURSEUR DU STAFF A ENVOYÉE, et que le serveur n'a pas encore
// renvoyée.
struct HomunculusStaffPending {
  int value = 0;
  // L'instant de l'envoi, sur l'horloge d'ImGui, en secondes.
  double sent_at_s = 0.0;
};

// VRAI TANT QUE `pending` EST ATTENDUE : la valeur servie ne l'a pas rejointe
// et le délai n'est pas passé. Le curseur la montre pendant ce temps, au lieu
// de la valeur servie, périmée.
[[nodiscard]] bool HomunculusStaffAwaited(const std::optional<HomunculusStaffPending>& pending,
                                          int served, double now_s) noexcept;

// CE QU'A FAIT UN CURSEUR DU STAFF pendant une image.
struct HomunculusStaffGesture {
  // La valeur a bougé : glisser, flèche ou saisie.
  bool changed = false;
  // Le curseur est saisi : on le glisse, ou on tape sa valeur.
  bool active = false;
  // Il vient d'être lâché après avoir changé.
  bool released_after_edit = false;
  // Le bouton gauche est tenu.
  bool mouse_held = false;
};

// LA VALEUR À ENVOYER au terme du geste, rien tant qu'il dure.
//
// 🔴 RIEN NE PART PENDANT LE GESTE : la valeur vit dans `draft` tant qu'on
// glisse, et tant qu'une flèche est tenue — elle répète sans saisir le
// curseur, donc seul le relâchement du bouton dit que le geste est fini.
[[nodiscard]] std::optional<int> HomunculusStaffSettle(int value,
                                                       const HomunculusStaffGesture& gesture,
                                                       std::optional<int>& draft);

// LES NIVEAUX QUE `@homlevel` DOIT AJOUTER pour mener `served` à `target`, rien
// quand il n'y a rien à ajouter.
//
// 🔴 RIEN NON PLUS TANT QU'UN NIVEAU EST ATTENDU (`awaited`) : la commande est
// relative et irréversible, et la différence calculée sur un niveau servi
// périmé ajouterait une seconde fois les mêmes niveaux.
[[nodiscard]] std::optional<int> HomunculusStaffLevelsToAdd(int target, int served,
                                                            bool awaited) noexcept;

// LES HUIT PALIERS D'INTIMITÉ, dans l'ordre du serveur. ⚠ `ui` NE CONNAÎT PAS
// `sim` : `app` traduit.
enum class HomunculusIntimacyLabel : std::uint8_t {
  kHateWithPassion,
  kHate,
  kAwkward,
  kShy,
  kNeutral,
  kCordial,
  kLoyal,
  kUnknown,
};

// UNE LIGNE DE COMPÉTENCE.
struct HomunculusSkillRow {
  std::uint16_t skill_id = 0;
  // Vide quand l'arbre de l'installation ne la nomme pas.
  std::string_view name;
  // Le nom INTERNE (« HAMI_CASTLE »), que seul le staff voit au survol. Vide
  // quand l'arbre de l'installation ne la porte pas.
  std::string_view idname;
  SkinImage icon;
  int level = 0;
  int sp_cost = 0;
  int range_cells = 0;
  bool passive = false;
  // ELLE SE LANCE SUR SOI : elle part tout de suite. Les autres s'arment, et le
  // clic suivant désigne leur cible ou leur case.
  bool self_cast = false;
  // Le verdict du serveur : un niveau de plus serait accepté.
  bool upgradable = false;
};

struct HomunculusTabView {
  // Vrai quand le serveur a dit qu'il existe.
  bool present = false;
  // La fiche est arrivée.
  bool sheet_known = false;

  std::string_view name;
  // Le nom de ressource de l'espèce (« AMISTR »).
  std::string_view species;
  int level = 0;
  bool renamed = false;
  bool resting = false;
  bool alive = false;

  std::int64_t hp = 0;
  std::int64_t max_hp = 0;
  std::int64_t sp = 0;
  std::int64_t max_sp = 0;
  std::int64_t exp = 0;
  std::int64_t exp_next = 0;
  int hunger = 0;
  int intimacy = 0;
  HomunculusIntimacyLabel intimacy_label = HomunculusIntimacyLabel::kUnknown;
  bool auto_feed = false;

  int atk = 0;
  int matk = 0;
  int hit = 0;
  int crit = 0;
  int def = 0;
  int mdef = 0;
  int flee = 0;
  int aspd = 0;
  int amotion_ms = 0;
  int attack_range_cells = 0;

  int skill_points = 0;
  std::span<const HomunculusSkillRow> skills;

  // LE PROFIL D'IA EN VIGUEUR, déjà traduit ; vide sans IA à régler.
  std::string_view ai_profile;
  // LA PROPOSITION DE RECOPIE de profil, déjà traduite ; vide sans elle.
  std::string_view ai_proposal;

  // LE DROIT DU STAFF, relu par `app` à chaque image : il montre la section
  // « Staff », et l'identifiant au survol d'une compétence. ⚠ D'AFFICHAGE
  // SEULEMENT : le serveur revalide chaque commande.
  bool staff_allowed = false;
  // ÉVOLUÉ, quand `app` connaît la classe ; vide sinon, et le serveur juge.
  std::optional<bool> evolved;
  // Les formes que `@hommutate` peut donner.
  std::span<const StaffHomunculusChoice> mutations;
  // LE NIVEAU LE PLUS HAUT que le serveur laisse atteindre à CET homoncule :
  // une forme mutée a son propre plafond. Posé par `app` depuis les règles du
  // serveur.
  int staff_level_most = 0;

  // ACTIF : ni au repos, ni sans PV. Le serveur jette sans un mot le
  // nourrissage, le renommage et l'abandon d'un homoncule qui ne l'est pas.
  [[nodiscard]] bool active() const noexcept { return sheet_known && !resting && alive; }
};

struct HomunculusTabState {
  bool renaming = false;
  std::array<char, kHomunculusNameBytes> new_name = {};
  bool delete_asked = false;
  std::array<char, kHomunculusNameBytes> delete_confirm = {};
  // Le compte rendu du dernier geste.
  std::string status;
  // La section des compétences, repliable — ouverte par défaut.
  bool skills_open = true;

  // ── LA SECTION « STAFF », repliable — fermée par défaut ───────────────────
  bool staff_open = false;
  // 🔴 LES BROUILLONS NE VIVENT QUE PENDANT LE GESTE — glisser, ou flèche
  // tenue. Hors geste, chaque curseur montre la valeur envoyée tant qu'elle est
  // attendue, puis ce que le serveur a renvoyé.
  std::optional<int> hunger_draft;
  std::optional<int> intimacy_draft;
  // Le niveau VISÉ, et non le nombre de niveaux à ajouter.
  std::optional<int> level_draft;
  std::optional<HomunculusStaffPending> hunger_pending;
  std::optional<HomunculusStaffPending> intimacy_pending;
  // ⚠ TANT QU'IL EST LÀ, LE CURSEUR DU NIVEAU EST GRISÉ : voir
  // `HomunculusStaffLevelsToAdd`.
  std::optional<HomunculusStaffPending> level_pending;
  // Le rang de la forme choisie pour `@hommutate`.
  int mutation_rank = 0;
  std::array<char, kHomunculusTalkBytes> talk = {};
};

enum class HomunculusVerb : std::uint8_t {
  kNothing,
  kRename,
  kFeed,
  kAutoFeed,
  kDelete,
  kSkillUp,
  kUseSkill,
  kDescribeSkill,
  // Ouvrir la fenêtre « IA du compagnon ».
  kOpenAi,
  // Répondre à la proposition de recopie de profil.
  kAcceptAiProposal,
  kDeclineAiProposal,
  // Une commande du staff, voir `HomunculusStaffOrder`.
  kStaff,
};

// 🔴 UNE INTENTION, ET NON LA COMMANDE : c'est `app` qui sait que « régler la
// faim » s'écrit « @homhungry ».
enum class HomunculusStaffOrder : std::uint8_t {
  kNone,
  // Pose la faim à `staff_amount` (0 à 100).
  kSetHunger,
  // Pose l'intimité à `staff_amount` (0 à 1000).
  kSetIntimacy,
  // AJOUTE `staff_amount` niveaux : la commande du serveur est relative.
  kAddLevels,
  // Retire au sort les statistiques de croissance.
  kShuffle,
  // Écrit les statistiques et leurs bornes de croissance.
  kStats,
  // Écrit la fiche résumée.
  kInfo,
  kEvolve,
  // Mute vers la classe `staff_job`.
  kMutate,
  // Fait dire `staff_text` à l'homoncule.
  kTalk,
};

struct HomunculusTabAction {
  HomunculusVerb verb = HomunculusVerb::kNothing;
  // `kRename` : le nom tapé, en UTF-8.
  std::string name;
  // `kAutoFeed` : la valeur demandée.
  bool auto_feed = false;
  std::uint16_t skill_id = 0;
  int skill_level = 0;
  // `kStaff` : la commande et ce qu'elle porte.
  HomunculusStaffOrder staff_order = HomunculusStaffOrder::kNone;
  int staff_amount = 0;
  std::uint16_t staff_job = 0;
  std::string staff_text;
};

[[nodiscard]] HomunculusTabAction DrawHomunculusTab(HomunculusTabState& state,
                                                    const HomunculusTabView& view,
                                                    const SkinArt& art);

}  // namespace ro::ui
