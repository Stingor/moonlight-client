#include "ro/ui/web_door.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_widgets.h"

#include "ui_text.h"

namespace ro::ui {
namespace {

constexpr ImU32 kRefusalColor = IM_COL32(255, 96, 96, 255);
constexpr ImU32 kWaitingColor = IM_COL32(150, 130, 40, 255);

// LA TEINTE D'UNE SESSION OCCUPÉE, sur la ligne elle-même.
//
// 🔴 DEUX COULEURS ET NON UNE, parce que ce qu'on perd n'est pas la même chose.
// Un compte simplement en ligne coûte une session à quelqu'un ; un compte en
// vente automatique coûte une BOUTIQUE, qui est en ligne pour une raison. Les
// valeurs sont choisies pour se lire sur le corps clair de la fenêtre.
constexpr ImU32 kOnlineRowColor = IM_COL32(200, 170, 110, 255);
constexpr ImU32 kAutotradeRowColor = IM_COL32(120, 195, 165, 255);

// Les mêmes, en plus vif, pour la phrase d'avertissement : elle se lit une
// fois, la ligne se parcourt du coin de l'œil.
constexpr ImU32 kOnlineWarningColor = IM_COL32(230, 190, 110, 255);
constexpr ImU32 kAutotradeWarningColor = IM_COL32(140, 215, 185, 255);

// LE PLUS DE LIGNES QUE LA LISTE MONTRE D'UN COUP. Au-delà elle défile.
//
// ⚠ C'EST UN PLAFOND, PAS UNE HAUTEUR : la liste se rétrécit au nombre de
// comptes. Une boîte de huit rangées devant deux comptes est un trou dans la
// fenêtre, et elle laisse croire qu'il y a autre chose plus bas.
constexpr int kVisibleAccounts = 8;

// Ce que la boîte prend autour de ses rangées, en pixels : sans elle, la
// dernière ligne touche le cadre.
constexpr float kListPaddingPixels = 8.0F;

// CE QU'UNE LIGNE DIT D'UN COMPTE — TOUT, ET SUR UNE SEULE LIGNE.
//
// 🔴 UNE LIGNE ET NON DEUX, et c'est ce qui rend la liste lisible. Sur deux
// lignes, huit comptes prennent la moitié d'un écran et l'on cherche le bon en
// glissant ; sur une, l'œil balaie une colonne de noms.
//
// ⚠ LES DRAPEAUX SONT ENTRE CROCHETS ET DANS LA LIGNE, dans l'ordre de ce
// qu'ils coûtent : banni d'abord, il empêche ; l'échoppe ensuite, parce qu'on
// la ferme ; « en ligne » en dernier, il ne fait que prévenir. Un compte banni
// garde son second drapeau — les deux faits sont vrais en même temps.
[[nodiscard]] std::string AccountLine(const WebAccountRow& account) {
  std::array<char, 64> counted = {};
  std::snprintf(counted.data(), counted.size(), core::Tr("%d personnage(s)"), account.characters);
  std::string line = account.label + "  (" + counted.data() + ")";
  // ⚠ LES CROCHETS SONT DANS LE CODE, PAS AU CATALOGUE : c'est de la
  // ponctuation, et les trois mots qu'ils encadrent y sont déjà, traduits.
  // Les y remettre avec leurs crochets aurait fait trois entrées de plus
  // disant la même chose, et deux catalogues à retraduire pour rien.
  const auto badge = [&line](const char* what) {
    line += "  [";
    line += what;
    line += "]";
  };
  // Le niveau staff vient en tête : il dit QUEL compte c'est, les drapeaux qui
  // suivent disent ce qu'il en coûte de le prendre.
  if (account.group_id > 0) {
    std::array<char, 64> level = {};
    std::snprintf(level.data(), level.size(), core::Tr("niveau GM %d"), account.group_id);
    badge(level.data());
  }
  if (account.banned) {
    badge(core::Tr("BANNI"));
  }
  if (account.autotrade) {
    badge(core::Tr("boutique ouverte"));
  } else if (account.online) {
    badge(core::Tr("en ligne"));
  }
  if (!account.last_login.empty()) {
    line += "   " + account.last_login;
  }
  return line;
}

// CE QUE CHOISIR CE COMPTE-LÀ VA COÛTER, en une phrase, JUSTE AU-DESSUS DU
// BOUTON.
//
// 🔴 LE DRAPEAU DIT L'ÉTAT, LA PHRASE DIT LA CONSÉQUENCE, et les deux sont
// nécessaires. « en ligne » sur une ligne de liste n'apprend rien à qui ne
// connaît pas la règle du serveur — une seule session par compte. Ce qu'il faut
// écrire est ce qui va se passer s'il clique.
void DrawConsequence(const WebAccountRow& account, float width) {
  if (!account.online && !account.autotrade) {
    return;
  }
  ImGui::PushStyleColor(ImGuiCol_Text,
                        account.autotrade ? kAutotradeWarningColor : kOnlineWarningColor);
  ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width);
  ImGui::TextUnformatted(
      account.autotrade
          ? core::Tr("Ce compte tient une boutique en vente automatique : le choisir "
                     "fermera la boutique et déconnectera le marchand.")
          : core::Tr("Ce compte est déjà connecté : le choisir déconnectera la session "
                     "en cours."));
  ImGui::PopTextWrapPos();
  ImGui::PopStyleColor();
  ImGui::Spacing();
}

// CE QUE LE BOUTON DISCORD LAISSE AUTOUR DE SON LOGO, en pixels.
constexpr float kDiscordPaddingPixels = 6.0F;

// LA PORTE DISCORD : un bouton DU JEU, avec la marque de Discord peinte dedans.
//
// 🔴 LE CADRE EST AU JEU, L'IMAGE EST À DISCORD. Redessiner le logo aux
// couleurs de RO le rendrait méconnaissable — une marque se reconnaît ou elle
// ne sert à rien ; peindre à l'inverse un bouton entier en « blurple » poserait
// une pièce étrangère au milieu d'un écran habillé. Chacun garde ce qui est à
// lui.
//
// ⚠ À SA TAILLE NATIVE ET SUR UN PIXEL ENTIER : le logo fait 105 × 16, et
// l'étirer d'un demi-pixel suffit à le rendre flou. On l'aligne donc sur la
// grille de l'écran plutôt que sur le centre exact du bouton.
//
// 🔴 SUR UNE FACE SOMBRE, LA FORME DU LOGO À L'ENCRE DU LIBELLÉ. La face est
// sombre quand l'habillage écrit ses boutons en clair ; le bleu de Discord y a
// la clarté du gris qui l'entoure, et le logo y perd sa lecture.
[[nodiscard]] bool DrawDiscordButton(const SkinImage& colored, const SkinImage& shape,
                                     const SkinArt& art, float width) {
  const ImU32 label_ink = art.look().button_text;
  const bool face_is_dark = !PaperIsDark(label_ink);
  const bool as_shape = face_is_dark && shape.usable();
  const SkinImage& logo = as_shape ? shape : colored;
  if (!logo.usable()) {
    // ⚠ REPLI SUR LE LIBELLÉ : l'image vit dans l'archive du serveur, et une
    // installation peut ne pas la porter. Un bouton sans logo vaut mieux
    // qu'une porte sans bouton.
    return SkinButton(core::Tr("Se connecter avec Discord"), art, ImVec2(width, 0.0F));
  }
  float wide = logo.width;
  float high = logo.height;
  const float room = width - kDiscordPaddingPixels * 2.0F;
  if (wide > room && room > 0.0F) {
    high *= room / wide;
    wide = room;
  }
  const bool clicked =
      SkinButton("##discord", art, ImVec2(width, high + kDiscordPaddingPixels * 2.0F));
  const ImVec2 corner_min = ImGui::GetItemRectMin();
  const ImVec2 corner_max = ImGui::GetItemRectMax();
  const ImVec2 middle((corner_min.x + corner_max.x) * 0.5F, (corner_min.y + corner_max.y) * 0.5F);
  const ImVec2 at(std::floor(middle.x - wide * 0.5F), std::floor(middle.y - high * 0.5F));
  ImGui::GetWindowDrawList()->AddImage(logo.id, at, ImVec2(at.x + wide, at.y + high),
                                       ImVec2(0.0F, 0.0F), ImVec2(1.0F, 1.0F),
                                       PaintedTint(as_shape ? label_ink : IM_COL32_WHITE));
  return clicked;
}

// LE FORMULAIRE DU SITE.
//
// 🔴 CET ORDRE EST UNE HIÉRARCHIE, pas une suite de contrôles empilés. Ce que
// le joueur cherche vient d'abord — ses identifiants, le bouton —, ce qui ne
// sert qu'une fois vient en bas — regrouper ses comptes —, et la porte qu'on
// ne prend que quand le site est en panne vient en DERNIER, derrière un trait,
// annoncée par une question plutôt que par un bouton.
[[nodiscard]] WebDoorAction DrawSignIn(WebDoorState& state, const WebDoorView& view,
                                       const SkinArt& art, float width) {
  WebDoorAction action;
  ImGui::TextUnformatted(core::Tr("Connecte-toi avec ton compte Moonlight"));
  ImGui::Spacing();

  // ⚠ « IDENTIFIANT » ET NON « COMPTE » : ce champ ne prend pas un compte de
  // JEU — c'est précisément ce que cette porte évite de demander — mais le
  // pseudonyme du forum. Les deux mots séparent les deux choses à l'endroit
  // même où on les confondrait.
  ImGui::TextUnformatted(core::Tr("Identifiant"));
  ImGui::SetNextItemWidth(width);
  ImGui::InputText("##membre", state.member.data(), state.member.size());

  ImGui::TextUnformatted(core::Tr("Mot de passe"));
  ImGui::SetNextItemWidth(width);
  const bool submitted_here =
      ImGui::InputText("##membre_mdp", state.password.data(), state.password.size(),
                       ImGuiInputTextFlags_Password | ImGuiInputTextFlags_EnterReturnsTrue);

  // ── LES DEUX MÉMOIRES ────────────────────────────────────────────────────
  //
  // 🔴 LA SECONDE IMPLIQUE LA PREMIÈRE, DANS LES DEUX SENS. Retenir un mot de
  // passe sans son identifiant ne remplirait qu'un champ sur deux ; et oublier
  // l'identifiant en gardant le mot de passe laisserait un secret sur le
  // disque au service de personne. La paire reste cohérente aux deux bouts.
  bool remember_member = state.remember_member;
  if (SkinCheckBox(core::Tr("Se souvenir de moi"), &remember_member, art)) {
    state.remember_member = remember_member;
    state.remember_password = state.remember_password && remember_member;
    action.memory_changed = true;
  }
  bool remember_password = state.remember_password;
  if (SkinCheckBox(core::Tr("Se souvenir du mot de passe"), &remember_password, art)) {
    state.remember_password = remember_password;
    state.remember_member = state.remember_member || remember_password;
    action.memory_changed = true;
  }

  ImGui::Spacing();
  const bool filled = state.member[0] != '\0' && state.password[0] != '\0';
  // ⚠ ENTRÉE VALIDE DEPUIS N'IMPORTE OÙ, et pas seulement depuis le champ du
  // mot de passe : c'est le geste qu'on fait après avoir tapé son identifiant,
  // et c'est aussi le seul geste qui reste quand les deux champs arrivent
  // déjà remplis par la mémoire.
  const bool entered = ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
                       ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false);
  if ((SkinButton(core::Tr("Se connecter"), art, ImVec2(width, 0.0F)) || submitted_here ||
       entered) &&
      filled) {
    action.verb = WebDoorVerb::kSignIn;
  }

