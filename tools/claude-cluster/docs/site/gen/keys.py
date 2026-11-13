#!/usr/bin/env python3
# =============================================================================
# keys.py - the table of the key bindings, generated from the [keys] section of templates/config.toml
# =============================================================================

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import _toml  # noqa: E402

# what each action does (the file only comments some of them), English / French
DESC = {
    "palette": ("command palette: every action", "palette de commandes : toutes les actions"),
    "next_session": ("next session", "session suivante"),
    "prev_session": ("previous session", "session précédente"),
    "global": ("the global session", "la session globale"),
    "new_session": ("new session", "nouvelle session"),
    "close_session": ("close the session (to the trash)", "fermer la session (vers la corbeille)"),
    "allow": ("allow the pending permission", "autoriser la permission en attente"),
    "always": ("allow and keep the rule", "autoriser et garder la règle"),
    "deny": ("deny the pending permission", "refuser la permission en attente"),
    "interrupt": ("stop the running turn; none: clear the prompt (copy when text is selected); twice on an empty prompt: quit", "arrêter le tour en cours ; aucun : vider le prompt (copier si du texte est sélectionné) ; deux fois sur un prompt vide : quitter"),
    "undo": ("undo in the prompt", "annuler dans le prompt"),
    "redo": ("redo in the prompt", "rétablir dans le prompt"),
    "select_all": ("select all of the prompt", "tout sélectionner dans le prompt"),
    "cut": ("cut the selection", "couper la sélection"),
    "paste": ("paste text, or an image of the clipboard as [Image #N]", "coller du texte, ou une image du presse-papiers en [Image #N]"),
    "stash": ("put the prompt aside (the stash)", "mettre le prompt de côté (la réserve)"),
    "unstash": ("take the last stashed prompt back", "reprendre le dernier prompt mis de côté"),
    "stashes": ("list of the stashed prompts: load, remove, clear all", "liste des prompts mis de côté : charger, retirer, tout vider"),
    "take_back": ("take a queued or running prompt back to the editor; on an empty prompt: the rewind chooser; twice on a written prompt: clear it", "reprendre dans l'éditeur un prompt en file ou en cours ; sur un prompt vide : le choix du point où revenir ; deux fois sur un prompt écrit : le vider"),
    "history": ("history of the prompts sent to the session", "historique des prompts envoyés à la session"),
    "edit": ("edit the prompt in an editor (`[ui] editor`, else nvim, vim, vi, $VISUAL, $EDITOR...)", "éditer le prompt dans un éditeur (`[ui] editor`, sinon nvim, vim, vi, $VISUAL, $EDITOR...)"),
    "expand": ("thinking and tool results in full / folded", "réflexion et résultats d'outils en entier / repliés"),
    "search": ("search in the transcript of the session", "chercher dans la transcription de la session"),
    "mode_cycle": ("next permission mode (auto, acceptEdits, plan, bypassPermissions, manual)", "mode de permission suivant (auto, acceptEdits, plan, bypassPermissions, manual)"),
    "help": ("every key", "toutes les touches"),
    "transcript_top": ("jump to the first message (many terminals keep Shift+PageUp: Ctrl+Home always arrives)", "aller au premier message (beaucoup de terminaux gardent Shift+PageUp : Ctrl+Home arrive toujours)"),
    "transcript_bottom": ("jump to the end; new output follows again", "aller à la fin ; les nouvelles sorties sont de nouveau suivies"),
    "session_up": ("session above (the list and the grid scroll to keep it in view)", "session au-dessus (la liste et la grille défilent pour la garder visible)"),
    "session_down": ("session below", "session en dessous"),
    "session_left": ("session on the left (grid) / previous one in the list", "session à gauche (grille) / précédente dans la liste"),
    "session_right": ("session on the right (grid) / next one in the list", "session à droite (grille) / suivante dans la liste"),
    "grid_open": ("grid: open the focused session in the list layout (Shift+Enter needs a terminal that reports it: kitty, foot, wezterm...; Alt+O always works)", "grille : ouvrir la session ciblée dans la disposition liste (Shift+Enter demande un terminal qui le transmet : kitty, foot, wezterm... ; Alt+O marche toujours)"),
    "session_page_up": ("a page of sessions up (the grid: its rows)", "une page de sessions vers le haut (la grille : ses lignes)"),
    "session_page_down": ("a page of sessions down", "une page de sessions vers le bas"),
    "session_first": ("the first session", "la première session"),
    "session_last": ("the last session", "la dernière session"),
    "push_to_talk": ("push-to-talk: start / stop listening", "appuyer pour parler : démarrer / arrêter l'écoute"),
    "mute": ("stop the voice reading the answer", "arrêter la lecture à voix haute"),
    "layout": ("next layout", "disposition suivante"),
    "settings": ("settings page", "page de réglages"),
    "stats": ("statistics (the Stats session)", "statistiques (la session Stats)"),
    "graph": ("the graph view: the global session and the sessions around it", "la vue graphe : la session globale et les sessions autour"),
    "graph_up": ("pan up (AZERTY: Z Q S D)", "déplacer vers le haut (AZERTY : Z Q S D)"),
    "graph_left": ("pan left", "déplacer vers la gauche"),
    "graph_down": ("pan down", "déplacer vers le bas"),
    "graph_right": ("pan right", "déplacer vers la droite"),
    "graph_zoom_in": ("zoom in (the wheel zooms around the pointer)", "zoomer (la molette zoome autour du pointeur)"),
    "graph_zoom_out": ("zoom out", "dézoomer"),
    "graph_fit": ("the whole graph in the view", "tout le graphe dans la vue"),
    "graph_focus": ("the selected node in the middle", "le nœud sélectionné au centre"),
    "graph_next": ("select the next node", "sélectionner le nœud suivant"),
    "graph_prev": ("select the previous node", "sélectionner le nœud précédent"),
    "graph_open": ("open the selected session (a double click too)", "ouvrir la session sélectionnée (un double clic aussi)"),
    "graph_legend": ("show / hide the legend line", "afficher / masquer la légende"),
    "graph_map": ("show / hide the minimap", "afficher / masquer la mini-carte"),
    "graph_close": ("leave the graph view (Alt+G again too)", "quitter la vue graphe (Alt+G aussi)"),
    "restore": ("reopen a closed session (the trash)", "rouvrir une session fermée (la corbeille)"),
    "prefix": ("prefix, then an arrow: the session on the left / right / above / below (like tmux)", "préfixe, puis une flèche : la session à gauche / à droite / au-dessus / en dessous (comme tmux)"),
    "quit": ("quit (from anywhere; a session still working: asked)", "quitter (de partout ; une session qui travaille encore : demandé)"),
}


