#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/ui/chat_link.h"
#include "ro/ui/item_link_cell.h"
#include "ro/ui/rich_text.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// ── LE JOURNAL DES QUÊTES ───────────────────────────────────────────────────
//
// Une seule fenêtre : la liste à gauche, le détail de la quête choisie à
// droite. Cinq onglets — en cours, recommandées, suspendues, en attente,
// historique —, des catégories repliables, un tri, une recherche plein texte et
// un filtre de catégorie.
//
// 🔴 `ui` NE TIENT AUCUN ÉTAT DE JEU : `app` compose la vue à chaque image —
// lignes déjà filtrées, triées et groupées selon l'état d'interface qu'elle lit
// ici, textes déjà traduits — et reçoit en retour l'intention du joueur. La
// fenêtre ne garde que de l'état d'INTERFACE : onglet, filtre, recherche, tri,
// sélection, groupes repliés.

// L'identité de la fenêtre : clé de sa place et de la pile d'Échap.
inline constexpr const char* kQuestWindowId = "quest_log";

// ⚠ LES VALEURS SONT GRAVÉES dans `savedata\quetes.yml` : un onglet neuf prend
// la suivante, et l'ordre d'affichage est `kQuestTabOrder`.
enum class QuestTab : std::uint8_t {
  // Les quêtes actives, sauf les attentes.
  kActive = 0,
  // Les quêtes que le joueur a suspendues (le cadenas).
  kSuspended = 1,
  // Les quêtes sorties du journal, retenues par le client.
  kHistory = 2,
  // Des affiches qui invitent à commencer un contenu ; pas des quêtes.
  kRecommended = 3,
  // Les quêtes actives qui ne font qu'attendre leur échéance.
  kWaiting = 4,
};
inline constexpr std::size_t kQuestTabCount = 5;
// L'ordre des onglets à l'écran.
inline constexpr std::array<QuestTab, kQuestTabCount> kQuestTabOrder = {
    QuestTab::kActive, QuestTab::kRecommended, QuestTab::kSuspended, QuestTab::kWaiting,
    QuestTab::kHistory};

enum class QuestSort : std::uint8_t {
  // Les plus récemment reçues d'abord.
  kRecent = 0,
  // La plus proche de son échéance d'abord ; les quêtes sans limite à la fin.
  kTimeLeft = 1,
  kName = 2,
  // La plus avancée d'abord.
  kProgress = 3,
};
inline constexpr std::size_t kQuestSortCount = 4;

// La recherche tient dans ce tampon, zéro final compris.
inline constexpr std::size_t kQuestSearchBytes = 64;

// L'ART DE LA FENÊTRE, lu par `app` dans `renew_questui`. Une pièce absente
// n'est pas dessinée ; la fenêtre reste lisible sans aucune.
struct QuestArt {
  // Le fond d'une ligne, au repos et sélectionnée (330×46).
  SkinImage row;
  SkinImage row_selected;
  // Le cadenas : quête active, puis suspendue (20×17).
  SkinImage lock_active;
  SkinImage lock_suspended;
  // La case « suivie » (14×14).
  SkinImage track_on;
  SkinImage track_off;
  // Le filigrane du détail vide (le Poring de `bg_detail_quest`), et celui de
  // l'onglet des recommandations.
  SkinImage detail_empty;
  SkinImage detail_recommended;
};

// UN FILTRE de la barre : « all », « normal », « etc »…, son libellé, et
// l'icône qui le représente (peut être inutilisable : le libellé suffit).
struct QuestCategoryView {
  std::string_view key;
  std::string_view label;
  SkinImage image;
};

// UNE CHASSE, telle que la liste et le détail la montrent.
struct QuestHuntView {
  // « Poring Lv.1~Lv.10 », déjà composé.
  std::string_view label;
  std::int32_t killed = 0;
  // Zéro tant que le serveur ne l'a pas dit : la ligne n'a pas de compte.
  std::int32_t total = 0;
  bool done = false;
  // Le monstre dont on peut ouvrir la fiche ; zéro pour une chasse générique.
  std::uint32_t mob_id = 0;
};

