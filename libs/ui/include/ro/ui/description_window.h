#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/chat_link.h"
#include "ro/ui/item_link_cell.h"
#include "ro/ui/rich_text.h"
#include "ro/ui/shortcut_bar.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/staff_item_grant.h"
#include "ro/ui/wheel_gate.h"

namespace ro::ui {

// LA FENÊTRE DE DESCRIPTION : ce qu'un objet EST.
//
// Le nom d'une case ne dit que ce qu'on possède ; la description dit ce que
// l'objet fait, ce qu'il pèse, qui peut le porter. C'est la fenêtre que le
// client ouvre au Ctrl+clic droit, et la seule qui montre le texte du jeu tel
// que ses tables l'écrivent — couleurs et liens compris.
//
// ⚠ ELLE SE LIT, ELLE NE SE POSSÈDE PAS. On y décrit aussi bien un objet du sac
// qu'une carte qu'on n'a jamais eue : sa clé est un IDENTIFIANT, pas une place.
// Une place disparaît quand on consomme ; la description reste ouverte.

// Ce que la fenêtre montre.
// ── OÙ S'OUVRE UNE DESCRIPTION ──────────────────────────────────────────────
//
// L'ancre dit dans quelle DIRECTION la fenêtre s'étend depuis le curseur :
// « bas-droite » la fait descendre à droite de lui, son coin haut-gauche posé
// dessus. ⚠ L'ORDRE EST CELUI DU RÉGLAGE, et le fichier l'écrit en nombre.
enum class DescriptionAnchor : std::uint8_t {
  kTopLeft,
  kTopRight,
  kBottomLeft,
  kBottomRight,
  kCenter,
};
inline constexpr std::size_t kDescriptionAnchorCount = 5;
// L'écart d'origine entre le curseur et la fenêtre, sur chaque axe.
inline constexpr int kDescriptionCursorGapPixels = 12;

struct DescriptionPlacement {
  // Faux : la fenêtre rouvre là où on l'a laissée.
  bool at_cursor = true;
  DescriptionAnchor anchor = DescriptionAnchor::kBottomRight;
  float offset_x = static_cast<float>(kDescriptionCursorGapPixels);
  float offset_y = static_cast<float>(kDescriptionCursorGapPixels);
};

// ── LA PIÈCE PORTÉE SUR SOI : l'aperçu du « ViewID » ────────────────────────
//
// Survoler « ViewID : N » montre le personnage du joueur qui porte la pièce —
// coiffe ou cape —, qu'on tourne à la molette. `app`
// compose le pantin à l'image qui suit le survol, et la bulle le montre dès
// qu'il est là.
//
// La boîte de l'aperçu, et la stature du corps dedans : une taille ABSOLUE, pour
// que le personnage garde la même taille d'une pièce survolée à la suivante.
inline constexpr float kDescriptionDollWidthPixels = 260.0F;
inline constexpr float kDescriptionDollHeightPixels = 240.0F;
inline constexpr float kDescriptionDollBodyPixels = 120.0F;

// L'EFFET QUE `app` PEINT AVEC LE PANTIN — un effet de chapeau —, au moment
// même où l'interface se dessine.
//
// 🔴 UN RAPPEL DE DESSIN, ET NON UNE IMAGE : un effet se mélange à ce qu'il a
// derrière lui, et derrière la bulle il y a le jeu. Une image transparente
// perdrait tout ce qui s'ajoute ; le rappel peint dans l'image elle-même, à la
// place de la bulle. Voir `ro::render::BeginInsetPaint`.
//
// 🔴 DEUX PASSES, COMME LA PARTIE : ce que la partie peint AVANT les corps — la
// file du sol, les fichiers qu'une fiche range avant le personnage — passe
// derrière l'image du pantin, le reste devant. Un seul rappel placé d'un côté
// ou de l'autre ne peut pas montrer un effet dont une moitié passe derrière.
struct DollOverlay {
  // Nul : rien à peindre.
  ImDrawCallback paint = nullptr;
  void* painter = nullptr;
};

// La passe qu'un rappel peint.
enum class DollOverlayPass : std::uint8_t {
  kBehind,  // avant l'image du pantin : ce que les corps recouvrent
  kFront,   // après elle
};

// CE QUE LE RAPPEL REÇOIT, copié dans la liste de dessin : son peintre, le
// rectangle du pantin en coordonnées d'écran de l'interface, et sa passe.
struct DollOverlayCall {
  void* painter = nullptr;
  ImVec2 min{0.0F, 0.0F};
  ImVec2 max{0.0F, 0.0F};
  DollOverlayPass pass = DollOverlayPass::kFront;
};

struct DescriptionDollState {
  // Le pantin composé, posé par `app`.
  SkinImage image;
  // Son effet, posé par `app` après le pantin ; vide sans effet.
  DollOverlay overlay;
  // LE CORPS SE PEINT PAR LE RAPPEL, derrière les effets de devant : un effet
  // le change — le rapetisse, l'éclaire, le double —, ce que l'image composée
  // ne sait pas montrer. L'image ne se pose alors pas ; sa place reste.
  bool body_overlaid = false;
  // La direction du .ACT, que la molette tourne sur huit crans.
  std::uint8_t direction = 0;
  WheelGateState wheel;
  // La ligne est survolée à cette image : `app` compose le pantin.
  bool wanted = false;
};

// LA BULLE DU PANTIN, au-dessus du curseur : ni fond ni bord, seul le
// personnage se montre. Rien tant que `app` ne l'a pas composé.
void DrawItemDollTooltip(const DescriptionDollState& doll);

// LE RÔLE D'UN MORCEAU DU NOM. Il décide de sa couleur, jamais de son texte.
enum class DescriptionNameTone : std::uint8_t {
  kOrdinary,
  kCreator,         // le forgeron, nommé par le serveur
  kUnknownCreator,  // le forgeron, dont le nom n'est pas encore là
};
struct DescriptionNamePiece {
  std::string_view text;
  DescriptionNameTone tone = DescriptionNameTone::kOrdinary;
};

// L'ITEM DANS LA LISTE DE RAMASSAGE AUTOMATIQUE — ou la liste absente, quand
// le serveur ne l'a pas dite : le bouton ne se montre pas.
enum class DescriptionAutoloot : std::uint8_t { kUnavailable, kOut, kIn };

// UNE CASE DE CARTE d'un objet qu'on tient : la carte ou l'enchant serti, ou
// rien. ⚠ `id` NUL EST UNE CASE VIDE, et elle se montre : c'est une place qu'on
// peut encore remplir.
struct DescriptionCardRow {
  std::uint32_t id = 0;
  std::string_view name;
  SkinImage icon;
};

// UNE OPTION ALÉATOIRE d'un objet qu'on tient. `text` est son libellé, déjà
// rempli de sa valeur ; vide quand la table du client ne la connaît pas, et la
// fenêtre écrit alors son numéro.
struct DescriptionOptionRow {
  std::string_view text;
  int id = 0;
  int value = 0;
  int param = 0;
};

// UNE LIGNE DE LA TABLE DE TIRAGE d'un conteneur — boîte, album, branche : ce
// qu'il peut rendre, et à quelle fréquence.
struct DescriptionOddsRow {
  std::uint32_t id = 0;
  // Un MONSTRE qu'une branche invoque, ou un objet.
  bool monster = false;
  // Le libellé de la table, en UTF-8.
  std::string_view name;
  // L'icône de l'objet ; vide pour un monstre.
  SkinImage icon;
  // La chance, en pour-cent, et le tirage d'où elle vient : `weight` sur
  // `total`. Le groupe zéro est le lot GARANTI.
  double percent = 0.0;
  int weight = 0;
  int total = 0;
  int group = 0;
};

// ── LES ONGLETS DU SERVEUR ─────────────────────────────────────────────────
//
// Ce que le serveur seul sait d'un objet : les monstres qui le laissent tomber,
// et son script. ⚠ « PAS ENCORE SU » N'EST PAS « VIDE » : un onglet ne se grise
// que sur une réponse arrivée et vide, jamais en attendant — sinon la barre
// clignoterait à chaque ouverture.
enum class DescriptionFetch : std::uint8_t {
  // Rien n'est parti : pas de serveur à qui demander.
  kUnasked,
  kWaiting,
  kReady,
};

// Un monstre qui laisse tomber l'objet.
struct DescriptionDropRow {
  std::uint32_t monster_id = 0;
  std::string_view name;
  // Zéro ordinaire, un mini-boss, deux MVP.
  std::uint8_t boss_rank = 0;
  bool mvp_reward = false;
  // En centièmes de pour-cent : 10 000 = 100 %.
  std::uint32_t rate_hundredths = 0;
};

struct DescriptionDropsView {
  DescriptionFetch fetch = DescriptionFetch::kUnasked;
  std::span<const DescriptionDropRow> rows;
  bool truncated = false;
  int treasure_excluded = 0;
};

struct DescriptionComboMember {
  std::uint32_t item_id = 0;
  std::string_view name;
};

struct DescriptionComboView {
  std::span<const DescriptionComboMember> members;
  std::string_view script;
};

struct DescriptionScriptsView {
  DescriptionFetch fetch = DescriptionFetch::kUnasked;
  // Le serveur ne connaît pas l'objet.
  bool unknown = false;
  std::string_view main;
  std::string_view equip;
  std::string_view unequip;
  std::span<const DescriptionComboView> combos;
};

struct DescriptionView {
  std::uint32_t item_id = 0;
  // Le catalogue du vote shop, pour l'entrée du menu d'un item.
  std::span<const VoteShopOffer> vote_shop;
  // Le nom DÉCORÉ, prêt à montrer : raffinement, cartes, emplacements. `app` le
  // compose quand l'objet est encore là, et se rabat sur le nom de base sinon.
  std::string_view name;
  // Les lignes de la table, BALISAGE COMPRIS. Voir `rich_text.h`.
  std::span<const std::string> lines;
  // L'illustration, ou l'icône à défaut. ⚠ `app` A DÉJÀ CHOISI : `ui` ne sait
  // pas qu'il existe trois images pour un objet.
  SkinImage picture;