  // ⚠ LA PORTE DISCORD EST UN BOUTON, PAS UN ONGLET : elle mène exactement au
  // même endroit — la liste des comptes —, et en faire un écran à part
  // donnerait trois chemins là où il y en a deux.
  ImGui::Spacing();
  if (DrawDiscordButton(view.discord, view.discord_shape, art, width)) {
    action.verb = WebDoorVerb::kDiscord;
  }

  // ── PLUSIEURS COMPTES DU SITE À REGROUPER ────────────────────────────────
  //
  // ⚠ UN LIEN ET NON UN BOUTON : ce qui est au bout part dans le navigateur,
  // et un bouton au milieu d'un formulaire promet une action DANS le
  // formulaire.
  ImGui::Spacing();
  FaintText(core::Tr("Plusieurs comptes à regrouper ?"));
  ImGui::SameLine();
  if (SkinHyperlink(core::Tr("Fusionner mes comptes"))) {
    action.verb = WebDoorVerb::kMergeAccounts;
  }

  // ── LE REPLI CLASSIQUE ───────────────────────────────────────────────────
  //
  // 🔴 EN BAS, DERRIÈRE UN TRAIT, ET EN PETIT — jamais un onglet en haut. Ce
  // ne sont pas deux façons équivalentes d'entrer : celle-ci ne sert qu'à qui
  // a un compte de jeu direct, ou à qui trouve le site en panne. La mettre à
  // égalité en tête de fenêtre ferait hésiter tout le monde à chaque
  // lancement, sur une question qui n'a qu'une bonne réponse.
  ImGui::Spacing();
  ImGui::Separator();
  FaintText(core::Tr("Tu préfères te connecter à un compte Ragnarok ?"));
  if (SkinSmallButton(core::Tr("Utiliser le login classique"), art)) {
    action.verb = WebDoorVerb::kUseClassic;
  }
  return action;
}