// UNE RÉCOMPENSE D'ITEM : son icône, son nom, et combien.
struct QuestRewardView {
  std::uint32_t item_id = 0;
  std::string_view name;
  SkinImage icon;
  std::uint32_t count = 0;
};

// LE PNJ D'UNE QUÊTE : là où le guidage mène.
struct QuestNpcView {
  // « prt_fild03 (365, 255) », déjà composé, et le nom affiché de la carte.
  std::string_view label;
  std::string_view map_name;
  std::int32_t cell_x = 0;
  std::int32_t cell_y = 0;
};

// UNE QUÊTE DU JOURNAL.
//
// ⚠ DES VUES : elles pointent dans les tampons de `app`, valables le temps de
// l'image.
struct QuestRowView {
  std::uint32_t quest_id = 0;
  std::string_view title;
  std::string_view summary;
  // L'icône de catégorie ; inutilisable : la ligne n'en montre pas.
  SkinImage icon;
  std::string_view icon_label;
  bool active = true;
  bool tracked = false;
  // Reçue depuis la dernière fois que le joueur l'a regardée.
  bool fresh = false;
  // Toutes ses chasses sont faites : il reste à retourner voir le PNJ.
  bool objectives_met = false;
  // Le temps qui reste, déjà composé (« 2 h 13 min », « expirée ») ; vide : sans
  // limite. `expired` et `urgent` choisissent sa couleur.
  std::string_view time_left;
  bool urgent = false;
  bool expired = false;
  // UNE QUÊTE D'ATTENTE : son temps compte vers le moment où elle sera prête, et
  // `expired` veut dire « prête » — une bonne nouvelle, pas un échec.
  bool waiting = false;
  // L'avancée de 0 à 1 sur l'ensemble des chasses ; négative : aucune chasse.
  float progress = -1.0F;
  std::span<const QuestHuntView> hunts;

  // ── LE DÉTAIL ─────────────────────────────────────────────────────────────
  // Les lignes de la description, balisage du jeu compris (couleurs, <NAVI>).
  std::span<const std::string> description;
  // L'échéance en clair, « 2026/10/07 18:00 » ; vide : sans limite.
  std::string_view deadline;
  std::optional<QuestNpcView> npc;
  // « 1 500 » ; vide : pas de récompense de ce genre.
  std::string_view reward_base_exp;
  std::string_view reward_job_exp;
  std::span<const QuestRewardView> reward_items;
  // Le catalogue ne connaît pas cette quête : le détail le dit.
  bool unknown = false;
};

// UN GROUPE DE LIGNES, sous l'en-tête de sa catégorie ; un seul groupe sans
// en-tête quand le regroupement est éteint.
struct QuestGroupView {
  std::string_view key;
  // Vide : pas d'en-tête.
  std::string_view label;
  SkinImage image;
  std::span<const QuestRowView> rows;
};

// UNE QUÊTE SORTIE DU JOURNAL.
struct QuestHistoryView {
  std::uint32_t quest_id = 0;
  std::string_view title;
  SkinImage icon;
  // « 2026/10/06 16:40 », déjà composé.
  std::string_view left_at;
  // Comment elle est sortie — « Achevée », « Abandonnée »… —, déjà traduit ;
  // vide quand on ne le sait pas.
  std::string_view ending;
  // L'étape qui l'a remplacée, et son titre : la ligne en fait un lien. Zéro :
  // aucune.
  std::uint32_t next_quest_id = 0;
  std::string_view next_title;
};

// UNE AFFICHE RECOMMANDÉE : elle invite à commencer un contenu, et n'est pas
// une quête — ni cadenas, ni suivi, ni échéance.
struct QuestRecommendedView {
  std::uint32_t entry_id = 0;
  std::string_view title;
  std::string_view summary;
  SkinImage icon;
  std::string_view icon_label;
  // L'illustration du détail ; inutilisable : il n'en montre pas.
  SkinImage image;
  // Jamais ouverte par ce personnage.
  bool fresh = false;
  // Le PNJ de départ.
  std::optional<QuestNpcView> npc;
  // Les blocs de texte, balisage du jeu compris ; un bloc peut tenir plusieurs
  // lignes.
  std::span<const std::string> blocks;
};

