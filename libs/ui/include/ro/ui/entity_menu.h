#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ro/ui/skin_art.h"

struct ImFont;

namespace ro::ui {

// ── LE MENU DU CLIC DROIT SUR UNE ENTITÉ ────────────────────────────────────
//
// Ce que le joueur peut faire de ce qu'il vient de désigner dans le monde — ou
// d'un cadre du HUD de cible, qui se comporte comme l'entité elle-même.
//
// Cette couche DESSINE et rend ce qui a été choisi ; elle ne décide de rien.
// Les lignes, leur ordre, leurs grisages et leurs raisons viennent d'`app`, qui
// les compose à l'ouverture et route la réponse.
//
// 🔴 UNE LIGNE SANS OBJET SE GRISE ET DIT POURQUOI, elle ne disparaît pas : un
// joueur qui ne voit pas « Inviter dans la guilde » ne peut pas distinguer
// « impossible ici » de « ce client ne sait pas le faire ». La raison part en
// infobulle, y compris sur la ligne grisée — c'est justement celle-là qu'on
// survole pour comprendre.

// La question qu'une ligne pose AVANT d'agir. Aucune : elle agit tout de suite.
enum class EntityMenuQuestion : std::uint8_t {
  kNone,
  // Décharger un PNJ, pour tous les joueurs connectés.
  kNpcUnload,
  // Bannir un COMPTE, sans fin.
  kAccountBlock,
  // Pas une confirmation mais une SAISIE : le delta de points d'event. Elle
  // emprunte la même boîte parce qu'elle pose la même sorte de pause — le menu
  // se ferme, la question reste.
  kEventPoints,
  // Céder la direction du groupe, expulser un membre, retirer un ami : aucun
  // ne se défait d'un clic, l'autre devant rendre la main ou accepter de
  // nouveau.
  kPartyLeader,
  kPartyExpel,
  kFriendRemove,
  // Nourrir son pet : le client le demande toujours, parce qu'un pet qui n'a
  // pas faim PERD de l'intimité à manger.
  kPetFeed,
  // Nourrir son homoncule, demandé de même : rassasié, il perd de l'intimité.
  kHomunculusFeed,
  // UNE SAISIE encore, comme les points d'event : les minutes d'un bon point de
  // manière, qui raccourcit le silence, ou d'un mauvais, qui l'allonge.
  kMannerGood,
  kMannerBad,
};

// Le pas d'un don de points d'event, dans un sens comme dans l'autre. Le serveur
// reborne de son côté : ceci ne fait que cadrer la saisie.
inline constexpr int kEventPointsStep = 50;

// Les minutes d'un point de manière : la saisie part de une, et ne dépasse pas
// ce que le client d'origine laisse partir.
inline constexpr int kMannerMinutesFirst = 1;
inline constexpr int kMannerMinutesMost = 30000;
// Les deux pas de la saisie : un clic, et Ctrl + clic — une heure.
inline constexpr int kMannerMinutesStep = 1;
inline constexpr int kMannerMinutesFastStep = 60;

struct EntityMenuItem {
  // ⚠ DÉJÀ TRADUIT par l'appelant.
  std::string label;
  // Ce que l'appelant reconnaîtra dans la réponse. ⚠ OPAQUE ICI : cette couche
  // ne sait pas ce qu'une ligne fait, seulement qu'on l'a choisie.
  int verb = 0;
  // Un trait AVANT cette ligne. Il ouvre un bloc — dans le menu, ou dans le
  // sous-menu du staff pour une ligne qui y vit.
  bool separator = false;
  // L'ocre de ce qui n'est pas offert au joueur.
  bool staff = false;
  // Le rouge de ce qui RETIRE quelque chose au monde. Il l'emporte sur l'ocre.
  bool danger = false;
  bool disabled = false;
  // UNE CASE PLUTÔT QU'UNE ACTION : elle bascule un ÉTAT et ne ferme donc pas le
  // menu — on doit pouvoir cocher, puis parler au PNJ dans la foulée.
  bool toggle = false;
  bool checked = false;
  // DANS UN SOUS-MENU : celui que nomme `submenu_label`, ou « Outils du staff »
  // quand il est vide. ⚠ LES LIGNES D'UN MÊME SOUS-MENU SE SUIVENT SANS TROU : le
  // dessin ouvre le sous-menu à la première et le referme à la dernière. Une
  // ligne d'un autre bloc glissée au milieu y tomberait.
  bool submenu = false;
  // ⚠ DÉJÀ TRADUIT. Vide : le sous-menu du staff, en ocre.
  std::string submenu_label;
  EntityMenuQuestion question = EntityMenuQuestion::kNone;
  // L'infobulle, vide pour aucune. ⚠ DÉJÀ TRADUITE.
  std::string tip;
};

// Ce que le menu montre, composé UNE FOIS à l'ouverture.
struct EntityMenuView {
  // L'entité visée, recopiée dans la question qu'une ligne pose : le menu peut
  // se rouvrir sur autre chose pendant que la boîte est à l'écran, et c'est bien
  // celle NOMMÉE DANS LA QUESTION qui doit recevoir l'action.
  std::uint32_t subject = 0;
  // Son nom, vide quand on ne le connaît pas ; et sa famille, déjà traduite.
  std::string name;
  std::string family;
  // VRAI : le nom s'écrit dans la police coréenne de l'état — le fichier d'un
  // modèle de la carte, que la police de l'interface ne sait pas dessiner.
  bool hangul_name = false;
  std::vector<EntityMenuItem> items;
};

// Ce que le menu garde d'une image à l'autre.
struct EntityMenuState {
  // LA POLICE DES NOMS CORÉENS, posée par l'appelant ; nulle : celle de
  // l'interface. Voir `EntityMenuView::hangul_name`.
  ImFont* hangul_font = nullptr;
  // POSÉ PAR L'APPELANT : ouvrir à la prochaine image, sous la souris.
  bool open_requested = false;
  bool open = false;

  // LA QUESTION EN COURS, et ce qu'elle vise, recopiés au clic.
  bool question_requested = false;
  EntityMenuQuestion question = EntityMenuQuestion::kNone;
  int question_verb = 0;
  std::uint32_t question_subject = 0;
  std::string question_name;
  // 🔴 REMISE À SON DÉPART À CHAQUE QUESTION : gardée d'une fois sur l'autre,
  // un « +50 » resterait armé et partirait sur le joueur suivant d'un simple
  // « Appliquer » — un don qu'on n'a pas voulu. Zéro pour les points d'event,
  // une minute pour un point de manière.
  int points = 0;
};

// Ce que le joueur a fait à cette image. Un verbe nul : rien.
struct EntityMenuAnswer {
  // La ligne choisie.
  int chosen = 0;
  // La case basculée, et l'état qu'on lui a demandé.
  int toggled = 0;
  bool toggled_on = false;
  // La question tranchée par l'affirmative : son verbe, l'entité qu'elle
  // nommait, et la saisie pour les points d'event ou de manière.
  int confirmed = 0;
  std::uint32_t confirmed_subject = 0;
  int points = 0;
};

// Dessine le menu s'il est ouvert, puis la boîte de la question en cours.
//
// ⚠ À APPELER À CHAQUE IMAGE, MÊME MENU FERMÉ : la question se pose quand le
// menu est déjà refermé, et une boîte qu'on cesse de dessiner disparaît sans
// réponse. `view` nul : il n'y a rien à ouvrir, mais la question en cours se
// dessine quand même.
[[nodiscard]] EntityMenuAnswer DrawEntityMenu(EntityMenuState& state, const EntityMenuView* view,
                                              const SkinArt& art);

}  // namespace ro::ui
