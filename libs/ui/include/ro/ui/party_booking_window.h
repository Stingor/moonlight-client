#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "ro/core/i18n.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/window_name.h"

namespace ro::ui {

// LA RECHERCHE DE GROUPE : chercher les annonces de recrutement — par niveau,
// par lieu ou par métier —, publier la sienne, en changer les métiers, la
// retirer, et chuchoter à l'auteur d'une annonce.
//
// 🔴 LA FENÊTRE NE PARLE PAS AU SERVEUR : chaque bouton devient une intention,
// qu'`app` envoie. Elle ne connaît ni les codes de lieu ni ceux des métiers :
// la vue les lui donne tout faits, avec leurs noms.

inline constexpr const char* kPartyBookingWindowId = "party_booking";
inline constexpr const char* kPartyBookingWindowTitle = core::TrText("Recherche de groupe");

// LA TAILLE À LA PREMIÈRE OUVERTURE, en pixels d'interface.
inline constexpr float kPartyBookingFirstWidthPixels = 560.0F;
inline constexpr float kPartyBookingFirstHeightPixels = 520.0F;
// La largeur d'un champ de niveau et d'une liste déroulante.
inline constexpr float kPartyBookingLevelFieldPixels = 120.0F;
inline constexpr float kPartyBookingComboPixels = 200.0F;

// Les cases de métier d'une annonce, et la marque d'une case vide.
inline constexpr std::size_t kPartyBookingSlotCount = 6;
inline constexpr std::int16_t kPartyBookingEmptySlot = -1;
using PartyBookingSlots = std::array<std::int16_t, kPartyBookingSlotCount>;
inline constexpr PartyBookingSlots kPartyBookingNoSlots = {
    kPartyBookingEmptySlot, kPartyBookingEmptySlot, kPartyBookingEmptySlot,
    kPartyBookingEmptySlot, kPartyBookingEmptySlot, kPartyBookingEmptySlot};
// Le code de lieu « partout ».
inline constexpr std::int16_t kPartyBookingEverywhere = 0;

// UN LIEU, et le code qui part sur le fil.
struct PartyBookingPlaceView {
  std::string_view name;
  std::int16_t map_code = kPartyBookingEverywhere;
};

// UNE RÉGION et ses lieux.
struct PartyBookingRegionView {
  std::string_view name;
  std::span<const PartyBookingPlaceView> places;
};

// UN MÉTIER OU UN RÔLE recherché.
struct PartyBookingJobView {
  std::int16_t code = kPartyBookingEmptySlot;
  std::string_view name;
};

// UNE FAMILLE de métiers, déjà traduite.
struct PartyBookingJobGroupView {
  std::string_view label;
  std::span<const PartyBookingJobView> jobs;
};

// UNE ANNONCE, ses textes composés par `app`.
struct PartyBookingRowView {
  std::string_view leader;
  // « 40~55 »
  std::string_view levels;
  std::string_view place;
  std::string_view jobs;
  // L'heure de publication, « 14:05 ».
  std::string_view posted;
};

// CE QUE LA FENÊTRE MONTRE, composé par `app` à chaque image.
struct PartyBookingView {
  std::span<const PartyBookingRegionView> regions;
  std::span<const PartyBookingJobGroupView> job_groups;
  std::span<const PartyBookingRowView> rows;
  // Une recherche est partie, et sa page n'est pas arrivée.
  bool searching = false;
  // Au moins une page est arrivée.
  bool searched = false;
  bool more_on_server = false;
  // L'ANNONCE DU JOUEUR, si elle existe.
  bool has_mine = false;
  PartyBookingRowView mine;
  // Son rang : un autre rang recharge les métiers que l'on modifie.
  std::uint32_t mine_index = 0;
  PartyBookingSlots mine_jobs = kPartyBookingNoSlots;
  // Une publication attend son accusé.
  bool registering = false;
  int least_level = 1;
  int most_level = 1;
};

// LE FILTRE D'UNE RECHERCHE : le serveur filtre par lieu OU par métier.
enum class PartyBookingFilter : std::uint8_t {
  kNone,
  kPlace,
  kJob,
};

struct PartyBookingWindowState {
  bool open = false;
  bool was_open = false;
  // LA RECHERCHE. Un niveau nul cherche à tous les niveaux.
  int search_level = 0;
  PartyBookingFilter filter = PartyBookingFilter::kNone;
  // Rangs dans `PartyBookingView::regions` et dans ses lieux ; négatif : aucun.
  int search_region = -1;
  int search_place = 0;
  std::int16_t search_job = kPartyBookingEmptySlot;
  // LA PUBLICATION.
  int register_level = 1;
  int register_region = -1;
  int register_place = 0;
  PartyBookingSlots register_jobs = kPartyBookingNoSlots;
  // LES MÉTIERS EN COURS DE MODIFICATION, rechargés quand l'annonce change.
  PartyBookingSlots edited_jobs = kPartyBookingNoSlots;
  std::uint32_t edited_index = 0;
  bool edited_loaded = false;
  WindowName name{kPartyBookingWindowTitle, kPartyBookingWindowId};
};

enum class PartyBookingVerb : std::uint8_t {
  kNothing,
  kSearch,
  kNextPage,
  kRegister,
  kUpdate,
  kDelete,
  // Chuchoter à l'auteur, dans la conversation ou dans sa fenêtre.
  kWhisper,
  kWhisperWindow,
};

struct PartyBookingAction {
  PartyBookingVerb verb = PartyBookingVerb::kNothing;
  std::int16_t level = 0;
  std::int16_t map_code = kPartyBookingEverywhere;
  std::int16_t job = kPartyBookingEmptySlot;
  PartyBookingSlots jobs = kPartyBookingNoSlots;
  // L'auteur à qui chuchoter.
  std::string name;
};

// ── LES RÈGLES, séparées du dessin pour être éprouvées ──────────────────────

// Le nombre de cases remplies.
[[nodiscard]] std::size_t PartyBookingSlotsUsed(const PartyBookingSlots& slots);
// Coche ou décoche ce code. Les cases restent tassées au début. Faux, sans rien
// changer, quand il faudrait une septième case.
[[nodiscard]] bool TogglePartyBookingSlot(PartyBookingSlots& slots, std::int16_t code);
// Le lieu choisi par ces rangs, `kPartyBookingEverywhere` hors des bornes.
[[nodiscard]] std::int16_t PartyBookingChosenPlace(const PartyBookingView& view, int region,
                                                   int place);
// L'intention du bouton « Rechercher » : le niveau, puis le lieu OU le métier
// que le filtre choisit.
[[nodiscard]] PartyBookingAction PartyBookingSearchAsked(const PartyBookingWindowState& state,
                                                         const PartyBookingView& view);
// L'intention du bouton « Publier » : niveau, lieu et métiers cochés.
[[nodiscard]] PartyBookingAction PartyBookingRegisterAsked(const PartyBookingWindowState& state,
                                                           const PartyBookingView& view);

[[nodiscard]] PartyBookingAction DrawPartyBookingWindow(PartyBookingWindowState& state,
                                                        const PartyBookingView& view,
                                                        const SkinArt& art);

}  // namespace ro::ui
