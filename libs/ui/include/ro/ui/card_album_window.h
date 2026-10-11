#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// L'ALBUM DE CARTES : UN CLASSEUR.
//
// Deux pages côte à côte, chacune une grille de POCHETTES — une par carte du
// catalogue, avec l'illustration de la carte dedans. Une pochette ouverte montre
// l'illustration en couleur et sa réserve en pastille ; une pochette scellée
// montre une silhouette grise et son cachet. Des intercalaires classent les
// pages par emplacement d'équipement, et une barre suit la complétion.
//
// 🔴 LA PREMIÈRE COPIE D'UNE CARTE EST SACRIFIÉE : elle ouvre sa pochette pour
// toujours. Les suivantes s'y rangent et restent retirables. Ce geste-là ne se
// défait pas, et c'est pourquoi il a UNE seule porte — une confirmation, que le
// joueur peut choisir de sauter dans les réglages.
//
// Les cartes ENTRENT depuis le sac : glissée sur le classeur, ou « Vers l'album »
// de son menu, une carte se range dans sa pochette ouverte ou se propose au
// sacrifice. Elles en SORTENT en glissant une pochette sur le sac. Le sac EST
// donc le plateau de l'album : il n'y a pas de cartes « en main » ici.
//
// ⚠ `ui` NE CONNAÎT NI LE PROTOCOLE NI LE MONDE : elle reçoit le catalogue déjà
// nommé, les cartes du sac, les vignettes demandées, et rend des intentions.

// ⚠ L'IDENTITÉ NE SE TRADUIT PAS : c'est elle qui porte la place que le joueur a
// donnée à la fenêtre.
inline constexpr const char* kCardAlbumWindowId = "card_album";

// LA CHARGE D'UNE POCHETTE QU'ON GLISSE HORS DU CLASSEUR : l'identifiant de la
// carte, sur quatre octets.
//
// ⚠ UN NOM À ELLE, et non celui des objets du sac : une pochette n'est pas une
// place d'inventaire. Seul le sac l'accepte — lâchée sur la barre de raccourcis
// ou sur le monde, elle ne fait rien, et c'est ce que le refus d'ImGui garantit
// sans que personne ait à le vérifier.
inline constexpr const char* kCardAlbumDragPayload = "ro_album_card";

// LES DEUX ALBUMS D'OBJETS qui rendent des cartes — l'Old et le Mystical Card
// Album. Leur provenance se lit en macarons au coin des pochettes.
inline constexpr std::size_t kCardAlbumSourceCount = 2;

// LES FAMILLES D'EMPLACEMENT, une par intercalaire (hors « Tous »). Une carte
// peut en porter plusieurs : une carte de tête qui se pose en haut ET au milieu.
//
// ⚠ `ui` NE LIT PAS LES BITS D'EMPLACEMENT DU SERVEUR : c'est `app`, qui les
// connaît par `sim`, qui range chaque carte dans ses familles.
inline constexpr std::uint8_t kCardFamilyArmor = 1U << 0U;
inline constexpr std::uint8_t kCardFamilyWeapon = 1U << 1U;
inline constexpr std::uint8_t kCardFamilyShield = 1U << 2U;
inline constexpr std::uint8_t kCardFamilyGarment = 1U << 3U;
inline constexpr std::uint8_t kCardFamilyShoes = 1U << 4U;
inline constexpr std::uint8_t kCardFamilyAccessory = 1U << 5U;
inline constexpr std::uint8_t kCardFamilyHead = 1U << 6U;

// Une demande du staff sur un album d'objets, en attente du script qui la
// portera au serveur. Voir `app/items/card_album_delta.h`.
enum class CardAlbumIntent : std::uint8_t { kNone, kAdd, kRemove };

// CE QUE L'UN DES DEUX ALBUMS D'OBJETS DIT D'UNE CARTE.
struct CardAlbumSourceMark {
  bool present = false;
  // La chance, en pour-cent.
  double percent = 0.0;
  // « N chances sur M ». ⚠ `total` NUL QUAND LE DÉNOMINATEUR N'EST PAS UNIQUE
  // (plusieurs tirages) : la mention se tait alors, plutôt que de citer un M qui
  // ne vaut que pour une partie des chances.
  int weight = 0;
  int total = 0;
  CardAlbumIntent intent = CardAlbumIntent::kNone;
  int intent_rate = 0;
};

