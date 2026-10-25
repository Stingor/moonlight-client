// TOUT CE QUI SE VOYAIT S'ÉTEINT AU CHANGEMENT DE CARTE, même carte comprise :
// effets, nombres, rémanences, gestes en attente, menus qui désignent une
// entité, et les textures que la carte quittée a seule réclamées.

#include "game/game.h"
#include "world/entity_menu.h"

namespace moonclient {

void Game::ForgetWhatPlayed() {
  sky_raised_ = false;
  name_card_asker_.Forget();
  // L'horloge des modèles n'est PAS ici : elle ne s'efface qu'avec la scène, et
  // un téléport sur la même carte n'en change pas — voir l'arrivée sur carte.
  if (effects_.has_value()) {
    effects_->Forget();
  }
  effect_lights_.clear();
  if (effect_nodes_.has_value()) {
    effect_nodes_->Forget();
  }
  if (pieces_.numbers.has_value()) {
    pieces_.numbers->Forget();
  }
  if (pieces_.item_toast.has_value()) {
    pieces_.item_toast->Forget();
  }
  if (actor_sounds_.has_value()) {
    actor_sounds_->Forget();
  }
  footprints_.Forget();
  ghosts_.Forget();
  // Ce qui s'effaçait sur l'ancienne carte n'a rien à faire sur la nouvelle.
  scene_actors_.fading.clear();
  scene_actors_.corpses.clear();
  pieces_.falcons.Forget();
  pieces_.carts.Forget();
  pieces_.map_sprites.Forget();
  pieces_.ailment_tints.Forget();
  pieces_.cart_choice.Forget();
  pieces_.talkbox.Forget();
  pieces_.npc_illustration.Forget();
  drops_.Forget();
  // Le monde purge la cible à l'arrivée ; les gestes en attente partent avec.
  targeting_.Forget();
  // Le nom annoncé est déjà le nouveau, celui qu'on a tenté de charger encore
  // l'ancien.
  const bool same_map = world_.map_name() == attempted_map_;
  // Le cercle et les relances de Quick cast visaient l'ancienne carte ; sur la
  // même, l'item tenu continue — voir `QuickCast::ForgetAim`.
  if (same_map) {
    quick_cast_.ForgetAim();
  } else {
    quick_cast_.Forget();
  }
  // ⚠ LE SORT EN ATTENTE AUSSI, et c'est le sort AU SOL qui l'exige : une
  // action sur un acteur tombe d'elle-même quand il disparaît, une action sur
  // une case ne tombe jamais — le personnage repartirait vers les mêmes
  // coordonnées, sur la carte où il vient d'arriver, et y poserait le sort.
  // Le curseur armé vise la même carte révolue.
  grimoire_.CancelPending();
  grimoire_.Disarm();
  // LA FLÈCHE RENAÎT CACHÉE, et son animation repart, même carte comprise.
  // L'engagement, lui, ne tombe que sur une AUTRE carte.
  EnterMap(world_mouse_.engagement, same_map);
  marker_pose_ = MarkerPose();
  marker_epoch_ms_.reset();
  // 🔴 LE MENU NE SURVIT PAS À L'ENTITÉ QU'IL DÉSIGNE, ni la question qu'il
  // pose : sur une autre carte, le même GID peut désigner autre chose. Le dessin
  // suivant referme ce qui reste ouvert.
  windows_.entity_menu.state = ro::ui::EntityMenuState{};
  menu_gestures_.ask.reset();
  // « VOIR L'ÉQUIPEMENT » SE FERME ET OUBLIE : le serveur exige la même carte,
  // et une fiche qu'on ne peut plus redemander serait périmée sans le dire.
  if (viewed_equipment_.has_value()) {
    viewed_equipment_->Forget(inspected_, windows_.view_equip.state);
  }
  // 🔴 LES TEXTURES QUE LA CARTE QUITTÉE A SEULE RÉCLAMÉES S'EN VONT. Les caches
  // des renderers ne se purgent jamais d'eux-mêmes : sans cet appel, chaque
  // image × palette rencontrée resterait en mémoire vidéo toute la séance.
  // L'oubli garde ce qui a servi depuis le précédent, donc la carte qu'on
  // vient de quitter : un aller-retour ne retéléverse rien.
  sprites_.ForgetUnusedTextures();
  effect_renderer_.ForgetUnusedTextures();
  pieces_.overlay.ForgetUnusedTextures();
}

}  // namespace moonclient