  // 🔴 EST-CE LE DESSIN D'UNE CARTE ? `ui` ne saurait pas le dire — l'appartenance
  // se lit dans une table de l'installation —, et une seule chose en dépend : la
  // fenêtre laisse alors le survol de la vignette montrer le dessin EN ENTIER.
  // C'est ce que le client fait, et ce dessin est le seul qui mérite d'être vu
  // en grand : les cartes font environ 300 sur 460, réduites à 120.
  bool picture_is_card = false;
  // LE DESSIN EN ENTIER, DANS L'APERÇU AUSSI — quand on survole une pochette de
  // l'album. On y regarde des cartes : la carte à sa taille est justement ce
  // qu'on vient voir, et la réduire à une vignette ne dirait rien de plus que la
  // pochette elle-même.
  bool picture_whole = false;
  // L'équipement cassé prend un suffixe et une ombre rouge, comme au client.
  bool damaged = false;
  // L'APPARENCE DE LA PIÈCE TENUE, et si elle se montre sur un pantin — une
  // coiffe ou une cape. Zéro, et faux, pour tout le reste.
  std::uint16_t view_id = 0;
  bool doll_possible = false;
  // LA PIÈCE A UN EFFET DE CHAPEAU : son illustration montre le pantin, nu s'il
  // le faut, qui porte l'effet. Voir `DollOverlay`.
  bool doll_effect = false;
  // UN LIVRE QU'ON POSSÈDE : « Lire » et « Lecture auto » paraissent.
  bool readable_book = false;
  DescriptionAutoloot autoloot = DescriptionAutoloot::kUnavailable;
  // LE NOM EN MORCEAUX, quand l'un d'eux se colore — le forgeron d'un objet
  // forgé ou signé. Vide sinon. 🔴 LA BARRE DE TITRE NE SAIT PAS COLORER : la
  // fenêtre réécrit alors le nom en tête de son corps.
  std::span<const DescriptionNamePiece> name_pieces;

