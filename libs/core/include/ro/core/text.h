#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ro::core {

// Le texte du JEU et le nôtre ne sont pas dans le même encodage.
//
// Le serveur ne convertit rien : il relaie les octets que les clients lui
// donnent, et écrit ses propres messages avec les octets de ses fichiers de
// configuration. Le client d'origine, lui, écrit et lit ces octets dans la page
// de codes ANSI du système — CP1252 sur une installation occidentale, où tient
// tout le français. Un octet 0xE9 y est « é » ; lu comme de l'UTF-8, c'est le
// début d'une séquence de trois octets qui n'arrive jamais, et le caractère
// disparaît.
//
// Notre code, lui, travaille en UTF-8 de bout en bout : c'est ce que le clavier
// donne, c'est ce que la police attend. La conversion se fait donc à la
// FRONTIÈRE du réseau, dans les deux sens, et jamais ailleurs.

// LES CODES DE COULEUR DU JEU : `^RRGGBB`, six chiffres hexadécimaux après un
// accent circonflexe.
//
// Les scripts de PNJ, les descriptions d'objets et la conversation en sont
// pleins. Le client les LIT et colore ce qui suit ; ne pas les lire ne laisse
// pas un texte neutre, cela écrit « ^1F81D4Warp Agent^000000 » à l'écran.
//
// ⚠ `^000000` N'EST PAS DU NOIR. C'est le RETOUR À LA COULEUR PAR DÉFAUT, et la
// différence n'est pas théorique : une fenêtre au fond sombre écrirait en noir
// sur noir la moitié de chaque dialogue, puisque c'est par là que les scripts
// terminent chaque passage coloré.

// Un morceau de texte d'une seule couleur.
//
// ⚠ `text` EST UNE VUE SUR LE TEXTE DONNÉ, et ne lui survit pas. C'est ce qui
// permet de découper une ligne sans rien copier — mais l'appelant doit garder
// sa chaîne tant qu'il lit les morceaux.
struct ColorRun {
  std::string_view text;
  // La couleur demandée, en 0xRRGGBB. N'a de sens que si `colored` est vrai.
  std::uint32_t rgb = 0;
  // Faux tant qu'aucune couleur n'est posée, et à nouveau faux après
  // `^000000` : l'appelant emploie alors SA couleur de texte.
  bool colored = false;
};

// Découpe un texte en morceaux d'une seule couleur. Un texte sans code rend un
// seul morceau ; un code qui n'est pas suivi de six chiffres hexadécimaux
// reste du texte, comme chez le client.
[[nodiscard]] std::vector<ColorRun> SplitColorRuns(std::string_view text);

// ⚠ ET UNE CHAÎNE TEMPORAIRE NE SE DÉCOUPE PAS : les morceaux lui survivraient
// d'une expression, et liraient de la mémoire libérée. Ce refus est une ERREUR
// DE COMPILATION, pas une convention — la même faute, écrite ailleurs sans
// garde-fou, a coûté une commande qui rendait toujours la même valeur.
std::vector<ColorRun> SplitColorRuns(std::string&&) = delete;

// Un litteral se decoupe, lui : il vit aussi longtemps que le programme. Cette
// surcharge existe pour LEVER L AMBIGUITE que le refus ci-dessus cree — sans
// elle, « SplitColorRuns("abc") » ne saurait plus lequel des deux choisir.
[[nodiscard]] inline std::vector<ColorRun> SplitColorRuns(const char* text) {
  return SplitColorRuns(std::string_view(text));
}

// LE NOMBRE QUI SUIT UNE COMMANDE, ou rien : ligne qui ne commence pas par
// cette commande, argument absent, ou argument illisible. Les espaces qui
// séparent les deux sont mangés.
//
// ⚠ ELLE PREND UNE VUE, ET C'EST LE POINT. Écrire `ligne.substr(...)` dans une
// `string_view` quand `ligne` est une `std::string` rend une chaîne TEMPORAIRE :
// la vue pend dès la fin de l'expression, la lecture rend n'importe quoi, et
// rien ne le signale. Ici la source est déjà une vue, donc le découpage est sûr
// par construction — et la fonction étant pure, elle s'éprouve sans jeu.
//
// ⚠ ET ELLE NE SUIT PAS LA LOCALE, contrairement à `strtof` : sous une locale
// où la virgule sépare les décimales, celui-ci s'arrête au point et rend un
// nombre tronqué sans rien dire.
[[nodiscard]] std::optional<float> NumberAfterCommand(std::string_view line,
                                                      std::string_view command);

// Le point de code suivant, et l'avancée dans la chaîne.
//
// Une séquence invalide rend le caractère de remplacement et n'avance que d'un
// octet : du texte mal encodé se lit de travers, mais ne fait pas boucler la
// lecture.
inline constexpr char32_t kReplacementCharacter = 0xFFFD;
[[nodiscard]] char32_t NextCodePoint(std::string_view utf8, std::size_t& at);

