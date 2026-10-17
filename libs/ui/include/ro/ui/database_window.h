#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/ui/chat_link.h"
#include "ro/ui/description_window.h"
#include "ro/ui/item_link_cell.h"
#include "ro/ui/monster_sheet_window.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// ── LA BASE DE DONNÉES ──────────────────────────────────────────────────────
//
// Les objets et les monstres du serveur, en deux onglets (ADR 0116). À gauche
// une liste, cherchée par nom ou par identifiant et filtrée ; à droite la FICHE
// de ce qu'on a choisi. Les liens d'une fiche mènent à une autre — un butin à
// son objet, un objet aux monstres qui le lâchent —, et ◀ ▶ remontent le fil.
//
// 🔴 LA FICHE D'UN MONSTRE EST CELLE DE LA FENÊTRE DE MONSTRE, et celle d'un
// objet celle de la fenêtre de description : un seul dessin chacune
// (`DrawMonsterSheetBody`, `DrawItemSheetBody`), posé ici dans le panneau de
// droite.
//
// LES LIGNES DE LA LISTE SE MANIENT COMME TOUT OBJET OU TOUT MONSTRE : le clic
// visite la fiche, Maj + clic pose le lien dans la saisie, le clic droit ouvre
// LE menu des liens (`DrawItemLinkMenu`), Ctrl + clic droit décrit.
//
// La fenêtre ne possède rien du jeu : elle tient sa saisie, ses filtres et le
// fil de ses fiches ; `app` lit la base, compose les vues et route les gestes.

inline constexpr const char* kDatabaseWindowId = "###database";

enum class DatabaseKind : std::uint8_t { kItem, kMonster };

// CE QU'UNE FICHE MONTRE : un objet ou un monstre, par son identifiant.
struct DatabaseTarget {
  DatabaseKind kind = DatabaseKind::kItem;
  std::uint32_t id = 0;
  bool operator==(const DatabaseTarget&) const = default;
};

// Le plus de fiches que le fil retient ; les plus anciennes tombent.
inline constexpr std::size_t kDatabaseHistoryMost = 64;

// LE FIL DES FICHES VISITÉES, que ◀ et ▶ parcourent comme un navigateur : une
// fiche ouverte au milieu du fil en coupe la suite.
class DatabaseHistory {
 public:
  // Montre cette fiche. La même que la courante ne s'empile pas.
  void Visit(DatabaseTarget target);
  // Montre la fiche où mène le CLAVIER de la liste : la première touche
  // l'empile, les suivantes la remplacent, et revenir sur la fiche de départ
  // la retrouve sans la doubler. Parcourir une liste aux flèches ne noie pas
  // le fil, et ◀ ramène à la fiche d'où le clavier est parti.
  void Step(DatabaseTarget target);
  bool Back() noexcept;
  bool Forward() noexcept;
  [[nodiscard]] bool can_go_back() const noexcept { return at_ > 0; }
  [[nodiscard]] bool can_go_forward() const noexcept { return at_ + 1 < visited_.size(); }
  [[nodiscard]] std::optional<DatabaseTarget> current() const;

 private:
  std::vector<DatabaseTarget> visited_;
  std::size_t at_ = 0;
  // La fiche courante vient du clavier.
  bool stepped_ = false;
};

inline constexpr int kDatabaseItemTab = 0;
inline constexpr int kDatabaseMonsterTab = 1;
inline constexpr std::size_t kDatabaseQueryBytes = 64;
// Le rang « tous » de chaque liste déroulante de filtre.
inline constexpr int kDatabaseAnyChoice = 0;
// Le plus haut niveau qu'un filtre propose.
inline constexpr int kDatabaseHighestLevel = 999;

// ── LES FILTRES ─────────────────────────────────────────────────────────────
//
// Les libellés se marquent par `core::TrText` : la traduction se fait à la
// lecture. Chaque liste commence par « tous » ; un rang au-delà désigne une
// entrée de la table, au rang moins un.

// Les types d'objet : le libellé, et le `Type` de l'item_db.
struct DatabaseItemType {
  const char* label = "";
  std::string_view server_type;
};
inline constexpr std::array<DatabaseItemType, 12> kDatabaseItemTypes = {{
    {core::TrText("Soins"), "Healing"},
    {core::TrText("Consommables"), "Usable"},
    {core::TrText("Consommables différés"), "DelayConsume"},
    {core::TrText("Divers"), "Etc"},
    {core::TrText("Armures"), "Armor"},
    {core::TrText("Armes"), "Weapon"},
    {core::TrText("Cartes"), "Card"},
    {core::TrText("Œufs de familier"), "PetEgg"},
    {core::TrText("Équipements de familier"), "PetArmor"},
    {core::TrText("Munitions"), "Ammo"},
    {core::TrText("Équipements d'ombre"), "ShadowGear"},
    {core::TrText("Items de la boutique"), "Cash"},
}};