  // LA PIÈCE PORTÉE AU MÊME EMPLACEMENT, quand le client en compare une — voir
  // `sim::EquippedToCompare`. Nulle sinon. ⚠ SA VUE À ELLE, et elle ne compare
  // rien : la comparaison ne s'emboîte pas.
  const DescriptionView* compared = nullptr;

  // ── CE QUE PORTE L'OBJET QU'ON TIENT ──────────────────────────────────────
  //
  // ⚠ DES DONNÉES D'INSTANCE : elles n'existent que pour un objet du sac, d'un
  // contenant ou d'un lien — jamais pour une entrée de catalogue.
  //
  // Les cases de carte, jusqu'à la dernière occupée ou au nombre d'emplacements.
  std::span<const DescriptionCardRow> cards;
  int card_slots = 0;
  std::span<const DescriptionOptionRow> options;
  // 🔴 UN ŒUF DE FAMILIER : ses cases portent la fiche du familier, pas des
  // cartes, et son drapeau « cassé » veut dire ÉCLOS.
  bool pet_egg = false;
  bool pet_inside = false;
  bool pet_renamed = false;
  // Le libellé de son rang d'intimité, vide s'il est inconnu.
  std::string_view pet_intimacy;

  // 🔴 L'ÉTIQUETTE « CRAFT » : ce que l'Atlas des recettes sait de l'item, et
  // son bouton. C'est un renseignement qu'on vient chercher dans une
  // description, et que rien d'autre dans le jeu ne donne : un joueur qui
  // ramasse un matériau n'a AUCUN moyen d'apprendre qu'il sert à quelque chose.
  //
  // ⚠ NULLE QUAND L'ITEM N'A RIEN À VOIR AVEC LA FABRICATION : on n'annonce que
  // le POSITIF. Une mention « non fabricable » sur des milliers d'items serait
  // du bruit — et fausse quand le fichier de recettes manque. Les deux textes
  // sont déjà traduits, composés par `app`.
  const char* craft_label = nullptr;
  const char* craft_tip = nullptr;