// UNE POCHETTE : une carte du catalogue, telle que le serveur la décrit.
struct CardAlbumPocketView {
  std::uint32_t card_id = 0;
  std::uint16_t reserve = 0;
  bool unlocked = false;
  // Voir `kCardFamilyArmor`.
  std::uint8_t families = 0;
  // Zéro ordinaire, un mini-boss, deux MVP — voir `CardAlbumView::dropper_known`.
  std::uint8_t dropper_rank = 0;
  // Le NOM COMPLET, « Poring Card » ; la fenêtre le raccourcit sous la pochette.
  // ⚠ VIDE quand l'installation ne connaît pas la carte : elle reste trouvable
  // par son identifiant.
  std::string name;
  // L'emplacement principal, terme du jeu en anglais.
  std::string_view slot_label;
  std::array<CardAlbumSourceMark, kCardAlbumSourceCount> sources{};
};

// L'UN DES DEUX ALBUMS D'OBJETS.
struct CardAlbumSourceView {
  std::uint32_t item_id = 0;
  // La lettre du macaron, « O » ou « M ».
  std::string_view letter;
  // Son nom tel que l'installation le donne.
  std::string_view name;
  // Le poids le plus fréquent de sa table : ce que la demande d'ajout propose.
  int typical_rate = 0;
};

// Une carte que le sac porte : ce qu'on peut offrir à l'album.
struct CardAlbumHandCard {
  std::uint16_t client_slot = 0;
  std::uint32_t card_id = 0;
  int amount = 0;
};

// LES IMAGES D'UNE POCHETTE, demandées par la fenêtre à l'image précédente.
struct CardAlbumArt {
  std::uint32_t card_id = 0;
  bool silhouette = false;
  // La vignette à la taille exacte de la pochette. ⚠ VIDE AVEC `pending` : elle
  // arrive à l'image suivante, la pochette montre l'icône en attendant.
  SkinImage picture;
  bool pending = false;
  // L'icône du sac, vingt-quatre pixels.
  SkinImage icon;
};

// CE QUE LA FENÊTRE DEMANDE QU'ON LUI PRÉPARE pour l'image suivante.
struct CardAlbumArtAsk {
  std::uint32_t card_id = 0;
  bool silhouette = false;
};

// LE TON D'UN COMPTE RENDU.
enum class CardAlbumTone : std::uint8_t { kDone, kWarning, kRefusal };

struct CardAlbumView {
  std::span<const CardAlbumPocketView> pockets;
  // CHANGE QUAND LE CATALOGUE CHANGE — une réponse, une langue, une demande du
  // staff. C'est ce qui refait l'ordre des pages, et rien d'autre.
  std::uint32_t revision = 0;
  int unlocked_count = 0;
  std::int64_t total_reserve = 0;
  // Le serveur envoie-t-il la nature du monstre ? Sinon les boutons de nature se
  // cachent plutôt que de montrer un « MVP » toujours vide.
  bool dropper_known = false;
  // Un autre compte de jeu du même compte Moonlight tient l'album.
  bool blocked = false;
  // Une demande est partie, et rien n'est encore revenu.
  bool loading = false;

  std::span<const CardAlbumHandCard> hand;
  std::span<const CardAlbumSourceView> sources;
  std::span<const CardAlbumArt> art;
  // La largeur et la hauteur des vignettes servies, en pixels. Une vignette
  // d'une autre taille (l'échelle vient de changer) ne se dessine pas.
  int art_width = 0;
  int art_height = 0;

  // Le dernier compte rendu, VIDE quand il n'y a plus rien à dire.
  std::string_view notice;
  CardAlbumTone notice_tone = CardAlbumTone::kDone;
  // Ce que le dernier geste du STAFF a donné — à part, parce que ce n'est pas
  // le serveur qui a répondu, c'est un fichier qui a été écrit.
  std::string_view staff_notice;
  bool staff_notice_ok = true;