// LES EMPLACEMENTS, regroupés comme un joueur les pense : une arme à deux
// mains est une arme, un accessoire se porte à gauche comme à droite.
inline constexpr std::array<std::string_view, 2> kDatabaseWeaponPlaces = {"Right_Hand",
                                                                          "Both_Hand"};
inline constexpr std::array<std::string_view, 3> kDatabaseAccessoryPlaces = {
    "Both_Accessory", "Left_Accessory", "Right_Accessory"};
inline constexpr std::array<std::string_view, 5> kDatabaseCostumePlaces = {
    "Costume_Head", "Costume_Head_Top", "Costume_Head_Mid", "Costume_Head_Low", "Costume_Garment"};
inline constexpr std::array<std::string_view, 7> kDatabaseShadowPlaces = {
    "Shadow_Armor",           "Shadow_Weapon",         "Shadow_Shield",        "Shadow_Shoes",
    "Shadow_Right_Accessory", "Shadow_Left_Accessory", "Shadow_Both_Accessory"};
inline constexpr std::array<std::string_view, 1> kDatabaseHeadTopPlaces = {"Head_Top"};
inline constexpr std::array<std::string_view, 1> kDatabaseHeadMidPlaces = {"Head_Mid"};
inline constexpr std::array<std::string_view, 1> kDatabaseHeadLowPlaces = {"Head_Low"};
inline constexpr std::array<std::string_view, 1> kDatabaseArmorPlaces = {"Armor"};
inline constexpr std::array<std::string_view, 1> kDatabaseShieldPlaces = {"Left_Hand"};
inline constexpr std::array<std::string_view, 1> kDatabaseGarmentPlaces = {"Garment"};
inline constexpr std::array<std::string_view, 1> kDatabaseShoesPlaces = {"Shoes"};
inline constexpr std::array<std::string_view, 1> kDatabaseAmmoPlaces = {"Ammo"};

struct DatabaseItemPlace {
  const char* label = "";
  std::span<const std::string_view> server_places;
};
inline constexpr std::array<DatabaseItemPlace, 12> kDatabaseItemPlaces = {{
    {core::TrText("Haut de tête"), kDatabaseHeadTopPlaces},
    {core::TrText("Milieu de tête"), kDatabaseHeadMidPlaces},
    {core::TrText("Bas de tête"), kDatabaseHeadLowPlaces},
    {core::TrText("Armure"), kDatabaseArmorPlaces},
    {core::TrText("Arme"), kDatabaseWeaponPlaces},
    {core::TrText("Bouclier"), kDatabaseShieldPlaces},
    {core::TrText("Cape"), kDatabaseGarmentPlaces},
    {core::TrText("Chaussures"), kDatabaseShoesPlaces},
    {core::TrText("Accessoire"), kDatabaseAccessoryPlaces},
    {core::TrText("Costume"), kDatabaseCostumePlaces},
    {core::TrText("Ombre"), kDatabaseShadowPlaces},
    {core::TrText("Munition"), kDatabaseAmmoPlaces},
}};

// LES RACES, ÉLÉMENTS ET TAILLES, dans l'ordre des NOMBRES du serveur
// (`e_race`, `e_element`, `e_size`) : le rang dans la table est le nombre.
inline constexpr std::array<const char*, 10> kDatabaseRaces = {
    core::TrText("Informe"), core::TrText("Mort-vivant"), core::TrText("Brute"),
    core::TrText("Plante"),  core::TrText("Insecte"),     core::TrText("Poisson"),
    core::TrText("Démon"),   core::TrText("Demi-humain"), core::TrText("Ange"),
    core::TrText("Dragon"),
};
inline constexpr std::array<const char*, 10> kDatabaseElements = {
    core::TrText("Neutre"),      core::TrText("Eau"),   core::TrText("Terre"),
    core::TrText("Feu"),         core::TrText("Vent"),  core::TrText("Poison"),
    core::TrText("Sacré"),       core::TrText("Ombre"), core::TrText("Fantôme"),
    core::TrText("Mort-vivant"),
};
inline constexpr std::array<const char*, 3> kDatabaseSizes = {
    core::TrText("Petit"), core::TrText("Moyen"), core::TrText("Grand")};
// Les rangs, dans l'ordre du serveur : ordinaire, mini-boss, MVP.
inline constexpr std::array<const char*, 3> kDatabaseRanks = {
    core::TrText("Ordinaires"), core::TrText("Mini-boss"), core::TrText("MVP")};

