#pragma once

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "ro/ui/chat_input.h"
#include "ro/ui/chat_link.h"
#include "ro/ui/chat_log.h"
#include "ro/ui/chat_strip.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// UNE CONVERSATION 1:1 : la fenêtre « With Nom » du client d'origine, avec son
// fil et SA saisie.
//
// 🔴 LA CIBLE EST FIGÉE. Ce qu'on tape ici part à ce correspondant et à nul
// autre : pas de sigil, pas de mode, pas de case « Pseudo ». C'est tout
// l'intérêt d'une fenêtre à part — on ne peut pas s'y tromper de destinataire.
//
// 🔴 ET RIEN N'Y ENTRE DANS L'HISTORIQUE DE FRAPPE : il est partagé avec la
// barre de la conversation, et une flèche du haut y ressortirait un message
// privé dans une ligne qui part en public.
//
// ⚠ LE CLIC GAUCHE SUR LE PSEUDO DU CORRESPONDANT RAMÈNE LE CLAVIER ICI, et non
// dans la barre de la conversation : c'est déjà à lui qu'on écrit, et la
// réponse partirait sinon d'une autre fenêtre que celle qu'on regarde.

// LES MESURES DU CLIENT, en pixels d'interface : la taille d'ouverture, et les
// bornes entre lesquelles elle se redimensionne PAR PAS — le bord saute de
// trente-deux pixels en trente-deux.
inline constexpr float kWhisperFirstWidthPixels = 280.0F;
inline constexpr float kWhisperFirstHeightPixels = 120.0F;
inline constexpr float kWhisperMostWidthPixels = 512.0F;
inline constexpr float kWhisperMostHeightPixels = 384.0F;
inline constexpr float kWhisperSizeStepPixels = 32.0F;
// Le décalage de chaque fenêtre neuve sur la précédente, en cascade.
inline constexpr float kWhisperCascadePixels = 17.0F;

// CE QUE LA FENÊTRE MONTRE, recomposé à chaque image.
struct WhisperWindowView {
  // Le correspondant : il nomme la fenêtre, donc sa place à l'écran.
  std::string_view peer;
  // Le titre, déjà composé : « With  Nom », sa guilde, la marque d'un non-lu.
  std::string_view title;
  // Le fil, déjà trié et décoré.
  std::span<const ChatLineView> lines;
  ChatLogStyle log_style;
  ChatLogLayout* layout = nullptr;
  // Son rang parmi les fenêtres ouvertes : il décale la première ouverture.
  int cascade = 0;
  // La liaison est-elle ouverte ? Fermée, la saisie se grise.
  bool can_send = true;
  // LES LIENS DÉJÀ POSÉS DANS SA SAISIE, dans l'ordre de pose : leurs pastilles.
  std::span<const ChatInputLink> input_links;
  // La palette de sa saisie : les émoticones s'y écrivent en `:nom:`, les emoji
  // s'y insèrent. ⚠ RIEN NE PART COMME ÉMOTION DU JEU : seuls les voisins la
  // verraient, jamais le correspondant.
  std::span<const ChatEmotion> emotions;
  std::span<const ChatEmojiGroup> emojis;
  // Le catalogue du vote shop, pour le menu d'une pastille.
  std::span<const VoteShopOffer> vote_shop;
};

struct WhisperWindowState {
  // L'interrupteur que la pile d'Échap baisse : baissé, la fenêtre rend sa
  // fermeture comme à la croix.
  bool open = true;
  // SA SAISIE, de la même espèce que celle de la conversation : un lien et un
  // emoji s'y posent par le même dépôt.
  ChatInputState input;
  // Le geste en cours dans sa palette d'émoticones.
  ChatStripState picker;
  // Demandé par un geste : passer devant, et donner le clavier à la saisie.
  bool raise = false;
  // Le clavier à la saisie, à l'image suivante.
  bool wants_focus = false;
};

struct WhisperWindowAction {
  // La ligne validée par Entrée, déjà vidée de la saisie.
  std::optional<std::string> said;
  // Un geste sur un lien du fil. ⚠ LE CLIC GAUCHE SUR UN PSEUDO N'Y EST JAMAIS :
  // la fenêtre l'a déjà joué en rendant le clavier à sa saisie.
  ChatLinkAsk link;
  // La croix.
  bool closed = false;
  // La fenêtre avait le focus : son non-lu est lu.
  bool focused = false;
  // Un geste sur une pastille de SA saisie.
  ChatLinkAsk input_link;
  // ⚠ TOUJOURS MOINS DE ZÉRO : la palette de la 1:1 écrit le `:nom:` au lieu
  // d'envoyer l'émotion, que le correspondant ne verrait pas.
  int chose_emotion = -1;
  // Sa saisie a le clavier : c'est la dernière active.
  bool input_active = false;
};

// L'IDENTITÉ IMGUI DE LA FENÊTRE D'UN CORRESPONDANT : c'est elle qui porte sa
// place à l'écran, et par elle que la pile d'Échap la retrouve.
[[nodiscard]] std::string WhisperWindowId(std::string_view peer);

[[nodiscard]] WhisperWindowAction DrawWhisperWindow(WhisperWindowState& state,
                                                    const WhisperWindowView& view,
                                                    const SkinArt& art);

}  // namespace ro::ui
