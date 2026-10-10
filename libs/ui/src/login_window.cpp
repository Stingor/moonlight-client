#include "ro/ui/login_window.h"

#include <algorithm>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/window_name.h"

namespace ro::ui {
namespace {

// LA LARGEUR COMMUNE DES CONTRÔLES : tout ce que la fenêtre offre.
//
// 🔴 DYNAMIQUE ET NON FIGÉE, parce que la fenêtre s'auto-dimensionne : elle se
// règle sur son élément le plus large, et un formulaire calibré en dur pour le
// français laisse une bande vide à droite dès qu'un libellé traduit dépasse.
//
// ⚠ À APPELER EN DÉBUT DE LIGNE : la mesure part du curseur, donc après un
// `SameLine` elle ne rend plus que ce qui reste.
//
// ⚠ RIEN NE DOIT DEMANDER PLUS QUE CE QU'ELLE REND, sans quoi la fenêtre
// grandirait d'un cran à chaque image. Un contrôle exactement large de cette
// valeur contribue exactement la largeur courante : le calcul a un point fixe,
// et la taille se stabilise en une image ou deux. Le plancher, lui, ne pousse
// qu'une fois vers le haut.
[[nodiscard]] float FormWidth() {
  return std::max(ImGui::GetContentRegionAvail().x, kLoginFormMinWidth);
}

// La couleur d'un refus. Le rouge du client pour ce qui a échoué.
constexpr ImU32 kRefusalColor = IM_COL32(255, 96, 96, 255);

// LE VOILE D'ATTENTE : sombre, et pas tout à fait opaque.
//
// ⚠ ON LAISSE DEVINER CE QU'IL Y A DESSOUS : un noir plein donnerait un écran
// éteint, qui est exactement ce dont on essaie de se distinguer.
constexpr ImU32 kCoverColor = IM_COL32(0, 0, 0, 205);
constexpr ImU32 kCoverTextColor = IM_COL32(232, 232, 238, 255);

// Vrai si le champ porte quelque chose.
[[nodiscard]] bool Filled(const std::array<char, kLoginFieldBytes>& field) {
  return field[0] != '\0';
}

// Le formulaire proprement dit. Rend vrai quand il demande à partir.
[[nodiscard]] bool DrawFields(LoginWindowState& state, bool inert, const SkinArt& art) {
  ImGui::TextUnformatted(core::Tr("Compte"));
  ImGui::SetNextItemWidth(FormWidth());
  ImGui::InputText("##compte", state.account.data(), state.account.size());

  ImGui::TextUnformatted(core::Tr("Mot de passe"));
  ImGui::SetNextItemWidth(FormWidth());
  // ⚠ LE MASQUAGE EST UN DRAPEAU, et c'est le seul de ce fichier qui compte :
  // un mot de passe en clair à l'écran se lit par-dessus l'épaule, et le décor
  // vivant derrière n'y change rien.
  const bool submitted_here =
      ImGui::InputText("##motdepasse", state.password.data(), state.password.size(),
                       ImGuiInputTextFlags_Password | ImGuiInputTextFlags_EnterReturnsTrue);

  bool remember = state.remember_account;
  if (SkinCheckBox(core::Tr("Se souvenir du compte"), &remember, art)) {
    state.remember_account = remember;
  }

  ImGui::Spacing();
  const bool clicked = SkinButton(core::Tr("Se connecter"), art, ImVec2(FormWidth(), 0.0F));
  // ⚠ ENTRÉE VALIDE DEPUIS N'IMPORTE OÙ, et pas seulement depuis le champ du
  // mot de passe : c'est le geste qu'on fait après avoir tapé son compte.
  const bool entered = ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
                       ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false);