  // « VOTE SHOP : N PTS » : l'item est au catalogue de la boutique, à ce prix.
  // Le bouton INFORME autant qu'il agit — le prix est la question d'après.
  //
  // 🔴 NUL QUAND L'ITEM N'Y EST PAS, et nul aussi tant que le catalogue n'est pas
  // arrivé : une absence ne prouve rien, on n'annonce que le POSITIF.
  const char* vote_shop_label = nullptr;

  // LE STAFF PEUT SE FAIRE DONNER L'ITEM : la ligne « Staff : [N] [@item] »
  // paraît sous les boutons. Faux, elle n'existe pas. ⚠ FAUX AUSSI POUR LA
  // PIÈCE PORTÉE d'une comparaison : on demande l'item qu'on décrit.
  bool staff_item = false;
  // L'ITEM S'EMPILE-T-IL sur le serveur ? Vide : on ne sait pas, et la
  // quantité du staff n'est bornée que par une pile pleine.
  std::optional<bool> stackable;
  // Les cases libres du sac, qui bornent la demande d'un item non empilable.
  // Vide tant que le serveur n'a pas dit la capacité du sac : rien n'est borné.
  std::optional<int> free_bag_slots;

  // ── L'ONGLET « PROBABILITÉS » ─────────────────────────────────────────────
  //
  // Ce que rend l'objet quand c'est un CONTENEUR. Vide pour tout le reste, et
  // l'onglet n'existe alors pas.
  std::span<const DescriptionOddsRow> odds;
  // Plusieurs tirages : la colonne « Tirage » dit lequel.
  bool odds_many_groups = false;

  // Le nom de l'objet d'où l'on vient, pour « < Retour ». Vide sans retour.
  std::string_view back_name;

  // ── LES ONGLETS DU SERVEUR ────────────────────────────────────────────────
  //
  // ⚠ FAUX DANS L'APERÇU, qui n'a pas d'onglets, et hors ligne, où personne ne
  // répondrait : les quatre onglets n'existent alors pas.
  bool server_tabs = false;
  DescriptionDropsView drops_ordinary;
  DescriptionDropsView drops_boss;
  DescriptionScriptsView scripts;
};

// Ce que la fenêtre garde d'une image à l'autre.
struct DescriptionWindowState {
  bool open = false;
  // L'objet montré. Zéro = rien à montrer, et la fenêtre reste fermée.
  std::uint32_t item_id = 0;
  // LA PLACE D'OÙ ELLE A ÉTÉ OUVERTE, quand c'était depuis le sac. Zéro sinon.
  //
  // ⚠ ELLE NE SERT QU'À DÉCORER LE NOM — raffinement, cartes — et sa disparition
  // n'est pas une panne : l'objet consommé, la description reste, sous son nom
  // de base.
  std::uint16_t from_slot = 0;

  // 🔴 ET DE QUEL CONTENANT ELLE SORT. Une place ne veut rien dire seule : la
  // place 3 du sac et la place 3 du coffre désignent deux objets différents.
  ItemHolder from_holder = ItemHolder::kInventory;
  // ⚠ ELLE S'OUVRE SOUS LE CURSEUR, comme au client — mais une seule fois. Une
  // fenêtre qui se replacerait à chaque image serait impossible à déplacer.
  bool place_at_mouse = false;
  // LE BOUTON « SIGNALER UN BUG » de sa barre de titre, posé par `app` depuis
  // les réglages.
  bool bug_report = false;
  // L'aperçu de la pièce portée, sous « ViewID : N ».
  DescriptionDollState doll;
  // LA COMPARAISON MONTRÉE, posée par `app` depuis les réglages : deux
  // colonnes, la pièce portée à gauche, la décrite à droite — l'ordre du client.
  bool compare = true;
  // QUAND LE TEXTE SE SÉLECTIONNE, posé par `app` depuis les réglages.
  RichTextSelect text_select = RichTextSelect::kWithModifier;
  // OÙ, EXACTEMENT, posé par `app` depuis les réglages. 🔴 SEULE UNE OUVERTURE
  // LA REPLACE : décrire un autre item fenêtre ouverte en
  // change le contenu, pas la place — sinon elle sauterait sous la souris à
  // chaque clic, loin de là où on la lisait.
  DescriptionPlacement placement;
  bool was_open = false;

