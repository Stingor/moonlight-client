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
#include "ro/ui/character_window.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// LA FENÊTRE D'ÉQUIPEMENT DU JEU : deux onglets, « Général » et « Costume »,
// dix places autour du personnage, la munition sous lui.
//
// 🔴 C'EST LA COPIE DE CELLE DU JEU, AU PIXEL : son fond peint, ses places, ses
// zones de clic, ses boutons et ses cases à cocher. La fiche de personnage
// existe à côté pour qui veut tout voir ; celle-ci est pour qui veut retrouver
// la sienne.
//
// 🔴 SA LARGEUR EST CELLE DU FOND. L'installation de Moonlight le livre en 340
// pixels au lieu de 280 : les noms y respirent, la colonne de droite s'écarte
// du personnage, et la règle d'écart est celle de `EquipWindowLayoutFor`.
//
// ⚠ LE GLISSER SE LÂCHE SUR TOUTE LA FENÊTRE, pas sur une place : c'est le
// serveur qui range la pièce, comme dans le jeu. Il ne part, lui, que de
// l'ICÔNE d'une place, quand la bulle et le clic droit répondent sur toute la
// rangée.
//
// ⚠ ELLE NE POSSÈDE AUCUN ÉTAT DE JEU : `app` compose les vingt places, la
// silhouette et ce que les boutons peuvent faire, et joue ce qu'elle rend.

// L'IDENTITÉ DE LA FENÊTRE, qui porte sa place. ⚠ ELLE NE SE TRADUIT PAS.
inline constexpr const char* kEquipWindowId = "equip";
// LE RÉGLAGE DE SA RÉDUCTION, retenu avec la disposition du personnage, comme
// le jeu retient la hauteur de la sienne.
inline constexpr const char* kEquipWindowMinimizedKey = "equip.minimized";

// ── LA GÉOMÉTRIE, EN PIXELS D'ART ───────────────────────────────────────────
//
// L'origine est le coin haut-gauche de la fenêtre, barre de titre comprise.

// La largeur du jeu, et celle de la fenêtre quand le fond manque.
inline constexpr float kEquipWindowGameWidthPixels = 280.0F;
// La hauteur entière, et celle d'une fenêtre réduite à sa barre.
inline constexpr float kEquipWindowHeightPixels = 187.0F;
inline constexpr float kEquipWindowTitleBarPixels = 17.0F;

// LES DIX PLACES, DANS L'ORDRE DES BITS DU SERVEUR : la place `n` montre la
// pièce portée au bit `n`. ⚠ C'EST L'ORDRE DU JEU, et il ne suit pas le
// dessin : les deux premières rangées sont les places 8 et 9.
//
//   gauche :  8 coiffe haute · 0 coiffe basse · 1 arme · 2 cape · 3 accessoire
//   droite :  9 coiffe du milieu · 4 armure · 5 bouclier · 6 chaussures · 7 accessoire
inline constexpr std::size_t kEquipWindowPlaceCount = 10;
// La munition, sous le personnage, hors des dix places.
inline constexpr std::size_t kEquipWindowAmmoPlace = kEquipWindowPlaceCount;

// LA BOÎTE DE LA SILHOUETTE, en pixels d'art, À L'ÉCHELLE UN : le jeu pose le
// personnage sans le réduire, ses pieds au point `EquipWindowAvatarFeet`. La
// boîte va du haut de la fenêtre à ce point, et de toute sa largeur : une
// coiffe large en sort sur les côtés comme dans le jeu, sans le déplacer.
inline constexpr float kEquipWindowAvatarFeetPixels = 149.0F;

enum class EquipWindowTab : std::uint8_t { kGeneral, kCostume };

// LES COLONNES ET LES LARGEURS QUI DÉPENDENT DE LA LARGEUR DU FOND.
struct EquipWindowLayout {
  float width = kEquipWindowGameWidthPixels;
  // L'icône de la colonne de droite, et le liseré de survol qui l'entoure.
  float right_icon = 0.0F;
  float right_highlight = 0.0F;
  // Le début d'un nom à droite, écrit vers la droite comme à gauche.
  float right_name = 0.0F;
  // La largeur qu'un nom remplit avant de passer à la ligne.
  float name_room = 0.0F;
};

// LA DISPOSITION D'UN FOND DE CETTE LARGEUR.
//
// 🔴 AU-DELÀ DE 280, LA COLONNE DE DROITE S'ÉCARTE DE CE QUI S'AJOUTE, et les
// noms prennent la place libérée : la droite commence plus loin du
// personnage, que la largeur grandit aussi — il est centré —, et un nom long
// tient sur une ligne. Sous 280, c'est la disposition du jeu.
[[nodiscard]] EquipWindowLayout EquipWindowLayoutFor(float width);

// LA PLACE SOUS UN POINT, en pixels d'art depuis le coin de la fenêtre :
// l'ICÔNE seule (prise, double-clic), ou toute la RANGÉE (bulle, clic droit).
// La munition se rend comme `kEquipWindowAmmoPlace`. Rien hors d'une place.
[[nodiscard]] std::optional<std::size_t> EquipWindowIconAt(const EquipWindowLayout& layout,
                                                           ImVec2 at);
[[nodiscard]] std::optional<std::size_t> EquipWindowEntryAt(const EquipWindowLayout& layout,
                                                            ImVec2 at);

