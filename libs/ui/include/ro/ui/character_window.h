#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/character_staff.h"
#include "ro/ui/guild_tab.h"
#include "ro/ui/homunculus_tab.h"
#include "ro/ui/mercenary_tab.h"
#include "ro/ui/preset_tab.h"
#include "ro/ui/skill_window.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/wheel_gate.h"

namespace ro::ui {

// LA FICHE DE PERSONNAGE : le mannequin, les costumes, et les nombres.
//
// ⚠ ELLE NE CHERCHE RIEN ET NE CALCULE RIEN. Chaque case lui arrive avec sa
// pièce déjà trouvée, chaque nombre déjà dérivé. `ui` ne connaît ni les
// emplacements du serveur ni la table des objets : elle dessine ce qu'on lui
// donne et rend ce que le joueur a demandé.
//
// ⚠ ET C'EST LA MÊME FENÊTRE QUE LE GRIMOIRE, qui en est le premier onglet. Une
// seconde fenêtre « statut » à côté d'une fenêtre « personnage » est ce que le
// client d'origine fait ; ici, il n'y en a qu'une.

// ── LA GÉOMÉTRIE DU MANNEQUIN, EN PIXELS D'ART ──────────────────────────────
//
// Ces nombres décident de la PLACE, pas de l'apparence : les changer déplace
// les cases les unes par rapport à l'avatar, et le mannequin cesse d'être celui
// qu'on connaît.
inline constexpr float kEquipSlotPixels = 44.0F;
inline constexpr float kEquipSlotGapPixels = 6.0F;
inline constexpr float kCharacterAvatarPixels = 130.0F;

// Le bloc entier : une colonne de cases, l'avatar, l'autre colonne. Il se CENTRE
// dans le volet plutôt que de se coller à gauche.
inline constexpr float kCharacterDollBlockPixels = kEquipSlotPixels + kEquipSlotGapPixels +
                                                   kCharacterAvatarPixels + kEquipSlotGapPixels +
                                                   kEquipSlotPixels;

// Les planchers des deux volets. ⚠ CE NE SONT PAS LEURS LARGEURS : chacune se
// mesure à la police courante, et ces nombres ne font que l'empêcher de
// descendre sous ce qui reste lisible.
inline constexpr float kCharacterDollPaneMinimum = 280.0F;
inline constexpr float kCharacterStatsPaneMinimum = 240.0F;
// ⚠ PLUS ÉTROIT QUE LES DEUX AUTRES : il ne porte que des ajusteurs et des
// boutons, jamais une colonne de valeurs à aligner.
inline constexpr float kCharacterStaffPaneMinimum = 200.0F;

// ── LES COULEURS ────────────────────────────────────────────────────────────
//
// ⚠ LE MANNEQUIN N'EMPLOIE AUCUN BITMAP DU JEU, et c'est mesuré, pas supposé :
// fond de case et fond de panneau sont des aplats, et une case vide est du
// TEXTE. Il n'y a donc aucune pièce à relever dans l'archive pour cette fenêtre.
//
// Le fond d'une case est le gris des champs de saisie — la même valeur des deux
// côtés, ce qui n'est pas un hasard : c'est le gris de l'habillage du jeu. Il
// suit l'habillage : `SkinLook::slot`.
inline constexpr ImU32 kEquipSlotBorder = IM_COL32(0, 0, 0, 80);
inline constexpr float kEquipSlotRounding = 4.0F;
// La marge entre le bord de la case et l'icône.
inline constexpr float kEquipSlotPadding = 3.0F;

// Le fond du panneau où vit l'avatar, à peine plus clair que le corps, suit
// l'habillage : `SkinLook::doll`.

// La marge entre le bord du panneau et la silhouette. Elle écarte le sprite des
// coins arrondis, où il se ferait rogner.
inline constexpr float kCharacterAvatarPadding = 4.0F;

// ⚠ LA STATURE NE DESCEND PAS SOUS LA MOITIÉ DU PANNEAU, quitte à laisser
// déborder ce qui se pose à CÔTÉ du personnage. Une coiffe de costume qui porte
// un compagnon ou une monture double l'enveloppe : tout faire rentrer donne
// alors un personnage minuscule sous un grand vide, alors que la fiche montre
// AVANT TOUT un personnage. C'est le seul bouton — le monter rogne davantage
// les sprites latéraux, le baisser rapetisse celui qui en porte un.
inline constexpr float kCharacterAvatarBodyFloor = 0.50F;

// L'abréviation d'une case vide. Grise, parce qu'elle nomme un manque.
inline constexpr ImU32 kEquipSlotEmptyText = IM_COL32(120, 120, 120, 255);
// En dessous de cette taille un caractère ne se lit plus : on cesse de rétrécir.
inline constexpr float kEquipSlotEmptyMinimumFont = 6.0F;

// Le fond d'un libellé de caractéristique. ⚠ IL S'ARRÊTE AU LIBELLÉ et ne court
// pas jusqu'au bout de la rangée : c'est ce qui laisse les valeurs alignées sur
// leur colonne au lieu de flotter dans une bande.
inline constexpr ImU32 kCharacterStatRowBackground = IM_COL32(165, 170, 180, 55);

// ── LES ONGLETS ─────────────────────────────────────────────────────────────
//
// ⚠ ÉQUIPEMENT ET COSTUME PARTAGENT LE MÊME CORPS : même mannequin, même volet
// de nombres à côté. Seules les cases changent — dix d'un côté, quatre de
// l'autre —, ce qui est exactement pourquoi ce sont deux onglets et non deux
// fenêtres.
//
// ⚠ LES PRESETS, LES TITRES ET LA GUILDE ONT CHACUN LEUR ONGLET, PLEINE
// LARGEUR, dans cet ordre entre le costume et le grimoire : c'est leur place.
//
// ⚠ L'HOMONCULE ET LE MERCENAIRE N'ONT D'ONGLET QUE QUAND ILS EXISTENT, après le
// grimoire : c'est la garde du raccourci du client, qui refuse d'ouvrir leur
// fenêtre sans eux.
enum class CharacterTab : std::uint8_t {
  kEquipment,
  kCostume,
  kPresets,
  kTitles,
  kGuild,
  kGrimoire,
  kHomunculus,
  kMercenary,
};

// CE QUE LES DEUX ONGLETS DES COMPAGNONS MONTRENT.
struct CompanionTabsView {
  HomunculusTabView homunculus;
  MercenaryTabView mercenary;
};

// ── LES CASES DU MANNEQUIN ──────────────────────────────────────────────────
//
// L'ordre est celui du DESSIN, colonne gauche de haut en bas, puis colonne
// droite, puis la rangée du bas. C'est `app` qui sait quel emplacement du
// serveur tombe dans laquelle.
//
// 🔴 LES DEUX ACCESSOIRES SONT NOMMÉS COMME LE SERVEUR LES NOMME, et l'écran
// affiche L'INVERSE — « Acc. R » sur la gauche, « Acc. L » sur la droite. Ce
// n'est pas une erreur à réparer : le personnage se montre de face, donc sa
// gauche est à notre droite, et c'est l'étiquette que le joueur connaît.
enum class EquipSlot : std::uint8_t {
  kHeadTop,
  kHeadLow,
  kGarment,
  kAccessoryLeft,
  kHeadMid,
  kArmor,
  kShoes,
  kAccessoryRight,
  kWeapon,
  kShield,
  kAmmo,
};

inline constexpr std::size_t kEquipSlotCount = static_cast<std::size_t>(EquipSlot::kAmmo) + 1;

// Les quatre places d'apparat, dans le même ordre de dessin.
inline constexpr std::size_t kCostumeSlotCount = 4;

// Ce qu'une case montre.
struct EquipSlotView {
  // Le nom COMPOSÉ, prêt à montrer. Vide quand la case est libre — et c'est ce
  // vide qui fait écrire l'abréviation.
  std::string_view name;
  SkinImage icon;
  // La place de la pièce dans le sac, pour que le geste sache quoi retirer.
  std::uint16_t client_slot = 0;
  // ⚠ L'IDENTIFIANT AUSSI, et il ne fait pas double emploi : une case de
  // raccourci survit à la session et retient l'identifiant, là où le geste
  // d'un instant se contente de la place. Voir `ItemDrag`.
  std::uint32_t item_id = 0;
  int refine_level = 0;
  // La munition arrive en pile ; tout le reste vient à l'unité.
  int amount = 0;
  bool damaged = false;

