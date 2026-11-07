#pragma once

// LE CURSEUR D'UNE COMMANDE DU STAFF : il pose une valeur sur le serveur, donc
// il n'envoie qu'au terme du geste et montre la valeur envoyée le temps que le
// serveur la renvoie. Les fiches de l'homoncule et du familier le partagent,
// et le volet du staff pour la vitesse de marche.

#include <optional>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// LE TEMPS PENDANT LEQUEL UNE VALEUR ENVOYÉE EST ATTENDUE, en secondes. Le
// serveur renvoie la fiche en une fraction de seconde ; un refus, lui, ne
// renvoie rien, et le curseur doit bien finir par revenir à la valeur servie.
inline constexpr double kStaffSliderAwaitSeconds = 5.0;

// LE REPOS QUI CLÔT UN GESTE SANS BOUTON TENU, en secondes : au-delà, une salve
// de crans de molette ou de clics sur une flèche est finie. Du même ordre que
// la durée d'une salve de molette.
inline constexpr double kStaffSliderRestSeconds = 0.35;

// UNE VALEUR QU'UN CURSEUR DU STAFF A ENVOYÉE, et que le serveur n'a pas encore
// renvoyée.
struct StaffSliderPending {
  int value = 0;
  // L'instant de l'envoi, sur l'horloge d'ImGui, en secondes.
  double sent_at_s = 0.0;
};

// VRAI TANT QUE `pending` EST ATTENDUE : la valeur servie ne l'a pas rejointe
// et le délai n'est pas passé. Le curseur la montre pendant ce temps, au lieu
// de la valeur servie, périmée.
[[nodiscard]] bool StaffSliderAwaited(const std::optional<StaffSliderPending>& pending, int served,
                                      double now_s) noexcept;

// CE QU'A FAIT UN CURSEUR DU STAFF pendant une image.
struct StaffSliderGesture {
  // La valeur a bougé : glisser, flèche ou saisie.
  bool changed = false;
  // Le curseur est saisi : on le glisse, ou on tape sa valeur.
  bool active = false;
  // Il vient d'être lâché après avoir changé.
  bool released_after_edit = false;
  // Le bouton gauche est tenu.
  bool mouse_held = false;
};

// LA VALEUR À ENVOYER au terme du geste, rien tant qu'il dure.
//
// 🔴 RIEN NE PART PENDANT LE GESTE : la valeur vit dans `draft` tant qu'on
// glisse, et tant qu'une flèche est tenue — elle répète sans saisir le
// curseur, donc seul le relâchement du bouton dit que le geste est fini.
[[nodiscard]] std::optional<int> StaffSliderSettle(int value, const StaffSliderGesture& gesture,
                                                   std::optional<int>& draft);

// LA VALEUR QUE MONTRE UN CURSEUR HORS GESTE : l'envoyée tant qu'elle est
// attendue, la servie sinon. Une attente échue s'efface ici.
[[nodiscard]] int StaffSliderShown(std::optional<StaffSliderPending>& pending, int served,
                                   double now_s);

// LE CURSEUR LUI-MÊME. `shown` est ce qu'il montre hors geste. Rend la valeur à
// envoyer, rien tant qu'il n'y a rien à envoyer.
//
// ⚠ IL NE PREND PAS LA MOLETTE : un cran effleuré en faisant défiler une fiche
// enverrait une commande, et certaines ne se défont pas.
[[nodiscard]] std::optional<int> StaffReleasedSlider(const char* label, int shown, int lowest,
                                                     int highest, std::optional<int>& draft,
                                                     const SkinArt& art);

// LE BROUILLON D'UN CURSEUR QUI PREND LA MOLETTE, et l'instant de son dernier
// changement, sur l'horloge d'ImGui, en secondes.
struct StaffWheelDraft {
  std::optional<int> value;
  double changed_at_s = 0.0;
};

// LE CURSEUR QUI PREND LA MOLETTE, réservé à une commande sans conséquence
// durable. Rend la valeur à envoyer, rien tant que le geste dure ou si la valeur
// finale est celle qu'il montrait déjà.
//
// 🔴 UN CRAN NE PART PAS TOUT SEUL : chaque envoi coûte une ligne de conversation
// et un calcul au serveur. La salve se résume en un seul envoi, après
// `kStaffSliderRestSeconds` sans changement.
[[nodiscard]] std::optional<int> StaffWheelSlider(const char* label, int shown, int lowest,
                                                  int highest, StaffWheelDraft& draft,
                                                  const SkinArt& art);

}  // namespace ro::ui