// Ajoute un point de code à une chaîne UTF-8.
void AppendUtf8(std::string& out, char32_t code);

// Du texte du jeu vers le nôtre. L'ASCII passe inchangé.
[[nodiscard]] std::string Utf8FromCp1252(std::string_view bytes);

// CES OCTETS SONT-ILS DÉJÀ DE L'UTF-8 ?
//
// ⚠ DE L'ASCII PUR REND FAUX, et c'est voulu : les deux tables coïncident,
// donc la question ne se pose pas et le chemin habituel convient.
[[nodiscard]] bool LooksLikeUtf8(std::string_view bytes);

// COMBIEN D'OCTETS D'UNE SÉQUENCE UTF-8 COUPÉE traînent en fin de chaîne.
// Zéro quand la fin est propre, ou quand l'erreur est ailleurs.
//
// 🔴 CE N'EST PAS UNE PRÉCAUTION THÉORIQUE. Le serveur remplit des
// champs de TAILLE FIXE et coupe à l'octet près, sans savoir ce qu'il coupe —
// or un emoji pèse quatre octets. Une coupe au mauvais endroit rend la chaîne
// invalide, et sans ce rattrapage c'est le message ENTIER qui repart en 1252 :
// on ne perd pas le dernier caractère, on perd la phrase en mojibake.
[[nodiscard]] std::size_t DanglingUtf8TailLength(std::string_view bytes);

// LA MÊME CHAÎNE, SANS SA SÉQUENCE COUPÉE. Pour ce que NOUS tronquons à l'octet
// — un `snprintf` dans un tampon fixe, une copie bornée — : un nom coréen ou
// accentué coupé au milieu d'un caractère s'afficherait sinon avec un carré
// « caractère manquant » en queue.
[[nodiscard]] inline std::string_view WithoutDanglingUtf8(std::string_view bytes) {
  return bytes.substr(0, bytes.size() - DanglingUtf8TailLength(bytes));
}

// DU TEXTE DU FIL VERS LE NÔTRE, quel que soit son encodage.
//
// 🔴 LE FIL PORTE LES DEUX. Le serveur relaie les octets qu'on lui
// donne : nos emoji reviennent en UTF-8, alors que ce que le client d'origine
// envoie est du 1252. Repasser de l'UTF-8 par la table 1252 le DOUBLE — un
// « é » devient « Ã© », et un emoji disparaît en bouillie.
//
// ⚠ ET LES SÉLECTEURS DE VARIANTE SONT RETIRÉS. U+FE0F suit presque tous les
// emoji venus d'un panneau système ; il ne se contente pas d'être invisible, il
// PREND DE LA PLACE. Un moteur de texte complet l'absorbe au façonnage ; ImGui
// n'en fait pas et dessine bêtement son glyphe, vide mais large.
[[nodiscard]] std::string Utf8FromWire(std::string_view bytes);

// Du nôtre vers celui du jeu. Un caractère que CP1252 ne porte pas devient un
// point d'interrogation : mieux vaut un trou visible qu'un octet inventé, que
// le serveur relaierait à tout le monde.
[[nodiscard]] std::string Cp1252FromUtf8(std::string_view utf8);

// LE MÊME, MAIS QUI REFUSE PLUTÔT QUE DE REMPLACER. Rend rien dès qu'un
// caractère n'a pas de représentation en 1252.
[[nodiscard]] std::optional<std::string> ExactCp1252FromUtf8(std::string_view utf8);

// UNE PHRASE, TELLE QU'ELLE PART SUR LE FIL.
//
// 🔴 1252 TANT QUE POSSIBLE, UTF-8 SEULEMENT S'IL LE FAUT. Une phrase
// accentuée ordinaire part exactement comme avant — mêmes octets dans les
// journaux, dans la base, pour les commandes d'administration — et seule celle
// qui contient vraiment un caractère hors 1252, un emoji, bascule. Le lecteur,
// lui, accepte les deux (voir `Utf8FromWire`).
//
// 🔴 ET CE N'EST PAS UN REMPLAÇANT DE `Cp1252FromUtf8` : les mélanger
// casse des choses en silence. Un nom de personnage, une cible de chuchotement,
// un nom de canal sont des IDENTIFIANTS — le serveur les compare OCTET PAR
// OCTET à ce que sa base contient, en 1252. Les envoyer en UTF-8 ferait échouer
// la recherche au premier accent. Ceux-là passent par `Cp1252FromUtf8`,
// toujours. Ici on ne parle que de PHRASES, dont le serveur relaie les octets.
[[nodiscard]] std::string WireFromUtf8Text(std::string_view utf8);

}  // namespace ro::core
