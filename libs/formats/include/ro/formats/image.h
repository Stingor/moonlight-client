#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "ro/core/error.h"

namespace ro::formats {

// Le format du fichier d'où sort une image décodée.
enum class ImageFile : std::uint8_t { kOther, kBmp, kTga, kJpeg, kPng };

// Image décodée, en RGBA8.
//
// La rangée 0 est le HAUT de l'image. BMP comme TGA rangent le plus souvent
// leurs pixels de bas en haut ; les décodeurs remettent dans l'ordre naturel
// une fois pour toutes, plutôt que de laisser chaque appelant se demander dans
// quel sens il lit.
struct Image {
  std::int32_t width = 0;
  std::int32_t height = 0;
  std::vector<std::uint8_t> pixels;
  // LE FICHIER D'ORIGINE, posé par `DecodeImage`. Le jeu ne découpe pas une
  // BMP comme une TGA : voir `ro::render::SceneCutoutOf`.
  ImageFile file = ImageFile::kOther;

  [[nodiscard]] bool empty() const noexcept { return pixels.empty(); }
};

// Rend transparent le magenta pur (255, 0, 255).
//
// Convention du client, pas du format : RO s'en sert comme couleur de
// transparence. La fonction est séparée du décodage pour que les parseurs
// restent des lecteurs de format, et que le choix de l'appliquer revienne à
// l'appelant.
//
// ⚠ SEUL L'ALPHA TOMBE : le pixel effacé GARDE son magenta. Tout ce qui mélange
// deux texels le ramènerait en frange rose au bord de l'image ; c'est le
// TÉLÉVERSEMENT qui repeint ces pixels de la couleur opaque la plus proche,
// parce que c'est lui qui choisit le filtrage. Voir
// `ro::render::BleedTransparentEdges`.
void ApplyMagentaColorKey(Image& image);

// La transparence des textures du SOL et du DÉCOR, telle que le jeu les
// téléverse : en seize bits, cinq par canal. Un pixel devient transparent dès
// que ses cinq bits de poids fort tombent sur le magenta — rouge et bleu à 248
// ou plus, vert à 7 ou moins —, et il devient NOIR.
//
// ⚠ LE NOIR EST VOULU : c'est la valeur que le jeu écrit pour un texel vide.
// Le filtrage bilinéaire le mêle au bord de la découpe, et le seuil de rejet
// n'en laisse passer que la frange la plus opaque — voir
// `ro::render::kCutoutAlphaReference`. Rien ne doit donc repeindre ces texels.
void ApplyTextureColorKey(Image& image);

// La transparence des textures d'un FICHIER D'EFFET (`.str`), telle que le jeu
// les téléverse : en seize bits, avec la couleur clé ET LE NOIR.
//
// Une image sans canal alpha — BMP, JPEG — passe en cinq bits par canal ; une
// image qui en a un — TGA, PNG — en quatre, alpha compris. Un texel y devient
// transparent quand ses bits de poids fort tombent :
//   · sur le magenta : rouge et bleu pleins, vert nul ;
//   · sur le NOIR : rouge, vert et bleu nuls — sous huit à cinq bits, sous
//     seize à quatre. Son alpha n'y change rien : un noir OPAQUE part ;
//   · pour une image à canal alpha, sur un alpha nul : sous seize.
//
// 🔴 LE NOIR NE PART QUE DANS UN `.str`. Le jeu ne le demande que pour les
// textures de ses fichiers d'effet ; celles des effets procéduraux, du sol ou
// de l'interface gardent leur noir. C'est ce qui ôte son fond au bouclier de
// Smite, dont la couche se peint « un vers zéro », en remplaçant ce qui est
// dessous : le noir d'une BMP y serait un carré opaque.
//
// Seul l'alpha tombe, comme pour `ApplyMagentaColorKey` : c'est le
// téléversement qui repeint ces texels de la couleur opaque voisine.
void ApplyEffectFileColorKey(Image& image);

// La même transparence pour une texture d'EFFET PROCÉDURAL ou de classe : la
// couleur clé et l'alpha nul à la précision de seize bits, mais SANS le noir,
// que le jeu garde peint hors d'un `.str`.
void ApplyEffectTextureColorKey(Image& image);

// LA PROFONDEUR D'UNE TEXTURE D'EFFET, telle que le jeu la téléverse : seize
// bits par texel. Une image sans canal alpha garde cinq bits par canal et un
// bit d'alpha ; une image qui en a un en garde quatre, alpha compris. Chaque
// canal est relu comme la carte le fait, ses bits de poids fort recopiés dans
// ceux de poids faible.
//
// CE QUE ÇA SE VOIT. Sur les 22 291 textures que nomment les `.str`, l'écart
// au huit bits atteint 7 sur 255 en cinq bits, 15 en quatre — alpha compris —,
// et un dégradé lisse s'y fait en MARCHES de 8 ou de 17 : 9,3 % des voisins
// presque égaux d'une BMP, 2,9 % d'une TGA, tombent de part et d'autre d'un
// palier. Une marche de 17, soit près de 7 %, se lit en anneaux dans une fumée
// ou une lueur douce.
void QuantizeToEffectTextureDepth(Image& image);

// DÉTOURE UNE IMAGE PEINTE SUR DU PAPIER BLANC — l'illustration de collection
// d'un item : le fond devient transparent, et l'image peut se poser sur
// n'importe quel panneau, sombre compris.
//
// Trois parts, et l'image d'origine se retrouve à l'identique posée sur du
// blanc :
//   · le FOND, ce qui reste presque blanc en partant des bords, tombe ;
//   · le LISERÉ qui le borde perd du blanc à proportion de sa blancheur, et sa
//     couleur en est nettoyée — sans quoi un halo clair cernerait l'objet ;
//   · une OMBRE ISOLÉE, un amas de gris peu saturé que seul le fond entoure, se
//     lit comme une couche posée sur le blanc : elle devient une ombre
//     translucide.
//
// ⚠ UN GRIS QUI TOUCHE L'OBJET RESTE OPAQUE : c'est une partie claire de
// l'objet — l'acier d'une jambière, la face d'une pièce —, et la traiter en
// ombre la viderait.
//
// Un pixel déjà transparent — le magenta du client — compte comme du fond.
void MattePaperBackground(Image& image);

// RETOURNE POUR UN PAPIER SOMBRE UNE IMAGE PEINTE SUR DU BLANC — le fond d'une
// ligne, un cadre, un bouton : chaque pixel devient l'encre qu'il était, voir
// `core::InkForDarkPaper`. Posée sur un papier sombre, l'image en garde le
// dessin, les filets clairs là où ils étaient gris.
//
// ⚠ POUR L'ART D'INTERFACE, pas pour un objet peint : un trophée, un tampon
// garderaient leur dessin mais perdraient leur modelé. Ceux-là se détourent.
void RemapForDarkPaper(Image& image);

// RÉDUIT À SA FORME UNE MARQUE D'UNE SEULE ENCRE PEINTE SUR DU BLANC — un logo :
// chaque pixel devient blanc, opaque à proportion de sa part d'encre. Le papier
// et les jours de la marque tombent, le liseré lissé contre le blanc devient un
// bord adouci, et la marque se peint ensuite de la couleur qu'on veut en
// teintant l'image.
//
// ⚠ L'ENCRE EST LA BLANCHEUR MANQUANTE LA PLUS RÉPANDUE hors du papier : sur
// une image de plusieurs encres, les plus claires deviendraient translucides.
// Un pixel déjà transparent le reste.
void ReduceToInkShape(Image& image);

// Décode d'après l'EXTENSION du nom de fichier.
//
// Le TGA n'a pas de signature : impossible de le reconnaître à son contenu sans
// heuristique fragile. Le nom, lui, est toujours disponible — les formats RO se
// citent entre eux par leur chemin.
[[nodiscard]] core::Result<Image> DecodeImage(std::string_view file_name,
                                              std::span<const std::uint8_t> data);

}  // namespace ro::formats
