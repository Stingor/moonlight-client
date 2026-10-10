#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "ro/ui/chat_link.h"
#include "ro/ui/item_link_cell.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// ── LA FICHE DU FAMILIER ───────────────────────────────────────────────────
//
// Une fenêtre FLOTTANTE et autonome : on la regarde en jouant, souvent pour la
// seule faim, et elle reste ouverte dans un coin. Elle porte d'un seul tenant ce
// que le jeu répartissait entre la fiche, son menu de commandes et la fenêtre
// d'évolution : le sprite, le nom (qui se renomme une fois), l'espèce et le
// niveau, l'accessoire, les jauges de faim et d'intimité, les cinq ordres,
// l'auto-alimentation et les évolutions, dépliées dans un panneau.
//
// Elle se dessine en FLUX et colle à son contenu : confirmations, liste
// d'évolutions et panneau déplié changent sa hauteur de beaucoup.
//
// Les refus que le client opposait sans un mot sont DITS : l'accessoire absent
// grise son bouton, l'évolution impossible dit pourquoi, le nom déjà changé ne
// se propose plus.

// Le mot de l'intimité, traduit au dessin.
enum class PetIntimacyLabel : std::uint8_t {
  kHateWithPassion,
  kHate,
  kAwkward,
  kShy,
  kNeutral,
  kCordial,
  kLoyal,
  kUnknown,
};

// Les ordres que la fiche donne au familier.
enum class PetOrder : std::uint8_t {
  kFeed,
  kPerform,
  kReturnToEgg,
  kUnequipAccessory,
};

// CE QUE LA SECTION D'ÉVOLUTION A À DIRE.
enum class PetEvolutionShown : std::uint8_t {
  // L'intimité n'atteint pas le seuil où le client propose l'évolution.
  kIntimacyTooLow,
  // Le sac ne porte pas l'œuf du familier sorti.
  kEggUnknown,
  // L'œuf n'a aucune recette.
  kNoRecipe,
  // La liste des cibles suit.
  kTargets,
};

// POURQUOI LE BOUTON « FAIRE ÉVOLUER » EST GRISÉ, dans l'ordre du serveur.
enum class PetEvolutionBlock : std::uint8_t {
  kNone,
  kIntimacy,
  kAccessory,
  kMaterials,
};

struct PetEvolutionTargetView {
  std::uint32_t egg_id = 0;
  // L'espèce de l'œuf cible.
  std::string species;
};

struct PetEvolutionMaterialView {
  std::uint32_t item_id = 0;
  SkinImage icon;
  std::string name;
  std::int64_t owned = 0;
  std::int64_t needed = 0;
};

// LE PANNEAU DÉPLIÉ : les deux espèces sprite à sprite, les matériaux et ce
// qu'on en possède, le refus dit avant le clic.
struct PetEvolutionPanelView {
  // L'œuf cible que ce panneau décrit ; zéro tant qu'aucun n'est composé.
  std::uint32_t target_egg_id = 0;
  std::uint32_t source_egg_id = 0;
  SkinImage source_sprite;
  SkinImage target_sprite;
  std::string source_species;
  std::string target_species;
  std::vector<PetEvolutionMaterialView> materials;
  PetEvolutionBlock block = PetEvolutionBlock::kNone;
};

struct PetWindowView {
  // Le familier est sorti, et son acteur est là à cette image.
  bool present = false;

  SkinImage sprite;
  std::string name;
  bool renameable = false;
  // Le plus long nom que le serveur garde, en octets SUR LE FIL.
  std::size_t name_max_bytes = 0;
  std::string species;
  int level = 0;
  // Vide quand le familier n'en porte pas.
  std::string accessory;
  bool has_accessory = false;

  int hunger = 0;
  int previous_hunger = 0;
  int hunger_most = 0;
  // La faim est au palier où le familier perd de l'intimité.
  bool starving = false;
  int intimacy = 0;
  int intimacy_most = 0;
  PetIntimacyLabel intimacy_label = PetIntimacyLabel::kUnknown;

