#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/align_grid.h"
#include "ro/ui/hud_frame.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// LA GRILLE D'ICÔNES DE MENU : les boutons qui ouvrent les fenêtres du jeu —
// statut, équipement, inventory, skills, guilde, options…
//
// 🔴 C'EST LA PORTE D'ENTRÉE DE CEUX QUI NE CONNAISSENT PAS LES TOUCHES. Tout ce
// qu'elle ouvre s'ouvre aussi au clavier ; mais un joueur qui arrive cherche une
// icône avant de chercher Alt+E, et une fenêtre sans icône est une fenêtre
// qu'il ne trouvera pas.
//
// ⚠ ELLE NE FAIT RIEN ELLE-MÊME. Elle rend la commande cliquée ; c'est `app`
// qui sait quelle fenêtre elle ouvre, et par le même chemin que la touche —
// deux chemins finiraient par se comporter autrement.
//
// ⚠ UNE ICÔNE DONT LA FENÊTRE N'EXISTE PAS ENCORE RESTE À SA PLACE. Le clic
// ouvre alors une fenêtre d'attente qui le dit, plutôt que de ne rien faire :
// un bouton muet se lit comme une panne. Et l'icône ne bougera pas le jour où
// sa fenêtre arrivera — les autres non plus.

// LES COMMANDES DE LA GRILLE, DANS L'ORDRE OÙ LE CLIENT LES POSE.
//
// 🔴 L'ORDRE EST CELUI DE LA GRILLE : une icône masquée ne laisse pas de trou,
// la suivante prend sa place. Le réordonner déplacerait toutes les icônes que
// le joueur a dans l'œil.
enum class MenuCommand : std::uint8_t {
  kStatus,
  kEquipment,
  kItem,
  kSkill,
  kBooking,
  kParty,
  kGuild,
  kBattleground,
  kQuest,
  kWorldMap,
  kNavigation,
  kOption,
  kBank,
  kReplay,
  kMail,
  kAchievement,
  kTip,
  kCashShop,
  kShortcut,
  kSocialNetwork,
  kAttendance,
  kAdventurerAgency,
  kReputation,
  kAdventureGuide,
  kProbability,
  // ── LES ICÔNES DE CE CLIENT, que le jeu n'a pas ───────────────────────────
  //
  // ⚠ APRÈS CELLES DU JEU, et c'est l'ordre qui le dit : elles prennent les
  // cases qui suivent la dernière icône du jeu, sans en déplacer aucune. Leur
  // art suit les conventions de nom du jeu, livré dans le dossier `data\` de
  // l'installation.
  //
  // L'ALBUM DE CARTES, puis L'ATLAS DES RECETTES.
  kCardAlbum,
  kCraftAtlas,
  // LE CARNET DE CHASSE MVP : album, Atlas, puis carnet.
  kMvpTracker,
  // LA FICHE DU PET, que le jeu n'ouvre que par le menu du pet. Son art
  // `bt_pet` se livre comme les trois précédents ; sans lui, le carré de
  // secours.
  kPetWindow,
  // LE TABLEAU DES CLASSEMENTS, que le jeu n'a pas. Aucun art `bt_ranking` ne
  // se livre : le carré de secours, à sa place.
  kRankingBoard,
  // ── HORS DE LA GRILLE ─────────────────────────────────────────────────────
  //
  // LE BOUTON DU VOTE SHOP, posé à part. C'est une icône du jeu,
  // mais pas une case de sa grille : il n'y prend aucun rang, et sa place
  // d'origine est la sienne. Le jeu le montre ou le cache par `/cashshop`.
  kCashShopButton,
};
inline constexpr std::size_t kMenuCommandCount =
    static_cast<std::size_t>(MenuCommand::kCashShopButton) + 1;

// UNE ENTRÉE DU CATALOGUE.
struct MenuIconEntry {
  MenuCommand command = MenuCommand::kStatus;
  // Le nom du bitmap, entre `bt_` et `.bmp` : `menu_icon\bt_<stem>.bmp`.
  std::string_view stem;
  // 🔴 LA GRILLE LA MONTRE-T-ELLE ? Huit commandes du jeu n'ont pas d'icône
  // chez le client de ce serveur — des services que le serveur n'offre pas —,
  // et en dessiner une annoncerait une fonction qui n'existera jamais ici.
  bool shown = false;
  // A-t-elle un second bitmap, « nouveau », pour signaler du neuf (courrier non
  // lu, succès débloqué…) ? Six commandes seulement.
  bool has_news_art = false;
  // 🔴 HORS DE LA GRILLE : un bouton à part, dont l'unique bitmap vit dans
  // `basic_interface\` (voir `MenuButtonPath`). Il ne prend aucun rang, donc ne
  // décale aucune case, et sa place d'origine est `DefaultMenuButtonCorner`.
  bool off_grid = false;
};

// Le catalogue entier, dans l'ordre de `MenuCommand`.
[[nodiscard]] std::span<const MenuIconEntry> MenuIconCatalogue();
[[nodiscard]] const MenuIconEntry& MenuIconOf(MenuCommand command);

