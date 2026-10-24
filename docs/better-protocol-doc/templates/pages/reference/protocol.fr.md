+++
title = "Protocole réseau"
summary = "Les trames entre le client et le serveur, octet par octet."
keywords = "protocole trame message handshake octets"
+++

Chaque valeur est lue dans le code : le parseur et les énumérations des types de messages.

## Trame {#framing}

Une transmission est `ENQ` + contenu + `EOT` ; le premier octet du contenu est son type.

## Messages {#messages}

:::message HELLO 0x01
| champ | type | taille | description |
|---|---|---|---|
| type | u8 | 1 | `0x01` |
| version | u16 | 2 | version du protocole, gros-boutiste |
| nom | utf8 | var | le nom du client, 32 octets au plus |
La première trame d'une connexion ; toute autre trame avant : le serveur la ferme.
:::

## Poignée de main {#handshake}

:::html
<ol class="seq">
  <li><div><span class="dir">client → serveur</span><br><code>HELLO</code> avec sa version.</div></li>
  <li><div><span class="dir">serveur → client</span><br><code>WELCOME</code> ou <code>ERROR 0x10</code> (version refusée).</div></li>
</ol>
:::