  [[nodiscard]] bool filled() const noexcept { return client_slot != 0; }
};

// UN COMPAGNON QU'ON PEUT APPELER : chariot, monture, faucon.
//
// ⚠ `ui` NE SAIT PAS CE QU'EST UN CHARIOT. Elle reçoit une icône, un libellé et
// deux états ; c'est `app` qui traduit ce que le monde porte, et le RANG lui
// revient tel quel pour qu'elle sache de qui l'on parle. Même frontière que
// pour les cases du mannequin.
//
// ⚠ ET UNE CASE N'EXISTE QUE SI LA COMPÉTENCE EST APPRISE. Un métier qui n'aura
// jamais de chariot ne doit pas en voir une grisée : c'est `app` qui écarte, et
// la fenêtre dessine ce qu'elle reçoit.
struct CompanionCaseView {
  // Ce que `app` rendra dans l'action si l'on clique. Opaque ici.
  std::uint8_t rank = 0;
  // Le nom de la compétence qui l'appelle, déjà composé — l'infobulle.
  std::string_view name;
  SkinImage icon;
  // Là, ou pas. Une case tenue se peint comme un onglet ouvert.
  bool active = false;
};

// Le plus grand nombre de cases de compagnon. Trois, et le serveur n'en connaît
// pas d'autres — le loup, le dragon et l'armure mécanique existent chez lui
// sans qu'aucun paquet ne les propose.
inline constexpr std::size_t kCompanionCaseMost = 3;

// L'en-tête, au-dessus de l'avatar.
struct CharacterHeaderView {
  std::string_view name;
  // Le métier, tel que la table du jeu le nomme.
  std::string_view job;
  std::int64_t base_level = 0;
  std::int64_t job_level = 0;
  // ⚠ LA VIE VIENT DE L'ACTEUR, pas des nombres du personnage : c'est `app` qui
  // croise les deux, et `ui` ne voit que le résultat.
  std::int64_t hp = 0;
  std::int64_t max_hp = 0;
  std::int64_t sp = 0;
  std::int64_t max_sp = 0;
};

// ── L'AVATAR ────────────────────────────────────────────────────────────────

// LES POSES QUE LA FICHE PROPOSE.
//
// ⚠ UN SOUS-ENSEMBLE DES ACTIONS DU .ACT : la mort,
// le coup reçu, le gel et le ramassage en sont écartés — ils n'apprennent rien
// sur un personnage et se regardent mal. `ui` ne sait pas quel rang d'animation
// porte chacune : c'est `app` qui traduit.
enum class CharacterPose : std::uint8_t { kIdle, kWalk, kSit, kReady };

inline constexpr std::size_t kCharacterPoseCount = 4;

// Les poses qui se JOUENT, par opposition à celles qui se tiennent. ⚠ LA MARCHE
// ET LA GARDE SEULES : le client ne fait défiler le corps que pour ces deux
// rangs-là, et les autres ne sont pas des animations mais des poses.
[[nodiscard]] bool CharacterPoseAnimates(CharacterPose pose);

// ⚠ LA GARDE NE SE TIENT QUE DANS LES QUATRE DIRECTIONS CARDINALES, et c'est
// ainsi que le client la montre. La molette y saute donc de deux crans, et
// prendre cette pose recale sur un cran pair — sinon elle ne se verrait pas
// tourner du tout depuis une direction impaire.
[[nodiscard]] bool CharacterPoseIsCardinal(CharacterPose pose);

// Le libellé d'une pose, « (animé) » compris quand elle défile.
[[nodiscard]] const char* CharacterPoseLabel(CharacterPose pose, bool animated);

// CE QUE LA PLACE DE L'AVATAR MONTRE, ET CE QU'ELLE DEMANDE.
//
// 🔴 `ui` NE COMPOSE PAS DE PERSONNAGE : elle reçoit une image déjà faite, et
// dit en retour quelle pose et quelle direction elle veut voir. La composition
// demande la garde-robe, les sprites et la carte graphique, dont aucune n'a sa
// place ici.
//
// ⚠ LA BOÎTE EST MESURÉE ICI ET LUE PAR `app`, à l'image SUIVANTE. Elle dépend
// de l'échelle d'interface, que seule la fenêtre connaît au moment du dessin ;
// une image de retard sur un redimensionnement ne se voit pas.
struct CharacterAvatarState {
  float box_width = 0.0F;
  float box_height = 0.0F;
  // La direction du .ACT : zéro est la face tournée vers nous, et le tour se
  // fait dans le sens des aiguilles d'une montre.
  std::uint8_t direction = 0;
  // ⚠ LA GARDE ANIMÉE PAR DÉFAUT : c'est la pose sous
  // laquelle un joueur se reconnaît, et le repos donne une fiche inerte.
  CharacterPose pose = CharacterPose::kReady;
  bool animated = true;
  // Le portail qui empêche de faire tourner le personnage en parcourant la
  // fiche. Voir `ro/ui/wheel_gate.h`.
  WheelGateState wheel;
};

// ── LES TITRES ──────────────────────────────────────────────────────────────

// UN TITRE QU'ON PEUT PORTER.
struct CharacterTitleView {
  std::int32_t title_id = 0;
  // Le libellé, déjà composé. ⚠ JAMAIS VIDE : un titre que la table ne nomme
  // pas s'écrit par son numéro, sans quoi sa ligne serait un blanc cliquable.
  std::string_view label;
};

// CE QUE L'ONGLET MONTRE.
//
// 🔴 `ui` NE SAIT PAS QUELS TITRES ON POSSÈDE. Le serveur ne le dit pas : c'est
// `app` qui croise les succès réclamés avec la table de l'installation, et la
// fenêtre reçoit une liste déjà nommée et déjà triée.
struct CharacterTitlesView {
  // Zéro : aucun titre porté.
  std::int32_t equipped = 0;
  std::string_view equipped_label;
  std::span<const CharacterTitleView> owned;
};

// La place du filtre de l'onglet, terminateur compris.
inline constexpr std::size_t kTitleFilterBytes = 32;

// Une des six caractéristiques.
struct CharacterAttributeView {
  std::int64_t base = 0;
  // Ce que l'équipement ajoute. Montré à part : « 40 + 12 » dit d'où vient le
  // total, « 52 » ne dit rien.
  std::int64_t bonus = 0;
  // Ce que le cran suivant coûte en points. ⚠ ZÉRO VEUT DIRE « PAS ENCORE DIT »,
  // et non « gratuit » : le bouton reste alors éteint.
  std::int64_t cost = 0;
};

// Les nombres du volet de droite.
// UNE RANGÉE DE LA SECTION « BONUS D'ÉQUIPEMENT ».
//
// 🔴 CE QUE LE SERVEUR ENVOIE N'EST PAS CE QUI S'AFFICHE : il pousse
// quarante-neuf apports, dont presque tous valent zéro sur un personnage donné.
// Le tri et le nom se font là où l'on connaît les deux — `app` —, et la fenêtre
// reçoit une liste déjà courte. C'est la même règle que les onze cases du
// mannequin : `ui` ne sait pas d'où vient ce qu'elle dessine.
//
// ⚠ LES TEXTES SONT DÉJÀ TRADUITS et vivent aussi longtemps que la vue : ce sont
// des littéraux passés par `core::Tr`, jamais des tampons d'une image.
// EN QUOI SE COMPTE UNE VALEUR DE BONUS.
//
// 🔴 CINQ UNITÉS DANS LA MÊME TRAME, et RIEN DANS LA VALEUR NE DIT LAQUELLE.
// « 125 » vaut cent vingt-cinq points de vie, cent vingt-cinq pour cent, cent
// vingt-cinq millisecondes, douze et demi pour cent ou un et quart pour cent
// selon le champ d'où il sort. C'est le code du serveur qui tranche, et il le
// fait à l'endroit où l'on connaît son sens.
enum class BonusUnit {
  kFlat,
  kPercent,
  kMilliseconds,
  // Un MILLIÈME : les taux d'auto-sort, que le serveur compte par mille et qui
  // s'écrivent « 12,5 % ».
  kPerMille,
  // Un CENTIÈME DE POUR CENT : les chances d'infliger un statut et les bonus de
  // butin, que le serveur compte de un à dix mille, et qui s'écrivent
  // « 1,25 % ».
  kCentiPercent,
};

struct CharacterBonusRow {
  std::string_view label;
  std::string_view tooltip;
  std::int32_t value = 0;
  BonusUnit unit = BonusUnit::kFlat;
};

// LA VALEUR D'UNE RANGÉE, écrite dans son unité : la fiche la dessine, la
// discussion l'écrit dans ses lignes d'attributs d'équipement.
void WriteBonusValue(char* into, std::size_t room, const CharacterBonusRow& row);

struct CharacterStatsView {
  // Dans l'ordre du client : force, agilité, constitution, intelligence,
  // dextérité, chance.
  std::array<CharacterAttributeView, 6> attributes = {};
  std::int64_t status_point = 0;

