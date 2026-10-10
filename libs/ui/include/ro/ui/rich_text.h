#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>

namespace ro::ui {

// LE TEXTE BALISÉ DU CLIENT : couleurs, liens, et de quoi le sélectionner.
//
// Les descriptions d'objets et de compétences arrivent avec le balisage du jeu,
// et `vfs` le conserve exprès. Ce module le LIT et en fait une disposition :
// une boîte par glyphe, avec sa couleur, son lien et sa place.
//
// 🔴 IMGUI NE SAIT PAS SÉLECTIONNER DU TEXTE COLORÉ. Ses widgets de texte sont
// d'un seul ton et ne rendent aucune position ; `InputText` en lecture seule
// perdrait les couleurs et les liens. Il n'y a pas de demi-chemin : ou l'on
// renonce à la sélection, ou l'on pose soi-même chaque glyphe. Ce module fait
// la seconde.
//
// ── LE BALISAGE, TEL QUE LE CLIENT L'ÉCRIT ──────────────────────────────────
//
// `^RRGGBB`   change la couleur. ⚠ `^000000` NE VEUT PAS DIRE NOIR : c'est le
//             retour à la couleur par défaut du bloc.
// `<URL>texte<INFO>adresse</INFO></URL>`   un lien vers le dehors.
// `<NAVI>texte<INFO>carte,x,y,…</INFO></NAVI>`   un lien vers un lieu.
// `<ITEM>nom<INFO>id</INFO></ITEM>`   un lien vers un objet, par son
//             identifiant. Une charge qui n'en est pas un ne fait pas de lien :
//             le nom reste du texte, dans la couleur du flux.
// `<TIPBOX>texte<INFO>id</INFO></TIPBOX>`   une fiche d'aide, par son rang.
//             Le client ne lit pas ces fiches : balises et charge s'effacent,
//             le texte se lit comme le reste du flux.
//
// 🔴 LA COULEUR EST UN FLUX, PAS UN ATTRIBUT DE LIGNE. Un `^RRGGBB` ouvert
// reste actif sur les lignes SUIVANTES jusqu'à sa fermeture : les tables du jeu
// écrivent la couleur une fois, en tête de paragraphe, et la referment plusieurs
// lignes plus bas. Réinitialiser à chaque ligne décolorerait tout sauf la
// première.

// CE QU'UN LIEN VISE.
enum class RichLinkKind : std::uint8_t {
  // Le dehors : une adresse http. ⚠ `ui` NE L'OUVRE PAS — elle rend l'intention,
  // et `app` décide. Ouvrir un navigateur depuis une donnée du jeu est un geste
  // vers l'extérieur, et il ne se prend pas au fond d'une couche de dessin.
  kAddress,
  // Un lieu : « carte,x,y,… ». Le client y envoie le personnage.
  kPlace,
  // Un objet : son identifiant en décimal. Le clic le décrit, Maj + clic le
  // repose dans la saisie, le clic droit ouvre son menu — les gestes de tout
  // lien d'objet.
  kItem,
};

// UN LIEU, lu dans la charge d'un lien `kPlace` : « carte,x,y,… ». Rien quand
// la charge ne commence pas ainsi.
struct RichPlace {
  std::string map;
  int x = 0;
  int y = 0;
};
[[nodiscard]] std::optional<RichPlace> ReadRichPlace(std::string_view payload);

// L'OBJET, lu dans la charge d'un lien `kItem` : un identifiant décimal non
// nul, seul. Rien sinon.
[[nodiscard]] std::optional<std::uint32_t> ReadRichItem(std::string_view payload);

struct RichLink {
  RichLinkKind kind = RichLinkKind::kAddress;
  // La charge, telle qu'elle est écrite entre `<INFO>` et `</INFO>`. ⚠ ELLE NE
  // S'AFFICHE PAS : seul le texte qui l'entoure se montre.
  std::string payload;
};

// UNE BOÎTE = UN GLYPHE AFFICHÉ.
//
// 🔴 UN GLYPHE, PAS UN OCTET. Un caractère non-ASCII tient sur plusieurs octets
// en UTF-8 ; le découper en octets le dessinerait et le sélectionnerait morceau
// par morceau — des losanges à l'écran, et une copie coupée en plein accent.
struct RichBox {
  float left = 0.0F;
  float right = 0.0F;
  // Le haut de la rangée, en pixels depuis le début du bloc.
  float top = 0.0F;
  int row = 0;
  // Où ce glyphe commence dans le texte plat, et sa longueur en octets. C'est
  // par là que passent la sélection et la copie.
  int offset = 0;
  int length = 0;
  ImU32 color = 0;
  // Le rang du lien qui le porte, ou moins un.
  int link = -1;
};

// LE BLOC DISPOSÉ : le texte plat, ses glyphes, ses liens.
//
// ⚠ LE TEXTE PLAT N'EST PAS LA SOURCE : le balisage en est retiré, les fins de
// ligne y sont des `\n`, et les replis doux ajoutent une espace. C'est ce qu'on
// copie, parce que c'est ce qu'on lit.
struct RichText {
  std::string text;
  std::vector<RichBox> boxes;
  std::vector<RichLink> links;
  // La hauteur occupée, en pixels.
  float height = 0.0F;
  // 🔴 LA LARGEUR SUR LAQUELLE IL A ÉTÉ DISPOSÉ, et c'est le dessin qui en a
  // besoin : sa zone de souris doit couvrir le bloc, et il ne peut pas la
  // redemander à la place courante. Dans une INFOBULLE, qui se dimensionne sur
  // son contenu, cette place vaut presque zéro — la zone se réduisait à un trait
  // et la sélection ne mordait plus nulle part.
  float width = 0.0F;
};

// COMMENT MESURER UN MORCEAU DE TEXTE.
//
// ⚠ PASSÉE PLUTÔT QUE PRISE À IMGUI, et c'est ce qui rend la disposition
// ESSAYABLE : le repli, la coupure d'un mot et le regroupement des octets d'un
// caractère sont exactement là où les fautes se logent, et un banc d'essai n'a
// ni police ni fenêtre. On lui donne une règle à lui — un pixel par octet — et
// les distances deviennent des nombres qu'on peut écrire dans un essai.
using RichMeasure = float (*)(std::string_view piece, void* owner);

// La disposition d'un bloc.
//
// `lines` sont les lignes de la source, balisage compris. `wrap_width` est la
// largeur où le texte se replie, `line_height` le pas d'une rangée.
[[nodiscard]] RichText LayOutRichText(std::span<const std::string> lines, ImU32 default_color,
                                      float wrap_width, float line_height, RichMeasure measure,
                                      void* owner);

// LE TEXTE QUE MONTRE UN LIEN, tiré du texte plat : la tête de son menu.
[[nodiscard]] std::string RichLinkText(const RichText& laid, int link);

// LA LONGUEUR VISIBLE D'UNE LIGNE : ce qu'elle montre, tokens de couleur et
// balises retirés. Sert à qui doit juger de la largeur d'une ligne SOURCE.
[[nodiscard]] int VisibleLength(std::string_view line);

// Les deux octets d'un couple hexadécimal, ou moins un si ce n'en est pas un.
[[nodiscard]] int HexByte(std::string_view pair);

// LA DISPOSITION D'UN BLOC AVEC LA POLICE COURANTE. C'est `LayOutRichText` dont
// la mesure et la hauteur de ligne viennent d'ImGui.
[[nodiscard]] RichText LayOutRichTextHere(std::span<const std::string> lines, ImU32 default_color,
                                          float wrap_width);

// LES COULEURS D'UN BLOC RENDUES LISIBLES SUR UN PAPIER : celles que le texte
// porte en `^RRGGBB`, choisies pour un fond clair. Une couleur qui s'y lit
// assez ne bouge pas. Voir `ReadableOn`.
void KeepReadable(RichText& laid, ImU32 paper);

// CE QUE LE JOUEUR A FAIT DANS UN BLOC.
struct RichTextAction {
  // Le rang du lien qu'il vient d'activer, ou moins un. ⚠ `ui` NE L'OUVRE PAS :
  // suivre une adresse est un geste vers le DEHORS, et il se décide chez
  // l'appelant — pas au fond d'une couche de dessin.
  int followed = -1;
  // Maj était tenu à ce clic : un lien d'objet se repose au lieu de s'ouvrir.
  bool followed_with_shift = false;
  // Le rang du lien survolé, ou moins un : l'aperçu n'attend pas un clic.
  int hovered = -1;
  // Le rang du lien sous un clic droit relâché, ou moins un : son menu.
  int menu = -1;
};

// QUAND LE TEXTE D'UN BLOC SE SÉLECTIONNE.
enum class RichTextSelect : std::uint8_t {
  // Au glisser, toujours : le bloc prend la souris, et la fenêtre ne se déplace
  // plus en le tirant.
  kFree,
  // Maj ou Ctrl tenu au clic. Sans eux, le glisser déplace la fenêtre comme
  // sur un fond nu ; un lien se suit toujours d'un clic.
  kWithModifier,
};

// DESSINE UN BLOC, ET LE LAISSE SE SÉLECTIONNER.
//
// ⚠ L'ÉTAT DE SÉLECTION VIT DANS LE RANGEMENT D'IMGUI, par identifiant de bloc
// et dans la fenêtre courante. `ui` ne possède rien de global, et une sélection
// qui survivrait à sa fenêtre désignerait un texte qui n'existe plus.
//
// ⚠ LE BLOC EST DESSINÉ TEL QU'IL A ÉTÉ DISPOSÉ : la disposition ne dépend pas
// de l'endroit — ses distances partent du coin du bloc —, et c'est ce dessin-ci
// qui y ajoute la position courante.
[[nodiscard]] RichTextAction DrawRichText(const char* id, const RichText& laid, float line_height,
                                          RichTextSelect select = RichTextSelect::kFree);

}  // namespace ro::ui