def cell(text: str) -> str:
    return re.sub(r"\s+", " ", text.replace("|", "\\|")).strip()


def keys(ctx) -> list[dict]:
    for sec in _toml.parse((ctx.code_root / "templates" / "config.toml").read_text()):
        if sec["section"] == "keys":
            return sec["keys"]
    return []


def group(key: str) -> str:
    if key.startswith("graph"):
        return "graph"
    if key.startswith("session_") or key in ("next_session", "prev_session", "prefix", "grid_open", "global", "new_session", "close_session", "restore"):
        return "sessions"
    if key in ("allow", "always", "deny", "interrupt", "mode_cycle"):
        return "permissions"
    if key in ("push_to_talk", "mute"):
        return "voice"
    return "general"


TITLES = {"general": ("Prompt and transcript", "Prompt et transcription"), "sessions": ("Sessions and navigation", "Sessions et navigation"), "permissions": ("Permissions and turns", "Permissions et tours"),
          "voice": ("Voice", "Voix"), "graph": ("Graph view", "Vue graphe")}


def render(ks: list[dict], lang: str) -> str:
    en = lang == "en"
    out = ["Every action has a default binding that can be changed in `[keys]` of `config.toml` or in the settings page (*Keys*): `action = \"key\"`, alternatives separated by `|`. A key is `Ctrl+<letter>`, `Ctrl+Shift+<letter>`, `Alt+<letter>`, `F1`..`F12`, `Tab`, `Escape`. Most terminals keep `Ctrl+Shift+<letter>` for themselves or send it as `Ctrl+<letter>`: use the `Alt+` alternative there. Generated from `templates/config.toml`.\n" if en else
           "Chaque action a une touche par défaut modifiable dans `[keys]` de `config.toml` ou dans la page de réglages (*Keys*) : `action = \"touche\"`, alternatives séparées par `|`. Une touche s'écrit `Ctrl+<lettre>`, `Ctrl+Shift+<lettre>`, `Alt+<lettre>`, `F1`..`F12`, `Tab`, `Escape`. La plupart des terminaux gardent `Ctrl+Shift+<lettre>` pour eux ou l'envoient comme `Ctrl+<lettre>` : utilisez l'alternative `Alt+`. Générée depuis `templates/config.toml`.\n"]
    for gid in ("general", "sessions", "permissions", "voice", "graph"):
        rows = [k for k in ks if group(k["key"]) == gid]
        if not rows:
            continue
        out.append("## %s {#k-%s}\n" % (TITLES[gid][0 if en else 1], gid))
        out.append("| %s | %s | %s |" % (("Action (`[keys]`)", "Default", "Description") if en else ("Action (`[keys]`)", "Défaut", "Description")))
        out.append("|---|---|---|")
        for k in rows:
            binding = k["default"].strip('"')
            shown = " ".join("<kbd>%s</kbd>" % b for b in binding.split("|"))
            out.append("| `%s` | %s | %s |" % (k["key"], shown, cell(DESC.get(k["key"], (k["desc"], k["desc"]))[0 if en else 1])))
        out.append("")
    out.append("## %s {#fixed}\n" % ("Fixed keys" if en else "Touches fixes"))
    out.append("<kbd>Alt+1</kbd> .. <kbd>Alt+9</kbd> session N; <kbd>Alt+Enter</kbd> / <kbd>Shift+Enter</kbd> new line in the prompt; <kbd>Esc</kbd> take the prompt back or rewind (see [Prompt editing](page:prompt-editing)); <kbd>Esc</kbd> <kbd>Esc</kbd> clear a written prompt; <kbd>↑</kbd> / <kbd>↓</kbd> a visual row of the prompt, the history on its first / last row." if en else
               "<kbd>Alt+1</kbd> .. <kbd>Alt+9</kbd> session N ; <kbd>Alt+Enter</kbd> / <kbd>Shift+Enter</kbd> nouvelle ligne dans le prompt ; <kbd>Esc</kbd> reprendre le prompt ou revenir en arrière (voir [Édition du prompt](page:prompt-editing)) ; <kbd>Esc</kbd> <kbd>Esc</kbd> vider un prompt écrit ; <kbd>↑</kbd> / <kbd>↓</kbd> une ligne visuelle du prompt, l'historique sur sa première / dernière ligne.")
    return "\n".join(out)


def pages(ctx) -> list[dict]:
    ks = keys(ctx)
    return [{"slug": "keys-reference", "section": "reference", "order": 40,
             "en": {"title": "Keys reference", "summary": "Every configurable key binding with its default - generated from [keys].", "keywords": "keys shortcuts bindings ctrl alt f1 palette", "md": render(ks, "en")},
             "fr": {"title": "Référence des touches", "summary": "Toutes les touches configurables avec leur valeur par défaut - générée depuis [keys].", "keywords": "touches raccourcis ctrl alt f1 palette", "md": render(ks, "fr")}}]


def search_entries(ctx) -> list[dict]:
    return [{"k": "keys", "t": k["key"], "d": "%s - %s" % (k["default"].strip('"'), DESC.get(k["key"], (k["desc"],))[0][:100]), "u": "keys-reference.html#k-%s" % group(k["key"]), "x": k["default"]} for k in keys(ctx)]
