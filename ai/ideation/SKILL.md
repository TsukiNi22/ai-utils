---
name: ideation
description: Ideation with the user through the built-in questions popup (interactive-questions) - new ideas on a subject (a project, a feature, a design, a name, what to do next), recommendations ranked by impact / effort, an honest opinion on ideas the user gives, variants or improvements of ideas mentioned - grounded in the real project, asked in rounds (which to keep, which to dig into, what to change), then a recap of what was kept and what happens next (do it now, backlog, later). Use whenever the user asks "des idées ?", "propose des idées", "tu as des recommandations ?", "ton avis sur...", "améliore / modifie cette idée", "brainstorm", "qu'est-ce que tu ajouterais", "si tu as d'autres idées propose-les", or summons /ideation.
---

# Ideation (ideas, recommendations, opinions, variants)

The user wants **ideas or an opinion**, not code yet: think, propose, let them choose with `interactive-questions`
(AskUserQuestion popup, French labels, never a numbered list to answer in the text), then act on what was chosen.

**Subject given with the skill:** the text after `/ideation` if any, otherwise the latest request of the user (the
project, feature, text or ideas it talks about).

## 1. Ground it first
- Read what the ideas are about before inventing: the code / README / docs of the project, its open tasks, the
  conversation, memory (what the user already asked for, refused or likes). Quick read-only look: no change yet.
- Never propose what already exists or what the user already refused; say so in one line when an idea of theirs
  already exists ("existe déjà : ...").
- Unclear subject (which project, which part, what for): ask it first, one round (like `clarify`).

## 2. Pick the mode from the request
| The user asks | Mode | What to produce |
|---|---|---|
| "des idées", "brainstorm", "qu'ajouterais-tu", "propose" | **New ideas** | 8 to 16 ideas grouped by theme (3-4 themes), each one line: what it does + why it helps |
| "recommandations", "que me conseilles-tu", "par quoi commencer" | **Recommendations** | the best few, ranked by impact / effort / risk, each with its reason |
| "ton avis", "c'est une bonne idée ?", "qu'en penses-tu" | **Opinion** | per idea: strengths, weaknesses, risks, an honest verdict (keep / change / drop) and a better alternative when there is one |
| "améliore", "modifie", "d'autres versions de", "mieux" | **Variants** | 2 to 4 variants per idea (smaller, bolder, cheaper, different angle), compared with previews |
| a mix, or "et si tu as d'autres idées" at the end of a task | the mix | the matching parts, in that order |

Each idea is **concrete** (what, where, how it would look), sized (effort S / M / L, risk) and honest: a weak idea of
the user is said weak with the reason, never praised to please; uncertainty is said ("à vérifier : ...").

## 3. Ask (skill `interactive-questions`, load it)
- A few lines of text first when the ideas need explaining (the themes, the trade-offs), then the popup.
- **New ideas / recommendations**: one question per theme, `multiSelect: true` ("Lesquelles garder ?"), 2-4 options
  each (an idea = a label of 1-5 words + its description with effort and gain); more ideas than fit: several rounds,
  the strongest first. Recommended ones marked `(Recommended)`.
- **Opinion**: one question per idea: keep as is / keep with the change proposed / replace by the alternative / drop.
- **Variants**: single choice per idea with a `preview` for each variant (ASCII mockup, code, config, text, a name).
- Then, for the kept ones, a round on **how deep / when**: do it now, plan it (tasks / backlog), a mockup or POC
  first, later. Never two questions that ask the same thing.
- "Explique plus" / "clarifie" as an answer: detail the options (an example, the cost, the risk) and ask again;
  a free text (*Other*) is read exactly: it often adds an idea or changes one - take it in the next round.

## 4. Close
- Recap in a few lines (or a small table): what was kept, changed, dropped, and the next step chosen.
- Kept ideas that are not done now go where they last: the task list (TaskCreate) or a file / backlog the user
  names; ask only when the place is unclear.
- "Do it now": start the work at once with the skills that fit (`/dev` routing); a mockup / POC asked: build it,
  then ask for its validation (again through the popup) before the real work.
- The answers are remembered for the rest of the conversation: a refused idea is not proposed again.
