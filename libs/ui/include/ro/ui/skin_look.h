#pragma once

#include <cstdint>

#include <imgui.h>

namespace ro::ui {

// LA PALETTE DU SKIN RO : CE QUI FAIT QU'UNE FENÊTRE EST UNE FENÊTRE DU JEU.
//
// ⚠ L'HABILLAGE N'EST PAS QUE DES BITMAPS, et c'est l'erreur qu'on a payée : les
// pièces — barre de titre, boutons, ascenseur — ne couvrent que le CHROME. Tout
// le reste d'une fenêtre est peint par l'interface à partir de ces couleurs-ci.
// Poser les pièces sans la palette donne une barre de titre du jeu au-dessus
// d'une fenêtre d'ImGui : onglets bleu vif, champs sombres, texte blanc.
//
// ⚠ LE CORPS EST PRESQUE BLANC, et c'est le point qu'on ne devine pas. Une
// fenêtre de Ragnarok est claire et son texte presque noir ; tout ce qu'on peint
// dedans — le rang sous une icône de compétence, les liserés d'état — est réglé
// pour ce fond-là. Un corps sombre inventé rend la moitié du contenu illisible.
//
// ⚠ LA CHATBOX EST L'EXCEPTION et ne prend pas cette palette : elle est
// translucide sur le monde. Voir `BeginChatFrame`.

// Le corps, et son cadre d'un pixel.
inline constexpr ImU32 kSkinBodyColor = IM_COL32(243, 245, 250, 255);
inline constexpr ImU32 kSkinBorderColor = IM_COL32(197, 197, 197, 255);
// Le texte du corps. Presque noir, parce que le corps est presque blanc.
inline constexpr ImU32 kSkinTextColor = IM_COL32(28, 30, 38, 255);

// 🔴 LE TITRE D'UNE FENÊTRE EST PLUS NOIR QUE SON CORPS : du NOIR PUR, là où le
// texte ordinaire est un gris-bleu très sombre. La différence est de trois
// valeurs sur deux cent cinquante-cinq, et elle se voit — le titre est posé sur
// un dégradé clair, où un gris se délave.
//
// ⚠ UNE COULEUR À ELLE, et non `ImGuiCol_Text` : ce sont deux rôles, et les
// confondre rend les barres de titre plus pâles que celles du client.
inline constexpr ImU32 kSkinTitleTextColor = IM_COL32(0, 0, 0, 255);
// LE TEXTE SECONDAIRE : les « (?) », les notes, les compteurs, les titres de
// groupe, ce qui est grisé. ⚠ PLUS FONCÉ QUE LE GRIS « ÉTEINT » D'IMGUI : le
// corps d'une fenêtre du jeu est CLAIR, et ce gris-là y est illisible. Une
// fenêtre habillée le pose en `ImGuiCol_TextDisabled`, et c'est celui-là qu'on
// lit.
inline constexpr ImU32 kSkinFaintTextColor = IM_COL32(110, 112, 122, 255);
// Les champs de saisie, et les en-têtes de tableau : un gris franc sur le corps.
inline constexpr ImU32 kSkinInputColor = IM_COL32(206, 206, 206, 255);
// CE QUI S'OUVRE AILLEURS : une adresse dans une description d'objet, le lien
// de fusion de comptes sur l'écran de connexion.
//
// ⚠ UNE SEULE POUR TOUT LE CLIENT. Deux bleus de lien apprendraient au joueur
// deux fois la même chose, et la seconde fois mal — il ne saurait plus lequel
// des deux se clique.
inline constexpr ImU32 kSkinLinkColor = IM_COL32(0x2E, 0x74, 0xD8, 255);
// Les onglets. L'actif est le bleu du jeu, l'inactif un gris à peine teinté.
inline constexpr ImU32 kSkinActiveTabColor = IM_COL32(130, 148, 200, 255);
inline constexpr ImU32 kSkinIdleTabColor = IM_COL32(210, 214, 222, 255);
// La ligne choisie d'une liste, et le fond d'un menu.
inline constexpr ImU32 kSkinSelectionColor = IM_COL32(156, 184, 234, 160);
inline constexpr ImU32 kSkinSelectionHoverColor = IM_COL32(156, 184, 234, 110);
inline constexpr ImU32 kSkinSelectionActiveColor = IM_COL32(126, 160, 224, 210);
inline constexpr ImU32 kSkinPopupColor = IM_COL32(242, 243, 246, 255);

// LE CORPS D'UNE INFOBULLE HABILLÉE : blanc, texte noir.
//
// ⚠ C'EST LE SEUL ENDROIT DU JEU OÙ LE TEXTE SE LIT SUR DU BLANC, comme les
// panneaux de description dont elle emprunte le cadre.
inline constexpr ImU32 kSkinnedTooltipBodyColor = IM_COL32(255, 255, 255, 255);
inline constexpr ImU32 kSkinnedTooltipTextColor = IM_COL32(0, 0, 0, 255);

// L'ÉTAT D'UNE FABRICATION, sur le corps clair : ce qui est là, ce qui manque,
// ce qui est refusé ou à surveiller. Les trois fenêtres de fabrication les
// partagent.
//
// ⚠ PAS LES VERTS NI LES AMBRES D'AILLEURS : ceux-ci ont été choisis pour une
// ligne de texte fin sur ce fond précis, et un vert vif y bave.
inline constexpr ImU32 kSkinCraftOkColor = IM_COL32(13, 107, 31, 255);
inline constexpr ImU32 kSkinCraftMissingColor = IM_COL32(166, 38, 38, 255);
inline constexpr ImU32 kSkinCraftWarnColor = IM_COL32(166, 102, 0, 255);
// Et ce qui informe : le bleu d'un raffinement, d'une chaîne qui avance.
inline constexpr ImU32 kSkinInfoColor = IM_COL32(30, 90, 175, 255);
// L'OR D'UNE RÉUSSITE DANS UNE FENÊTRE — trophée gagné, record, carte neuve,
// exploit atteint : un or sombre, qui se lit sur le corps clair (contraste au
// moins 4,5:1). ⚠ L'OR VIF reste celui des avis et du HUD, posés sur un fond
// sombre : sur le corps presque blanc, il ne se lit plus.
inline constexpr ImU32 kSkinGoldTextColor = IM_COL32(150, 100, 0, 255);
// LE VIOLET D'UNE ÉVOLUTION, sombre pour la même raison (contraste au moins
// 4,5:1 sur le corps clair).
inline constexpr ImU32 kSkinEvolveTextColor = IM_COL32(120, 60, 180, 255);

// UNE COULEUR DE TEXTE NOMMÉE PAR SON RÔLE.
//
// 🔴 CE QUE `app` COMPOSE NE CONNAÎT PAS L'HABILLAGE : il dit le rôle, et
// l'interface le résout au dessin avec l'habillage du moment. Une couleur
// figée à la composition resterait celle du thème d'alors — un journal écrit
// sous le classique se relirait en vert foncé sur un corps sombre.
enum class SkinTone : std::uint8_t {
  kText,   // le texte du corps
  kFaint,  // le texte secondaire
  kGood,   // ce qui est réuni, réussi
  kBad,    // ce qui manque, échoue, est refusé
  kWarn,   // ce qui est à surveiller
  kInfo,   // ce qui informe
};

// L'arrondi du corps, des champs et des poignées. Le haut de la fenêtre est
// couvert par l'art de la barre de titre, donc seul le bas se voit.
inline constexpr float kSkinRounding = 3.0F;
inline constexpr float kSkinGrabRounding = 6.0F;
inline constexpr float kSkinPopupRounding = 2.0F;
// La largeur de l'ascenseur, celle des pièces du jeu.
inline constexpr float kSkinScrollbarWidth = 13.0F;

// LES COULEURS QUE L'HABILLAGE RÈGLE EN PLUS, rangées ici pour que leur
// défaut n'ait qu'une source. Le fond d'une case d'équipement est le gris des
// champs ; celui du mannequin, à peine plus clair que le corps ; une carte
// d'objet est crème sous un bandeau sombre.
inline constexpr ImU32 kSkinSlotColor = IM_COL32(206, 206, 206, 255);
inline constexpr ImU32 kSkinDollColor = IM_COL32(228, 230, 236, 255);
inline constexpr ImU32 kSkinCardColor = IM_COL32(245, 243, 232, 255);
inline constexpr ImU32 kSkinCardBandColor = IM_COL32(58, 55, 48, 255);
inline constexpr ImU32 kSkinCardBandTextColor = IM_COL32(240, 238, 228, 255);

// LA TEINTE D'UNE LUMINOSITÉ, de zéro à un : un gris opaque qui assombrit
// l'image qu'il teinte. ⚠ ELLE NE PEUT QU'ASSOMBRIR : au-delà de un, l'image
// reste telle.
[[nodiscard]] ImU32 BrightnessTint(float brightness);

// LE PRODUIT DE DEUX TEINTES, canal par canal, transparence comprise : ce que la
// carte graphique fait d'une image peinte sous une teinte, puis sous une autre.
// Le blanc opaque est neutre.
[[nodiscard]] ImU32 MultiplyTint(ImU32 left, ImU32 right);

// LE CONTRASTE DE DEUX COULEURS, de 1 à 21 : le rapport de leurs luminances
// relatives, chacune augmentée d'un vingtième — la mesure du W3C.
[[nodiscard]] float ContrastRatio(ImU32 left, ImU32 right);

// LE CONTRASTE QU'UNE ENCRE DOIT AU PAPIER pour se lire : celui d'un texte
// gras, et c'est volontairement bas — une couleur choisie pour le blanc passe
// presque toujours, et le thème classique ne bouge pas.
inline constexpr float kSkinLeastContrast = 3.0F;

// UNE ENCRE LISIBLE SUR CE PAPIER : elle-même si elle s'y lit assez, sinon
// mêlée — juste assez — de noir ou de blanc, selon ce qui s'oppose au papier.
//
// 🔴 POUR LES COULEURS QU'ON NE CHOISIT PAS : le `^0000FF` d'une description
// d'item, écrit pour un fond blanc, disparaît sur un papier sombre. Son bleu
// s'éclaircit sans changer de teinte. Sa transparence reste la sienne.
[[nodiscard]] ImU32 ReadableOn(ImU32 ink, ImU32 paper, float least_ratio = kSkinLeastContrast);

// LA TEINTE DE L'ART D'UN BOUTON OU D'UNE CASE GRISÉS, sous `BeginDisabled` :
// l'art s'efface à 90/255, et le libellé passe à la couleur `TextDisabled`. Un
// widget peint à la main ne se grise pas de lui-même, comme celui d'ImGui.
inline constexpr ImU32 kSkinDisabledArtTint = IM_COL32(255, 255, 255, 90);

// L'HABILLAGE DES FENÊTRES : la palette ci-dessus, que le joueur règle.
//
// ⚠ CE QU'IL RÈGLE, CE SONT LES FENÊTRES DU SKIN, et elles seules. La chatbox,
// les jauges et les infobulles du HUD gardent les constantes : elles sont
// posées sur le monde, pas dans une fenêtre.
//
// Les défauts sont la palette du jeu : un habillage jamais touché ne change
// rien à l'écran.
//
// ⚠ UNE TEINTE MULTIPLIE L'IMAGE qu'elle peint : blanche, l'image reste telle ;
// grise, elle l'assombrit ; colorée, elle la colore. Elle ne peut pas
// l'éclaircir — un bouton blanc se fonce, un bouton noir ne s'éclaire pas.
struct SkinLook {
  // La luminosité des IMAGES de la fenêtre — barre de titre et ses boutons,
  // ascenseur, pied — et des images d'une page d'album. Ni le texte ni les
  // aplats : leurs couleurs se règlent à part.
  float art_brightness = 1.0F;
  // L'opacité de toute la fenêtre, contenu compris.
  float opacity = 1.0F;
  // L'arrondi du corps et des champs, en pixels à l'échelle un.
  float rounding = kSkinRounding;