// Le libellé de l'infobulle, DÉJÀ TRADUIT.
[[nodiscard]] const char* MenuIconLabel(MenuCommand command);

// LES QUATRE BITMAPS D'UNE ICÔNE.
//
// ⚠ QUATRE FICHIERS, PAS UN ATLAS NI UNE TEINTE : l'état enfoncé et l'état
// « nouveau » sont dessinés à part, et chacun a sa taille — le « nouveau »
// déborde vers le haut.
enum class MenuIconArt : std::uint8_t {
  kNormal,
  kPressed,
  kNews,
  kNewsPressed,
};

// LE NOM DE FICHIER D'UNE ICÔNE, pour un Doram ou non.
//
// ⚠ LE STATUT D'UN DORAM A SON PROPRE DESSIN (`status_doram`) : c'est la seule
// icône qui dépende du personnage.
[[nodiscard]] std::string_view MenuIconStem(MenuCommand command, bool doram);

// Le chemin complet, sous le dossier d'interface, écrit en UTF-8.
[[nodiscard]] std::string MenuIconPath(std::string_view stem, MenuIconArt art);

// LE CHEMIN DU BITMAP D'UN BOUTON HORS GRILLE : `basic_interface\<stem>.bmp`.
// ⚠ UN SEUL FICHIER pour les trois états — normal, survol, enfoncé.
[[nodiscard]] std::string MenuButtonPath(std::string_view stem);

// ── LA GÉOMÉTRIE DU CLIENT, en pixels d'interface à l'échelle 1 ─────────────
//
// Cinq icônes par rangée, au pas de 42 sur 43. ⚠ LE PAS, PAS LA TAILLE DU
// BITMAP : une icône qui n'aurait pas la taille attendue ne décale pas ses
// voisines.
inline constexpr std::size_t kMenuIconsPerRow = 5;
inline constexpr float kMenuIconColumnPitch = 42.0F;
inline constexpr float kMenuIconRowPitch = 43.0F;
// De combien l'icône « nouveau » monte au-dessus de la normale : son badge
// déborde en haut, et la place en est réservée au-dessus de la première rangée.
inline constexpr float kMenuIconNewsLift = 6.0F;
// Le côté du carré dessiné quand le bitmap manque à l'installation.
inline constexpr float kMenuIconFallbackSide = 32.0F;

// LE COIN D'UNE CASE, relatif au coin du contenu, pour la case de ce rang parmi
// les icônes MONTRÉES.
[[nodiscard]] ImVec2 MenuIconCellOffset(std::size_t shown_rank);

// La place de la grille entière pour ce nombre d'icônes, badge compris.
[[nodiscard]] ImVec2 MenuIconsGridSize(std::size_t shown_count);

// CE QU'UNE ICÔNE MONTRE, composé par `app`.
struct MenuIconView {
  MenuCommand command = MenuCommand::kStatus;
  SkinImage normal;
  SkinImage pressed;
  SkinImage news;
  SkinImage news_pressed;
  // 🔴 LA FENÊTRE EXISTE-T-ELLE DANS CE CLIENT ? Faux, le clic ouvre la fenêtre
  // d'attente au lieu de rendre la commande.
  bool ported = false;
  // Du neuf à signaler : le bitmap « nouveau » remplace alors le normal.
  bool news_lit = false;
  // UN ÉTAT EN COURS à signaler — en file au champ de bataille : une pastille
  // sur l'icône, pour celles qui n'ont pas de bitmap « nouveau ».
  bool dot_lit = false;
  // 🔴 SA FENÊTRE EST OUVERTE, et l'icône le montre en restant ENFONCÉE.
  //
  // ⚠ POUR LES ICÔNES DE CE CLIENT SEULEMENT. Chez le jeu, l'enfoncé ne dure
  // que le temps du clic ; les nôtres sont des BASCULES, et leur second bitmap
  // dit ce que fera le prochain clic. Sans lui, rien ne distingue une fenêtre
  // ouverte mais cachée derrière une autre d'une fenêtre jamais ouverte.
  bool shows_open = false;
  // La touche qui fait la même chose — « Alt+E » —, ou vide.
  std::string shortcut;
  // LE NOM QUE CE SERVEUR DONNE À CETTE FENÊTRE, quand il en a un à lui : il
  // remplace le libellé du catalogue. Vide, c'est celui-ci.
  std::string label;
};

struct MenuIconsState {
  // LA FENÊTRE D'ATTENTE, et la commande qu'elle remplace.
  bool placeholder_open = false;
  MenuCommand placeholder = MenuCommand::kStatus;
};

inline constexpr const char* kMenuPlaceholderId = "menu_placeholder";

