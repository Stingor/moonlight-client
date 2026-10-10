#pragma once

namespace ro::ui {

// ── LE LIEN DANS LE CHAT, OFFERT PAR UN ÉLÉMENT QUI A DÉJÀ SON MÉTIER ───────
//
// Une entrée de navigation, un onglet : leur clic a déjà un sens. Maj + clic y
// pose en plus le LIEN de ce qu'ils désignent dans la saisie du chat — « va voir
// ce réglage » est ce qu'on répond vingt fois par jour, et le décrire par un
// chemin ne marche jamais.
//
// `ui` NE COMPOSE RIEN : la fenêtre rend dans sa réponse CE qu'on a désigné, et
// `app` en fait un lien.
//
// 🔴 LE GESTE SE LIT SUR LA GÉOMÉTRIE, ET NON PAR LE RETOUR DE L'ÉLÉMENT. Une
// saisie de chat qui tient le clavier — c'est le cas juste après qu'on y a posé
// un lien — détient l'élément actif d'ImGui, et tant qu'elle le garde, un
// bouton dessiné avant elle ne se dit ni survolé ni cliqué, et un second lien
// d'affilée ne partirait pas. On lit donc Maj, l'appui et le rectangle du
// dernier élément nous-mêmes, avec une seule garde d'ImGui : qu'aucune AUTRE
// fenêtre ne soit par-dessus.

// Maj + clic gauche vient de s'enfoncer sur le DERNIER élément dessiné.
[[nodiscard]] bool ShiftClickedLastItem();

// Le dernier élément est survolé assez longtemps pour une infobulle, même quand
// la saisie du chat tient l'élément actif.
[[nodiscard]] bool HoveredForLinkTooltip();

// LA PHRASE QUI ANNONCE LE GESTE, déjà traduite : il n'a aucune trace visible,
// et sans elle il n'existerait que pour qui l'a lu quelque part.
[[nodiscard]] const char* ChatLinkGestureHint();

// LES DEUX D'UN COUP, sur le dernier élément : vrai au Maj + clic, l'aide au
// survol sinon. ⚠ À APPELER JUSTE APRÈS L'ÉLÉMENT — pour un onglet, après
// `BeginTabItem` et avant son contenu, qui deviendrait le dernier élément.
//
// `offered` faux — l'écran de connexion, qui n'a pas de chat — : rien, ni geste
// ni aide, et le Maj + clic redevient un clic ordinaire.
[[nodiscard]] bool OfferChatLink(bool offered);

}  // namespace ro::ui
