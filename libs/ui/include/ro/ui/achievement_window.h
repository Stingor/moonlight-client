#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include <imgui.h>

#include "ro/ui/gauge_fill.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/tracker_panel.h"

namespace ro::ui {

// LA FENÊTRE DES SUCCÈS, celle du client d'origine, au pixel près : un arbre
// d'onglets à gauche, un bandeau de score et de rang en haut, la liste des
// succès de l'onglet et le détail du succès choisi — ou, sur le premier
// onglet, le résumé. Tout y est peint avec l'art de l'installation
// (`achievement_re`), aux coordonnées du client, à l'échelle de l'interface.
//
// Plus deux pièces hors de la fenêtre : le PANNEAU DE SUIVI, qui montre à
// l'écran jusqu'à trois succès épinglés, et la BANNIÈRE qui annonce un succès
// achevé.
//
// `ui` NE POSSÈDE RIEN DU JEU : `app` compose la vue — rangées déjà filtrées et
// triées, textes déjà formatés et traduits — et lit ce que le joueur demande.

inline constexpr const char* kAchievementWindowId = "achievements";

// La fenêtre, en pixels d'art : la barre de titre comprise, comme le client.
inline constexpr float kAchievementWindowWidth = 750.0F;
inline constexpr float kAchievementWindowHeight = 468.0F;

// LES ONGLETS : le résumé, « Tous », puis six catégories. Le premier est
// toujours le résumé, et c'est lui qu'une fenêtre rouverte montre.
inline constexpr int kAchievementSummaryTab = 0;
// « TOUS » ignore catégorie et sous-catégorie : il liste le catalogue entier,
// y compris ce qu'aucune catégorie ne range.
inline constexpr int kAchievementAllTab = 1;
inline constexpr int kAchievementFirstCategoryTab = 2;
inline constexpr int kAchievementTabCount = 8;
// Un sous-onglet absent : l'onglet entier.
inline constexpr int kAchievementWholeTab = -1;
// Les catégories du résumé : les six onglets qui suivent « Tous ».
inline constexpr std::size_t kAchievementCategoryCount = 6;
// LA FORME DE L'ARBRE : combien de sous-onglets chaque onglet range, dans
// l'ordre des onglets. Un sous-onglet est le numéro de sous-catégorie du
// catalogue, compté à partir de zéro.
//
// ⚠ « EXPLOIT » A DEUX SOUS-ONGLETS À NOUS : l'album de cartes, puis les
// mini-jeux. Le jeu d'origine n'en range aucun sous cet onglet, et sa table de
// mots ne les nomme pas.
inline constexpr std::array<int, kAchievementTabCount> kAchievementSubtabCounts = {0, 0, 3, 6,
                                                                                   2, 2, 3, 2};
// Les sous-onglets d'« Exploit », dans l'ordre de l'arbre.
inline constexpr int kAchievementFeatCardAlbum = 0;
inline constexpr int kAchievementFeatMinigames = 1;

// L'onglet d'une catégorie du catalogue — 1 pour la première —, et
// l'inverse. Hors des catégories, l'onglet rendu n'en est aucun.
[[nodiscard]] constexpr int AchievementTabOfMajor(int major) {
  return major + kAchievementFirstCategoryTab - 1;
}
[[nodiscard]] constexpr int AchievementMajorOfTab(int tab) {
  return tab - kAchievementFirstCategoryTab + 1;
}
// Le résumé montre les deux derniers succès obtenus.
inline constexpr std::size_t kAchievementRecentCount = 2;
// Le détail a dix lignes d'objectif, comme le serveur dix compteurs.
inline constexpr std::size_t kAchievementObjectiveRows = 10;

// Aucun onglet survolé.
inline constexpr int kAchievementNothingHovered = -1;

// LE FILTRE DU BANDEAU. Les deux premiers sont ceux du client : tout, ou les
// récompenses à réclamer — les inachevés restent montrés dans les deux. Les
// deux suivants sont à nous : les succès commencés et pas encore achevés, et
// les succès suivis.
enum class AchievementFilter : std::uint8_t {
  kAll = 0,
  kUnclaimed = 1,
  kInProgress = 2,
  kTracked = 3,
};
inline constexpr std::size_t kAchievementFilterCount = 4;

// L'ORDRE DE LA LISTE. Le premier est celui du client : les achevés du plus
// ancien au plus récent, puis les autres par numéro. Les suivants trient la
// liste entière : progression décroissante, score décroissant, nom.
enum class AchievementSort : std::uint8_t {
  kGame = 0,
  kProgress = 1,
  kScore = 2,
  kName = 3,
};
inline constexpr std::size_t kAchievementSortCount = 4;

// LA BANNIÈRE D'UN SUCCÈS ACHEVÉ : celle du client, une plus discrète, ou
// aucune — une ligne dans la conversation la remplace alors.
enum class AchievementBannerStyle : std::uint8_t {
  kNative = 0,
  kDiscreet = 1,
  kNone = 2,
};
inline constexpr std::size_t kAchievementBannerStyleCount = 3;

// La recherche tient dans ce tampon, zéro final compris.
inline constexpr std::size_t kAchievementSearchBytes = 64;

// L'ART DE LA FENÊTRE, lu par `app` dans `achievement_re`. Une pièce absente ne
// se peint pas ; la fenêtre reste utilisable.
struct AchievementArt {
  SkinImage bg_tab, tab_out, tab_over, tab_press;
  SkinImage tab_sub_out, tab_sub_over, tab_sub_press, tab_subarrow;
  SkinImage bg_upper, upper_trophy, radio_on, radio_off;
  SkinImage bg_list, list_out, list_press, list_complete_out, list_complete_press;
  SkinImage list_pin, badge_complete, rewardbox_default, rewardbox_unclaimed;
  SkinImage bg_detail, detail_stamp, detail_bullet;
  SkinImage receive_out, receive_over, receive_press, receive_disabled;
  SkinImage reward_title, reward_buff, reward_item;
  SkinImage bg_summary;
  SkinImage scroll_arrow_up, scroll_arrow_down, scroll_bg_mid, scroll_bg_down;
  SkinImage scroll_thumb_up, scroll_thumb_mid, scroll_thumb_down;
};

// LES MOTS DE LA FENÊTRE, déjà traduits — espaces de bord compris. Vides, ils
// ne se dessinent pas.
struct AchievementWords {
  std::string_view window_title;
  // Le titre du résumé, et celui du bandeau sur le résumé.
  std::string_view summary_title;
  std::string_view recent;
  // Les libellés des deux filtres.
  std::string_view filter_all;
  std::string_view filter_unclaimed;
  // Chaque onglet, et ses sous-onglets.
  std::array<std::string_view, kAchievementTabCount> tabs;
  std::array<std::span<const std::string_view>, kAchievementTabCount> subtabs;
};

// Le coffre d'une rangée : aucune récompense à prendre, une récompense qui
// attend l'achèvement, une récompense à réclamer.
enum class AchievementChest : std::uint8_t { kNone, kWaiting, kUnclaimed };

// UNE RANGÉE DE LA LISTE.
struct AchievementRowView {
  std::uint32_t achievement_id = 0;
  std::string_view name;
  std::string_view summary;
  std::int32_t score = 0;
  SkinImage icon;
  bool completed = false;
  bool tracked = false;
  AchievementChest chest = AchievementChest::kNone;
  // La date d'achèvement déjà écrite, « 2020. 3.24 » ; vide tant que le succès
  // n'est pas obtenu.
  std::string_view date;
};

// L'état d'un objectif, qui choisit sa couleur.
enum class AchievementObjectiveState : std::uint8_t {
  // Rien de fait : bleu foncé.
  kUntouched,
  // Commencé : orange.
  kStarted,
  // Atteint : bleu clair, barré.
  kReached,
};

struct AchievementObjectiveView {
  // Le texte déjà écrit — « libellé ( 3 / 10 ) » pour un succès qui se compte.
  std::string_view text;
  AchievementObjectiveState state = AchievementObjectiveState::kUntouched;
  // La jauge d'un succès qui se compte ; rien pour une puce.
  std::optional<std::uint32_t> count;
  std::uint32_t target = 0;
  // Le succès que cette ligne désigne : la cliquer le choisit.
  std::optional<std::uint32_t> shortcut;
};

// Le bouton de réclamation : caché sans récompense à prendre, éteint tant que
// le succès n'est pas achevé.
enum class AchievementReceive : std::uint8_t { kHidden, kDisabled, kEnabled };

enum class AchievementRewardKind : std::uint8_t { kTitle, kBuff, kItem };

struct AchievementRewardView {
  AchievementRewardKind kind = AchievementRewardKind::kItem;
  // Ce que l'infobulle dit ; vide, pas d'infobulle.
  std::string_view tooltip;
  // L'icône de l'objet, posée dans le cadre.
  SkinImage item_icon;
  std::int32_t item_id = 0;
};

// LE DÉTAIL du succès choisi.
struct AchievementDetailView {
  std::uint32_t achievement_id = 0;
  std::string_view name;
  std::string_view details;
  bool completed = false;
  // Les objectifs se comptent : lignes plus hautes, et une jauge chacune.
  bool counted = false;
  std::span<const AchievementObjectiveView> objectives;
  AchievementReceive receive = AchievementReceive::kHidden;
  // Dans l'ordre du client : titre, buff, objet.
  std::span<const AchievementRewardView> rewards;
};

// UN DES DEUX DERNIERS SUCCÈS OBTENUS, sur le résumé.
struct AchievementRecentView {
  std::uint32_t achievement_id = 0;
  std::string_view name;
  std::string_view summary;
  std::int32_t score = 0;
  SkinImage icon;
  bool unclaimed = false;
  std::string_view date;
};

// Un compte « faits / tous ».
struct AchievementTally {
  std::uint32_t done = 0;
  std::uint32_t total = 0;
};

struct AchievementSummaryView {
  AchievementTally overall;
  std::array<AchievementTally, kAchievementCategoryCount> categories;
  // Le plus récent d'abord ; il peut n'y en avoir qu'un, ou aucun.
  std::span<const AchievementRecentView> recent;
};

// LE BANDEAU : le score, le rang et sa jauge, le compte de l'onglet.
struct AchievementUpperView {
  std::uint32_t total_points = 0;
  std::int16_t rank = 0;
  std::uint32_t rank_points = 0;
  std::uint32_t rank_span = 0;
  AchievementTally tab;
  // Le libellé de l'onglet ou du sous-onglet choisi.
  std::string_view title;
};

struct AchievementView {
  const AchievementArt* art = nullptr;
  AchievementWords words;
  AchievementUpperView upper;
  // Les rangées de l'onglet, filtrées et triées.
  std::span<const AchievementRowView> rows;
  // Le succès choisi ; rien quand la liste est vide.
  std::optional<AchievementDetailView> detail;
  AchievementSummaryView summary;
  // LA RECHERCHE EST EN COURS : la liste montre les succès de TOUS les
  // onglets qui la satisfont, résumé compris, et le bandeau le dit.
  bool searching = false;
  // Les récompenses qui attendent d'être réclamées, tous onglets confondus :
  // le bouton « Tout réclamer » de la barre de titre les compte.
  std::size_t claimable = 0;
  // La bannière choisie, que la puce de la barre de titre propose de changer.
  AchievementBannerStyle banner_style = AchievementBannerStyle::kNative;
  // Les réglages du panneau de suivi, commun aux quêtes, que la même puce
  // propose aussi : c'est un chemin quand les clics traversent le panneau.
  TrackerLook tracker_look;
  // L'onglet pour lequel cette vue a été composée. ⚠ C'EST LUI QUI CHOISIT
  // ENTRE RÉSUMÉ ET LISTE, pas l'état : un clic dans l'arbre change l'état
  // avant le dessin, et la vue de l'ancien onglet se peindrait sous le nouveau.
  int tab = kAchievementSummaryTab;
};

// CE QUE LA FENÊTRE GARDE D'UNE IMAGE À L'AUTRE. `app` le retient d'une
// ouverture à l'autre, et d'une session à l'autre — la recherche exceptée.
struct AchievementWindowState {
  bool open = false;
  int tab = kAchievementSummaryTab;
  int subtab = kAchievementWholeTab;
  AchievementFilter filter = AchievementFilter::kAll;
  AchievementSort sort = AchievementSort::kGame;
  // Ce que le joueur cherche, en UTF-8 ; vide, pas de recherche.
  std::array<char, kAchievementSearchBytes> search{};
  // Le succès choisi, par numéro : la liste se recompose à chaque image.
  std::optional<std::uint32_t> selected;
  // La première rangée montrée, et le premier sous-onglet montré.
  int list_scroll = 0;
  int subtab_scroll = 0;
  // L'onglet et le sous-onglet survolés, `kAchievementNothingHovered` pour
  // aucun. ⚠ LE DERNIER SURVOL RESTE affiché, comme chez le client, qui
  // n'éteint rien quand la souris s'en va.
  int hovered_tab = kAchievementNothingHovered;
  int hovered_subtab = kAchievementNothingHovered;
  // Le bouton « Signaler un bug » de la barre de titre, selon le réglage.
  bool bug_report = false;
};

// UNE PLACE DANS L'ARBRE : un onglet, et l'un de ses sous-onglets ou
// `kAchievementWholeTab` pour l'onglet entier.
struct AchievementTabHit {
  int tab = kAchievementSummaryTab;
  int subtab = kAchievementWholeTab;
};

// CE QUE LE JOUEUR A DEMANDÉ À CETTE IMAGE.
struct AchievementAction {
  // L'onglet a changé — clic dans l'arbre ou sur un filtre : `app` remet la
  // sélection sur la première rangée.
  bool tab_changed = false;
  std::optional<std::uint32_t> select;
  // Aller à ce succès : son onglet, sa rangée.
  std::optional<std::uint32_t> jump_to;
  std::optional<std::uint32_t> toggle_tracking;
  std::optional<std::uint32_t> claim;
  // Décrire l'objet en récompense — un clic droit sur son icône.
  std::optional<std::int32_t> describe_item;
  // Le tri ou la recherche ont changé : comme un changement d'onglet, la
  // sélection revient sur la première rangée.
  bool order_changed = false;
  // Réclamer toutes les récompenses qui attendent, une à une.
  bool claim_all = false;
  // Le joueur a choisi une autre bannière.
  std::optional<AchievementBannerStyle> banner_style;
  // Les réglages du panneau de suivi ont changé, ou il revient à sa place
  // d'origine.
  TrackerSettingsAnswer tracker_settings;
  // « Signaler un bug » : `app` joint ce que la fenêtre montre — le succès
  // choisi, sinon l'onglet ou le résumé.
  bool report_bug = false;
  // L'ONGLET OU LE SOUS-ONGLET dont il veut poser le lien dans le chat — Maj +
  // clic sur sa racine ou sur sa sous-ligne.
  std::optional<AchievementTabHit> link_tab;
};

// LE MODE LISTE : la liste et le détail plutôt que le résumé, et le bandeau
// complet — filtres, trophée, titre et compte. Il vaut sur un onglet de
// catégorie, et sur le résumé dès qu'une recherche est en cours.
[[nodiscard]] bool AchievementListMode(const AchievementView& view);

// OUVRE LA FENÊTRE SUR CETTE PLACE DE L'ARBRE, comme un lien qu'on suit : la
// recherche quittée, la liste en haut, et le sous-onglet visible dans sa bande.
// ⚠ L'APPELANT SIGNALE LE CHANGEMENT D'ONGLET à l'étage, qui remet alors la
// sélection sur la première rangée.
void OpenAchievementTab(AchievementWindowState& state, AchievementTabHit place);

[[nodiscard]] AchievementAction DrawAchievementWindow(AchievementWindowState& state,
                                                      const AchievementView& view,
                                                      const SkinArt& skin);

// ── LA GÉOMÉTRIE DE LA FENÊTRE, EN PIXELS D'ART ─────────────────────────────
//
// Des fonctions PURES : le dessin les appelle, les essais les éprouvent sans
// fenêtre. Toutes les ordonnées partent du haut du contrôle concerné.

// L'arbre : une racine, une sous-ligne, et au plus trois sous-lignes montrées.
// ⚠ LA RACINE FAIT 35 ET NON LES 40 DE SON ART : huit racines et trois
// sous-lignes tiennent ainsi au-dessus de la recherche. L'art perd des lignes
// en son milieu, ses bords restent nets.
inline constexpr float kAchievementTabRootHeight = 35.0F;
inline constexpr float kAchievementTabSubHeight = 26.0F;
inline constexpr int kAchievementTabSubShown = 3;

// La liste : cinq rangées de 81 pixels, et la molette avance de trois.
inline constexpr float kAchievementRowHeight = 81.0F;
inline constexpr int kAchievementRowsShown = 5;
inline constexpr int kAchievementWheelRows = 3;

// La bannière se coupe à ce nombre de caractères par ligne.
inline constexpr std::size_t kAchievementBannerLineChars = 72;

// Le haut d'une racine : les racines sous la racine choisie descendent de la
// bande de ses sous-lignes montrées.
[[nodiscard]] float AchievementTabTop(int tab, int chosen_tab, int chosen_subtab_count);

// Ce que touche un point de l'arbre. Une sous-ligne de la racine choisie, ou
// une racine entière ; RIEN sous la dernière racine ni au-dessus de la
// première.
[[nodiscard]] std::optional<AchievementTabHit> HitAchievementTab(float y, int chosen_tab,
                                                                 int chosen_subtab_count,
                                                                 int subtab_scroll, int tab_count);

// La plus grande valeur que prend un défilement : nul quand tout tient.
[[nodiscard]] int AchievementSubtabScrollRange(int subtab_count);
// Le défilement des sous-lignes qui montre ce sous-onglet dans la bande : en
// bas de la bande quand il ne tient pas dans la première page, nul sinon.
[[nodiscard]] int AchievementSubtabScrollShowing(int subtab, int subtab_count);
[[nodiscard]] int AchievementListScrollRange(std::size_t row_count);
[[nodiscard]] int ClampAchievementScroll(int scroll, int range);

// La rangée sous l'ordonnée `y` de la liste — un indice dans les rangées
// MONTRÉES : sous la dernière, rien.
[[nodiscard]] std::optional<std::size_t> AchievementListRowAt(float y, int list_scroll,
                                                              std::size_t row_count);

// L'abscisse de la récompense `index` parmi `count`, cadres de `frame_width`.
[[nodiscard]] float AchievementRewardX(std::size_t index, std::size_t count, float frame_width);

// La largeur du remplissage dans un intérieur de jauge large de
// `interior_width` pixels d'art : le pourcentage tronqué, puis la largeur
// tronquée au pixel. Pleine, elle vaut l'intérieur et jamais plus. Rien du
// tout — cadre compris — quand le compte dépasse le maximum.
[[nodiscard]] std::optional<float> AchievementGaugeFill(float interior_width, std::int64_t current,
                                                        std::int64_t maximum);

// L'INTÉRIEUR d'une jauge des succès, en pixels d'art : la jauge moins ses deux
// cadres d'un pixel. Ses coins sont droits, comme ceux de son cadre.
[[nodiscard]] RoundedBox AchievementGaugeInterior(ImVec2 gauge_min, ImVec2 gauge_max);

// LA BOÎTE REMPLIE de cet intérieur, depuis la gauche, en pixels d'art. Rien du
// tout quand le compte dépasse le maximum.
[[nodiscard]] std::optional<AxisBox> AchievementGaugeFillBox(const RoundedBox& interior,
                                                             std::int64_t current,
                                                             std::int64_t maximum);

// Le pouce d'un ascenseur vertical : son haut et sa longueur, flèches comprises
// dans la hauteur de la piste.
struct AchievementThumb {
  float top = 0.0F;
  float length = 0.0F;
};
[[nodiscard]] AchievementThumb AchievementScrollThumb(float track_height, int position, int range,
                                                      int page, int total);

// Les lignes de la bannière : des tranches de 72 caractères, jamais coupées au
// milieu d'un caractère.
[[nodiscard]] std::vector<std::string_view> SplitAchievementBanner(std::string_view text);

// ── LA BANNIÈRE D'UN SUCCÈS ACHEVÉ ──────────────────────────────────────────
//
// Une bande de la largeur de l'écran, cinquante pixels sous son bord, sans
// fond : le texte blanc ombré de noir, centré. Elle tient dix secondes après le
// DERNIER succès achevé ; un second remplace le texte et relance le délai.
//
// LA DISCRÈTE est une seule ligne, plus petite et sans gras, qui tient quatre
// secondes et s'efface pendant la dernière.

inline constexpr std::uint32_t kAchievementBannerMs = 10000;
inline constexpr std::uint32_t kAchievementDiscreetBannerMs = 4000;
inline constexpr std::uint32_t kAchievementDiscreetFadeMs = 1000;

// Combien de temps tient la bannière de ce style ; zéro pour aucune.
[[nodiscard]] std::uint32_t AchievementBannerLifeMs(AchievementBannerStyle style);

// L'opacité de la bannière discrète à cet âge, de 0 à 1 : pleine, puis
// décroissante en ligne droite pendant les `kAchievementDiscreetFadeMs`
// dernières millisecondes, nulle au-delà de sa vie.
[[nodiscard]] float AchievementDiscreetBannerAlpha(std::uint32_t elapsed_ms);

// `text` est la phrase déjà écrite ; vide, rien ne se dessine. `elapsed_ms`
// est l'âge de la bannière, qui règle l'effacement de la discrète.
void DrawAchievementBanner(std::string_view text, AchievementBannerStyle style,
                           std::uint32_t elapsed_ms);

}  // namespace ro::ui