  ImU32 body = kSkinBodyColor;
  ImU32 border = kSkinBorderColor;
  ImU32 title_text = kSkinTitleTextColor;
  ImU32 text = kSkinTextColor;
  ImU32 active_tab = kSkinActiveTabColor;
  ImU32 idle_tab = kSkinIdleTabColor;
  ImU32 input = kSkinInputColor;
  ImU32 table_header = kSkinInputColor;
  // Le fond des menus et des listes déroulantes.
  ImU32 popup = kSkinPopupColor;
  ImU32 slot = kSkinSlotColor;
  ImU32 doll = kSkinDollColor;
  // Une carte d'objet : son fond — le papier de l'album aussi —, son bandeau
  // de titre et le texte du bandeau.
  ImU32 card = kSkinCardColor;
  ImU32 card_band = kSkinCardBandColor;
  ImU32 card_band_text = kSkinCardBandTextColor;

  // La teinte des images de la fenêtre, sous leur luminosité. ⚠ PAS CELLES DE
  // L'ALBUM : ce sont des illustrations, et une teinte les dénaturerait.
  ImU32 art_tint = IM_COL32_WHITE;
  // LES BOUTONS : la teinte de leur art — boutons, flèches des listes —, celle
  // de leur art survolé, enfoncé ou retenu, et leur libellé.
  //
  // ⚠ DEUX TEINTES, PARCE QUE LE SKIN A DEUX IMAGES : le bouton au repos est
  // gris clair, le survolé bleu et plus sombre. Une seule teinte foncerait le
  // survol plus que le repos dans un thème sombre, et le rendrait gris-bleu
  // dans un thème chaud.
  //
  // 🔴 LE LIBELLÉ D'UN BOUTON N'EST PAS LE TEXTE DU CORPS. Il est posé sur l'art
  // du bouton, presque blanc dans le skin du jeu, et non sur le corps : un texte
  // clair choisi pour un corps sombre y disparaît.
  ImU32 button_tint = IM_COL32_WHITE;
  ImU32 button_hover_tint = IM_COL32_WHITE;
  ImU32 button_text = kSkinTextColor;
  // Les cases à cocher et les boutons radio.
  ImU32 check_tint = IM_COL32_WHITE;
  // Les curseurs de réglage : leur piste, leurs flèches et leur poignée.
  ImU32 slider_tint = IM_COL32_WHITE;
  // LES LISTES D'ITEMS — sac et charrette : leurs onglets, et la grille de
  // leurs cases.
  ImU32 item_tab_tint = IM_COL32_WHITE;
  ImU32 item_grid_tint = IM_COL32_WHITE;

