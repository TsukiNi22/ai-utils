+++
title = "Wire protocol"
summary = "The frames between the client and the server, byte by byte."
section = "reference"
order = 10
kind = "protocol"
keywords = "protocol frame message handshake bytes"
+++

Every value is read from the code: the parser and the enums of the message types.

## Framing {#framing}

A transmission is `ENQ` + payload + `EOT`; the first byte of the payload is its type.

## Messages {#messages}

:::message HELLO 0x01
| field | type | size | description |
|---|---|---|---|
| type | u8 | 1 | `0x01` |
| version | u16 | 2 | protocol version, big endian |
| name | utf8 | var | the client name, up to 32 bytes |
The first frame of a connection; any other frame first: the server closes it.
:::

## Handshake {#handshake}

:::html
<ol class="seq">
  <li><div><span class="dir">client → server</span><br><code>HELLO</code> with its version.</div></li>
  <li><div><span class="dir">server → client</span><br><code>WELCOME</code> or <code>ERROR 0x10</code> (version refused).</div></li>
</ol>
:::
