#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "ro/ui/chat_link.h"
#include "ro/ui/item_link_cell.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// ── « VOIR L'ÉQUIPEMENT » D'UN AUTRE JOUEUR ─────────────────────────────────
//
// Ce que le client d'origine ne fait pas, et qu'elle fait :
//   · elle RESTE ouverte et se rafraîchit quand on inspecte quelqu'un d'autre ;
//   · cartes, options, grade, raffinement, objet cassé, lisibles d'un coup ;
//   · la description et le lien de chat d'une pièce, qu'il interdisait sur
//     l'équipement d'autrui ;
//   · les emplacements VIDES : « il n'a pas de cape » est une information.
//
// Cette couche dessine et rend ce qui a été demandé ; `app` compose les lignes,
// le pantin et les icônes, et joue les gestes.

// Une pièce, telle que `app` l'a composée.
struct ViewEquipPieceView {
  SkinImage icon;
  // Le nom composé — raffinement, cartes, « [N] » —, DÉJÀ TRADUIT.
  std::string label;
  bool damaged = false;
  std::uint32_t item_id = 0;
  // Rendu tel quel avec un geste : `app` seul sait quelle pièce il désigne.
  std::uint32_t token = 0;
};

// Une ligne de la grille.
struct ViewEquipRowView {
  // Le nom de l'emplacement, DÉJÀ TRADUIT.
  const char* place = "";
  std::optional<ViewEquipPieceView> theirs;
  // Vide parce qu'une arme à deux mains tient aussi cette main.
  bool held_by_both_hands = false;
  // Ce que JE porte au même emplacement, pour la comparaison.
  std::optional<ViewEquipPieceView> mine;
  // Le même objet de BASE des deux côtés : ce n'est pas « la même pièce » —
  // raffinement, cartes et options peuvent différer —, mais l'œil doit le voir.
  bool same_base = false;
};

struct ViewEquipSectionView {
  // DÉJÀ TRADUIT, identité `###` comprise.
  const char* title = "";
  std::vector<ViewEquipRowView> rows;
  // La section hors grille — munition, ombre — ne se compare pas.
  bool comparable = true;
};

struct ViewEquipView {
  // Ce que le menu d'un item sait d'avance : vote shop, album.
  ItemLinkFacts link_facts;
  // DÉJÀ COMPOSÉ, identité `###` comprise : inspecter quelqu'un d'autre change
  // le titre, pas la place ni la taille de la fenêtre.
  std::string title;
  // « Homme » ou « Femme », DÉJÀ TRADUIT.
  const char* sex = "";
  // Depuis combien de secondes la fiche est relevée.
  int age_seconds = 0;
  // Les trois outils du staff, et le nom qu'ils prennent.
  bool staff = false;
  // On sait À QUI redemander : la réponse du serveur ne porte pas l'identifiant.
  bool can_refresh = false;
  // Le pantin, déjà rendu à la taille que la fenêtre a demandée ; inutilisable
  // quand il n'y a pas de silhouette à montrer.
  SkinImage doll;
  std::vector<ViewEquipSectionView> sections;
  int piece_count = 0;
};

// Ce que la fenêtre garde d'une image à l'autre.
struct ViewEquipWindowState {
  bool open = false;
  // Passer devant à l'image suivante : une fiche vient d'arriver.
  bool raise = false;
  // L'orientation du pantin : 0 de face, dans le sens des aiguilles d'une
  // montre, comme le .ACT les range.
  int doll_direction = 0;
  // Deux réglages que le fichier retient, et que la fenêtre porte elle-même.
  bool show_empty = true;
  bool compare = false;
  // La boîte du pantin, relevée au dessin, pour que `app` le compose à la
  // bonne taille à l'image suivante.
  float doll_width = 0.0F;
  float doll_height = 0.0F;
  // LE MENU D'UNE PIÈCE, armé au clic droit et ouvert plus bas : l'identifiant
  // d'un surgissant se hache avec la pile d'ids, et celle de la cellule n'est
  // pas celle où le menu se dessine.
  ItemLinkMenu menu;
};

// Ce que la fenêtre demande.
enum class ViewEquipVerb : std::uint8_t {
  kNothing,
  kRefresh,
  kCloneEquip,
  kCloneStat,
  kCloneJob,
};

struct ViewEquipAnswer {
  ViewEquipVerb verb = ViewEquipVerb::kNothing;
  // Un geste sur une pièce — description, lien, menu —, ou le survol pour
  // l'aperçu. Les mêmes que partout où un objet se montre.
  ChatLinkAsk link;
  // La pièce SURVOLÉE, quand `link.hovered_id` en désigne une : l'aperçu se
  // décore de ses cartes et de son raffinement, que l'identifiant ne dit pas.
  std::uint32_t hovered_token = 0;
};

// Dessine la fenêtre si elle est ouverte.
[[nodiscard]] ViewEquipAnswer DrawViewEquipWindow(ViewEquipWindowState& state,
                                                  const ViewEquipView& view, const SkinArt& art);

}  // namespace ro::ui
