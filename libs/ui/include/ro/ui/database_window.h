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
// 🔴 LA FICHE D'UN MONSTRE EST CELLE DE LA FENÊTRE DE MONSTRE : un seul dessin
// (`DrawMonsterSheetBody`), posé ici dans le panneau de droite.
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
  bool Back() noexcept;
  bool Forward() noexcept;
  [[nodiscard]] bool can_go_back() const noexcept { return at_ > 0; }
  [[nodiscard]] bool can_go_forward() const noexcept { return at_ + 1 < visited_.size(); }
  [[nodiscard]] std::optional<DatabaseTarget> current() const;

 private:
  std::vector<DatabaseTarget> visited_;
  std::size_t at_ = 0;
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

// LES ONGLETS DE LA FICHE D'UN OBJET, dans leur ordre. « Description » n'existe
// qu'avec une description, « Script » qu'avec un script du serveur.
enum class DatabaseItemTab : std::uint8_t { kGeneral, kDescription, kScript, kDroppers, kSellers };

struct DatabaseWindowState {
  bool open = false;
  bool raise = false;
  int tab = kDatabaseItemTab;
  // L'onglet de `tab` s'impose à la prochaine image.
  bool select_tab = true;
  // L'ONGLET DE LA FICHE D'UN OBJET QUE LE JOUEUR A CLIQUÉ, retenu d'un objet à
  // l'autre. Un objet qui ne l'a pas s'ouvre sur « Général » sans l'oublier :
  // l'objet suivant qui l'a le rouvre.
  DatabaseItemTab item_tab = DatabaseItemTab::kGeneral;
  std::array<char, kDatabaseQueryBytes> item_query{};
  std::array<char, kDatabaseQueryBytes> monster_query{};
  DatabaseFilters filters;
  DatabaseHistory history;
  // La liste doit défiler jusqu'à la fiche montrée : une fiche ouverte d'ailleurs.
  bool scroll_to_shown = false;
  // Le menu que le clic droit sur l'objet de la fiche a armé.
  ItemLinkMenu item_menu;
};

// UNE LIGNE DE LA LISTE : l'identifiant, le nom, et ce qu'on lit à droite.
struct DatabaseRowView {
  std::uint32_t id = 0;
  std::string_view name;
  std::string_view note;
};

// Une ligne « libellé : valeur » de la fiche d'un objet, déjà composée.
struct DatabaseFact {
  std::string_view label;
  std::string_view value;
};

// UN MONSTRE QUI LÂCHE L'OBJET, au taux réel déjà écrit.
struct DatabaseDropperRow {
  std::uint32_t monster_id = 0;
  std::string_view name;
  std::uint8_t rank = 0;
  std::string_view rate;
  bool mvp_reward = false;
};

// « VENDU CHEZ » : le Marché, interrogé sur les boutiques de PNJ.
enum class DatabaseSellersPhase : std::uint8_t {
  // Personne à qui demander : hors ligne.
  kOffline,
  // La demande est partie, ou partira à l'ouverture de l'onglet.
  kAsking,
  kReady,
  // Le serveur a refusé — trop tôt après une autre demande : « Réessayer ».
  kRefused,
};

struct DatabaseSellerRow {
  std::string_view npc;
  std::string_view place;
  std::string_view price;
};

struct DatabaseItemView {
  std::uint32_t item_id = 0;
  std::string_view name;
  SkinImage icon;
  std::span<const DatabaseFact> facts;
  // LA DESCRIPTION QUE LE JEU MONTRE, son aperçu tel quel ; nulle sans table.
  const DescriptionView* description = nullptr;
  // Les scripts du serveur, bruts ; vides s'il n'en a pas.
  std::string_view script;
  std::string_view equip_script;
  std::string_view unequip_script;
  std::span<const DatabaseDropperRow> droppers;
  DatabaseSellersPhase sellers_phase = DatabaseSellersPhase::kOffline;
  std::string_view sellers_status;
  std::span<const DatabaseSellerRow> sellers;
  // Le catalogue du vote shop, pour le menu de l'objet.
  std::span<const VoteShopOffer> vote_shop;
};

// Vrai quand la fiche de cet objet porte cet onglet.
[[nodiscard]] bool DatabaseItemTabShown(DatabaseItemTab tab, const DatabaseItemView& view);

struct DatabaseView {
  // La base se lit encore : la première ouverture.
  bool loading = false;
  // Ce qui empêche de la montrer — absente, illisible ; vide sinon.
  std::string_view status;
  // Les résultats de l'onglet montré, et ce qu'en dit le pied.
  std::span<const DatabaseRowView> rows;
  std::string_view summary;
  // La fiche montrée : l'une ou l'autre, ou aucune.
  const DatabaseItemView* item = nullptr;
  const MonsterSheetView* monster = nullptr;
};

struct DatabaseAnswer {
  // « Marché » : le Marché s'ouvre sur cet objet ; zéro sinon.
  std::uint32_t market_item = 0;
  // L'onglet « Vendu chez » est montré : la demande au Marché peut partir. Elle
  // n'attend que lui, pour qu'une fiche feuilletée ne coûte rien au serveur.
  bool sellers_shown = false;
  // « Réessayer » de « Vendu chez ».
  bool ask_sellers = false;
  // « Y aller » vers ce rang des vendeurs ; négatif sinon.
  int go_to_seller = -1;
  // Un geste sur l'objet de la fiche — sa description, son lien, son menu.
  ChatLinkAsk item;
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