  // Le texte secondaire et les liens. Voir `kSkinFaintTextColor` et
  // `kSkinLinkColor`.
  ImU32 faint_text = kSkinFaintTextColor;
  ImU32 link = kSkinLinkColor;
  // LA LIGNE CHOISIE D'UNE LISTE OU D'UN MENU, transparence comprise : son
  // survol et son appui s'en déduisent.
  ImU32 selection = kSkinSelectionColor;
  // LE PAPIER : le fond et le texte des bulles, des descriptions d'items et de
  // compétences, et de leurs aperçus. ⚠ LES COULEURS PROPRES D'UNE DESCRIPTION
  // Y SONT CHOISIES POUR UN FOND CLAIR : sur un papier sombre, celles qui ne se
  // liraient plus s'éclaircissent — voir `ReadableOn`.
  ImU32 paper = kSkinnedTooltipBodyColor;
  ImU32 paper_text = kSkinnedTooltipTextColor;
  // LES COULEURS D'ÉTAT DU TEXTE : voir `SkinTone`. ⚠ RÉGLÉES POUR LE CORPS :
  // un vert foncé qui se lit sur le clair disparaît sur le sombre.
  ImU32 good_text = kSkinCraftOkColor;
  ImU32 bad_text = kSkinCraftMissingColor;
  ImU32 warn_text = kSkinCraftWarnColor;
  ImU32 info_text = kSkinInfoColor;

  // La couleur d'un rôle de texte.
  [[nodiscard]] ImU32 Tone(SkinTone tone) const;
  // La teinte qui pose `art_brightness` et `art_tint` sur une image de la
  // fenêtre.
  [[nodiscard]] ImU32 ArtTint() const;
  // LA SÉLECTION SURVOLÉE, ET APPUYÉE : plus transparente, puis plus foncée et
  // plus couvrante. Les écarts du jeu, rapportés à la couleur choisie — celle
  // d'origine rend exactement les siennes.
  [[nodiscard]] ImU32 SelectionHover() const;
  [[nodiscard]] ImU32 SelectionActive() const;

  bool operator==(const SkinLook&) const = default;
};

}  // namespace ro::ui