struct QuestWindowView {
  // L'onglet pour lequel cette vue a été composée. ⚠ C'EST LUI QUI CHOISIT CE
  // QUE LE CORPS MONTRE, pas l'état : un clic sur un onglet change l'état avant
  // le dessin, et la vue de l'ancien onglet se peindrait sous le nouveau.
  QuestTab tab = QuestTab::kActive;
  std::span<const QuestGroupView> groups;
  std::span<const QuestHistoryView> history;
  std::span<const QuestRecommendedView> recommended;
  std::span<const QuestCategoryView> categories;
  // Le compte de chaque onglet, pour son libellé.
  std::array<std::size_t, kQuestTabCount> tab_counts{};
  // La quête choisie, si elle est encore dans le journal (ou dans l'historique,
  // sans détail de jeu : seuls titre et description).
  const QuestRowView* selected = nullptr;
  // L'affiche choisie, dans l'onglet des recommandations.
  const QuestRecommendedView* selected_recommended = nullptr;
  // Le catalogue manque : la fenêtre le dit au lieu de montrer des ids nus.
  // ⚠ LE TEXTE EST DÉJÀ TRADUIT.
  std::string_view catalog_problem;
  // Le panneau de suivi se montre (la case du pied de fenêtre).
  bool tracker_shown = false;
};

struct QuestWindowState {
  bool open = false;
  QuestTab tab = QuestTab::kActive;
  // La clé du filtre de catégorie ; « all » par défaut.
  std::string category = "all";
  std::array<char, kQuestSearchBytes> search{};
  QuestSort sort = QuestSort::kRecent;
  bool group_by_category = true;
  // Les clés de groupe repliés.
  std::vector<std::string> folded_groups;
  // La quête choisie ; zéro : aucune.
  std::uint32_t selected = 0;
  // L'affiche recommandée choisie ; zéro : aucune. Ses clés ne sont pas des
  // quêtes, d'où une sélection à part.
  std::uint32_t selected_recommended = 0;
  // Amener la sélection en vue à l'image suivante (posé par `app` quand le
  // suivi ou un lien ouvre la fenêtre sur une quête).
  bool scroll_to_selected = false;
  // Hisser la fenêtre au premier plan à l'image suivante.
  bool raise = false;
  // LE BOUTON « SIGNALER UN BUG » de sa barre de titre, posé par `app` depuis
  // les réglages.
  bool bug_report = false;
  // Le menu du lien d'une récompense.
  ItemLinkMenu reward_menu;
};

struct QuestWindowAction {
  // Le joueur vient de choisir cette quête (elle n'est plus « nouvelle »).
  std::optional<std::uint32_t> selected;
  // Le joueur vient d'ouvrir cette affiche recommandée.
  std::optional<std::uint32_t> read_recommended;
  // Suspendre (faux) ou reprendre (vrai) : le cadenas.
  std::optional<std::uint32_t> toggle_active;
  // Suivre ou ne plus suivre à l'écran : la case.
  std::optional<std::uint32_t> toggle_tracked;
  // Guider vers ce lieu : le PNJ, ou un lien <NAVI> de la description.
  std::optional<RichPlace> guide_to;
  // Ouvrir la fiche de ce monstre.
  std::uint32_t open_monster = 0;
  // Un geste sur une récompense — description, lien, menu —, ou son survol.
  ChatLinkAsk item;
  // Un lien d'adresse de la description, tel quel : `app` décide.
  std::string followed_address;
  // Oublier cette quête de l'historique.
  std::optional<std::uint32_t> forget;
  // Montrer cette quête : le lien vers l'étape suivante d'une ligne d'historique.
  std::optional<std::uint32_t> open_quest;
  // Tout l'historique.
  bool forget_all = false;
  // La case « Montrer le suivi » a changé.
  std::optional<bool> show_tracker;
  // La puce de la barre de titre : la page des réglages du journal et du suivi.
  bool open_settings = false;
  // Le joueur a demandé le rapport de bug depuis la barre de titre.
  bool report_bug = false;
};

[[nodiscard]] QuestWindowAction DrawQuestWindow(QuestWindowState& state,
                                                const QuestWindowView& view, const QuestArt& art,
                                                const SkinArt& skin);

}  // namespace ro::ui