  // 🔴 ET ELLE SE POSE DEVANT. Une fenêtre déjà née garde son rang : rouverte,
  // elle reparaîtrait au rang qu'elle avait la dernière fois — sous le sac,
  // d'où le clic vient justement de partir.
  //
  // ⚠ À CHAQUE DEMANDE, PAS SEULEMENT À L'OUVERTURE. Décrire un AUTRE objet
  // pendant qu'elle est ouverte n'offre aucun front montant : sans cela son
  // contenu changerait sans qu'elle remonte, et le joueur ne verrait rien se
  // passer.
  bool raise = false;

  // LE TITRE COMPOSÉ, et de quoi savoir s'il est encore bon. ⚠ RECOMPOSÉ
  // SEULEMENT QUAND IL LE FAUT, comme celui du coffre : c'est un nom d'objet, pas
  // un littéral du catalogue.
  //
  // ⚠ LE TITRE PORTE AUSSI « - Broken » : l'état abîmé fait partie de la clé,
  // sans quoi un objet réparé le garderait jusqu'au changement d'objet.
  std::string title;
  std::string title_for;
  std::size_t title_language = 0;
  bool title_damaged = false;
  bool title_comparing = false;

  // ── LE FIL DE NAVIGATION ──────────────────────────────────────────────────
  //
  // Un objet de la table de tirage se décrit À LA PLACE de celui qu'on lit, et
  // l'on revient d'un pas par « < Retour ». ⚠ LA PILE NE VAUT QUE POUR LE CHEMIN
  // SUIVI DEPUIS LA DESCRIPTION : fermée, ou un objet arrivé d'ailleurs (un clic
  // dans le sac pendant qu'elle est ouverte), elle ouvre un autre fil.
  std::vector<std::uint32_t> back;
  std::uint32_t navigated_to = 0;
  std::uint32_t last_item = 0;

