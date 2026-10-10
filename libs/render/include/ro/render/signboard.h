#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "ro/formats/image.h"
#include "ro/render/actor_pick.h"
#include "ro/render/font_atlas.h"
#include "ro/render/screen_overlay.h"
#include "ro/vfs/vfs.h"

namespace ro::render {

// ── L'ENSEIGNE QU'UN ACTEUR PORTE AU-DESSUS DE LUI ──────────────────────────
//
// Le titre d'un salon de discussion, ou celui d'une échoppe — de vente comme
// d'achat. Le client en fait une petite FENÊTRE de 140 × 34 pixels, dont il
// peint la surface une fois pour toutes, puis qu'il recale chaque image sur la
// tête de son acteur :
//
//   · un SALON : un cadre en six pièces de bitmap, une queue au milieu du bas,
//     et la porte du salon à gauche — ouverte, ou fermée à clef ;
//   · une ÉCHOPPE : un aplat blanc aux coins arrondis, un filet gris à deux
//     pixels du bord, et la bourse du marchand — ou la pile de pièces de
//     l'acheteur — à gauche.
//
// Le titre s'écrit en noir, aligné à gauche, dans une zone de 106 pixels ; plus
// long, il se coupe en « ... ».
//
// ⚠ CETTE COUCHE NE SAIT PAS CE QU'EST UNE ÉCHOPPE. Elle compose des surfaces et
// pose un titre ; c'est `app` qui choisit laquelle d'après ce que le monde dit.
// C'est la même frontière que partout : `render` ne dépend pas de `sim`.

// LES DIMENSIONS DE LA FENÊTRE, en pixels.
inline constexpr int kSignboardWidthPixels = 140;
inline constexpr int kSignboardHeightPixels = 34;

// LA ZONE DU TITRE : son coin haut-gauche dans la fenêtre, sa taille. Elle
// laisse à gauche la place de l'icône, et trois pixels à droite. ⚠ ELLE ROGNE
// LE TEXTE : ce qui en sort ne se peint pas.
inline constexpr glm::ivec2 kSignboardTitleAt = {31, 8};
inline constexpr int kSignboardTitleRightMarginPixels = 3;
inline constexpr int kSignboardTitleWidthPixels =
    kSignboardWidthPixels - kSignboardTitleAt.x - kSignboardTitleRightMarginPixels;
inline constexpr int kSignboardTitleHeightPixels = 15;

// Ce qui remplace la fin d'un titre trop long.
inline constexpr std::string_view kSignboardEllipsis = "...";

// LE COIN HAUT-GAUCHE DE L'ICÔNE dans la fenêtre. L'icône se pose à sa taille
// propre, son magenta transparent.
inline constexpr glm::ivec2 kSignboardIconAt = {3, 3};

// De combien la fenêtre monte au-dessus de la TÊTE de son acteur, à partir de
// son bord haut. ⚠ L'ÉCHOPPE MONTE DEUX FOIS PLUS que le salon : la bulle d'un
// salon chevauche la tête, l'enseigne d'une échoppe pose sa base dessus.
inline constexpr int kChatRoomSignLiftPixels = 17;
inline constexpr int kShopSignLiftPixels = 34;

// LE CADRE D'UN SALON. Les coins se posent aux quatre coins ; les bords haut et
// bas se répètent au PAS de douze pixels entre deux marges de douze, quelle que
// soit la largeur des pièces — celles de l'installation en font quatorze, et
// chacune recouvre donc le début de la suivante.
inline constexpr int kChatRoomFrameStepPixels = 12;
// La rangée du bas s'arrête six pixels au-dessus du bord de la fenêtre : c'est
// la place de la QUEUE, qui descend jusqu'au bord, centrée.
inline constexpr int kChatRoomTailRoomPixels = 6;

// L'ÉCHOPPE : son aplat, l'aplat de celle que la recherche d'échoppes désigne,
// et le filet intérieur. L'aplat laisse les six pixels du bas vides, comme la
// rangée basse du salon.
inline constexpr std::array<std::uint8_t, 3> kShopSignFill = {255, 255, 255};
inline constexpr std::array<std::uint8_t, 3> kSpotlitShopSignFill = {255, 250, 125};
inline constexpr std::array<std::uint8_t, 3> kShopSignEdge = {195, 195, 195};
inline constexpr int kShopSignEdgeInsetPixels = 2;
inline constexpr int kShopSignBottomRoomPixels = 6;

// Les pièces de l'installation. ⚠ UN HABILLAGE PEUT LES COUVRIR : celles du
// cadre sont redessinées par l'archive du serveur, qui passe avant `data.grf`.
inline constexpr std::string_view kChatRoomFrameTopLeftPath =
    "data\\texture\\유저인터페이스\\sysbox_lu.bmp";
inline constexpr std::string_view kChatRoomFrameTopRightPath =
    "data\\texture\\유저인터페이스\\sysbox_ru.bmp";
inline constexpr std::string_view kChatRoomFrameTopPath =
    "data\\texture\\유저인터페이스\\sysbox_mu.bmp";
inline constexpr std::string_view kChatRoomFrameBottomLeftPath =
    "data\\texture\\유저인터페이스\\sysbox_ld.bmp";
inline constexpr std::string_view kChatRoomFrameBottomRightPath =
    "data\\texture\\유저인터페이스\\sysbox_rd.bmp";
inline constexpr std::string_view kChatRoomFrameBottomPath =
    "data\\texture\\유저인터페이스\\sysbox_md.bmp";
inline constexpr std::string_view kChatRoomFrameTailPath =
    "data\\texture\\유저인터페이스\\sysbox_arr_l.bmp";
inline constexpr std::string_view kOpenRoomIconPath =
    "data\\texture\\유저인터페이스\\chat_open.bmp";
inline constexpr std::string_view kLockedRoomIconPath =
    "data\\texture\\유저인터페이스\\chat_close.bmp";
inline constexpr std::string_view kVendingIconPath = "data\\texture\\유저인터페이스\\shop.bmp";
inline constexpr std::string_view kBuyingIconPath = "data\\texture\\유저인터페이스\\buyingshop.bmp";

// Les sept pièces du cadre d'un salon, leur magenta déjà transparent.
struct ChatRoomFrame {
  formats::Image top_left;
  formats::Image top_right;
  formats::Image top;
  formats::Image bottom_left;
  formats::Image bottom_right;
  formats::Image bottom;
  formats::Image tail;
};

// LA SURFACE D'UN SALON, telle que le client la peint.
//
// 🔴 LES PIÈCES DU CADRE SE RECOPIENT TELLES QUELLES, LEUR TRANSPARENT AVEC :
// une pièce posée après une autre EFFACE ce qu'elle recouvre là où elle est
// transparente. C'est ainsi que la queue ouvre le bord bas du cadre. L'icône,
// elle seule, se pose par-dessus sans rien effacer.
//
// Une pièce absente ne pose rien ; la surface garde sa taille.
[[nodiscard]] formats::Image ComposeChatRoomSurface(const ChatRoomFrame& frame,
                                                    const formats::Image& icon);

// LA SURFACE D'UNE ÉCHOPPE : l'aplat arrondi, le filet, l'icône par-dessus.
[[nodiscard]] formats::Image ComposeShopSurface(const formats::Image& icon,
                                                const std::array<std::uint8_t, 3>& fill);

// LES SURFACES DONT LE JEU A BESOIN, composées au chargement.
//
// ⚠ LEUR ADRESSE EST LEUR IDENTITÉ pour la couche d'écran, qui reconnaît un
// atlas par pointeur pour ne le téléverser qu'une fois : elles doivent rester
// en place aussi longtemps que les quads qui les désignent.
struct SignboardSurfaces {
  formats::Image open_room;
  formats::Image locked_room;
  formats::Image vending;
  formats::Image buying;
  // Celles que la recherche d'échoppes désigne : le même dessin sur fond jaune.
  formats::Image spotlit_vending;
  formats::Image spotlit_buying;
};

// Lit les pièces et compose les six surfaces. Une pièce absente se dit au
// journal et manque au dessin, sans rien empêcher d'autre.
[[nodiscard]] SignboardSurfaces LoadSignboardSurfaces(vfs::Vfs& vfs);

// LE TITRE TEL QU'IL S'ÉCRIT : entier s'il tient dans sa zone, sinon raccourci
// caractère par caractère par la fin, jusqu'à ce que lui et « ... » y tiennent.
[[nodiscard]] std::string FitSignboardTitle(const FontAtlas& font, std::string_view title);

// Le coin haut-gauche de la fenêtre, sur la grille des pixels, pour une tête à
// `head_px` : centrée sur son abscisse, `lift_pixels` au-dessus. ⚠ LES
// COORDONNÉES SE TRONQUENT VERS ZÉRO, comme une conversion entière : au-dessus
// ou à gauche de l'écran, ce n'est pas l'arrondi par défaut.
[[nodiscard]] glm::vec2 SignboardTopLeft(glm::vec2 head_px, int lift_pixels);

// LE CADRE QU'UN CLIC VISE : la fenêtre ENTIÈRE, ses coins et ses six pixels
// transparents du bas compris. Le client teste le rectangle, pas les pixels.
[[nodiscard]] ScreenRect SignboardRect(glm::vec2 top_left);

// La zone où le titre s'écrit, à l'écran : c'est elle que la souris survole
// pour lire en entier un titre coupé.
[[nodiscard]] ScreenRect SignboardTitleRect(glm::vec2 top_left);

// Les quads d'une enseigne : sa surface, puis son titre. `shown_title` est ce
// que rend `FitSignboardTitle`. Une surface vide ne pose que le titre.
[[nodiscard]] std::vector<ScreenQuad> ComposeSignboard(const FontAtlas& font,
                                                       const formats::Image& surface,
                                                       std::string_view shown_title,
                                                       glm::vec2 top_left);

}  // namespace ro::render