  // Les paires se montrent « base + bonus », comme le jeu les montre.
  std::int64_t atk1 = 0;
  std::int64_t atk2 = 0;
  std::int64_t matk1 = 0;
  std::int64_t matk2 = 0;
  std::int64_t def1 = 0;
  std::int64_t def2 = 0;
  std::int64_t mdef1 = 0;
  std::int64_t mdef2 = 0;

  std::int64_t hit = 0;
  std::int64_t flee1 = 0;
  // ⚠ L'ESQUIVE PARFAITE ET LE CRITIQUE SONT DES POUR CENT ENTIERS. Le serveur
  // les envoie déjà divisés par dix ; les rediviser montrerait un dixième de la
  // vraie valeur.
  std::int64_t flee2 = 0;
  std::int64_t critical = 0;
  // Déjà dérivée du délai de motion par `sim::DisplayedAspd`.
  std::int64_t aspd = 0;

  // CE QUE L'ÉQUIPEMENT APPORTE, seulement ce qui vaut quelque chose.
  //
  // ⚠ VIDE VEUT DIRE DEUX CHOSES, et la section se tait pareil dans les deux
  // cas : ou le serveur n'a rien dit — il ne parle qu'aux clients qu'il a
  // reconnus (ADR 0040) —, ou le personnage n'a aucun apport. Un en-tête
  // repliable ouvert sur rien dirait « cassé » là où il n'y a rien à dire.
  std::span<const CharacterBonusRow> equipment_bonuses;