  // ⚠ LES DEUX CHAMPS REMPLIS, ET LA GARDE EST ICI plutôt que sur le bouton :
  // un bouton grisé ne dit pas POURQUOI il l'est, et le joueur clique quand même
  // sur Entrée. Rien ne part, et le formulaire reste tel qu'il est.
  return !inert && (clicked || submitted_here || entered) && Filled(state.account) &&
         Filled(state.password);
}

// Le code PIN. ⚠ La production ne le demande jamais ; le poste de test, si.
[[nodiscard]] bool DrawPincode(LoginWindowState& state, bool refused, const SkinArt& art) {
  ImGui::TextUnformatted(refused ? core::Tr("Code PIN refusé — recommencez")
                                 : core::Tr("Entrez votre code PIN"));
  ImGui::SetNextItemWidth(FormWidth());
  const bool submitted_here =
      ImGui::InputText("##pin", state.pincode.data(), state.pincode.size(),
                       ImGuiInputTextFlags_Password | ImGuiInputTextFlags_CharsDecimal |
                           ImGuiInputTextFlags_EnterReturnsTrue);

  ImGui::Spacing();
  const bool clicked = SkinButton(core::Tr("Valider"), art, ImVec2(FormWidth(), 0.0F));
  return clicked || submitted_here;
}

// LA LARGEUR PLANCHER D'UNE LISTE DÉROULANTE, en pixels.
//
// ⚠ EN DESSOUS, L'APERÇU NE DIT PLUS RIEN : « Malgun Gothic » se réduit à
// « Malg… ». La fenêtre s'élargit alors une fois pour l'accueillir, et tout le
// reste suit, puisque les autres contrôles lisent la même largeur.
constexpr float kComboMinWidth = 110.0F;

// COMMENT LES DEUX LISTES SE PARTAGENT LA LARGEUR.
//
// ⚠ QUARANTE / SOIXANTE, et ce n'est pas un goût : les noms de langue sont
// courts — « Français », « Español » —, les noms de police beaucoup moins.
constexpr float kLanguageShare = 0.4F;

// LES DEUX LISTES DU PREMIER ÉCRAN, SUR UNE SEULE RANGÉE.
//
// 🔴 ELLES SE PARTAGENT LA LARGEUR DU FORMULAIRE, libellés DÉDUITS. Cette
// rangée est la plus large de la fenêtre, donc la seule qui puisse en fixer la
// taille : des largeurs en dur l'élargiraient pour elle seule pendant que le
// reste du formulaire garderait la sienne, et laisseraient une bande vide à
// droite — d'autant plus visible que les libellés s'allongent en anglais et en
// espagnol.
//
// 🔴 ET LES LIBELLÉS SE MESURENT TRADUITS : « Idioma » n'a pas la largeur de
// « Langue ». Les mesurer en français donnerait une rangée juste dans une
// langue et fausse dans les deux autres.
void DrawPickers(const LoginFirstScreenView& first, const SkinArt& art,
                 LoginFirstScreenAction& asked) {
  const char* language_label = core::Tr("Langue");
  const char* font_label = core::Tr("Police");
  const ImGuiStyle& style = ImGui::GetStyle();
  const float labels = ImGui::CalcTextSize(language_label, nullptr, true).x +
                       ImGui::CalcTextSize(font_label, nullptr, true).x;
  // Une liste suivie de son libellé, deux fois : deux espacements internes, et
  // celui qui sépare les deux groupes.
  const float gaps = style.ItemInnerSpacing.x * 2.0F + style.ItemSpacing.x;
  const float together = std::max(FormWidth() - labels - gaps - SkinComboChromeWidth(art) * 2.0F,
                                  kComboMinWidth * 2.0F);
  const float language_width = std::max(together * kLanguageShare, kComboMinWidth);

  const bool language_known =
      first.language >= 0 && first.language < static_cast<int>(first.languages.size());
  ImGui::SetNextItemWidth(language_width);
  int language_step = 0;
  const bool language_open = BeginSkinCombo(
      language_label,
      language_known ? first.languages[static_cast<std::size_t>(first.language)].name.c_str() : "",
      art, &language_step);
  // Une langue que l'installation n'a pas se saute, comme la liste la grise.
  const int language_count = static_cast<int>(first.languages.size());
  for (int to = first.language + language_step;
       language_step != 0 && language_known && to >= 0 && to < language_count;
       to += language_step) {
    if (first.languages[static_cast<std::size_t>(to)].available) {
      asked.language = to;
      break;
    }
  }
  if (language_open) {
    for (int rank = 0; rank < static_cast<int>(first.languages.size()); ++rank) {
      const LoginLanguage& one = first.languages[static_cast<std::size_t>(rank)];
      ImGui::BeginDisabled(!one.available);
      if (ImGui::Selectable(one.name.c_str(), rank == first.language) && rank != first.language) {
        asked.language = rank;
      }
      ImGui::EndDisabled();
      if (rank == first.language) {
        ImGui::SetItemDefaultFocus();
      }
    }
    EndSkinCombo();
  }

  // 🔴 LA POLICE APRÈS LA LANGUE, et ce n'est pas qu'une question de mise en
  // page : changer de langue vide le catalogue, et tout libellé traduit lu
  // AVANT la bascule pointerait dans ce qui vient d'être jeté. Chez nous la
  // bascule n'a pas lieu ici — la fenêtre rend une INTENTION que `app`
  // exécutera après le dessin —, mais l'ordre reste le bon : il ne coûte rien,
  // et il tiendra le jour où quelqu'un posera la langue sur place.
  ImGui::SameLine();
  const bool font_known = first.font >= 0 && first.font < static_cast<int>(first.fonts.size());
  ImGui::SetNextItemWidth(together - language_width);
  int font_step = 0;
  const bool font_open = BeginSkinCombo(
      font_label, font_known ? first.fonts[static_cast<std::size_t>(first.font)].c_str() : "", art,
      &font_step);
  if (const int to = SkinComboStepped(first.font, font_step, static_cast<int>(first.fonts.size()));
      font_known && to != first.font) {
    asked.font = to;
  }
  if (font_open) {
    for (int rank = 0; rank < static_cast<int>(first.fonts.size()); ++rank) {
      if (ImGui::Selectable(first.fonts[static_cast<std::size_t>(rank)].c_str(),
                            rank == first.font) &&
          rank != first.font) {
        asked.font = rank;
      }
      if (rank == first.font) {
        ImGui::SetItemDefaultFocus();
      }
    }
    EndSkinCombo();
  }
}

// LES TROIS RÉGLAGES DU PREMIER ÉCRAN.
//
// 🔴 HORS DU SWITCH, ET DONC VISIBLES À TOUTES LES ÉTAPES — y compris sur un
// refus, où un joueur qui ne comprend pas le message a justement besoin d'en
// changer la langue.
[[nodiscard]] LoginFirstScreenAction DrawFirstScreen(const LoginFirstScreenView& first,
                                                     const SkinArt& art) {
  LoginFirstScreenAction asked;
  ImGui::Spacing();
  ImGui::Separator();
  DrawPickers(first, art, asked);

  // ── LE DÉCOR ──────────────────────────────────────────────────────────────
  //
  // ⚠ SA PLACE EST ICI, avec la langue et la police : les trois réglages du
  // PREMIER écran, ceux qu'on ne peut pas aller chercher dans le panneau du
  // jeu sans s'être connecté d'abord. Et celui-ci gouverne précisément ce
  // qu'on a sous les yeux.
  bool wanted = first.backdrop_wanted;
  if (SkinCheckBox(core::Tr("Ville en fond"), &wanted, art)) {
    asked.backdrop_toggled = true;
    asked.backdrop_wanted = wanted;
  }
  if (ImGui::IsItemHovered()) {
    SkinTooltipFormat("%s", core::Tr("Montre une ville du serveur en direct derrière cet "
                                     "écran, tirée au sort à chaque connexion. Décoche si "
                                     "ta machine ou ta connexion préfèrent s'en passer."));
  }

  // ⚠ SEULEMENT QUAND UNE VILLE EST EN PLACE : sans session à refermer, ce
  // bouton n'aurait rien à relancer. Il disparaît donc de lui-même une fois le
  // décor décoché, ou quand le serveur a coupé.
  if (first.backdrop_wanted && first.backdrop_live) {
    ImGui::SameLine();
    if (SkinSmallButton(core::Tr("Changer de vue"), art)) {
      asked.backdrop_reroll = true;
    }
    if (ImGui::IsItemHovered()) {
      SkinTooltipFormat("%s", core::Tr("Tire une autre ville. Le lieu est choisi par le "
                                       "serveur à l'ouverture d'une session : en changer "
                                       "demande d'en rouvrir une, ce qui prend un instant."));
    }
  }
  return asked;
}

// LE MOTIF DU DERNIER REFUS DU SERVEUR DE JEU.
//
// 🔴 SOUS LES DEUX PORTES. Un refus du jeu — compte banni,
// déjà connecté, jeton périmé — arrive APRÈS que le site a fait son travail,
// donc pendant que la porte Moonlight est à l'écran. Dessiné seulement sous le
// formulaire classique, il n'atteindrait personne : le joueur retomberait sur
// un formulaire de site qui aurait l'air de n'avoir rien fait.
//
// ⚠ SOUS LE FORMULAIRE ET NON DANS UNE BOÎTE : une modale par-dessus l'écran
// de connexion demande un clic de plus pour revenir là où l'on était déjà.
//
// ⚠ ET IL S'ENROULE sur la largeur du formulaire : « ce compte est banni ; le
// serveur n'a pas dit jusqu'à quand » tient sur deux lignes, et une ligne qui
// traverse l'écran ferait grandir la fenêtre à sa mesure.
void DrawRefusal(const std::string& message) {
  if (message.empty()) {
    return;
  }
  ImGui::Spacing();
  ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + FormWidth());
  ImGui::PushStyleColor(ImGuiCol_Text, kRefusalColor);
  ImGui::TextUnformatted(message.c_str());
  ImGui::PopStyleColor();
  ImGui::PopTextWrapPos();
}

}  // namespace