// Le libellé TRADUIT d'un `Type` ou d'un emplacement de la base, sans la casse ;
// nul quand les tables ci-dessus ne le nomment pas.
[[nodiscard]] const char* DatabaseItemTypeLabel(std::string_view server_type);
[[nodiscard]] const char* DatabaseItemPlaceLabel(std::string_view server_place);

// L'entrée choisie d'une liste de filtre, TRADUITE : « Tous » au rang zéro,
// sinon `values` au rang moins un. `values` se passe en français, et un rang
// hors de la liste est borné.
[[nodiscard]] const char* DatabaseFilterChoice(int chosen, std::span<const char* const> values);

// Le texte d'une liste de filtre FERMÉE : le nom du filtre, puis l'entrée
// choisie — « Type - Tous », « Race - Démon » —, pour qu'on sache toujours
// quelle liste est laquelle. `filter_name` se passe déjà traduit.
[[nodiscard]] std::string DatabaseFilterShown(const char* filter_name, int chosen,
                                              std::span<const char* const> values);

// Les rangs choisis des listes, et les bornes de niveau ; zéro : sans borne.
struct DatabaseFilters {
  int item_type = kDatabaseAnyChoice;
  int item_place = kDatabaseAnyChoice;
  int item_level_min = 0;
  int item_level_max = 0;
  int race = kDatabaseAnyChoice;
  int element = kDatabaseAnyChoice;
  int size = kDatabaseAnyChoice;
  int rank = kDatabaseAnyChoice;
  int level_min = 0;
  int level_max = 0;
  bool operator==(const DatabaseFilters&) const = default;
};

struct DatabaseWindowState {
  bool open = false;
  bool raise = false;
  int tab = kDatabaseItemTab;
  // L'onglet de `tab` s'impose à la prochaine image.
  bool select_tab = true;
  std::array<char, kDatabaseQueryBytes> item_query{};
  std::array<char, kDatabaseQueryBytes> monster_query{};
  DatabaseFilters filters;
  DatabaseHistory history;
  // La liste doit défiler jusqu'à la fiche montrée : une fiche ouverte d'ailleurs.
  bool scroll_to_shown = false;
  // L'ÉTAT DE LA FICHE D'UN OBJET, celle de la fenêtre de description posée
  // dans le panneau de droite : son onglet retenu, ses filtres, son menu.
  DescriptionWindowState item_sheet;
  // Le menu que le clic droit sur une ligne de la liste a armé.
  ItemLinkMenu list_menu;
};

// UNE LIGNE DE LA LISTE : l'identifiant, le nom, et ce qu'on lit à droite.
struct DatabaseRowView {
  std::uint32_t id = 0;
  std::string_view name;
  std::string_view note;
};

struct DatabaseView {
  // La base se lit encore : la première ouverture.
  bool loading = false;
  // Ce qui empêche de la montrer — absente, illisible ; vide sinon.
  std::string_view status;
  // Les résultats de l'onglet montré, et ce qu'en dit le pied.
  std::span<const DatabaseRowView> rows;
  std::string_view summary;
  // La fiche montrée : l'une ou l'autre, ou aucune. Celle d'un objet est LA
  // fiche de la fenêtre de description (`DrawItemSheetBody`).
  const DescriptionView* item = nullptr;
  const MonsterSheetView* monster = nullptr;
  // Ce que le menu d'une ligne de la liste sait d'avance : vote shop, album.
  ItemLinkFacts link_facts;
};

struct DatabaseAnswer {
  // Ce que la fiche d'un objet a demandé — les mêmes demandes que dans la
  // fenêtre de description, que `app` route de même. ⚠ Un lien vers un AUTRE
  // objet y a déjà été suivi : la base a visité sa fiche.
  DescriptionWindowAction item;
  // LES GESTES DE TOUT OBJET OU DE TOUT MONSTRE, sur une ligne de la liste :
  // Maj + clic, le menu du clic droit, Ctrl + clic droit, et le survol. Le
  // clic simple, lui, visite la ligne.
  ChatLinkAsk list;
  // Les gestes de la fiche de monstre, que `app` route comme ceux de la
  // fenêtre de monstre.
  MonsterSheetAnswer monster;
};

// Ouvre la fenêtre au premier plan, sur cette fiche quand il y en a une : son
// onglet se montre, et la liste défile jusqu'à elle.
void OpenDatabaseOn(DatabaseWindowState& state, std::optional<DatabaseTarget> target);

// Dessine la fenêtre si elle est ouverte. `monster_state` est l'état de LA
// fiche de monstre, que le panneau de droite emprunte.
[[nodiscard]] DatabaseAnswer DrawDatabaseWindow(DatabaseWindowState& state,
                                                const DatabaseView& view,
                                                MonsterSheetWindowState& monster_state,
                                                const SkinArt& art);

}  // namespace ro::ui