  // CE QUI NE VAUT QUE CONTRE QUELQU'UN, dans sa propre section.
  //
  // ⚠ SÉPARÉE, ET PAS PAR GOÛT : celle-ci est LONGUE — un personnage équipé en
  // porte des dizaines — et elle se replie donc à part.
  std::span<const CharacterBonusRow> conditional_bonuses;

  // Ce qui tient à une compétence — ou à un statut, que le serveur fait voyager
  // dans le même champ.
  std::span<const CharacterBonusRow> skill_bonuses;

  // Ce qui tient à un objet : un bonus de butin, et rien d'autre à ce jour.
  std::span<const CharacterBonusRow> item_bonuses;
};

// LES CRANS DE LARGEUR DE LA FENÊTRE, mesurés à l'image précédente.
//
// 🔴 TROIS, ET LE TROISIÈME SE CONFOND AVEC LE SECOND tant qu'il n'y a pas de
// volet de plus à montrer. C'est ce qui permettra d'ajouter le volet du staff
// sans toucher au calage : `extra` cessera d'être égal à `wide`, et la bascule à
// deux crans deviendra d'elle-même une bascule à trois.
//
// ⚠ ILS VIVENT DANS L'ÉTAT, pas dans une variable de fichier : `ui` ne possède
// rien, et le rappel de calage les reçoit par l'adresse de cet état.
struct CharacterWidthSteps {
  // Le mannequin seul.
  float narrow = 0.0F;
  // Le mannequin et les nombres.
  float wide = 0.0F;
  // Un volet de plus. Égal à `wide` tant qu'aucun n'existe.
  float extra = 0.0F;
  // Un onglet qui a besoin de toute la largeur interdit le cran étroit.
  bool force_wide = false;
  // POSER LE CRAN LE PLUS LARGE, pour UNE image.
  //
  // ⚠ CE N'EST PAS UN PLANCHER MAIS UN ORDRE : il sert à REPOSER un volet dont
  // le droit vient d'arriver, et le joueur doit pouvoir rétrécir à l'image
  // suivante. Une demande qui survivrait clouerait la fenêtre à sa largeur.
  bool force_extra = false;
  // ⚠ FAUX À LA PREMIÈRE IMAGE, et le rappel ne fait alors rien : caler sur des
  // crans nuls collerait la fenêtre à zéro avant d'avoir rien mesuré.
  bool measured = false;
};

// CE QUE LA FENÊTRE SAIT DU MONDE ET DE SES SŒURS, posé par l'appelant à chaque
// image. ⚠ ELLE NE L'ÉCRIT JAMAIS, et c'est pour cela qu'il vit hors de son
// état : `ui` ne possède aucun état de jeu.
struct CharacterWindowView {
  // La silhouette composée, posée par `app`. Vide tant qu'il n'y en a pas — et
  // la place le dit alors par un mot, plutôt que par un décor quelconque.
  SkinImage avatar_image;
  // 🔴 LE DROIT EST POSÉ PAR `app`, ET RELU À CHAQUE IMAGE : un droit retiré en
  // cours de session doit faire disparaître le volet — et son cran de largeur —
  // sans attendre une reconnexion.
  //
  // ⚠ ET IL DEMANDE LE VOLET DES NOMBRES. Ses commandes s'alignent sur les
  // rangées de celui-ci : sans elles, elles se poseraient en face de rien.
  bool staff_allowed = false;
  // Ce que le volet du staff sait du monde. ⚠ IL NE SERT QUE SOUS LE DROIT.
  StaffPaneView staff;
  // Ce que la ligne du staff du grimoire en sait, sous le même droit.
  GrimoireStaffView grimoire_staff;
  // LES COMPAGNONS QU'ON PEUT APPELER, posés par l'appelant.
  //
  // ⚠ DES VUES, et elles ne valent que pour l'image. Vides quand le personnage
  // n'a appris aucune des trois compétences — et la colonne ne paraît alors
  // pas du tout, ce qui est le cas de la plupart des métiers.
  std::span<const CompanionCaseView> companions;
  // « MONTRER MON ÉQUIPEMENT » aux autres joueurs, tel que le serveur l'a dit.
  // Posé par `app` à chaque image ; rien tant qu'il ne l'a pas dit, et la case
  // se montre alors décochée.
  std::optional<bool> equipment_shown;
  // « VOIR LES COSTUMES », tel que le serveur l'a dit, et de même. ⚠ COCHÉE
  // TANT QU'IL N'A RIEN DIT, à l'inverse de sa sœur : l'une et l'autre montrent
  // alors le réglage d'un personnage neuf.
  std::optional<bool> costumes_shown;
};

// Ce que la fenêtre garde d'une image à l'autre.
struct CharacterWindowState {
  bool open = false;
  // L'ÉPINGLE DE LA BARRE DE TITRE : Échap ne referme plus cette fenêtre-là.
  // ⚠ RETENUE D'UNE SESSION À L'AUTRE, à côté de sa place, par `RememberWindow`.
  bool pinned = false;
  // L'onglet MONTRÉ. ⚠ C'EST UN RELEVÉ, PAS UN ORDRE : la barre l'écrit à chaque
  // image depuis ce qu'ImGui a choisi.
  CharacterTab tab = CharacterTab::kEquipment;
  // L'ONGLET QU'ON DEMANDE, pour UNE image.
  //
  // 🔴 IMGUI POSSÈDE LA SÉLECTION, et il la décide au moment où il DESSINE sa
  // barre : écrire `tab` depuis une touche se fait écraser dans la foulée, la
  // fenêtre s'ouvre — et sur l'onglet d'avant. Seul `ImGuiTabItemFlags_SetSelected`
  // le lui impose, et il ne vaut que pour l'image où on le pose.
  //
  // ⚠ QUI DEMANDE ÉCRIT LES DEUX. `tab` sert AVANT la barre, à calculer les crans
  // de largeur : ne poser que la demande laisserait la fenêtre une image trop
  // étroite pour le grimoire, qu'on verrait s'élargir après coup.
  std::optional<CharacterTab> tab_request;
  // LA SILHOUETTE, sa pose et sa direction. ⚠ ELLE SURVIT À LA SESSION, comme
  // l'épingle : c'est un choix de consultation, et le retrouver au même endroit
  // vaut mieux que de le reprendre à chaque ouverture.
  CharacterAvatarState avatar;
  // Le survol d'une pièce montre sa description entière plutôt que son seul nom.
  // ⚠ UN RÉGLAGE PAR FENÊTRE, comme pour les trois fenêtres d'objets : celui qui
  // veut l'aperçu dans son sac ne le veut pas forcément sur son mannequin.
  bool hover_preview = false;
  // L'onglet qui existait avant les autres, avec son filtre et son arbre ouvert.
  SkillWindowState grimoire;
  // L'onglet des presets : le nom qu'on tape, la touche qu'on attend, et le
  // compte rendu du dernier geste.
  PresetTabState presets;
  // Le filtre de l'onglet des titres, sur leur libellé.
  std::array<char, kTitleFilterBytes> title_filter = {};
  // L'onglet de la guilde : ses saisies, ses confirmations, son éditeur.
  GuildTabState guild;
  // Les onglets de l'homoncule et du mercenaire.
  HomunculusTabState homunculus;
  MercenaryTabState mercenary;
  // ⚠ LE VOLET DES NOMBRES SE CACHE QUAND LA FENÊTRE EST ÉTROITE, et ce
  // souvenir sert à le dire : un volet coupé en deux vaut moins qu'un volet
  // absent, et un ascenseur sur un panneau vide n'annonce rien.
  bool stats_shown = false;