LoginWindowAction DrawLoginWindow(LoginWindowState& state, const LoginWindowView& view,
                                  const SkinArt& art) {
  LoginWindowAction action;

  // L'ILLUSTRATION DU CLIENT D'ORIGINE, quand le joueur a éteint la ville.
  //
  // ⚠ SUR LA LISTE DE FOND ET NON SUR CELLE D'UNE FENÊTRE : elle couvre l'écran
  // entier, et la liste d'une fenêtre est découpée à ses bords.
  //
  // ⚠ ÉTIRÉE PLEIN CADRE, comme la salle du banquet : l'image fait 1024 × 768,
  // et c'est ce que le natif en fait. Lui garder ses proportions laisserait
  // deux bandes noires sur un écran large.
  if (view.backdrop.usable()) {
    const ImGuiViewport* screen = ImGui::GetMainViewport();
    ImGui::GetBackgroundDrawList()->AddImage(
        view.backdrop.id, screen->Pos,
        ImVec2(screen->Pos.x + screen->Size.x, screen->Pos.y + screen->Size.y));
  }

  // LE VOILE D'ATTENTE, par-dessus tout le reste.
  //
  // ⚠ SUR LA LISTE DE PREMIER PLAN, qui se compose APRÈS toutes les fenêtres :
  // on peut donc le poser ici, avant même d'ouvrir la nôtre, et il couvrira
  // quand même ce qu'elle aura peint.
  //
  // ⚠ SA COULEUR DE TEXTE NE VIENT PAS DE LA PALETTE : celle-ci est faite pour
  // le corps CLAIR d'une fenêtre du jeu, et y serait illisible sur un voile
  // sombre.
  if (!view.busy.empty()) {
    const ImGuiViewport* screen = ImGui::GetMainViewport();
    ImDrawList* over = ImGui::GetForegroundDrawList();
    over->AddRectFilled(screen->Pos,
                        ImVec2(screen->Pos.x + screen->Size.x, screen->Pos.y + screen->Size.y),
                        kCoverColor);
    const ImVec2 measured = ImGui::CalcTextSize(view.busy.c_str());
    over->AddText(ImVec2(screen->Pos.x + (screen->Size.x - measured.x) * 0.5F,
                         screen->Pos.y + (screen->Size.y - measured.y) * 0.5F),
                  kCoverTextColor, view.busy.c_str());
  }

  // 🔴 `FirstUseEver` ET NON `Always` : reposée à chaque image, la position
  // annule tout déplacement — la fenêtre revient au centre sous le curseur de
  // qui essaie de la bouger, et rien n'indique pourquoi. Elle s'ouvre donc
  // centrée, puis appartient au joueur. Le décor lui donne une raison de la
  // déplacer : regarder la ville derrière.
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5F, 0.5F));

  // ⚠ LA PORTE SE DÉCIDE AVANT LE TITRE, parce que le titre en dépend : sans
  // site, il n'y a pas de porte Moonlight à annoncer.
  const bool moonlight = view.web_door_offered && state.door == LoginDoor::kMoonlight;

  // ⚠ DEUX TITRES, UNE SEULE IDENTITÉ — tous deux composent « …###login ».
  // C'est la même fenêtre qui change de nom sur sa barre : la place et la
  // taille que le joueur lui a données lui restent quand il change de porte.
  // ⚠ SANS CROIX — `nullptr` : une croix qui n'a rien derrière elle ne ferme
  // pas, elle quitte le jeu, et ce geste appartient à la fenêtre du système.
  static WindowName classic_name(kLoginWindowTitle, kLoginWindowId);
  static WindowName moonlight_name(kMoonlightWindowTitle, kLoginWindowId);
  // LES RÉGLAGES DU LOGIN, DANS LA BARRE DE TITRE : le bouton ne bouge pas quand
  // le corps change de taille d'une porte à l'autre. ⚠ GRISÉ PENDANT LA
  // CONNEXION, qui referme leur fenêtre.
  const bool settings_offered = view.stage != LoginStage::kConnecting;
  SetNextWindowTitleAction(core::Tr("Réglages"),
                           settings_offered
                               ? core::Tr("Affichage, son, skin et taille de l'interface")
                               : core::Tr("Pas pendant la connexion"),
                           settings_offered);
  const bool unfolded =
      BeginSkinnedWindow(moonlight ? moonlight_name.Get() : classic_name.Get(), nullptr, art,
                         SkinnedTitle::kBar, ImGuiWindowFlags_AlwaysAutoResize);
  // ⚠ LU AVANT LE DEMI-TOUR : repliée, la fenêtre montre encore sa barre et
  // ses boutons.
  action.open_settings = TitleActionClicked(0);
  if (!unfolded) {
    EndSkinnedWindow();
    return action;
  }

  // 🔴 PAS DE RETOUR ANTICIPÉ ICI, et c'est ce qui permet aux trois réglages du
  // premier écran d'être atteints par les DEUX portes. Sortir tout de suite
  // après la porte Moonlight les aurait rendus invisibles à qui s'y trouve —
  // c'est-à-dire, justement, à qui vient de recevoir un refus qu'il ne
  // comprend peut-être pas.
  if (moonlight) {
    action.web = DrawWebDoor(state.web, view.web, art, FormWidth());
    // 🔴 LE CHANGEMENT DE PORTE SE CONSOMME ICI, et n'atteint jamais `app` :
    // c'est cette fenêtre qui possède `state.door`, et la porte elle-même ne
    // peut pas se refermer sur elle-même. L'intention s'efface avec, sinon
    // `app` la verrait passer sans savoir qu'en faire.
    if (action.web.verb == WebDoorVerb::kUseClassic) {
      state.door = LoginDoor::kClassic;
      action.web.verb = WebDoorVerb::kNothing;
    }
    DrawRefusal(view.message);
    action.first_screen = DrawFirstScreen(view.first_screen, art);
    EndSkinnedWindow();
    return action;
  }

  const bool inert = view.stage == LoginStage::kConnecting;
  if (inert) {
    ImGui::BeginDisabled();
  }

  switch (view.stage) {
    case LoginStage::kForm:
    case LoginStage::kConnecting:
      if (DrawFields(state, inert, art)) {
        action.verb = LoginVerb::kSubmit;
      }
      break;
    case LoginStage::kPincode:
      if (DrawPincode(state, view.pincode_refused, art)) {
        action.verb = LoginVerb::kSubmitPincode;
      }
      break;
  }

  if (inert) {
    ImGui::EndDisabled();
    ImGui::Spacing();
    ImGui::TextUnformatted(core::Tr("Connexion…"));
  }

  DrawRefusal(view.message);

  // ── LE CHEMIN DU RETOUR ──────────────────────────────────────────────────
  //
  // 🔴 SYMÉTRIQUE DU REPLI : le formulaire classique se quitte comme il s'est
  // ouvert. Sans ce retour, qui a cliqué par curiosité devrait redémarrer le
  // client.
  //
  // ⚠ SEULEMENT S'IL Y A UNE PORTE OÙ REVENIR : sans site, ce formulaire est
  // le seul écran qui existe.
  if (view.web_door_offered) {
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextUnformatted(core::Tr("Tu as un compte Moonlight ?"));
    ImGui::PopStyleColor();
    if (SkinSmallButton(core::Tr("Utiliser le login Moonlight"), art)) {
      state.door = LoginDoor::kMoonlight;
    }
  }

  action.first_screen = DrawFirstScreen(view.first_screen, art);
  EndSkinnedWindow();
  return action;
}

}  // namespace ro::ui