// LE CHOIX SE DÉPLACE AUX FLÈCHES, et la liste boucle.
//
// ⚠ ELLE PARCOURT AUSSI LES COMPTES BANNIS, délibérément : les sauter en
// silence rendrait la navigation imprévisible — on appuie deux fois, il se
// déplace de trois. Le bouton « Jouer » se grise de lui-même, ce qui dit la
// même chose sans surprendre.
void MoveChoice(WebDoorState& state, int count) {
  if (count <= 0 || ImGui::GetIO().WantTextInput) {
    return;
  }
  // ⚠ AVEC RÉPÉTITION : on parcourt une liste, pas un interrupteur.
  if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true)) {
    state.chosen = state.chosen < 0 ? 0 : (state.chosen + 1) % count;
    state.scroll_to_chosen = true;
  } else if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true)) {
    state.chosen = state.chosen <= 0 ? count - 1 : state.chosen - 1;
    state.scroll_to_chosen = true;
  }
}

// LA LISTE DES COMPTES DE JEU.
[[nodiscard]] WebDoorAction DrawAccounts(WebDoorState& state, const WebDoorView& view,
                                         const SkinArt& art, float width) {
  WebDoorAction action;
  const auto count = static_cast<int>(view.accounts.size());

  ImGui::TextUnformatted(core::Tr("Choisis un compte de jeu"));
  // ⚠ LE MODE D'EMPLOI EST SUR L'ÉCRAN, pas dans la tête du joueur : rien
  // n'indique qu'une liste se parcourt aux flèches ni qu'un double-clic joue.
  ImGui::TextDisabled("%s", core::Tr("Flèches pour naviguer, Entrée ou double-clic pour jouer."));
  ImGui::Separator();

  MoveChoice(state, count);

  // ⚠ LA BOÎTE SE RÈGLE SUR LE NOMBRE DE COMPTES, jusqu'au plafond : voir
  // `kVisibleAccounts`. Une rangée au moins, pour que le cadre existe même
  // devant une liste vide.
  const int shown = std::clamp(count, 1, kVisibleAccounts);
  const float row = ImGui::GetTextLineHeightWithSpacing();
  ImGui::BeginChild("##comptes",
                    ImVec2(width, static_cast<float>(shown) * row + kListPaddingPixels),
                    ImGuiChildFlags_Borders);
  for (int rank = 0; rank < count; ++rank) {
    const WebAccountRow& account = view.accounts[static_cast<std::size_t>(rank)];
    ImGui::PushID(rank);
    // ⚠ GRISÉ ET PAS SEULEMENT ROUGE : un compte banni ne se jouera pas, et le
    // grisé le dit dans le langage de l'interface plutôt que par une couleur
    // qu'il faut avoir apprise.
    ImGui::BeginDisabled(account.banned);
    const bool busy = (account.online || account.autotrade) && !account.banned;
    if (busy) {
      ImGui::PushStyleColor(ImGuiCol_Text,
                            account.autotrade ? kAutotradeRowColor : kOnlineRowColor);
    }
    // ⚠ LE TEXTE EST LE LIBELLÉ DU `Selectable`, et non un dessin par-dessus
    // en déplaçant le curseur : un déplacement en dernière position n'a rien
    // derrière lui pour valider l'agrandissement, et ImGui lève son bandeau
    // rose.
    if (ImGui::Selectable(AccountLine(account).c_str(), state.chosen == rank,
                          ImGuiSelectableFlags_AllowDoubleClick)) {
      state.chosen = rank;
      // ⚠ LE DOUBLE CLIC JOUE, comme sur la tablée : c'est le geste qu'on fait
      // sans y penser devant une liste.
      if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && account.usable()) {
        action.verb = WebDoorVerb::kChoose;
        action.account = rank;
      }
    }
    if (busy) {
      ImGui::PopStyleColor();
    }
    // ⚠ AMENER LE CHOIX DANS LA VUE, à la première image et à chaque
    // déplacement au clavier : sans cela il sort de la boîte par le bas et
    // l'on croit que la flèche ne fait rien.
    if (state.chosen == rank && (ImGui::IsWindowAppearing() || state.scroll_to_chosen)) {
      ImGui::SetScrollHereY();
    }
    ImGui::EndDisabled();
    ImGui::PopID();
  }
  ImGui::EndChild();
  state.scroll_to_chosen = false;

  ImGui::Spacing();
  const bool can_play = state.chosen >= 0 && state.chosen < count &&
                        view.accounts[static_cast<std::size_t>(state.chosen)].usable();
  if (can_play) {
    DrawConsequence(view.accounts[static_cast<std::size_t>(state.chosen)], width);
    // ⚠ ENTRÉE JOUE LE COMPTE EN COURS, comme au formulaire.
    if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
        ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)) {
      action.verb = WebDoorVerb::kChoose;
      action.account = state.chosen;
    }
  }

  ImGui::BeginDisabled(!can_play);
  if (SkinButton(core::Tr("Jouer"), art, ImVec2(width, 0.0F))) {
    action.verb = WebDoorVerb::kChoose;
    action.account = state.chosen;
  }
  ImGui::EndDisabled();
  if (SkinButton(core::Tr("Retour"), art, ImVec2(width, 0.0F))) {
    action.verb = WebDoorVerb::kRestart;
  }
  return action;
}

}  // namespace