  // ── L'ONGLET « PROBABILITÉS » ─────────────────────────────────────────────
  //
  // 🔴 IL DISPARAÎT dès qu'on suit un lien vers un objet ordinaire, et ImGui
  // rend alors la sélection au premier onglet : le retour au conteneur
  // rouvrirait sur « Description ». On redemande donc la sélection à la
  // RÉAPPARITION de l'onglet, une fois — la redemander à chaque image
  // clouerait le joueur dessus.
  bool odds_tab_present = false;
  bool odds_tab_sticky = false;
  // Le filtre par nom, retenu PAR OBJET : passer d'une boîte à l'autre n'hérite
  // pas du filtre de la précédente.
  std::unordered_map<std::uint32_t, ImGuiTextFilter> odds_filters;
  std::vector<int> odds_order;
  // Les filtres des deux onglets de sources, par objet eux aussi, et leur ordre.
  std::unordered_map<std::uint32_t, ImGuiTextFilter> ordinary_filters;
  std::unordered_map<std::uint32_t, ImGuiTextFilter> boss_filters;
  std::vector<int> drops_order;
  // Le menu d'une ligne, armé dans la table et ouvert hors d'elle : une cellule
  // change la pile d'identifiants, et le surgissant ne s'y retrouverait pas.
  ItemLinkMenu link_menu;
  // La quantité que le staff demande. ⚠ RETENUE POUR LA SESSION, d'un item à
  // l'autre : on se fait souvent donner plusieurs items au même compte.
  int staff_item_amount = 1;
};

// Ce que le joueur y a demandé.
struct DescriptionWindowAction {
  // L'adresse d'un lien qu'il vient de suivre, vide sinon.
  //
  // 🔴 `ui` NE L'OUVRE PAS. Suivre une adresse lance un programme du système ;
  // c'est un geste vers le DEHORS, déclenché par une donnée du jeu, et il se
  // décide chez `app` — pas au fond d'une couche de dessin.
  //
  // ⚠ UNE CHAÎNE PAR VALEUR, PAS UNE VUE : la disposition d'où elle sort meurt
  // à la fin de l'image, et l'appelant lit cette réponse APRÈS. Une vue y
  // pendrait. Le prix ne se paie qu'au clic.
  std::string followed;
  RichLinkKind followed_kind = RichLinkKind::kAddress;
  // L'étiquette « Craft » a été cliquée : l'Atlas s'ouvre sur l'item.
  bool open_craft_atlas = false;
  // Le bouton du vote shop a été cliqué : la boutique s'ouvre, l'item au panier.
  bool open_vote_shop = false;
  // « + alootid » ou « - alootid » : l'item entre dans la liste, ou en sort.
  bool toggle_autoloot = false;
  // La case « Comparer » a changé, et vaut maintenant `compare`.
  bool compare_changed = false;
  bool compare = false;
  // « Signaler un bug » : `app` compose le contexte de l'item montré.
  bool report_bug = false;
  // « Lire » et « Lecture auto », sur un livre.
  bool read_book = false;
  bool recite_book = false;
  // Un geste sur une ligne de la table de tirage — ou son survol, pour
  // l'aperçu. ⚠ DÉCRIRE UN OBJET N'EN SORT PAS : la fenêtre navigue elle-même.
  ChatLinkAsk link;
  // « Rafraichir » d'un onglet de sources : oublier la réponse, et redemander —
  // un taux bouge avec l'équipement et les événements.
  bool refresh_ordinary = false;
  bool refresh_boss = false;
  // Le bouton « @item » du staff : l'item décrit, à la quantité du champ.
  std::optional<StaffItemGrant> staff_item;
};

inline constexpr const char* kDescriptionWindowTitle = core::TrText("Description");
inline constexpr const char* kDescriptionWindowId = "description";

// LA COULEUR DU NOM DANS L'APERÇU : le brun des en-têtes du client.
//
// ⚠ ELLE NE SERT PAS À LA FENÊTRE : là-bas le nom est dans la barre de titre, et
// c'est l'habillage qui décide de son encre.
inline constexpr ImU32 kDescriptionNameColor = IM_COL32(77, 61, 26, 255);
// L'ombre rouge d'un équipement cassé, décalée d'un pixel sous le nom.
inline constexpr ImU32 kDescriptionBrokenShadow = IM_COL32(250, 80, 80, 255);

// LA FENÊTRE : celle qu'on ouvre au Ctrl+clic droit, et qui reste.
[[nodiscard]] DescriptionWindowAction DrawDescriptionWindow(DescriptionWindowState& state,
                                                            const DescriptionView& view,
                                                            const SkinArt& art);

// L'APERÇU : ce que le survol montre.
//
// 🔴 CE N'EST PAS UNE FENÊTRE PLUS PETITE, et la différence se voit partout :
// elle n'a pas de barre de titre, donc le nom est DANS son corps ; elle ne se
// clique pas, donc l'identifiant s'y écrit en clair au lieu du lien ; et elle
// est étroite, donc son texte se serre à DROITE de l'illustration au lieu de
// passer dessous.
//
// ⚠ ELLE OUVRE ET FERME SON INFOBULLE ELLE-MÊME, et il le faut : son habillage
// — le cadre en boîte peint DERRIÈRE un contenu dont on ne connaît la taille
// qu'après — est inséparable de son ouverture.
void DrawDescriptionPreview(const DescriptionView& view, const SkinArt& art);

// LE FIL DE NAVIGATION de la description d'un objet.
//
// Tient la pile de retour à jour, une fois par image, AVANT le dessin : fermée,
// ou un objet arrivé d'ailleurs, et le fil repart de zéro. `DrawDescriptionWindow`
// l'appelle lui-même.
void NoteDescriptionNavigation(DescriptionWindowState& state);
// Décrit cet objet À LA PLACE de celui qu'on lit, et empile celui-ci — le geste
// d'un lien suivi DEPUIS la description.
void NavigateDescription(DescriptionWindowState& state, std::uint32_t item_id);

// LA LARGEUR MAXIMALE DE L'APERÇU.
//
// ⚠ UN PLAFOND, PAS UNE LARGEUR. L'infobulle se dimensionne sur son contenu :
// une description courte donne une petite bulle, une longue s'arrête là. Une
// largeur FIXE laisserait un grand vide sous « Potion rouge ».
inline constexpr float kDescriptionPreviewWidest = 330.0F;
// Et sa hauteur ne dépasse pas cette part de l'écran : au-delà, elle sortirait
// par le haut ou par le bas selon la place, ce qui la rend illisible.
inline constexpr float kDescriptionPreviewScreenShare = 0.8F;

// LA PREMIÈRE LIGNE EST-ELLE LE LIEN DE BASE DE DONNÉES ?
//
// Les tables du client ouvrent presque toujours leur description par un
// `<URL>…</URL>` vers un site externe. ⚠ LES DEUX VUES N'EN FONT PAS LA MÊME
// CHOSE, et c'est mesuré chez le client :
//
//   LA FENÊTRE LA MONTRE, à la place de la ligne « ID : N » — le lien PORTE déjà
//   l'identifiant, « ItemID : 12103 », et il est cliquable. Écrire les deux,
//   c'est le même nombre sur deux lignes.
//
//   L'APERÇU LA SAUTE et écrit « ID : N » : une infobulle ne se clique pas, un
//   lien y serait mort.
[[nodiscard]] bool IsDatabaseLinkLine(std::string_view line);

// ── LA MÊME FENÊTRE, POUR UNE COMPÉTENCE ────────────────────────────────────
//
// 🔴 DEUX FENÊTRES, UN SEUL DESSIN. Le client en a deux — `0xc` pour un objet,
// `0x2e` pour une compétence — et ce client les rend avec les mêmes aides :
// même texte balisé, même sélection, mêmes liens. Ce qui diffère est ce
// qu'elles PORTENT, et rien d'autre.
//
// ⚠ ET ELLES S'OUVRENT ENSEMBLE SANS SE GÊNER : une description d'objet ouverte
// ne ferme pas celle d'une compétence. Deux états, deux fenêtres.

// LE CAST D'UNE COMPÉTENCE À UN RANG, tout en millisecondes.
//
// 🔴 DEUX TEMPS D'INCANTATION, ET ILS NE SE RÉDUISENT PAS PAREIL : le VARIABLE
// tombe avec la DEX et l'INT et se coupe à l'interruption, le FIXE ne bouge que
// sous certains états. Les additionner perdrait la moitié de ce qu'on vient
// demander.
struct SkillCastLevelView {
  int cast_variable_ms = 0;
  int cast_fixed_ms = 0;
  int cooldown_ms = 0;
  int after_delay_ms = 0;
};

// L'ESTIMATION DE DÉGÂTS, telle que le serveur la rend.
struct SkillDamageView {
  // ⚠ FAUX TANT QUE RIEN N'EST ARRIVÉ, et ce n'est pas « zéro dégât » : la
  // fenêtre doit dire qu'elle attend plutôt que d'écrire un compte inventé.
  bool told = false;
  // 0 : l'estimation vaut. 1 : la compétence ne fait pas de dégâts. 2 : cible
  // introuvable.
  int status = 0;
  // Ce que le coup EST aux yeux du serveur : 1 arme, 2 magie, le reste divers.
  int attack_kind = 0;
  int level = 0;
  int hits = 0;
  std::int64_t least = 0;
  std::int64_t most = 0;
  std::int64_t average = 0;
  // ⚠ TROIS CIBLES, ET LE ZÉRO N'EST PAS « AUCUNE » : zéro est le mannequin
  // neutre sans défense, `kDamageAgainstSelf` est soi-même, le reste est un vrai
  // monstre.
  std::uint32_t target_mob_id = 0;
  std::string_view target_name;
};

// La cible « soi-même », le miroir du duel. ⚠ LA MÊME VALEUR QUE LE FIL, et elle
// ne se déduit pas : c'est le serveur qui la définit.
inline constexpr std::uint32_t kSkillDamageAgainstSelf = 0xFFFFFFFFU;

// Ce que la fenêtre de compétence montre.
struct SkillDescriptionView {
  std::uint16_t skill_id = 0;
  // Le libellé du client — « Heal ». ⚠ PAS LE NOM INTERNE, qui nomme le
  // fichier de l'icône.
  std::string_view name;
  // Les lignes de la table, BALISAGE COMPRIS. Voir `rich_text.h`.
  //
  // ⚠ LES DEUX PREMIÈRES SONT L'EN-TÊTE — « Heal - ID:28 » puis
  // « Max Level: 10 ». Le client les tient dans une boîte à part, au-dessus du
  // corps ; nous les laissons à leur place dans le fichier, qui est la même.
  std::span<const std::string> lines;
  SkinImage icon;
  // LE COÛT EN POINTS DE COMPÉTENCE, au rang qui compte. Zéro quand la table n'en
  // donne aucun — une compétence passive n'en coûte pas.
  int sp_cost = 0;

