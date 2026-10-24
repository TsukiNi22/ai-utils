+++
title = "Comment marche le cache"
summary = "Pourquoi une deuxième exécution est rapide, expliqué simplement ou techniquement."
keywords = "cache hash succès échec éviction"
+++

:::simple tldr
**En une phrase :** le programme se souvient de ce qu'il a déjà calculé, pour ne pas le calculer deux fois.
:::

:::technical tldr
**Définition :** un cache adressé par contenu - la clé est le hash des entrées, un succès coûte O(1), un échec le calcul complet.
:::

## L'idée {#idea}

:::simple
Comme noter le résultat d'une longue addition sur un post-it : la fois suivante, on lit le post-it.
:::

:::technical
Chaque résultat est rangé sous `hash(entrées)` ; les mêmes entrées donnent la même clé, le résultat rangé est donc valide par construction.
:::

## Comment ça marche {#how}

:::html
<figure class="stepper" data-steps="3">
  <ol class="captions">
    <li><b>Demande.</b> Le programme a besoin du résultat de <code>build(a)</code>.</li>
    <li><b>Recherche.</b> Il calcule la clé de <code>a</code> et la trouve dans le cache.</li>
    <li><b>Réponse.</b> Le résultat rangé revient tout de suite : rien n'est construit.</li>
  </ol>
  <svg viewBox="0 0 760 140" role="img" aria-label="demande, cache, résultat">
    <rect id="fq" class="box" x="40" y="45" width="160" height="50" rx="8"/><text class="lbl" x="120" y="75" text-anchor="middle">build(a)</text>
    <rect id="fc" class="box-alt" x="300" y="45" width="160" height="50" rx="8" data-hl="2"/><text class="lbl" x="380" y="75" text-anchor="middle">cache</text>
    <rect id="fr" class="box" x="560" y="45" width="160" height="50" rx="8" data-show="3-"/><text class="lbl" x="640" y="75" text-anchor="middle" data-show="3-">résultat</text>
    <path class="edge" data-from="fq" data-to="fc" marker-end="url(#arrf)"/>
    <path class="edge" data-from="fc" data-to="fr" marker-end="url(#arrf)" data-show="3-"/>
    <defs><marker id="arrf" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto"><path class="arrow" d="M0 0L10 5L0 10z"/></marker></defs>
  </svg>
</figure>
:::

## Testez-vous {#quiz}

:::html
<details class="quiz"><summary>Pourquoi un résultat du cache n'est-il jamais périmé ?</summary><p>Sa clé est le hash de ses entrées : autres entrées, autre clé.</p></details>
:::
