#pragma once

#include <imgui.h>

namespace ro::ui {

// LE PORTAIL DE LA MOLETTE : parcourir une fenêtre ne doit RIEN régler au
// passage.
//
// 🔴 TROIS GARDES, et chacune a été payée. Un DÉLAI DE POSE — le curseur doit
// s'immobiliser sur la zone avant qu'elle prenne la molette. Un VERROU DE SALVE
// de défilement — tant que la fenêtre défile, aucune zone ne prend rien, y
// compris une fois la liste arrivée en butée : c'est LÀ que le curseur
// s'immobilise sur une case et que le réglage accidentel se produisait. Une
// SALVE DE RÉGLAGE — une zone qu'on règle ne se fait pas reprendre la molette
// au milieu du geste.
//
// ⚠ L'ÉTAT SE PASSE PAR RÉFÉRENCE : il vit dans la fenêtre qui s'en sert, comme
// tout le reste de `ui`. Deux fenêtres ouvertes ont deux portails, et c'est
// juste — le curseur n'est que sur une.
struct WheelGateState {
  // La zone survolée à la dernière image observée…
  ImGuiID hovered = 0;
  // …et le numéro de cette image : le survol est-il CONTINU ?
  //
  // ⚠ LE SURVOL EST NEUF AUSSI QUAND L'IMAGE PRÉCÉDENTE NE SURVOLAIT PAS : sans
  // ce test, revenir sur une zone quittée depuis longtemps hériterait de son
  // ancien chrono et prendrait la molette aussitôt.
  int hovered_frame = -2;
  double hovered_since = 0.0;
  // La fin de la salve de défilement en cours.
  double scroll_until = 0.0;
  // La zone en train d'être réglée, et la fin de SA salve.
  ImGuiID tuning = 0;
  double tuning_until = 0.0;
};

// LA MOLETTE SUR CETTE ZONE, ou zéro.
//
// `zone` identifie la zone d'une image à l'autre — `ImGui::GetItemID()` pour un
// widget, `ImGui::GetID(nom)` pour un rectangle dessiné à la main. `engaged`
// vaut vrai quand l'appelant tient déjà un geste sur elle : le portail s'ouvre
// alors sans attendre.
//
// ⚠ LA POSSESSION SE REVENDIQUE À CHAQUE IMAGE, pas au moment où un cran
// arrive : ImGui applique le défilement au DÉBUT de l'image, donc ce qu'on
// revendique ne vaut que pour la SUIVANTE. Revendiquer au cran laisserait la
// fenêtre défiler ce cran-là EN PLUS de régler.
//
// Le cran rendu est CONSOMMÉ : plus personne ne le relit.
[[nodiscard]] float WheelOver(WheelGateState& state, ImGuiID zone, bool hovered,
                              bool engaged = false);

// LE PORTAIL DES CHAMPS À MOLETTE — curseurs, champs glissants, listes
// déroulantes : des widgets que tant de fenêtres posent sans leur prêter
// d'état. Ils partagent UN portail, celui du contexte d'ImGui : le pointeur
// n'en survole qu'un à la fois.
//
// `UiContext` lui attache le sien à sa naissance ; il vit aussi longtemps que
// lui. ⚠ IL OCCUPE `ImGuiIO::UserData` de ce contexte, qui n'a pas d'autre
// usage ici.
void AttachFieldWheelGate(WheelGateState& state);

// LA MOLETTE SUR CE CHAMP, ou zéro — `WheelOver` sur le portail partagé.
//
// Sans portail attaché — un contexte d'ImGui nu —, les mêmes gardes se lisent
// dans ce qu'ImGui mesure déjà : le DÉLAI DE POSE sur le survol continu du
// champ, et le VERROU DE SALVE sur la fenêtre qu'une molette est en train de
// faire défiler.
//
// ⚠ À APPELER À CHAQUE IMAGE où le champ est posé : la possession se
// revendique d'avance, pour la même raison que chez `WheelOver`. Le cran rendu
// est consommé.
[[nodiscard]] float WheelOverField(ImGuiID zone, bool hovered);

// LA MOLETTE SUR LE DERNIER ÉLÉMENT POSÉ, ou zéro : `WheelOverField` sur lui,
// survolé au sens d'ImGui. À appeler juste après l'élément.
[[nodiscard]] float WheelOverItem();

}  // namespace ro::ui