  // ── CE QUE LE SERVEUR SEUL SAIT ───────────────────────────────────────────
  //
  // ⚠ VIDES TANT QU'ON N'A PAS DEMANDÉ, et c'est le cas NORMAL : ces deux-là ne
  // viennent pas avec la description, ils se demandent quand on ouvre leur
  // onglet. La fenêtre dit alors qu'elle attend.
  std::span<const SkillCastLevelView> cast;
  SkillDamageView damage;
};

// Ce que la fenêtre de compétence garde d'une image à l'autre. Mêmes règles que
// celle d'un objet : voir `DescriptionWindowState`.
struct SkillDescriptionWindowState {
  bool open = false;
  std::uint16_t skill_id = 0;
  // LE BOUTON « SIGNALER UN BUG » de sa barre de titre, posé par `app` depuis
  // les réglages.
  bool bug_report = false;
  bool place_at_mouse = false;
  bool raise = false;
  // Où elle s'ouvre, posé par `app` depuis les réglages ; et si elle l'était à
  // l'image d'avant — seule une OUVERTURE la replace.
  DescriptionPlacement placement;
  bool was_open = false;
  RichTextSelect text_select = RichTextSelect::kWithModifier;

  // ⚠ LE TITRE PORTE AUSSI LE COÛT EN SP, qui change avec le niveau décrit :
  // il fait partie de la clé, sans quoi Gloria niveau 5 montrerait le coût du 1.
  std::string title;
  std::string title_for;
  std::size_t title_language = 0;
  int title_sp_cost = 0;