// ── LA PLACE DES ICÔNES, posée par le joueur ────────────────────────────────
//
// 🔴 CHAQUE ICÔNE EST UN CADRE LIBRE, SANS HABILLAGE. Le jeu les pose dans une
// grille nue, sans fenêtre autour ; ici le joueur les range une à une, où il
// veut, comme ses jauges : aimantées entre elles et à la grille d'alignement, et
// emportées en bloc au CTRL quand elles se touchent.
//
// ⚠ VERROUILLÉES, ELLES SE CLIQUENT ; déverrouillées, elles se déplacent et
// n'ouvrent rien. Un même geste ne peut pas vouloir dire les deux.

// La place d'une icône.
struct MenuIconPlace {
  // ⚠ NÉGATIF = JAMAIS POSÉE : l'icône prend alors sa case de la grille du jeu.
  // Une position posée est toujours dans l'écran, donc jamais négative.
  float x = -1.0F;
  float y = -1.0F;
  // Le bord d'ancrage de chaque axe, en rang d'`AnchorSide` : voir `HudRect`.
  int across = 0;
  int down = 0;
  // Masquée par le joueur. ⚠ ELLE GARDE SA PLACE : la remontrer la rend là où
  // elle était.
  bool hidden = false;
};

struct MenuIconsLook {
  // ⚠ VERROUILLÉES PAR DÉFAUT : une icône qui s'attrape par accident en pleine
  // mêlée est une icône perdue.
  bool locked = true;
  bool sticky = true;
  // Une place par commande du catalogue, indexée par son rang.
  std::array<MenuIconPlace, kMenuCommandCount> places{};
};

[[nodiscard]] bool MenuIconPlaced(const MenuIconPlace& place) noexcept;

// La marge entre la grille d'origine et les bords de l'écran.
inline constexpr float kMenuIconsScreenMargin = 8.0F;

// LE COIN QU'UNE ICÔNE JAMAIS POSÉE PREND : sa case dans la grille du jeu, la
// grille entière rangée au coin haut-GAUCHE de l'écran. Le coin haut-droit est
// celui de la minimap.
//
// 🔴 IL SE CALCULE ET NE SE PERSISTE PAS. Un défaut écrit dans le fichier serait
// celui d'UNE taille d'écran, et suivrait le joueur qui change de résolution.
//
// ⚠ `grid_rank` EST SON RANG PARMI LES ICÔNES QUE LE CATALOGUE MONTRE, masquées
// par le joueur comprises : masquer une icône ne déplace pas ses voisines.
[[nodiscard]] ImVec2 DefaultMenuIconCorner(std::size_t grid_rank, float scale);

// LE COIN D'ORIGINE D'UN BOUTON HORS GRILLE : sous la grille, contre le bord
// gauche.
//
// ⚠ PAS À SA DROITE : c'est là que la rangée des états commence, et elle
// grandit vers la droite — voir `MenuIconsHomeRight`.
inline constexpr float kMenuButtonGap = 8.0F;
[[nodiscard]] ImVec2 DefaultMenuButtonCorner(std::size_t grid_count, float scale);

// LE BORD DROIT DE LA GRILLE D'ORIGINE, à l'écran, pour ce qui se range à côté
// d'elle. Compté sur une rangée pleine : le catalogue montre bien plus de
// `kMenuIconsPerRow` icônes, et un voisin ne doit pas bouger quand le joueur en
// masque une.
[[nodiscard]] float MenuIconsHomeRight(float scale);

// Oublie toutes les places posées : chaque icône revient à sa case d'origine.
// ⚠ LES MASQUÉES LE RESTENT : on range, on ne remontre pas.
void ForgetMenuIconPlaces(MenuIconsLook& look) noexcept;

// Ce que la grille rend à l'appelant, pour cette image.
struct MenuIconsAnswer {
  std::optional<MenuCommand> clicked;
  // Une icône a bougé : à l'appelant de persister.
  bool moved = false;
};

// CE QUE PRODUIT LE CLIC SUR UNE ICÔNE.
//
// Une icône portée rend sa commande. Une icône qui ne l'est pas ouvre la
// fenêtre d'attente — ou la referme, si c'est déjà la sienne : un second clic
// sur le même bouton est le geste qu'on fait pour s'en débarrasser.
[[nodiscard]] std::optional<MenuCommand> PressMenuIcon(MenuIconsState& state,
                                                       const MenuIconView& icon);

// Dessine les icônes et la fenêtre d'attente.
//
// ⚠ `icons` NE PORTE QUE LES ICÔNES QUE LE CATALOGUE MONTRE, dans son ordre :
// c'est leur rang qui décide de la case d'origine — sauf pour un bouton hors
// grille, qui a la sienne. Celles que le joueur a masquées y sont, et ne se
// dessinent pas.
//
// ⚠ DÉVERROUILLÉES, LES ICÔNES JAMAIS POSÉES SE SÈMENT à leur case d'origine :
// le bloc CTRL les emporte, et il ne peut emporter qu'une place écrite.
[[nodiscard]] MenuIconsAnswer DrawMenuIcons(MenuIconsState& state,
                                            std::span<const MenuIconView> icons,
                                            MenuIconsLook& look, const AlignGrid& grid,
                                            HudGrip& grip, const SkinArt& art);

}  // namespace ro::ui
