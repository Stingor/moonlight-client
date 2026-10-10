#pragma once

#include "ro/audio/audio_engine.h"
#include "ro/render/gl_context.h"
#include "ro/render/item_drop.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/item_obtain_toast.h"

#include "game/settings_file.h"
#include "game/ui_stage.h"
#include "world/map_music.h"

namespace moonclient {

class ChatTabsView;
class EffectsInPlay;

// CE QU'UNE OPTION VA TOUCHER. Passé par pointeurs plutôt que gardé : les
// options ne POSSÈDENT rien du moteur, elles ne font que lui parler. C'est la
// même discipline que le routage du chat, et elle achète la même chose — un
// moteur audio absent ne rend pas les réglages inutilisables.
//
// ⚠ LE FICHIER EN FAIT PARTIE, et ce n'est pas une commodité : un réglage qui
// bouge sans que le fichier l'apprenne est un réglage perdu à la partie
// suivante. Le mettre ici rend l'oubli impossible — toute méthode qui change
// quelque chose reçoit déjà de quoi l'enregistrer.
//
// ⚠ DANS SON PROPRE FICHIER, et c'est le plafond qui l'a voulu : `GameOptions`
// touchait ses huit cents lignes, et cette structure-ci n'est pas la classe —
// c'est ce qu'on lui tend. Les deux se lisent séparément.
struct OptionTargets {
  ro::audio::AudioEngine* audio = nullptr;
  // Rétablir la musique ne suffit pas : elle ne se redemande qu'au changement
  // de carte. Voir `MapMusic::Forget`.
  MapMusic* music = nullptr;
  UiStage* interface = nullptr;
  // La conversation VIVANTE, dont la borne d'éviction est un réglage.
  ro::sim::World* world = nullptr;
  // Ce qui GARDE la conversation d'une partie à l'autre, et sait l'oublier.
  ChatTabsView* chat = nullptr;
  // Le suivi rigide de la caméra. Il vit dans le cadrage, qui seul sait où elle
  // en est de son rattrapage.
  bool* fixed_camera = nullptr;
  // CE QU'UN BUTIN FAIT EN TOMBANT. Il vit dans le module qui tient les arcs en
  // cours, lequel le relit à chaque image : rien à recomposer en le posant.
  ro::render::ItemDropSettings* item_drop = nullptr;
  // LA POIGNÉE DU BANDEAU D'OBTENTION. ⚠ ELLE SE POUSSE, contre le reste du
  // bandeau qui se relit : deux réglages DÉPLACENT le bandeau sous elle — le
  // verrou et le centrage — et sans ordre de la replacer, ImGui garderait la
  // position qu'elle avait à sa création.
  ro::ui::ItemObtainAnchorGrip* item_obtain_grip = nullptr;
  // LA FENÊTRE DU SYSTÈME. ⚠ C'est `render` qui la tient, et c'est voulu : elle
  // est un fait d'affichage, pas de jeu.
  ro::render::GlContext* display = nullptr;
  // LES DEUX BASCULES D'EFFET. ⚠ Elles se POUSSENT : la troupe les relit à
  // chaque image, et elle n'a pas de chemin vers les réglages.
  EffectsInPlay* effects = nullptr;
  // « ESSAYER MAINTENANT » — le bouton de l'écran de veille.
  //
  // ⚠ UNE DEMANDE ET NON UN APPEL : entrer en veille demande la CAMÉRA et le
  // compteur d'inactivité, que les réglages ne possèdent pas. Ils arment, la
  // boucle joue — exactement comme les trois gestes de la capture, qui
  // demandent l'écran et l'horloge.
  bool* afk_try_now = nullptr;
  // L'ATLAS DES RECETTES, que le bouton de sa page ouvre.
  ro::ui::CraftAtlasWindowState* craft_atlas = nullptr;
  SettingsFile* file = nullptr;
};

}  // namespace moonclient