  // La pochette que le dernier ordre a touchée, surlignée le temps du compte
  // rendu. `highlight_serial` change à chaque nouvel ordre abouti : la fenêtre
  // tourne alors ses pages jusqu'à elle, UNE fois.
  std::uint32_t highlight_card = 0;
  std::uint32_t highlight_serial = 0;

  // L'édition des albums d'objets est-elle ouverte à ce joueur ?
  bool staff = false;
};

// Ce que la fenêtre demande au serveur.
enum class CardAlbumVerb : std::uint8_t {
  kNothing,
  kRefresh,
  // La fenêtre se ferme : rendre le verrou.
  kClose,
  // `slot` = place du sac ; une copie. `rest` = ce qui suivra dans la pochette.
  kSacrifice,
  // `slot` = place du sac.
  kDeposit,
  // `card_id` seul.
  kWithdraw,
};

// Une demande du staff sur un album d'objets.
struct CardAlbumStaffAsk {
  std::uint32_t album_id = 0;
  std::uint32_t card_id = 0;
  CardAlbumIntent intent = CardAlbumIntent::kNone;
  int rate = 0;
  // Vrai : annuler la demande posée sur ce couple.
  bool cancel = false;
};

struct CardAlbumWindowAction {
  CardAlbumVerb verb = CardAlbumVerb::kNothing;
  std::uint16_t slot = 0;
  std::uint32_t card_id = 0;
  int amount = 0;
  int rest = 0;
  // La fenêtre a refusé toute seule, sans rien envoyer : l'objet offert n'est
  // pas une carte de monstre.
  bool refused_not_a_card = false;
  // Décrire cet objet — une carte, ou un album d'objets par son macaron.
  std::uint32_t describe_id = 0;
  // Ouvrir la page de cet objet dans la base de données du site.
  std::uint32_t database_id = 0;
  // La carte sous la souris, pour l'aperçu de description.
  std::uint32_t hovered_id = 0;
  // La puce de la barre de titre : les réglages de l'album.
  bool settings_asked = false;
  // « Signaler un bug » : `app` joint ce que les pages montrent — la vue, la
  // double page, le dialogue en cours et le dernier compte rendu.
  bool report_bug = false;
  CardAlbumStaffAsk staff;
};

// LES RÉGLAGES, retenus d'une partie à l'autre. ⚠ LE PAPIER DES PAGES ET LA
// LUMINOSITÉ DES IMAGES n'en sont pas : ils suivent l'habillage du skin
// (`SkinLook::card` et `SkinLook::art_brightness`).
struct CardAlbumLook {
  // L'icône d'inventaire devant le nom, sous la pochette. OFF : l'illustration
  // dit déjà la carte, et l'icône prend sur la largeur du nom.
  bool name_icon = false;
  // Les macarons « O » / « M ». ON : ils ne commandent rien, et répondent à la
  // question que pose une pochette scellée — « comment j'obtiens celle-là ? ».
  bool sources = true;
  // Le liseré de nature : bleu mini-boss, orange MVP. ON, pour la même raison.
  bool dropper_rim = true;
  // Sacrifier sans la confirmation. OFF : ce réglage AGIT, sur un geste
  // irréversible.
  bool auto_sacrifice = false;
};

// Une action qui attend une QUANTITÉ.
struct CardAlbumQuantityAsk {
  CardAlbumVerb verb = CardAlbumVerb::kNothing;
  std::uint16_t slot = 0;
  std::uint32_t card_id = 0;
  int most = 0;
  int amount = 1;
};

struct CardAlbumWindowState {
  bool open = false;
  // Dessinée à l'image précédente. ⚠ C'EST CE QUI DIT QU'ELLE VIENT DE SE
  // FERMER, quelle que soit la main qui l'a fermée : il faut alors rendre le
  // verrou au serveur. `app` le remet à faux quand la SESSION se termine — il
  // n'y a alors plus de verrou à rendre.
  bool shown = false;
  bool raise = false;
  CardAlbumLook look;
  // Le bouton « Signaler un bug » de la barre de titre, selon le réglage.
  bool bug_report = false;

