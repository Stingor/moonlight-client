#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>

#include "ro/ui/chat_link.h"
#include "ro/ui/chat_markup.h"
#include "ro/ui/chat_rich_art.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// LE FIL DE DISCUSSION, REPLIÉ À LA MAIN.
//
// ⚠ CE N'EST PAS UNE BOUCLE DE `TextWrapped`, et trois choses l'interdisent.
//
//   1. LA TAILLE DU TEXTE EST EXPLICITE, de la mesure au dessin. C'est ce qui
//      rend le log indépendant de l'échelle de la fenêtre : celle-ci n'habille
//      que les onglets, les boutons et la saisie, tandis que le fil a SON
//      réglage de taille. `TextWrapped` prendrait la police courante et les deux
//      réglages n'en feraient plus qu'un.
//   2. LE PAS VERTICAL EST PLUS SERRÉ QUE CELUI D'IMGUI, qui est très aéré pour
//      du chat. Et il doit être EXACT : toute rangée visuelle fait un multiple
//      entier du pas — un seul pour du texte, plusieurs pour une vignette —,
//      c'est ce qui permet au défilement de tomber en face du texte au lieu de
//      couper une ligne en deux.
//   3. PLUSIEURS COULEURS SE SUIVENT SUR UNE MÊME RANGÉE — l'horodatage gris
//      devant la phrase, et demain les fragments d'une ligne à liens. ImGui ne
//      sait pas enchaîner deux couleurs sur un texte qui se replie.
//
// 🔴 UNE LIGNE SE MET EN PAGE UNE FOIS, ET NON À CHAQUE IMAGE — voir
// `ChatLogLayout`. La conversation garde mille lignes PAR GENRE, et l'onglet qui
// montre les dix genres les additionne : remettre en page le fil entier à chaque
// image coûte 4,2 ms en Release pour dix mille lignes, le quart d'une image à
// 60 i/s, pour n'en peindre qu'une trentaine. Par le cache, 0,02 ms.
//
// ⚠ CE CHIFFRE SE REVÉRIFIE, il ne se cite pas : l'essai est là, masqué par
// défaut, et se relance d'un `ro_tests "[mesure]"`. Le chat est précisément
// l'endroit où le coût par image s'est déjà payé en gels.

// L'horodatage, en gris : il se lit quand on le cherche, et s'oublie sinon.
inline constexpr ImU32 kChatStampColor = IM_COL32(150, 150, 150, 255);
// L'étiquette du mode diagnostic. Assez vive pour qu'on la distingue du texte
// — elle n'est là que le temps d'une recherche de panne, pas pour se fondre.
inline constexpr ImU32 kChatTagColor = IM_COL32(120, 200, 255, 255);
// Ce qu'une ligne sans couleur reçoit. ⚠ ZÉRO VEUT DIRE « L'ORDINAIRE », pas du
// noir — sur un fond sombre, du noir serait un trou.
inline constexpr ImU32 kChatPlainColor = IM_COL32(255, 255, 255, 255);
// Les liens et leurs gestes vivent dans `chat_link.h` : le fil n'est pas la
// seule surface à en montrer, la barre de saisie en montre aussi.

// De combien le pas des lignes démarre PLUS SERRÉ que la taille du texte.
//
// ⚠ CE N'EST PAS UNE MARGE NÉGATIVE ARBITRAIRE : c'est le calage du skin, repris
// tel quel. L'interligne réglable s'ajoute à ce pas-là, et l'accord entre le
// texte et sa grille en dépend — le changer décale le fil de son ascenseur.
inline constexpr float kChatLineTightening = 6.0F;
// Le pas ne descend pas sous cette valeur : un interligne dégénéré rendrait la
// division par lui absurde, et le défilement avec.
inline constexpr float kChatSmallestLineStep = 4.0F;

