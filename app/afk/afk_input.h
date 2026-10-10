#pragma once

#include <cstdint>

namespace moonclient {

// LE COMPTEUR D'INACTIVITÉ, et ce qu'il AVALE.
//
// Il ne connaît pas SDL : la boucle traduit ses événements en `AfkInput`, et ce
// fichier n'a qu'une décision à prendre. C'est ce qui le rend essayable sans
// fenêtre.
//
// 🔴 DEUX HORODATAGES, ET LES CONFONDRE CASSE L'UN OU L'AUTRE :
//   · le dernier geste, SANS FILTRE — c'est lui qui repousse l'endormissement.
//     Le filtrer ferait s'endormir le client pendant que le joueur promène sa
//     souris, au seul motif qu'il a demandé un réveil au clavier ;
//   · le dernier geste AUTORISÉ À RÉVEILLER — c'est lui que la veille interroge
//     une fois endormie.
// Hors du mode « clavier et souris », les deux divergent : c'est tout l'objet
// du réglage.

// D'OÙ VIENT UN GESTE, et ce qu'il est.
enum class AfkInputKind : std::uint8_t {
  kKeyPress,
  kKeyRelease,
  // La saisie de texte. ⚠ ELLE COMPTE COMME UN APPUI : une touche morte ou une
  // saisie asiatique n'émet parfois que celle-là.
  kTextInput,
  kMouseMove,
  kMouseWheel,
  kMousePress,
  kMouseRelease,
};

// UN GESTE, tel que la boucle le rapporte.
struct AfkInput {
  AfkInputKind kind = AfkInputKind::kKeyPress;
  // Le bouton, pour les deux derniers genres. Numéroté comme SDL — un pour le
  // gauche —, zéro quand il n'y en a pas.
  std::uint8_t button = 0;
  // La position, pour le mouvement seul.
  float x = 0.0F;
  float y = 0.0F;
};

class AfkIdle {
 public:
  // RAPPORTE UN GESTE, ET DIT S'IL DOIT ÊTRE AVALÉ — c'est-à-dire ne parvenir
  // ni au jeu ni à l'interface.
  //
  // 🔴 DEUX RÔLES EN UNE FONCTION, ET CE N'EST PAS DE L'ÉCONOMIE : il faut
  // décider d'avaler AU MOMENT MÊME où l'on constate le geste. Un simple
  // drapeau « on s'est réveillé », consommé plus tard, laisserait passer le clic
  // de l'image en cours — celui-là même qui déplacerait le personnage dans une
  // direction que le joueur n'a pas choisie, puisqu'il visait un décor vu sous
  // un autre angle.
  [[nodiscard]] bool Note(const AfkInput& input, std::uint32_t now_ms);

  // Ce qui a le droit de réveiller — voir `AfkWake`. Reposé à chaque battement,
  // comme l'état de veille.
  void SetWakeMode(int mode) noexcept { wake_mode_ = mode; }

  // La veille est-elle en cours ? C'est ce qui décide s'il faut avaler.
  void SetSleeping(bool sleeping) noexcept;

  // MARQUE COMME « DÉJÀ AVALÉS » les boutons actuellement enfoncés.
  //
  // ⚠ SANS QUOI LE GESTE COMMENCÉ AVANT LA VEILLE VIENDRAIT LA TERMINER : un
  // bouton dont le jeu a vu le relâchement sans avoir vu l'appui, c'est un clic
  // fantôme — et selon ce qu'il survole, un glisser-déposer qui finit dans le
  // vide. `held` porte les boutons enfoncés, dans le même numérotage que
  // `AfkInput::button`.
  void SwallowHeldButtons(std::uint32_t held) noexcept { swallowed_ |= held; }

  // Depuis combien de temps plus rien ne bouge. ⚠ ZÉRO TANT QU'AUCUN GESTE
  // N'EST VENU : sans cela le client s'endormirait avant le premier de la
  // partie.
  [[nodiscard]] std::uint32_t IdleMs(std::uint32_t now_ms) const noexcept;

  // Un geste AUTORISÉ À RÉVEILLER est-il survenu depuis cet instant ?
  //
  // ⚠ LA QUESTION EST UNE COMPARAISON DE DATES, et non « l'inactivité est-elle
  // retombée sous un seuil ? » : un seuil aurait été un chiffre arbitraire à
  // choisir, là où la date répond exactement.
  [[nodiscard]] bool WokeSince(std::uint32_t instant_ms) const noexcept;

 private:
  // Le masque d'un bouton, tel que cette classe le range.
  [[nodiscard]] static std::uint32_t MaskOf(std::uint8_t button) noexcept;

  std::uint32_t last_input_ms_ = 0;
  std::uint32_t last_wake_ms_ = 0;
  // 🔴 UN TÉMOIN ET NON UN ZÉRO SENTINELLE : notre horloge part de zéro au
  // lancement, si bien qu'un zéro y voudrait dire à la fois « jamais » et « à
  // l'instant ».
  bool any_input_ = false;
  bool any_wake_ = false;
  bool sleeping_ = false;
  int wake_mode_ = 0;
  // Les boutons dont on a avalé l'appui : leur relâchement doit l'être aussi.
  std::uint32_t swallowed_ = 0;
  // La dernière place connue du curseur, pour distinguer un vrai mouvement de
  // celui que le système renvoie quand la fenêtre bouge sous une souris
  // immobile. Sans ce test, une animation quelconque sous le curseur suffirait
  // à empêcher la veille indéfiniment.
  float last_x_ = 0.0F;
  float last_y_ = 0.0F;
  bool any_move_ = false;
};

}  // namespace moonclient