  // ── LE VOLET DU STAFF, le troisième ───────────────────────────────────────
  //
  // LE VOLET ÉTAIT-IL OUVERT LA DERNIÈRE FOIS ? Posé par `app` depuis les
  // réglages du joueur, relu ici pour reposer le cran.
  //
  // 🔴 LE DROIT ARRIVE APRÈS LA FENÊTRE, et c'est ce qui rendait le volet
  // impossible à garder : le serveur ne dit le niveau de groupe qu'une fois la
  // carte entrée, si bien que les premières images voient `staff_allowed` faux.
  // Le plafond de largeur vaut alors le cran SANS volet — et ImGui y rabote la
  // fenêtre restaurée, puis réécrit la largeur rabotée dans son fichier. La
  // largeur était donc perdue AVANT que le droit n'arrive, chaque fois.
  bool staff_wanted = false;
  // Vrai dès que le cran a été reposé pour le droit EN COURS. ⚠ REMIS À FAUX
  // QUAND LE DROIT PART : une reconnexion doit reposer le volet.
  bool staff_restored = false;
  // ⚠ UN RELEVÉ DE LARGEUR, et non le choix du joueur : il dit si la fenêtre est
  // assez large pour le volet, à l'image où on le lit. C'est `staff_wanted` qui
  // porte le souvenir.
  bool staff_shown = false;
  StaffPaneState staff;

