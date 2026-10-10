#pragma once

// UNE ÉCHOPPE EN PRÉPARATION GÈLE LES TRANSFERTS entre le sac, la charrette et
// le coffre : la préparation lit ce qu'ils contiennent, et le serveur refuserait
// de toute façon. Les trois fenêtres le disent de la même façon — un bandeau en
// tête du corps, le même en tête des entrées de transfert de leurs menus, qui
// passent en grisé, et le refus dans le fantôme d'un glisser —, et `app`
// retient les demandes qui passeraient quand même.
//
// En-tête PRIVÉ : seules les trois fenêtres des contenants s'en servent.
namespace ro::ui {

// Le refus, tel que le bandeau et le fantôme d'un glisser le disent.
//
// ⚠ UNE FONCTION ET NON UNE CONSTANTE : le texte reste un littéral passé à
// `Tr`, le seul que le relevé des traductions sache lire.
[[nodiscard]] const char* TransfersFrozenText();

// Le bandeau rouge, en tête du corps d'une fenêtre ou des entrées de transfert
// d'un menu. Rien quand rien n'est gelé.
void DrawTransfersFrozenBanner(bool frozen);

}  // namespace ro::ui