  // ── LES TROIS ONGLETS ─────────────────────────────────────────────────────
  //
  // 🔴 LE CAST ET LES DÉGÂTS NE SONT PAS DANS LA DESCRIPTION, et ils ne peuvent
  // pas y être : ils se CALCULENT, sur l'équipement et les états du personnage,
  // et changent d'une minute à l'autre. Le texte du jeu, lui, est écrit une fois
  // pour toutes. Deux natures différentes, deux onglets.
  int tab = 0;
  // La cible de l'estimation : soi-même, ou le numéro de monstre saisi.
  //
  // ⚠ ZÉRO EST UNE CIBLE, pas une absence : c'est le mannequin neutre sans
  // défense, celui qui donne les dégâts BRUTS.
  bool damage_self = false;
  int damage_target = 0;
};

inline constexpr const char* kSkillDescriptionWindowId = "skill_description";

// CE QUE LE JOUEUR A DEMANDÉ À LA FENÊTRE D'UNE COMPÉTENCE.
//
// ⚠ À PART DE CELLE D'UN OBJET : elle porte les mêmes liens, et DEUX DEMANDES de
// plus. Les mêler ferait porter à la description d'une potion des champs qui ne
// la concernent pas.
struct SkillDescriptionAction {
  // L'adresse d'un lien qu'il vient de suivre, vide sinon. Voir
  // `DescriptionWindowAction` : `ui` ne l'ouvre pas.
  std::string followed;
  RichLinkKind followed_kind = RichLinkKind::kAddress;
  // « Signaler un bug » : `app` compose le contexte de la compétence montrée.
  bool report_bug = false;

  // La compétence dont on veut la table de cast, zéro sinon.
  //
  // ⚠ DEMANDÉE À CHAQUE IMAGE OÙ L'ONGLET EST OUVERT : c'est `app` qui décide de
  // ne pas harceler le serveur, parce que c'est lui qui sait ce qu'il a déjà.
  std::uint16_t want_cast = 0;
  // Et celle dont on veut l'estimation, avec la cible visée.
  std::uint16_t want_damage = 0;
  std::uint32_t damage_target = 0;
  // 🔴 REDEMANDER MÊME SI L'ON A DÉJÀ : le bouton « Calculer ». Ces chiffres
  // dépendent de l'équipement et des états, et rien n'avertit qu'ils ont changé.
  bool refresh = false;
};

// COMBIEN DE LIGNES D'EN-TÊTE la table écrit avant le corps.
//
// 🔴 DEUX, TOUJOURS : « <nom> - ID:<n> » puis « Max Level: <n> ». Le client les
// range dans une boîte de texte SÉPARÉE du corps, et c'est ce qui prouve que le
// découpage est une règle de la table et non une coïncidence.
inline constexpr std::size_t kSkillDescriptionHeaderLines = 2;

// LA FENÊTRE D'UNE COMPÉTENCE : celle qu'on ouvre au Ctrl + clic droit.
[[nodiscard]] SkillDescriptionAction DrawSkillDescriptionWindow(SkillDescriptionWindowState& state,
                                                                const SkillDescriptionView& view,
                                                                const SkinArt& art);

}  // namespace ro::ui