  CharacterWidthSteps width;
  // L'onglet pour lequel les crans ont été comptés à l'image précédente : un
  // changement d'onglet se lit contre lui, et il doit garder le volet ouvert.
  CharacterTab steps_tab = CharacterTab::kEquipment;
  // Ce que la fenêtre prend de marge et de cadre, mesuré DEDANS à l'image
  // précédente. Les crans s'expriment en largeur de FENÊTRE, les volets en
  // largeur de contenu : sans cette différence, la fenêtre serait toujours trop
  // étroite d'un padding et le volet des nombres ne s'afficherait jamais au cran
  // qui vient d'être calé pour lui.
  float chrome_width = 0.0F;
};

// Ce que le joueur a demandé pendant l'image.
enum class CharacterVerb : std::uint8_t {
  kNothing,
  // Retirer la pièce d'une case du mannequin.
  kTakeOff,
  // Porter la pièce qu'on vient de lâcher sur le mannequin.
  //
  // ⚠ LA CASE VISÉE NE DÉCIDE DE RIEN, et c'est le geste du client : on
  // lâche sur le mannequin, pas sur un emplacement. Le serveur sait où la
  // pièce va — il refuserait une place impossible — et exiger la bonne case
  // ferait échouer un geste qu'il aurait accepté.
  kWear,
  // Monter une caractéristique de `amount` crans.
  kRaiseAttribute,
};

struct CharacterWindowAction {
  CharacterVerb verb = CharacterVerb::kNothing;
  // La place dans le sac, pour `kTakeOff`.
  std::uint16_t slot = 0;
  // Le rang de la caractéristique, de zéro à cinq, pour `kRaiseAttribute`.
  std::uint8_t attribute = 0;
  int amount = 0;
  // Ctrl tenu pendant le lâcher : dans l'AUTRE main. Même variante que le
  // double-clic du sac, et elle ne veut rien dire par elle-même.
  bool alternate = false;
  // L'OBJET DONT IL VEUT LA DESCRIPTION, et d'où il vient. ⚠ LA PLACE EST CELLE
  // DU SAC : une pièce portée y vit toujours, avec un drapeau — le mannequin
  // n'est pas un contenant du serveur.
  std::uint32_t describe_id = 0;
  std::uint16_t describe_slot = 0;
  // La place dont il veut poser le lien dans la conversation, ou zéro.
  std::uint16_t link_slot = 0;
  // L'ONGLET dont il veut poser le lien — Maj + clic sur sa languette.
  std::optional<CharacterTab> link_tab;
  // CE QUE LA SOURIS SURVOLE, pour l'aperçu. ⚠ NUL PENDANT UN GLISSER : l'aperçu
  // masquerait la case où l'on vise à lâcher.
  std::uint32_t hovered_id = 0;
  std::uint16_t hovered_slot = 0;
  // LE COMPAGNON QU'ON VIENT DE BASCULER, par son rang ; moins de zéro si aucun.
  //
  // ⚠ BASCULER, ET NON APPELER : la case dit déjà s'il est là, et c'est le
  // MÊME clic qui le renvoie. `app` sait lequel des deux verbes envoyer.
  int toggled_companion = -1;
  // LE TITRE QU'ON VEUT PORTER — zéro pour n'en porter aucun. ⚠ JAMAIS CELUI
  // QU'ON PORTE DÉJÀ : le serveur ne répond pas à un titre redemandé.
  std::optional<std::int32_t> equip_title;
  // « MONTRER MON ÉQUIPEMENT » : l'état que la case demande. ⚠ ELLE NE CHANGE
  // PAS D'ELLE-MÊME : elle suit l'écho du serveur, que `app` repose dans
  // `CharacterWindowState::equipment_shown`.
  std::optional<bool> show_equipment;
  // « VOIR LES COSTUMES » : l'état que la case demande, sous la même règle.
  std::optional<bool> show_costumes;