  // ── Ce que les pages montrent ──────────────────────────────────────────────
  char filter[64] = {};
  int sort_mode = 0;
  int show_filter = 0;
  int slot_filter = 0;
  // Un bit par nature (1 << rang) ; zéro = toutes.
  std::uint8_t nature_mask = 0;

  // L'ordre des pochettes : des rangs dans `pockets`, filtrés et triés. Refait
  // quand le catalogue ou la vue change, pas à chaque image.
  std::vector<int> order;
  std::uint32_t order_for = 0;
  bool order_dirty = true;
  // La complétion de l'intercalaire, hors recherche et hors « débloquées » :
  // c'est la collection qu'on mesure, pas la vue.
  int family_total = 0;
  int family_unlocked = 0;

  // ── La pagination ──────────────────────────────────────────────────────────
  // Le rang dans `order` de la première pochette de la page de gauche. ⚠ UN
  // RANG ET NON UN NUMÉRO DE PAGE : le nombre de pochettes par page suit la
  // taille de la fenêtre, et garder le rang laisse le joueur devant les mêmes
  // cartes après un redimensionnement.
  int first = 0;
  int per_spread = 0;
  double flip_time = -1.0;
  std::uint32_t highlight_seen = 0;

  // ── Le calage par pochette entière ─────────────────────────────────────────
  ImVec2 cell_step{0.0F, 0.0F};
  ImVec2 chrome{0.0F, 0.0F};
  bool snap_ready = false;

  // ── Les demandes venues d'ailleurs, consommées à l'image suivante ─────────
  // Le sac offre la carte de cette place — glisser, ou « Vers l'album ».
  std::uint16_t offered_slot = 0;
  // Une pochette lâchée sur le sac : la reprendre.
  std::uint32_t withdraw_asked = 0;

  // ── Les dialogues ──────────────────────────────────────────────────────────
  // Le sacrifice qui attend sa confirmation. ⚠ LA BOÎTE EST DÉCLARÉE UNE FOIS,
  // HORS DE LA BOUCLE DE POCHETTES : ouverte depuis une pochette, elle se
  // replacerait au curseur de l'élément courant et sauterait d'une image à
  // l'autre.
  std::uint16_t confirm_slot = 0;
  std::uint32_t confirm_card = 0;
  int confirm_rest = 0;
  bool confirm_asked = false;
  CardAlbumQuantityAsk quantity;
  bool quantity_asked = false;
  // La demande d'ajout du staff, qui attend son poids de tirage.
  std::uint32_t rate_album = 0;
  std::uint32_t rate_card = 0;
  int rate_value = 1;
  bool rate_asked = false;

  // ── Les vignettes ──────────────────────────────────────────────────────────
  // Ce que la fenêtre a dessiné, pour que l'appelant prépare l'image suivante.
  std::vector<CardAlbumArtAsk> wanted;
  int wanted_width = 0;
  int wanted_height = 0;

  // Le titre, recomposé quand la complétion ou la langue change.
  std::string title;
  int title_unlocked = -1;
  int title_total = -1;
  std::size_t title_language = static_cast<std::size_t>(-1);
};

// La taille de la vignette d'une pochette, en pixels à l'échelle courante :
// 300 sur 400 réduit de deux et demi.
inline constexpr float kCardAlbumArtWidthPixels = 120.0F;
inline constexpr float kCardAlbumArtHeightPixels = 160.0F;

// Les poids de tirage qu'une demande d'ajout accepte, tels que le serveur les
// lit. Le plancher est un : un poids nul rendrait la carte intirable tout en la
// laissant s'afficher.
inline constexpr int kCardAlbumRateLeast = 1;
inline constexpr int kCardAlbumRateMost = 10000;

[[nodiscard]] CardAlbumWindowAction DrawCardAlbumWindow(CardAlbumWindowState& state,
                                                        const CardAlbumView& view,
                                                        const SkinArt& art);

// Le libellé d'une carte sous sa pochette : sans son emplacement entre
// crochets, ni le mot « Card » que TOUTES portent. Jamais vide.
[[nodiscard]] std::string CardAlbumShortName(std::string_view full_name);

}  // namespace ro::ui