// Une ligne prête à lire.
//
// ⚠ `text` EST UNE VUE, et ne vit que le temps de l'image : elle regarde la
// conversation du monde, qu'on ne recopie pas pour rien.
struct ChatLineView {
  std::string_view text;
  // CE QUE LE MODE DIAGNOSTIC MONTRE EN TÊTE, ou vide. C'est `app` qui le
  // compose : `ui` ne connaît pas les genres, elle ne fait que le peindre —
  // comme l'horodatage, et au même endroit.
  std::string_view tag;
  // Le numéro de son TYPE, « t16 », peint juste après sous la même forme : il
  // relie la ligne à la case de filtre qui la laisse passer. Vide hors
  // diagnostic.
  std::string_view type_tag;
  // ZÉRO veut dire « la couleur ordinaire », pas du noir.
  ImU32 color = 0;
  // L'heure à laquelle la ligne a été dite. ⚠ PAS UNE DURÉE : le joueur lit
  // « quand », jamais « il y a combien de temps ».
  std::uint8_t hour = 0;
  std::uint8_t minute = 0;
  std::uint8_t second = 0;
  // LES MORCEAUX QUI DÉSIGNENT UN OBJET, dans l'ordre et sans chevauchement.
  // Vide pour la quasi-totalité des lignes.
  std::span<const ChatLinkSpan> links;
  // L'IDENTITÉ DE LA LIGNE pour `ChatLogLayout` : CROISSANTE le long du fil, et
  // la même tant que la ligne se montre à l'identique — même texte, même
  // étiquette, mêmes morceaux. Zéro : aucune, et la ligne se remet en page à
  // chaque image.
  std::uint64_t id = 0;
  // LE TEXTE RICHE : les styles en suites d'octets du texte, et les images
  // posées dedans. Vides pour la quasi-totalité des lignes.
  std::span<const ChatStyleRun> styles;
  std::span<const ChatInlineImage> images;
  // La ligne nous nomme : un bandeau la signale.
  bool mention = false;
  // Où commence ce que la ligne DIT, après son auteur, en octets du texte.
  std::uint32_t body_offset = 0;
  // SA PLACE DANS LE FIL, pour compter ce qui est arrivé pendant qu'on lisait
  // plus haut : CROISSANTE, et stable quand la ligne change d'aspect — à la
  // différence de `id`. Zéro : inconnue, et la ligne ne compte jamais pour
  // nouvelle.
  std::uint64_t seq = 0;
};

// Ce que le joueur a demandé au fil.
struct ChatLogAction {
  // Un geste sur un lien, ou rien.
  ChatLinkAsk link;
  // Ctrl + molette : un cran de taille du texte, plus ou moins ; zéro sinon.
  int zoom_steps = 0;
};

// L'apparence du fil. Elle vient du joueur, par `app`.
struct ChatLogStyle {
  // La taille du texte du LOG, en pixels. Elle ne suit pas l'échelle de la
  // fenêtre — voir l'en-tête.
  float font_size = 0.0F;
  // L'interligne qui s'ajoute au pas, en pixels.
  float line_gap = 0.0F;
  bool timestamps = false;
  // Le catalogue du vote shop, pour l'entrée du menu d'un item.
  std::span<const VoteShopOffer> vote_shop;
  // De quoi peindre le texte riche, posé à chaque image. Nul : le texte nu.
  const ChatRichArt* rich = nullptr;
  // Le menu d'un lieu offre le `@warp` du staff.
  bool staff_warp = false;
  // La taille du texte en pourcentage, montrée un instant après un zoom.
  int font_percent = 100;
};

// Le pas vertical d'une rangée, pour ce style-là. Public parce que la chatbox en
// a besoin AVANT de dessiner : c'est lui qui quantifie la hauteur de la fenêtre.
[[nodiscard]] float ChatLineStep(const ChatLogStyle& style);
// Ce que le dernier glyphe dépasse SOUS le pas de sa rangée.
//
// ⚠ L'OUBLIER ROGNE LA LIGNE LA PLUS RÉCENTE, en permanence : le cumul des pas
// s'arrête au pas de la dernière ligne, pas sous son glyphe, et c'est ce cumul
// qui borne le défilement.
[[nodiscard]] float ChatLineOverhang(const ChatLogStyle& style);

