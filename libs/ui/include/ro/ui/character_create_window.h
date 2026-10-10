#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

#include "ro/core/i18n.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// LA CRÉATION D'UN PERSONNAGE : un nom, un sexe, une coupe, une teinte.
//
// ⚠ ET RIEN D'AUTRE, ce qui est court pour un écran de création de RO. Depuis
// PACKETVER 20120307 le serveur pose lui-même les six caractéristiques à un —
// le client d'origine ne les demande donc plus — et depuis 20151001 il lit la
// classe du paquet, mais n'en accepte que deux et l'on ne sait pas laquelle il
// autorise en production. Il ne reste que ces quatre choix.

// LA TAILLE DU CHAMP DU NOM, terminateur compris.
//
// 🔴 ELLE VIENT DU FIL : `CH_MAKE_CHAR` porte le nom sur VINGT-QUATRE octets,
// et ce qui dépasse est coupé à l'encodage. Laisser taper davantage promettrait
// un nom que le serveur ne recevrait pas — et son refus ressemblerait alors à
// « ce nom est déjà pris ».
//
// ⚠ DES OCTETS, PAS DES LETTRES : un nom accentué en compte deux par accent une
// fois converti. C'est la même borne que le serveur applique (`NAME_LENGTH`).
inline constexpr std::size_t kCharacterNameBytes = 24;

// COMBIEN DE COUPES ET DE TEINTES PROPOSER.
//
// 🔴 LE SERVEUR NE LES VALIDE PAS — `char_make_new_char` écrit `hair_style` et
// `hair_color` en base sans les lire — donc la borne est celle de
// l'INSTALLATION, pas du protocole. La fenêtre expose quatre-vingts coupes,
// bien au-delà des vingt-trois de l'écran d'origine, qui ne montre que les
// coupes dont il a un bitmap. Un numéro sans sprite donne une case vide, et
// c'est une réponse juste.
inline constexpr int kHairStyleCount = 80;
inline constexpr int kHairPaletteCount = 251;

// LA GRILLE : huit colonnes pour les coupes, afin que les quatre-vingts
// tiennent sans ascenseur ; les teintes en portent davantage, étant des
// pastilles.
inline constexpr int kHairStyleColumns = 8;
inline constexpr int kHairPaletteColumns = 16;

// LE PANTIN D'APERÇU, en pixels.
inline constexpr float kCreatePreviewWidth = 116.0F;
inline constexpr float kCreatePreviewHeight = 168.0F;

// ⚠ SÉPARÉS, comme partout : ImGui range l'état d'une fenêtre sous son TITRE,
// qui se traduit. Voir `kLoginWindowTitle`.
inline constexpr const char* kCreateWindowTitle = core::TrText("Créer un personnage");
inline constexpr const char* kCreateWindowId = "character_create";

// QUELLE GRILLE EST OUVERTE.
//
// ⚠ EN SURGISSANT, et non dans la fenêtre : quatre-vingts coupes et deux cent
// cinquante et une teintes y tiendraient mal, et le formulaire doit rester
// lisible quand on ne choisit rien. Même montage que l'éditeur de palette.
enum class CreatePicker : std::uint8_t { kNone, kHairStyle, kHairPalette };

// CE QUE LE JOUEUR A COMPOSÉ. Il appartient à l'APPELANT.
struct CharacterCreateState {
  // L'emplacement visé ; moins un quand la fenêtre est fermée.
  int slot = -1;
  std::array<char, kCharacterNameBytes> name = {};
  // 0 = femme, 1 = homme, comme le serveur.
  std::uint8_t sex = 1;
  int hair_style = 1;
  int hair_palette = 0;
  // La direction du pantin d'aperçu, que la molette fait tourner.
  int direction = 0;
  CreatePicker picker = CreatePicker::kNone;
  // LA HAUTEUR DU FORMULAIRE À L'IMAGE PRÉCÉDENTE, en pixels : on ne la connaît
  // qu'après l'avoir dessiné, et l'aperçu se dessine avant. Une image de retard,
  // qui ne se voit pas — la fenêtre ne change plus de taille une fois ouverte.
  float form_height_px = 0.0F;

  [[nodiscard]] bool open() const noexcept { return slot >= 0; }
};

// CE QUE LA GRILLE OUVERTE DEMANDE À `app`, écrit ICI pendant le dessin et lu
// à l'image SUIVANTE.
//
// ⚠ UNE IMAGE DE RETARD, ASSUMÉE : on ne sait quelles cases sont visibles
// qu'en les parcourant. Une case ouverte reste vide le temps d'une image, ce
// qui ne se voit pas. Même mécanique que `PalettePickerState`.
struct CreatePickerRequest {
  CreatePicker open = CreatePicker::kNone;
  int first = 0;
  int count = 0;
  // Le côté d'une case, en pixels d'écran : la taille à laquelle `app` compose.
  float cell = 0.0F;
};

// CE QUE LA FENÊTRE MONTRE, posé par `app` à chaque image.
struct CharacterCreateView {
  // Le pantin d'aperçu, composé avec ce que l'état porte. Inutilisable tant
  // que les sprites ne répondent pas.
  SkinImage doll;
  // Les vignettes de la grille ouverte, celle du numéro `thumbnails_first` en
  // tête. Une image inutilisable est une case VIDE — tous les numéros n'ont
  // pas de sprite, et c'est une réponse.
  std::span<const SkinImage> thumbnails;
  int thumbnails_first = 0;
  // La demande est partie ; on attend la réponse du serveur.
  bool pending = false;
  // Le motif du dernier refus, EN CLAIR et déjà traduit, ou vide.
  //
  // ⚠ IL VIENT DE `ro::net` : c'est la couche qui parle au serveur qui sait
  // lire ses codes.
  std::string refusal;
};

enum class CharacterCreateVerb : std::uint8_t {
  kNothing,
  // Envoyer la demande avec ce que l'état porte.
  kSubmit,
  // Refermer sans rien envoyer.
  kCancel,
};

struct CharacterCreateAction {
  CharacterCreateVerb verb = CharacterCreateVerb::kNothing;
  // Ce que la grille ouverte veut voir composé à l'image suivante.
  CreatePickerRequest picker;
  // LA BOÎTE DU PANTIN D'APERÇU, en pixels d'écran, relevée au dessin.
  //
  // ⚠ ELLE REMONTE DE LA FENÊTRE parce qu'elle dépend de l'ÉCHELLE de
  // l'interface, que `app` ne calcule pas — et la recalculer de son côté la
  // ferait diverger d'un pixel le jour où le joueur change cette échelle.
  // Même image de retard que la grille, et elle ne se voit pas davantage.
  float doll_width = 0.0F;
  float doll_height = 0.0F;
};

// DESSINE LA FENÊTRE DE CRÉATION. Sans effet quand l'état est fermé.
[[nodiscard]] CharacterCreateAction DrawCharacterCreate(CharacterCreateState& state,
                                                        const CharacterCreateView& view,
                                                        const SkinArt& art);

}  // namespace ro::ui
