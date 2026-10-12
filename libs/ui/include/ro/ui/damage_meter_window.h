#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include <imgui.h>

#include "ro/ui/chat_strip.h"

namespace ro::ui {

// ── LE COMPTEUR DE DÉGÂTS, posé sur le jeu ──────────────────────────────────
//
// Un en-tête qui dit l'état du combat et le
// rythme courant, une courbe en AIRE du rythme colonne par colonne, et trois
// tuiles de bilan — pic, moyenne, total. Aucun widget : tout est peint dans la
// liste de dessin de la fenêtre, parce qu'un HUD se lit d'un coup d'œil, comme
// une jauge, et pas comme une liste de libellés.
//
// Ce qu'il offre en plus :
//
// - QUATRE MODES — dégâts infligés, soins prodigués, dégâts subis, soins
//   reçus — et deux vues : NOUS, telle que décrite ci-dessus, ou LE GROUPE,
//   une barre par acteur et le détail par compétence au clic.
// - LE PALMARÈS du mode, un volet qui se déplie sous le bilan : les dix plus
//   gros faits, qui survivent aux combats. Chacun dit sa compétence, l'autre
//   bout et son âge ; une barre derrière lui le mesure au premier ; un fait qui
//   vient d'y entrer s'allume ; ceux du combat en cours portent un liseré.
// - UN MENU AU CLIC DROIT, qui porte tous les gestes du compteur sans passer
//   par le panneau de réglages — y compris quand il est verrouillé, depuis sa
//   barre de titre.
//
// ⚠ IL N'EST PAS DANS LA PILE D'ÉCHAP, comme le compteur d'images : c'est un
// instrument posé sur le jeu, et Échap destiné au menu le refermerait par
// surprise en plein combat.

// ⚠ JAMAIS TRADUIT : c'est la clé ImGui de la fenêtre, et son titre change à
// chaque image — il porte ce que le joueur y a coché. Ce qui suit les trois
// dièses fige l'identité,
// donc la place retenue.
inline constexpr const char* kDamageMeterWindowId = "###damage_meter";

// LE NOMBRE DE RANGS DU PALMARÈS. ⚠ LE VOLET A TOUJOURS CETTE HAUTEUR, rempli
// ou non : une fenêtre qui grandirait à chaque coup entrant sauterait sous
// l'œil en plein combat.
inline constexpr std::size_t kDamageMeterRecordRows = 10;

// CE QUE LE COMPTEUR MESURE — le miroir de `ro::sim::MeterMode`, que cette
// couche ne connaît pas.
enum class DamageMeterMode : std::uint8_t {
  kDamageDone = 0,
  kHealingDone,
  kDamageTaken,
  kHealingTaken,
  kCount,
};
inline constexpr std::size_t kDamageMeterModeCount =
    static_cast<std::size_t>(DamageMeterMode::kCount);

// LE SEGMENT LU par la vue du groupe : le combat en cours, le précédent, toute
// la session.
enum class DamageMeterSegment : std::uint8_t { kCurrent = 0, kPrevious, kSession, kCount };
inline constexpr std::size_t kDamageMeterSegmentCount =
    static_cast<std::size_t>(DamageMeterSegment::kCount);

// CE QUE LE CORPS MONTRE : NOUS — le rythme, la courbe, le bilan, le
// palmarès — ou LE GROUPE — une barre par acteur, et le détail d'un acteur
// par compétence.
enum class DamageMeterPane : std::uint8_t { kSelf = 0, kGroup };

// UNE BARRE DU GROUPE.
struct DamageMeterBarView {
  std::uint32_t id = 0;
  std::string_view name;
  std::int64_t amount = 0;
  float per_second = 0.0F;
  // Sa part du total du segment, de zéro à un.
  float share = 0.0F;
  bool self = false;
};

// UNE COMPÉTENCE DANS LE DÉTAIL D'UN ACTEUR.
struct DamageMeterSkillView {
  // DÉJÀ TRADUIT et nommé par l'appelant, qui tient les tables.
  std::string_view name;
  std::int64_t amount = 0;
  std::uint32_t events = 0;
  std::uint32_t critical = 0;
  std::int64_t biggest = 0;
  float share = 0.0F;
};

// UN FAIT DU PALMARÈS, tel que la fenêtre le montre.
struct DamageMeterRecordView {
  std::int64_t amount = 0;
  // CE QUI A FRAPPÉ OU SOIGNÉ : le nom de la compétence, ou « Attaque » — DÉJÀ
  // TRADUIT et nommé par l'appelant, qui tient les tables.
  std::string_view what;
  // Le niveau de la compétence, ou zéro pour ne rien écrire.
  std::uint16_t skill_level = 0;
  // L'AUTRE BOUT — la cible de ce qu'on inflige, la source de ce qu'on
  // encaisse —, ou vide quand on ne le sait pas.
  std::string_view other;
  std::uint16_t hits = 1;
  bool critical = false;
  // UN CUMUL DE SORT DE ZONE : la somme d'un battement, sur toutes ses cibles.
  bool area_cycle = false;
  // Porté pendant le combat EN COURS.
  bool this_fight = false;
  // Depuis combien de temps, en millisecondes.
  std::uint32_t age_ms = 0;
};

// L'APPARENCE ET LES BASCULES, telles que le joueur les a réglées.
struct DamageMeterLook {
  bool shown = false;
  // 🔴 UN SEUL RÉGLAGE POUR DEUX EFFETS : figer la fenêtre
  // et la rendre traversante sont la même intention — « il est posé, oublie-le ».
  bool locked = false;
  bool records = true;
  // La couleur du texte, qui teinte aussi le chrome ; celle de la courbe.
  // ⚠ TRANSPARENCE COMPRISE : elle multiplie celle de chaque trait.
  ImU32 text_color = IM_COL32(255, 204, 51, 255);
  ImU32 plot_color = IM_COL32(255, 204, 51, 255);
  // L'opacité du fond et de la barre de titre, de zéro à un.
  float background_opacity = 0.90F;
};

// CE QUE LE COMPTEUR MONTRE, en lecture seule.
struct DamageMeterView {
  DamageMeterLook look;
  DamageMeterMode mode = DamageMeterMode::kDamageDone;
  DamageMeterSegment segment = DamageMeterSegment::kCurrent;
  DamageMeterPane pane = DamageMeterPane::kSelf;
  // LE TITRE DE LA FENÊTRE, sans son identifiant : composé par l'appelant selon
  // ce que le joueur veut y lire — voir les réglages.
  std::string_view title;
  // NOTRE rythme, notre pic et notre total dans le mode.
  float dps = 0.0F;
  float peak_dps = 0.0F;
  std::int64_t total_damage = 0;
  bool in_combat = false;
  // La durée du combat en cours, ou zéro hors combat ; et celle qui divise le
  // total pour la moyenne — voir `DamageMeter::fight_span_ms`.
  std::uint32_t combat_ms = 0;
  std::uint32_t fight_span_ms = 0;
  // Les colonnes, dans l'ordre du tampon circulaire, et la plus ANCIENNE.
  std::span<const float> plot;
  std::size_t plot_offset = 0;
  std::uint32_t slot_ms = 0;
  std::span<const DamageMeterRecordView> records;
  // LE GROUPE dans le segment, du plus grand au plus petit ; et le détail de
  // l'acteur ouvert — `detail_id` nul quand aucun ne l'est.
  std::span<const DamageMeterBarView> bars;
  std::uint32_t detail_id = 0;
  std::string_view detail_name;
  std::span<const DamageMeterSkillView> detail;
  // L'horloge de l'appelant : elle fait battre la pastille de combat.
  std::uint32_t now_ms = 0;
  // La police grasse des chiffres, ou nul pour celle de l'interface.
  ImFont* bold = nullptr;
  // RANGÉ DANS UNE CONVERSATION : c'est un onglet qui le peint, et la fenêtre
  // à lui ne paraît pas.
  bool docked = false;
};

// CE QUE LA FENÊTRE RETIENT D'UNE IMAGE À L'AUTRE, et qui n'est pas un réglage.
struct DamageMeterState {
  // LA BARRE DE TITRE DE L'IMAGE PRÉCÉDENTE. ⚠ VERROUILLÉE, la fenêtre laisse
  // passer les clics PARTOUT SAUF LÀ : c'est ce qui garde la flèche de repli et
  // le menu du clic droit à portée, alors que le corps est traversant.
  ImVec2 title_min{0.0F, 0.0F};
  ImVec2 title_max{0.0F, 0.0F};
  // Le volet du palmarès tel qu'il était à l'image précédente : c'est en le
  // voyant changer que la fenêtre grandit ou rapetisse de sa hauteur.
  std::optional<bool> records_were_shown;
  // Le clic droit en cours a commencé sur la fenêtre : seul celui-là ouvre le
  // menu.
  bool right_press_here = false;
  // UNE PLACE IMPOSÉE pour la prochaine apparition : là où l'onglet du
  // compteur a été lâché hors de toute bande. Prise une fois, puis oubliée.
  std::optional<ImVec2> place_at;
  // LA BARRE DE TITRE EST TIRÉE : le compteur est en route, peut-être vers une
  // bande d'onglets où il se rangera.
  bool title_dragging = false;
  // RANGÉ DANS UNE CONVERSATION plutôt que flottant : l'interrupteur que retient
  // le profil de disposition. `app` en fait la vérité des onglets.
  bool docked = false;
};

// OÙ RANGER LE COMPTEUR : une fenêtre de conversation, et le rang qu'il y
// prendra dans SA bande — moins un pour tout au bout.
struct DamageMeterDock {
  std::uint32_t group = 0;
  int rank = -1;
};

// CE QUE LE JOUEUR A DEMANDÉ. `app` le route : `ui` ne possède ni le compteur
// ni les réglages.
struct DamageMeterAction {
  bool close = false;
  std::optional<bool> lock;
  std::optional<bool> records;
  bool reset_fight = false;
  bool forget_records = false;
  bool open_settings = false;
  // Le ranger dans une conversation : lâché sur une bande, ou par le menu.
  std::optional<DamageMeterDock> dock;
  std::optional<DamageMeterMode> mode;
  std::optional<DamageMeterSegment> segment;
  std::optional<DamageMeterPane> pane;
  // L'acteur dont on ouvre le détail, ou zéro pour revenir aux barres.
  std::optional<std::uint32_t> detail;
};

// LE NOM D'UN MODE, d'un segment, et l'unité d'un rythme — traduits.
[[nodiscard]] const char* DamageMeterModeName(DamageMeterMode mode);
[[nodiscard]] const char* DamageMeterSegmentName(DamageMeterSegment segment);
[[nodiscard]] const char* DamageMeterRateUnit(DamageMeterMode mode);

// Dessine le compteur dans SA fenêtre, s'il est montré et pas rangé.
//
// `board` porte les bandes d'onglets de la conversation telles que cette image
// les a dessinées : lâché sur l'une d'elles par sa barre de titre, le compteur
// demande à s'y ranger. ⚠ À APPELER APRÈS LES FENÊTRES DE CONVERSATION.
[[nodiscard]] DamageMeterAction DrawDamageMeter(DamageMeterState& state,
                                                const DamageMeterView& view,
                                                const ChatDragBoard* board = nullptr);

// LE CORPS SEUL, dans la place donnée et à partir du curseur : ce qu'un onglet
// de conversation peint quand le compteur y est rangé. La courbe cède d'abord ;
// le palmarès montre autant de rangs qu'il en tient.
void DrawDamageMeterBody(const DamageMeterView& view, ImVec2 size, DamageMeterAction* asked);

// « 12 345 » tant que le nombre se lit, « 123,4 k » puis « 12,34 M » au-delà —
// un compteur de dégâts franchit vite le million, et sept chiffres qui défilent
// ne se lisent pas. ⚠ LE SÉPARATEUR DÉCIMAL VIENT DU CATALOGUE : c'est une
// virgule en français, un point ailleurs.
[[nodiscard]] std::string FormatDamageAmount(std::int64_t amount);

// LE MONTANT EXACT, groupé par milliers d'une ESPACE — « 12 345 678 ».
//
// ⚠ PAS LA VIRGULE DU RESTE DE L'INTERFACE : le compteur écrit aussi « 123,4 k »,
// et une même virgule qui grouperait ici et décimerait là se lirait « douze
// virgule trois ». L'espace ne se confond avec rien, dans aucune langue.
[[nodiscard]] std::string GroupDamageDigits(std::int64_t amount);

}  // namespace ro::ui
