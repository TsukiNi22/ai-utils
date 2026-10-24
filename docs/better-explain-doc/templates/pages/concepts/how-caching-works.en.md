+++
title = "How the cache works"
summary = "Why a second run is fast, explained simply or technically."
section = "concepts"
order = 1
kind = "explain"
level = "simple"
keywords = "cache hash hit miss eviction"
+++

:::simple tldr
**In one sentence:** the program remembers what it already computed, so it does not compute it twice.
:::

:::technical tldr
**Definition:** a content-addressed cache - the key is the hash of the inputs, a hit costs O(1), a miss costs the full computation.
:::

## The idea {#idea}

:::simple
Like writing the answer of a long sum on a sticky note: next time, you read the note.
:::

:::technical
Each result is stored under `hash(inputs)`; the same inputs give the same key, so the stored result is valid by construction.
:::

## How it works {#how}

:::html
<figure class="stepper" data-steps="3">
  <ol class="captions">
    <li><b>Ask.</b> The program needs the result of <code>build(a)</code>.</li>
    <li><b>Look up.</b> It computes the key of <code>a</code> and finds it in the cache.</li>
    <li><b>Answer.</b> The stored result comes back at once: nothing is built.</li>
  </ol>
  <svg viewBox="0 0 760 140" role="img" aria-label="request, cache, result">
    <rect id="cq" class="box" x="40" y="45" width="160" height="50" rx="8"/><text class="lbl" x="120" y="75" text-anchor="middle">build(a)</text>
    <rect id="cc" class="box-alt" x="300" y="45" width="160" height="50" rx="8" data-hl="2"/><text class="lbl" x="380" y="75" text-anchor="middle">cache</text>
    <rect id="cr" class="box" x="560" y="45" width="160" height="50" rx="8" data-show="3-"/><text class="lbl" x="640" y="75" text-anchor="middle" data-show="3-">result</text>
    <path class="edge" data-from="cq" data-to="cc" marker-end="url(#arr)"/>
    <path class="edge" data-from="cc" data-to="cr" marker-end="url(#arr)" data-show="3-"/>
    <defs><marker id="arr" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto"><path class="arrow" d="M0 0L10 5L0 10z"/></marker></defs>
  </svg>
</figure>
:::

## Check yourself {#quiz}

:::html
<details class="quiz"><summary>Why can a cached result never be stale?</summary><p>Its key is the hash of its inputs: other inputs, other key.</p></details>
:::