// LA COULEUR D'UN NOM : celle du nombre de cartes posées AU-DELÀ des
// emplacements de la pièce — de zéro à quatre.
[[nodiscard]] ImU32 EquipWindowNameColor(std::uint8_t cards_past_slots);

// CE QU'UNE PLACE MONTRE.
struct EquipWindowPiece {
  // Le nom composé, l'icône, la place dans le sac et l'identifiant. Vide :
  // place libre, et rien ne s'y dessine — c'est le fond qui la nomme.
  EquipSlotView piece;
  // Les cartes posées au-delà des emplacements : la couleur du nom.
  std::uint8_t cards_past_slots = 0;
  // Les options aléatoires, que la bulle compte.
  std::uint8_t option_count = 0;
  // L'emblème du grade, vide sans grade.
  SkinImage grade_badge;
};

// UN COMPAGNON QUE LE BOUTON DE MONTURE PEUT RENVOYER : le chariot, la
// monture, le faucon. `rank` revient tel quel dans `toggled_companion`.
struct EquipWindowRider {
  int rank = -1;
  // Ce que la bulle et le menu en disent, déjà traduit.
  std::string_view label;
};

// CE QUE LA FENÊTRE SAIT DU MONDE, recomposé par l'appelant à chaque image.
struct EquipWindowView {
  std::array<EquipWindowPiece, kEquipWindowPlaceCount> general = {};
  // ⚠ L'ONGLET COSTUME MONTRE AUSSI L'ÉQUIPEMENT D'OMBRE, à la place ordinaire
  // qu'il double : armure, arme, bouclier, chaussures, accessoires.
  std::array<EquipWindowPiece, kEquipWindowPlaceCount> costume = {};
  EquipWindowPiece ammo;
  // LA SILHOUETTE DE L'ONGLET MONTRÉ, composée à la boîte de la fenêtre :
  // `EquipWindowAvatarBox`.
  SkinImage avatar;
  // LES PLACES OÙ LA PIÈCE TRAÎNÉE PEUT ALLER, entourées. Calculées par `app`
  // depuis `EquipWindowAction::dragged_slot` de l'image précédente.
  std::array<bool, kEquipWindowPlaceCount> highlighted = {};
  // Les deux réglages que les cases à cocher montrent, tels que le serveur
  // les a dits ; rien tant qu'il n'a rien dit.
  std::optional<bool> equipment_shown;
  std::optional<bool> costumes_shown;
  // « TOUT RETIRER » ATTEND LA RÉPONSE DU SERVEUR : le bouton reste éteint
  // jusqu'à elle, ou jusqu'à ce que l'équipement change.
  bool take_off_all_pending = false;
  // Ce que le bouton de monture peut renvoyer. Vide : pas de bouton.
  std::span<const EquipWindowRider> riders;
  // Le chariot est attelé : le bouton qui ouvre son contenu se montre.
  bool cart = false;
  std::string cart_tooltip;
  // LA TOUCHE QUI OUVRE ET FERME LA FENÊTRE, dite par la bulle de la croix.
  // Vide sans touche.
  std::string toggle_keys;
};

// Ce que la fenêtre garde d'une image à l'autre.
struct EquipWindowState {
  bool open = false;
  // RÉDUITE À SA BARRE, par le bouton ou d'un double-clic sur la barre.
  bool minimized = false;
  EquipWindowTab tab = EquipWindowTab::kGeneral;
  // LE COIN HAUT-GAUCHE À GARDER à l'image qui suit une réduction : la fenêtre
  // grandit et rétrécit par le bas.
  std::optional<ImVec2> keep_top_left;
  // ⚠ UN RELEVÉ, PAS UN RÉGLAGE : la boîte de la silhouette, mesurée au dessin
  // — elle dépend de la largeur du fond — et lue par `app` à l'image suivante.
  ImVec2 avatar_box = ImVec2(0.0F, 0.0F);
};

struct EquipWindowAction {
  // LES GESTES QUE LA FICHE CONNAÎT DÉJÀ, et que `app` joue par la même route :
  // retirer, porter, décrire, lier dans la conversation, les deux réglages, et
  // le compagnon à renvoyer.
  CharacterWindowAction sheet;
  // « TOUT RETIRER » : vrai pour l'onglet Costume, faux pour Général.
  std::optional<bool> take_off_all_costume;
  // Le bouton du chariot a été cliqué : son contenu s'ouvre ou se ferme.
  bool toggle_cart = false;
  // LA PLACE DU SAC DE LA PIÈCE TRAÎNÉE SUR LA FENÊTRE, ou zéro.
  std::uint16_t dragged_slot = 0;
};

// LA LARGEUR DE LA FENÊTRE, celle du fond — ou celle du jeu sans lui.
[[nodiscard]] float EquipWindowWidth(const SkinArt& art);

// LA BOÎTE OÙ `app` COMPOSE LA SILHOUETTE, en pixels de sprite : la largeur
// de la fenêtre, et sa hauteur jusqu'aux pieds.
[[nodiscard]] ImVec2 EquipWindowAvatarBox(const SkinArt& art);

[[nodiscard]] EquipWindowAction DrawEquipWindow(EquipWindowState& state,
                                                const EquipWindowView& view, const SkinArt& art);

}  // namespace ro::ui
