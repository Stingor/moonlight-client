#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/craft_log_view.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// L'AFFINAGE D'ARME : la fenêtre du skill Upgrade Weapon — la liste, les
// minerais, le plafond et la chance exacte, la chaîne automatique et son
// journal.
//
// 🔴 ELLE NE DÉCIDE DE RIEN. La liste vient du serveur ; ce qui se fait d'un
// geste — confirmer, envoyer, relancer, enchaîner — est décidé par la partie,
// qui recopie ici ce qu'il faut montrer. La fenêtre rend des VERBES.
//
// ⚠ SON OUVERTURE N'APPARTIENT PAS AU JOUEUR : c'est la liste du serveur qui
// l'ouvre. Et elle SURVIT à la tentative, pour qu'on lise le résultat et qu'on
// enchaîne.

inline constexpr const char* kWeaponRefineWindowTitle = core::TrText("Refine Weapon");
inline constexpr const char* kWeaponRefineWindowId = "weapon_refine";

// Les trois minerais, dans l'ordre des niveaux d'arme.
inline constexpr std::size_t kRefineOreCount = 3;

inline constexpr std::size_t kRefineFilterBytes = 64;

// UNE ARME DE LA LISTE.
struct WeaponRefineRowView {
  // La place à renvoyer, TELLE QUELLE — et l'identité de la ligne : la liste se
  // retrie, la place reste.
  std::int32_t index = 0;
  std::uint32_t item_id = 0;
  int refine = 0;
  // Le nom sans son « +N », que la colonne « + » porte déjà.
  std::string_view bare_name;
  // Le nom composé entier : c'est lui que le filtre cherche.
  std::string_view full_name;
  SkinImage icon;
  // Les cases de carte que la pièce possède, et combien sont prises.
  int slots = 0;
  int cards = 0;
  // La tentative atteint-elle le plafond du skill ?
  bool at_cap = false;
};

struct RefineOreView {
  std::uint32_t item_id = 0;
  std::string_view name;
  SkinImage icon;
  int owned = 0;
};

// CE QUE LA CHAÎNE AUTOMATIQUE FAIT, par priorité — un seul message à la fois.
enum class RefineChainNote : std::uint8_t {
  kNone,
  kRefineSoon,  // l'affinage automatique va partir
  kChaining,    // relance de la chaîne complète
  kRecasting,   // relance du skill seule
  kPaused,      // arrêtée à la demande
  kStopped,     // arrêtée d'elle-même, motif à l'appui
  kReady,       // le réglage est coché, la fenêtre attend sa confirmation
};

struct WeaponRefineView {
  // Le serveur n'a proposé aucune arme.
  bool empty_list = false;
  // Une tentative est partie : la liste est morte côté serveur, et se grise.
  bool consumed = false;
  // Une tentative attend sa réponse.
  bool awaiting = false;
  // Une relance est en route : la fin de session ne se dit pas.
  bool relaunch_coming = false;
  // La chaîne complète tourne : le bouton central devient « Arrêter ».
  bool chain_running = false;
  std::span<const WeaponRefineRowView> rows;
  std::array<RefineOreView, kRefineOreCount> ores = {};
  // Le niveau appris du skill, qui EST le plafond. Zéro : inconnu.
  int cap = 0;
  // La chance EXACTE de la tentative choisie, en pour cent. Moins un : on se tait.
  int chance = -1;
  RefineChainNote chain_note = RefineChainNote::kNone;
  // Le compteur du message de chaîne, et le motif d'arrêt.
  int chain_count = 0;
  std::string_view stop_reason;
  std::string_view last_result;
  SkinTone last_result_tone = SkinTone::kText;
  std::span<const CraftLogLineView> history;
  // La cible de la confirmation : nom composé, raffinement d'avant.
  std::string_view confirm_name;
  int confirm_refine = 0;

  // ── LES RÉGLAGES QUI CHANGENT LE DESSIN ─────────────────────────────────
  bool show_cards = true;
  bool show_filter = true;
  bool show_history = false;
  bool log_time = true;
  bool enter_key = false;
  bool hover_preview = true;

  // LA CHAÎNE EST ARMÉE POUR CETTE FENÊTRE : réglage coché ET confirmé depuis
  // son ouverture. Son bouton dit alors « Arrêter ».
  bool chain_armed = false;
};

enum class WeaponRefineVerb : std::uint8_t {
  kNothing,
  // Affiner l'arme `index` — bouton, double-clic ou Entrée. La partie décide
  // s'il faut d'abord confirmer.
  kRefine,
  // La modale a dit oui, ou non.
  kConfirm,
  kDismissConfirm,
  kRecast,
  // Arrêter la chaîne : elle se désarme, et le réglage se décoche.
  kStop,
  // La modale de la chaîne a dit oui : le réglage se coche, et la chaîne part.
  kStartChain,
  // 🔴 FERMER RÉPOND AU SERVEUR : c'est l'annulation qui désarme son skill.
  kClose,
};

struct WeaponRefineWindowAction {
  WeaponRefineVerb verb = WeaponRefineVerb::kNothing;
  std::int32_t index = 0;
  // La description d'une arme du sac (clic droit), ou d'un minerai (clic).
  std::int32_t describe_slot = 0;
  std::uint32_t describe_item_id = 0;
  bool open_settings = false;
  // Le survol, pour l'aperçu de description que l'étage dessine.
  std::int32_t hovered_slot = 0;
  std::uint32_t hovered_id = 0;
  // Ce que la liste a RÉELLEMENT montré à cette image : la chaîne vise la
  // sélection si elle est visible, sinon la première ligne affichée.
  bool list_drawn = false;
  bool selection_visible = false;
  std::int32_t first_visible = kNoRefineRow;
  static constexpr std::int32_t kNoRefineRow = -1;
};

struct WeaponRefineWindowState {
  // ⚠ CE N'EST PAS LE JOUEUR QUI LE POSE : la partie l'ouvre à l'arrivée d'une
  // liste, et le referme sur demande.
  bool open = false;
  // Le front montant : au premier plan à l'ouverture, et seulement là.
  bool raise = false;
  // L'arme choisie, par sa PLACE. Moins un : aucune.
  std::int32_t selected = WeaponRefineWindowAction::kNoRefineRow;
  std::array<char, kRefineFilterBytes> filter = {};
  // La confirmation : la place visée, moins un sans modale. `confirm_opened`
  // se lève à l'image qui l'ouvre, pour que la frappe d'Entrée qui l'a
  // déclenchée ne la valide pas aussitôt.
  std::int32_t confirm_index = WeaponRefineWindowAction::kNoRefineRow;
  bool confirm_opened = false;
  int confirm_frame = 0;
  // La confirmation de la chaîne automatique, ouverte par son bouton.
  bool chain_confirm = false;
  bool chain_confirm_opened = false;
};

[[nodiscard]] WeaponRefineWindowAction DrawWeaponRefineWindow(WeaponRefineWindowState& state,
                                                              const WeaponRefineView& view,
                                                              const SkinArt& art);

}  // namespace ro::ui