// La hauteur que ces lignes prendront, repli compris, sur cette largeur-là.
//
// ⚠ C'EST LE MÊME MOTEUR QUE LE DESSIN, sans peinture — pas une seconde
// implémentation. Deux moteurs divergeraient au premier cas limite retouché
// d'un seul côté (le mot trop large, le saut de ligne), et la fenêtre
// réserverait alors une hauteur qui ne serait plus celle du texte.
//
// Emploie la police COURANTE d'ImGui, comme le dessin : il faut donc un
// contexte, mais aucune fenêtre.
[[nodiscard]] float ChatLogHeight(std::span<const ChatLineView> lines, const ChatLogStyle& style,
                                  float wrap_width);

// QUELLE LIGNE PORTE UN SPOILER RÉVÉLÉ : par son identité quand elle en a une,
// sinon par son texte même.
//
// 🔴 COMPARÉE CHAMP À CHAMP, JAMAIS HACHÉE : deux lignes dont l'empreinte
// coïnciderait se révéleraient l'une l'autre.
struct ChatSpoilerKey {
  std::uint64_t line_id = 0;
  // Vide quand `line_id` n'est pas nul.
  std::string line_text;
  bool operator==(const ChatSpoilerKey&) const = default;
};
// Combien de révélations un fil retient ; les plus anciennes partent.
inline constexpr std::size_t kMostRevealedSpoilers = 256;

// CE QU'UN FIL A DÉJÀ MIS EN PAGE : la hauteur de chacune de ses lignes.
//
// 🔴 LE FIL EMPILE SES LIGNES, et c'est ce qui rend le cache juste : la hauteur
// de chacune suffit à placer toutes les autres. Une image ne met en page que
// les lignes qu'elle voit pour la première fois, puis ne peint que celles qui se
// voient.
//
// ⚠ UNE HAUTEUR DÉPEND DE LA POLICE, de la taille du texte, de l'interligne, de
// l'horodatage et de la largeur : tout se rejette dès que l'un d'eux change. La
// police se reconnaît à son adresse ET à son `FontId`, que l'atlas ne réattribue
// jamais — une police reconstruite peut retomber à la même adresse.
//
// ⚠ ELLE DÉPEND AUSSI DE SES IMAGES, que l'identité de la ligne ne décrit pas :
// une vignette qui arrive passe du carré d'attente à ses proportions. Le cache
// garde donc, par ligne, ce qui fixe la place de chaque image, et le compare
// champ à champ.
//
// ⚠ UN CACHE PAR FIL, tenu par qui distribue les identifiants des lignes : il
// vit exactement aussi longtemps qu'eux gardent leur sens.
class ChatLogLayout {
 public:
  // Met en page ce qui manque, et rend la hauteur du fil entier — débord du
  // dernier glyphe compris, comme `ChatLogHeight`, dont c'est le même moteur.
  //
  // Emploie la police COURANTE d'ImGui : il faut un contexte, mais aucune fenêtre.
  float Update(std::span<const ChatLineView> lines, const ChatLogStyle& style, float wrap_width);

  // La hauteur de chaque ligne, dans l'ordre de la dernière mise à jour.
  [[nodiscard]] std::span<const float> heights() const noexcept { return heights_; }
  // Combien de lignes la dernière mise à jour a dû mesurer : ce que le cache
  // n'a pas su rendre.
  [[nodiscard]] std::size_t measured() const noexcept { return measured_; }

  // LA DERNIÈRE LIGNE LUE, par son `seq` : collé en bas, le fil la voit ;
  // décollé, ce qui la suit compte pour nouveau.
  [[nodiscard]] std::uint64_t seen_through() const noexcept { return seen_through_; }
  void See(std::uint64_t seq) noexcept;
  // Les spoilers que le joueur a révélés d'un clic.
  [[nodiscard]] bool Revealed(const ChatSpoilerKey& key) const;
  void Reveal(ChatSpoilerKey key);

  // CE QUE LE MENU D'UNE LIGNE COPIERA, retenu au clic droit : la ligne a pu
  // partir, ou changer de rang, avant que le joueur choisisse.
  void HoldCopy(std::string whole_line, std::string body_only);
  [[nodiscard]] bool holds_copy() const noexcept { return holds_copy_; }
  [[nodiscard]] const std::string& held_line() const noexcept { return held_line_; }
  [[nodiscard]] const std::string& held_body() const noexcept { return held_body_; }

