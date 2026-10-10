#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/ui/chat_link.h"
#include "ro/ui/item_link_cell.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/staff_item_grant.h"

namespace ro::ui {

// ── L'ATLAS DES RECETTES ────────────────────────────────────────────────────
//
// Ce que le serveur sait fabriquer, et qu'on peut PARCOURIR.
//
// 🔴 LE JEU N'OFFRE AUCUNE FAÇON DE LE FAIRE. Le serveur n'envoie une liste de
// fabrication qu'après le lancement d'un skill, déjà filtrée sur ce que le
// personnage sait faire : on ne peut savoir ce qu'exige une recette avant
// d'avoir le job, ni ce que deviendront les matériaux qu'on ramasse. Les deux
// questions se posent pourtant AVANT de se spécialiser.
//
// ── UNE SEULE FICHE, QUATRE CHEMINS POUR Y ARRIVER ──────────────────────────
// La fiche est celle d'un ITEM, et elle dit tout ce que l'Atlas sait de lui :
// sa recette, ce qu'il permet de fabriquer, les flèches qu'il donne, et d'où ces
// flèches viennent. Les onglets ne sont donc pas quatre vues : ce sont quatre
// façons de DÉSIGNER l'item dont on veut la fiche — par skill, par produit, par
// matériau, et par source de flèches, dont la clé est le matériau consommé.
//
// ── LES GESTES : CEUX D'UNE CELLULE, PAS D'UN LIEN ──────────────────────────
// 🔴 ICI LE CLIC GAUCHE NAVIGUE, là où il décrit partout ailleurs : dans un
// index, le geste le plus courant est de SUIVRE l'entrée, et ouvrir une
// description à chaque saut obligerait à la refermer à chaque saut. Maj+clic
// pose le lien dans le chat, et le clic droit ouvre le menu de tout item — dont
// la description est la première entrée : rien n'est perdu.
//
// ── CE QU'ELLE NE DIT PAS ───────────────────────────────────────────────────
// AUCUNE CHANCE DE RÉUSSITE. Elle dépend de ce que le joueur tient au moment de
// fabriquer — kit, skill réellement lancé, stats —, dont l'Atlas, consulté hors
// contexte, ne sait rien. Un chiffre « moyen » serait un nombre faux affiché
// avec l'autorité d'un nombre juste.
//
// Cette couche dessine et navigue ; `app` lit le livre, compose les listes et
// la fiche de l'item choisi, et route les gestes.

inline constexpr const char* kCraftAtlasWindowId = "craft_atlas";

// La profondeur de la pile de retour. Un aller-retour prolongé entre deux
// matériaux ne doit pas faire enfler un historique dont personne ne remonte le
// début.
inline constexpr std::size_t kCraftAtlasBackDepth = 64;

// Le filtre, en octets, zéro final compris.
inline constexpr std::size_t kCraftAtlasFilterBytes = 64;

// LE NOMBRE DE FABRICATIONS QUE LE STAFF PEUT DEMANDER D'UN COUP. Assez pour
// éprouver une fabrication en série ; au-delà, les quantités butent de toute
// façon sur une pile pleine (`kStaffItemMostAmount`).
inline constexpr int kCraftAtlasMostCrafts = 100;

// UNE LIGNE D'ITEM, dans une liste ou dans la fiche.
//
// ⚠ LES CHAMPS DE CONTEXTE NE VALENT QUE LÀ OÙ ILS ONT UN SENS, et chaque liste
// dit lesquels elle remplit. Une structure par contexte aurait donné cinq
// lignes presque pareilles, et cinq façons de dessiner la même chose.
struct CraftAtlasRow {
  std::uint32_t item_id = 0;
  // Le nom de l'installation, jamais vide : « #id » à défaut.
  std::string_view name;
  // Utilisable seulement pour les lignes rapportées par `CraftAtlasAnswer::drawn`
  // à l'image précédente : voir là.
  SkinImage icon;
  // Ce que le sac en contient.
  int owned = 0;
  // PRODUIT : combien de fois sa recette est réalisable avec le sac. Moins un
  // quand on ne peut pas conclure — une recette faite de guides seulement.
  int craftable = -1;
  // MATÉRIAU : combien de recettes JOUABLES le consomment.
  int uses = 0;
  // INGRÉDIENT, USAGE, FLÈCHES : la quantité exigée, consommée ou obtenue.
  int quantity = 0;
  // INGRÉDIENT, USAGE : à posséder, mais non consommé — un guide.
  bool kept = false;
  // USAGE : le skill de la recette où il sert, déjà nommé.
  std::string_view skill_label;
  // INGRÉDIENT : l'item s'empile-t-il sur le serveur ? Vide : on ne sait pas.
  std::optional<bool> stackable;
};

// Les produits d'un skill, pour l'onglet des skills.
struct CraftAtlasGroup {
  int skill_id = 0;
  // Déjà nommé — « Cuisine (kit) » compris : voir `app/items/craft_atlas_view.h`.
  std::string_view label;
  std::span<const CraftAtlasRow> products;
};

// Combien d'exemplaires une fabrication réussie rend. Voir
// `app/items/craft_book.h`, qui dit d'où vient chaque mode.
enum class CraftAtlasYieldMode : std::uint8_t {
  kFixed,
  kPerUnit,
  kAllOrNone,
  // 🔴 PROPRE À CE SERVEUR, et rien en jeu ne l'annonce.
  kBonusRoll,
  kTable,
};

struct CraftAtlasRecipeView {
  // Zéro : la recette s'ouvre en utilisant un item, sans skill.
  int skill_id = 0;
  std::string_view skill_label;
  int skill_level = 0;
  int item_level = 0;
  // Faux quand aucune classe de ce serveur n'apprend le skill.
  bool learnable = true;
  CraftAtlasYieldMode yield_mode = CraftAtlasYieldMode::kFixed;
  int yield_least = 1;
  int yield_most = 1;
  // En POUR MILLE : la décimale compte.
  int yield_bonus_permille = 0;
  // Voir `CraftAtlasRow::craftable`.
  int craftable = -1;
  // Ce que la recette consomme : `quantity`, `kept`, `owned`.
  std::span<const CraftAtlasRow> ingredients;
};

// Un skill qui produit cet item par fournées.
struct CraftAtlasBulkView {
  std::string_view skill_label;
  int least = 0;
  int most = 0;
};

// LA FICHE DE L'ITEM CHOISI, composée par `app` pour `selected_id`.
struct CraftAtlasSheet {
  // `item_id` nul : rien n'est choisi.
  CraftAtlasRow item;
  std::optional<CraftAtlasRecipeView> recipe;
  std::span<const CraftAtlasBulkView> bulk;
  // Ce qu'UN exemplaire consommé donne, en une fois : `quantity`.
  std::span<const CraftAtlasRow> arrow_yields;
  // Les recettes jouables où il sert : le PRODUIT, avec `quantity`, `kept` et
  // `skill_label`.
  std::span<const CraftAtlasRow> uses;
  // Ce qu'on transforme pour obtenir ces flèches : la SOURCE, avec `quantity`
  // et `owned`.
  std::span<const CraftAtlasRow> arrow_sources;
};

struct CraftAtlasView {
  // Le fichier des recettes a été lu. Faux, la fenêtre dit l'absence au lieu
  // de montrer des listes vides, qui se liraient « rien ne se fabrique ici ».
  bool available = false;
  // L'onglet des skills : `craftable` sur chaque produit.
  std::span<const CraftAtlasGroup> groups;
  // Tout ce qui se fabrique, trié par nom : `craftable`.
  std::span<const CraftAtlasRow> products;
  // Tout ce qui sert dans une recette jouable, trié par nom : `owned`, `uses`.
  std::span<const CraftAtlasRow> materials;
  // Tout ce qui se transforme en flèches, trié par nom : `owned`.
  std::span<const CraftAtlasRow> arrow_sources;
  CraftAtlasSheet sheet;
  // Le réglage « ne montrer que ce qui est réalisable », que la barre d'outils
  // montre aussi.
  bool only_craftable = false;
  // Partager la recette est possible : l'item en a une, et le chat peut
  // recevoir un lien.
  bool can_share_recipe = false;
  // Le catalogue du vote shop, pour l'entrée du menu d'un item.
  std::span<const VoteShopOffer> vote_shop;
  // LE STAFF PEUT SE FAIRE DONNER LES INGRÉDIENTS : la ligne « fabrications »
  // paraît sous la recette. Faux, elle n'existe pas — rien n'est grisé, rien
  // n'est dit. Voir `StaffToolsAllowed`.
  bool staff_items = false;
  // Les cases libres du sac, qui bornent les ingrédients non empilables. Vide
  // tant que le serveur n'a pas dit la capacité du sac : rien n'est borné.
  std::optional<int> free_bag_slots;
};

// Ce que la fenêtre garde d'une image à l'autre.
struct CraftAtlasWindowState {
  bool open = false;
  // Passer devant à l'image suivante : on vient de l'ouvrir sur un item.
  bool raise = false;
  // L'item dont la fiche est montrée, zéro pour aucun.
  std::uint32_t selected_id = 0;
  // La pile du bouton « Retour ».
  std::vector<std::uint32_t> back;
  std::array<char, kCraftAtlasFilterBytes> filter{};
  // LE MENU D'UNE LIGNE, armé au clic droit et ouvert hors des listes : une
  // ligne change la pile d'ids, et le surgissant ne s'y retrouverait pas.
  ItemLinkMenu menu;
  // Le nombre de fabrications que le staff prépare. ⚠ RETENU POUR LA SESSION,
  // d'une recette à l'autre : on éprouve souvent plusieurs produits d'une même
  // série au même compte.
  int staff_crafts = 1;
};

// Ouvre l'Atlas SUR un item — la porte des autres fenêtres. L'historique repart
// à plat : on arrive par une porte, pas au milieu d'un parcours.
void OpenCraftAtlasOn(CraftAtlasWindowState& state, std::uint32_t item_id);

struct CraftAtlasAnswer {
  // Un geste de lien sur une ligne — Maj+clic, une entrée du menu —, et l'item
  // survolé, pour l'aperçu.
  ChatLinkAsk item;
  // La case « réalisable maintenant » de la barre d'outils vient de changer.
  std::optional<bool> only_craftable;
  // Poser le lien de la recette de l'item choisi dans la saisie du chat.
  bool share_recipe = false;
  // La puce de la barre de titre : les réglages de l'Atlas.
  bool open_settings = false;
  // « Obtenir tout » ou « Obtenir le manque » : les ingrédients que le staff
  // demande, déjà multipliés et déjà filtrés. Vide sans geste.
  std::vector<StaffItemGrant> staff_items;
  // 🔴 LES ITEMS DONT UNE LIGNE A ÉTÉ DESSINÉE, pour leurs icônes. L'Atlas
  // liste sept cents items, et un onglet n'en montre qu'une vingtaine : `app` ne
  // charge donc que celles qu'on a vues. ⚠ UNE IMAGE DE RETARD : une icône
  // paraît à l'image qui suit la première où sa ligne se dessine.
  std::vector<std::uint32_t> drawn;
};

// ── CE QU'EXIGE UNE RECETTE, ET CE QUI MANQUE ───────────────────────────────
//
// 🔴 UNE SEULE RÈGLE pour la ligne d'ingrédient qui dit « il en manque » et
// pour la demande du staff qui comble le manque : deux calculs finiraient par
// ne plus dire la même chose, et le staff recevrait autre chose que ce que la
// fiche annonce.

// Ce que `crafts` fabrications exigent de cet ingrédient. UN GUIDE — non
// consommé — n'en exige qu'un, quel que soit le compte.
[[nodiscard]] int CraftIngredientNeed(const CraftAtlasRow& ingredient, int crafts) noexcept;

// Ce qu'il manque de cet ingrédient pour `crafts` fabrications, d'après
// `owned`. Zéro : il suffit.
[[nodiscard]] int CraftIngredientShortfall(const CraftAtlasRow& ingredient, int crafts) noexcept;

enum class CraftAtlasGrantKind : std::uint8_t {
  // Tout ce qu'exigent les fabrications, sans regarder l'inventory.
  kAll,
  // Seulement ce qui manque ; un ingrédient suffisant n'est pas demandé.
  kMissing,
};

struct CraftAtlasGrantPlan {
  std::vector<StaffItemGrant> grants;
  // Le pire de ce que le sac a permis : `kFull` dès qu'un ingrédient non
  // empilable n'a plus trouvé une seule case.
  StaffItemRoom room = StaffItemRoom::kFits;
};

// LES DEMANDES DU STAFF POUR `crafts` FABRICATIONS, dans l'ordre de la
// recette. `crafts` est ramené à [1, `kCraftAtlasMostCrafts`], et chaque
// quantité à `kStaffItemMostAmount`.
//
// 🔴 LES CASES LIBRES SE PARTAGENT, dans l'ordre où le serveur reçoit les
// commandes : un ingrédient non empilable prend une case par exemplaire, une
// pile nouvelle d'un ingrédient empilable en prend une. Un ingrédient non
// empilable est ramené à ce qu'il en reste, et omis s'il n'en reste aucune ; un
// ingrédient empilable ou d'empilement inconnu n'est jamais borné.
[[nodiscard]] CraftAtlasGrantPlan CraftAtlasGrants(std::span<const CraftAtlasRow> ingredients,
                                                   int crafts, CraftAtlasGrantKind kind,
                                                   std::optional<int> free_bag_slots);

// ── L'APERÇU AU SURVOL D'UN LIEN DE RECETTE ─────────────────────────────────
//
// VOLONTAIREMENT SUCCINCT : il répond à « est-ce que je peux le faire, et avec
// quoi ? ». Le reste — rendement, index inverse — est à un clic, dans l'Atlas.
struct CraftRecipePreview {
  std::string_view product_name;
  // Zéro : la recette s'ouvre en utilisant un item.
  int skill_id = 0;
  std::string_view skill_label;
  int skill_level = 0;
  // `quantity` et `kept`.
  std::span<const CraftAtlasRow> ingredients;
};

// ⚠ ELLE OUVRE SON INFOBULLE ELLE-MÊME, hors de toute fenêtre : posée depuis
// celle qu'on survole, elle passerait dessous.
void DrawCraftRecipePreview(const CraftRecipePreview& preview, const SkinArt& art);

// Dessine la fenêtre si elle est ouverte.
[[nodiscard]] CraftAtlasAnswer DrawCraftAtlasWindow(CraftAtlasWindowState& state,
                                                    const CraftAtlasView& view, const SkinArt& art);

}  // namespace ro::ui