  // Ce que le grimoire a demandé, quand c'est lui qui était à l'écran.
  SkillWindowAction grimoire;
  // Ce que l'onglet des presets a demandé.
  PresetAction presets;
  // Et ce que le volet du staff a demandé, quand il était à l'écran.
  StaffAction staff;
  // Ce que l'onglet de la guilde a demandé.
  GuildAction guild;
  // Ce que les onglets de l'homoncule et du mercenaire ont demandé.
  HomunculusTabAction homunculus;
  MercenaryTabAction mercenary;
};

inline constexpr const char* kCharacterWindowTitle = core::TrText("Personnage");
inline constexpr const char* kCharacterWindowId = "character";

// Le libellé d'une case VIDE, tel que l'écran du jeu l'écrit.
//
// ⚠ ANGLAIS, ET C'EST VOULU : ce sont les mots que les joueurs lisent depuis
// toujours. La traduction les laisse tels quels, comme le reste du jargon.
// LE CRAN LE PLUS PROCHE D'UNE LARGEUR DEMANDÉE.
//
// 🔴 C'EST CE QUI FAIT APPARAÎTRE LE VOLET DES NOMBRES D'UN SEUL GESTE. Sans
// calage, élargir la fenêtre la laisse à une largeur quelconque : le volet
// reste caché tant qu'on n'a pas franchi son seuil au pixel près, et le joueur
// tire la poignée sans comprendre ce qui lui manque.
//
// Rend la largeur demandée telle quelle tant que les crans ne sont pas
// mesurés : caler sur des crans nuls collerait la fenêtre à zéro.
[[nodiscard]] float SnappedCharacterWidth(const CharacterWidthSteps& steps, float wanted);

[[nodiscard]] const char* EquipSlotLabel(EquipSlot slot);

// Les quatre places d'apparat, dans l'ordre du dessin.
[[nodiscard]] std::array<EquipSlot, kCostumeSlotCount> CostumeSlots();

[[nodiscard]] CharacterWindowAction DrawCharacterWindow(
    CharacterWindowState& state, const CharacterWindowView& view, const CharacterHeaderView& header,
    std::span<const EquipSlotView> slots, const CharacterStatsView& stats,
    const PresetTabView& presets, const CharacterTitlesView& titles, const GuildView& guild,
    std::span<const SkillGroupView> groups, SkillPointTally points, const SkinArt& art,
    const CompanionTabsView& companions = {});

}  // namespace ro::ui