 private:
  // Ce dont toutes les hauteurs dépendent, hors des lignes elles-mêmes.
  struct Key {
    const ImFont* font = nullptr;
    ImGuiID font_id = 0;
    float font_size = 0.0F;
    float line_gap = 0.0F;
    bool timestamps = false;
    float wrap_width = 0.0F;
    // Le texte riche : ses polices, la hauteur des vignettes, et ce qui se
    // peint en image plutôt qu'en texte.
    const ImFont* bold = nullptr;
    ImGuiID bold_id = 0;
    const ImFont* italic = nullptr;
    ImGuiID italic_id = 0;
    const ImFont* code = nullptr;
    ImGuiID code_id = 0;
    float picture_pixels = 0.0F;
    bool emote_images = false;
    bool item_icons = false;
    bool operator==(const Key&) const = default;
  };
  // CE QUI FIXE LA PLACE D'UNE IMAGE : son genre, si elle a de quoi se peindre,
  // ses proportions et son état.
  struct ImageFacts {
    ChatInlineKind kind = ChatInlineKind::kGameEmote;
    std::uint16_t emote_action = 0;
    // Une emote présente dans le sprite, ou une texture posée.
    bool paintable = false;
    float width_px = 0.0F;
    float height_px = 0.0F;
    ChatPictureState state = ChatPictureState::kUnknown;
    bool operator==(const ImageFacts&) const = default;
  };
  [[nodiscard]] static ImageFacts FactsOf(const ChatInlineImage& image, const ChatRichArt& rich);
  // Les images de `line` sont-elles celles que le cache tient de `first` à
  // `first + count` ?
  [[nodiscard]] bool SameImages(const ChatLineView& line, const ChatRichArt& rich,
                                std::uint32_t first, std::uint32_t count) const;

  Key key_;
  std::vector<std::uint64_t> ids_;
  std::vector<float> heights_;
  // Les images de chaque ligne, à la suite : la ligne `n` a les siennes de
  // `image_first_[n]` à `image_first_[n + 1]`.
  std::vector<std::uint32_t> image_first_;
  std::vector<ImageFacts> images_;
  // Les tampons de la mise à jour suivante, échangés avec les précédents : ils
  // gardent leur capacité d'une image à l'autre.
  std::vector<std::uint64_t> next_ids_;
  std::vector<float> next_heights_;
  std::vector<std::uint32_t> next_image_first_;
  std::vector<ImageFacts> next_images_;
  std::size_t measured_ = 0;
  std::uint64_t seen_through_ = 0;
  // De la plus ancienne révélation à la plus récente.
  std::deque<ChatSpoilerKey> revealed_;
  bool holds_copy_ = false;
  std::string held_line_;
  std::string held_body_;
};

// Dessine le fil dans la région courante, sur `height` pixels, et gère son
// défilement. À appeler entre le `Begin` de la chatbox et sa rangée de saisie.
//
// ⚠ L'HABILLAGE NE SERT QU'À L'ASCENSEUR — la seule pièce que la chatbox
// partage avec les fenêtres du jeu. Tout le reste de son cadre est peint à
// partir de couleurs, pas de bitmaps.
//
// `layout` est le cache de CE fil ; nul, le fil entier se remet en page à chaque
// image.
[[nodiscard]] ChatLogAction DrawChatLog(std::span<const ChatLineView> lines,
                                        const ChatLogStyle& style, float height, const SkinArt& art,
                                        ChatLogLayout* layout);

// CE QUE « COPIER LA LIGNE » MET DANS LE PRESSE-PAPIERS : l'horodatage s'il est
// montré, les étiquettes et le texte ; ou, `body_only`, le texte à partir de
// `body_offset`. Un spoiler y reprend ses `||…||` : collé ailleurs, il reste
// caché.
[[nodiscard]] std::string ChatLineCopyText(const ChatLineView& line, bool timestamps,
                                           bool body_only);

}  // namespace ro::ui