  // L'œuf porté a droit à l'auto-alimentation : la case n'existe qu'alors.
  bool auto_feed_listed = false;
  // L'état que le SERVEUR a confirmé.
  bool auto_feed = false;

  PetEvolutionShown evolution = PetEvolutionShown::kEggUnknown;
  // Le seuil d'intimité où le client propose l'évolution (strictement
  // au-dessus), et le minimum que le serveur exige.
  int evolution_offered_above = 0;
  int evolution_server_minimum = 0;
  std::vector<PetEvolutionTargetView> evolution_targets;
  PetEvolutionPanelView panel;

  // Ce qui refuse « nourrir » à deux instants du même geste : au clic, une
  // lettre en cours de rédaction ; à la confirmation, un échange en cours.
  bool mail_writing = false;
  bool trading = false;

  // LA NOURRITURE QU'IL MANGE, et combien l'inventory en porte : sans elle, le
  // repas est refusé. `food_known` faux quand la table de l'installation ne la
  // nomme pas.
  bool food_known = false;
  // L'item que la ligne désigne : elle se clique comme un lien de la
  // conversation.
  std::uint32_t food_item_id = 0;
  SkinImage food_icon;
  std::string food_name;
  std::int64_t food_in_bag = 0;
};

// L'état de la fenêtre, et ses deux réglages.
struct PetWindowState {
  bool open = false;
  // UNE TOUCHE OU L'ICÔNE DU MENU DEMANDE LA BASCULE. ⚠ CE N'EST PAS ELLES QUI
  // OUVRENT : la fiche se redemande au serveur en même temps, et c'est `app`,
  // qui tient la connexion, qui joue la demande.
  bool toggle_requested = false;
  bool focus_asked = false;

  bool feed_confirming = false;
  bool return_confirming = false;

  bool renaming = false;
  // LARGE EXPRÈS : la borne porte sur la forme envoyée, et se dit après coup.
  std::array<char, 64> rename_buffer = {};

  // Le retour de la dernière action, sous les boutons ; rouge pour un refus.
  std::string status;
  bool status_refused = false;

  // L'œuf CIBLE du panneau déplié ; zéro, replié.
  std::uint32_t evolution_target = 0;

  // Les cadres des sprites, en pixels d'écran, relevés au dessin : `app` y
  // compose les images de l'image suivante. Zéro avant le premier dessin.
  float sprite_box_px = 0.0F;
  float evolution_box_px = 0.0F;

  // Le menu d'un lien, ouvert HORS de la cellule qui l'a demandé : une cellule
  // change la pile d'ids, et le surgissant ne s'y retrouverait pas.
  ItemLinkMenu link_menu;

  // ── LES RÉGLAGES ─────────────────────────────────────────────────────────
  // Recopiés à chaque image depuis la page de réglages, qui les garde.
  //
  // Demander avant de remettre le familier dans son œuf : le jeu ne le
  // demandait pas, et l'erreur range le familier pour de bon.
  bool confirm_return_to_egg = true;
  // Animer le sprite, ou le tenir sur sa première image.
  bool animate_sprite = true;
};

struct PetWindowAction {
  std::optional<PetOrder> order;
  // Le nom à envoyer, déjà vérifié contre la borne.
  std::optional<std::string> rename;
  // L'auto-alimentation demandée.
  std::optional<bool> auto_feed;
  // L'œuf CIBLE de l'évolution demandée.
  std::uint32_t evolve_to = 0;
  // « Nourrir » refusé au clic : une lettre est en cours de rédaction.
  bool feed_refused_mail = false;
  // « Nourrir » refusé à la confirmation : un échange est en cours.
  bool feed_refused_trade = false;
  // Un geste sur l'item d'un lien du panneau — description, menu, chat.
  ChatLinkAsk link;
  // La puce de la barre de titre : ouvrir la page de réglages de la fiche.
  bool settings_asked = false;
};

// Ouvre la fiche, panneau replié et confirmations retirées.
void OpenPetWindow(PetWindowState& state);

[[nodiscard]] PetWindowAction DrawPetWindow(PetWindowState& state, const PetWindowView& view,
                                            const SkinArt& art);

}  // namespace ro::ui