WebDoorAction DrawWebDoor(WebDoorState& state, const WebDoorView& view, const SkinArt& art,
                          float width) {
  WebDoorAction action;
  switch (view.stage) {
    case WebDoorStage::kForm:
      return DrawSignIn(state, view, art, width);

    case WebDoorStage::kWaiting:
      ColoredText(kWaitingColor, core::Tr("Connexion au site…"));
      return action;

    case WebDoorStage::kInBrowser:
      // ⚠ ON NE DIT PAS « CLIQUE ICI » : le navigateur est déjà ouvert, et
      // c'est là que le joueur doit être. La seule chose utile à écrire est
      // qu'il peut revenir sans rien faire de plus.
      ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width);
      ImGui::TextUnformatted(
          core::Tr("Termine la connexion dans ton navigateur, puis reviens ici : "
                   "la liste de tes comptes s'affichera toute seule."));
      ImGui::PopTextWrapPos();
      ImGui::Spacing();
      if (SkinButton(core::Tr("Annuler"), art, ImVec2(width, 0.0F))) {
        action.verb = WebDoorVerb::kRestart;
      }
      return action;

    case WebDoorStage::kAccounts:
      return DrawAccounts(state, view, art, width);

    case WebDoorStage::kEnteringGame:
      // ⚠ RIEN À ANNULER ICI : le jeton est parti, et ce qui répond est le
      // serveur de jeu. Son refus, s'il y en a un, s'écrira sous cette
      // ligne — voir `DrawRefusal`.
      ColoredText(kWaitingColor, core::Tr("Connexion…"));
      return action;

    case WebDoorStage::kFailed:
      ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width);
      ColoredText(kRefusalColor, view.message.c_str());
      ImGui::PopTextWrapPos();
      ImGui::Spacing();
      // 🔴 LE RETOUR AU CHOIX QUAND LE JETON VIT ENCORE : après « ce compte est
      // déjà connecté », rien
      // ne justifie de retaper son mot de passe Moonlight. Le jeton signé
      // suffit à redemander un jeton de jeu.
      if (view.ticket_alive &&
          SkinButton(core::Tr("Choisir un compte"), art, ImVec2(width, 0.0F))) {
        action.verb = WebDoorVerb::kBackToAccounts;
      }
      if (SkinButton(core::Tr("Réessayer"), art, ImVec2(width, 0.0F))) {
        action.verb = WebDoorVerb::kRestart;
      }
      return action;
  }
  return action;
}

}  // namespace ro::ui
