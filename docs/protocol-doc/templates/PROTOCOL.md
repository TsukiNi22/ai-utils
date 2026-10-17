# Documentation

### Table of Contents
 - [{{NAME}} Protocol (v{{VERSION}})](#{{NAME_ANCHOR}}-protocol-v{{VERSION_ANCHOR}})
 - [Handshake](#handshake)
 - [Commands (Client)](#commands-client)
 - [Commands (Server)](#commands-server)
 - [Server Data Storage](#server-data-storage)

## {{NAME}} Protocol (v{{VERSION}})
> {{ACRONYM_EXPANDED}}

{{NAME}} is an application-layer protocol designed for {{PURPOSE}} over a reliable transport ({{TRANSPORT}}).

It provides:
 - {{FEATURE_1}}
 - {{FEATURE_2}}
 - Typed messages (data, events, commands, ...)
 - Framed transmission using control characters (ENQ, ETB, EOT)

### Payload

Each transmission MUST follow this structure:
```
ENQ <tag> ETB <payload> EOT
```
 - ENQ (`0x05`) = Start of transmission
 - ETB (`0x17`) = Separator
 - EOT (`0x04`) = End of transmission
 - tag = {{TAG}}
 - payload = message content ({{PAYLOAD_ENCODING}})

The first byte of the payload defines its type:
| Byte (hexa)  | Type                            |
| ------------ | ------------------------------- |
| SYN (`0x16`) | [Connection](#connection)       |
| EM  (`0x19`) | [Disconnection](#disconnection) |
| ACK (`0x06`) | [Validation](#validation)       |
| NAK (`0x15`) | [Error](#error)                 |
| SO  (`0x0E`) | [Key](#key)                     |
| ESC (`0x1B`) | [Data](#data)                   |
| SOH (`0x01`) | [Command](#command)             |

A peer receiving an unknown type byte MUST answer with an [Error](#error) (`127`) and MAY close the connection.

### Connection
```
SYN
```
 - SYN (`0x16`) = payload type byte (Connection)

### Disconnection
```
EM
```
 - EM (`0x19`) = payload type byte (Disconnection)

### Validation
```
ACK
```
 - ACK (`0x06`) = payload type byte (Validation)

### Error
```
NAK <code> ETB <message>
```
 - NAK (`0x15`) = payload type byte (Error)
 - ETB (`0x17`) = Separator
 - code = code of the error, defined in the table below ({{CODE_ENCODING}})
 - message = possible information on the error (Base64 encoded)

| Code (0-127) | Error           |
| ------------ | --------------- |
| `127`        | Unknown error   |
| `0`          | {{ERROR_0}}     |

### Key
```
SO <data> *(ETB <data>)
```
 - SO  (`0x0E`) = payload type byte (Key)
 - ETB (`0x17`) = Separator
 - data = key (Base64 encoded)

### Data
```
ESC <data> *(ETB <data>)
```
 - ESC (`0x1B`) = payload type byte (Data)
 - ETB (`0x17`) = Separator
 - data = information (Base64 encoded)

### Command
```
SOH <code> ETB <data> *(ETB <data>)
```
 - SOH (`0x01`) = payload type byte (Command)
 - ETB (`0x17`) = Separator
 - code = code of the command, defined in the table below ({{CODE_ENCODING}})
 - data = argument (Base64 encoded)

| Code (0-127) | Command                  |
| ------------ | ------------------------ |
| `0`          | [`/help`](#help)         |
| `1`          | [`/{{COMMAND}}`](#{{COMMAND}}) |

### Different Payload Type Resume
```
/* Connection    */ ENQ <tag> ETB SYN EOT
/* Disconnection */ ENQ <tag> ETB EM EOT
/* Validation    */ ENQ <tag> ETB ACK EOT
/* Error         */ ENQ <tag> ETB NAK <code> ETB <message> EOT
/* Key           */ ENQ <tag> ETB SO <data> *(ETB <data>) EOT
/* Data          */ ENQ <tag> ETB ESC <data> *(ETB <data>) EOT
/* Command       */ ENQ <tag> ETB SOH <code> ETB <data> *(ETB <data>) EOT
```

### Example
A Validation sent before the key exchange (tag of 0 byte), byte by byte:
```
05 17 06 04
ENQ ETB ACK EOT
```

## Handshake

When a new client connects, these steps are executed to allow {{HANDSHAKE_PURPOSE}}.

```
Client                                   Server
  | ENQ  ETB SYN EOT                        |
  |---------------------------------------->|
  |                        ENQ  ETB ACK EOT |
  |<----------------------------------------|
  | ENQ  ETB SO <RSA_public_key> EOT        |
  |---------------------------------------->|
  |  ENQ  ETB SO <AES_key> ETB <iv> EOT     |
  |<----------------------------------------|
  |      (every next frame: AES encrypted)  |
```

### First step (basic, tag = 0 byte)

The client sends a connection request (`ENQ  ETB SYN EOT`).

Server behavior:
 - If valid (connection request) → responds with Validation (`ENQ  ETB ACK EOT`)
 - If invalid (any other request) → responds with Disconnection and closes the connection (`ENQ  ETB EM EOT`)

### Second step (key exchange, tag = 0 byte)

This step establishes {{KEY_EXCHANGE}}.

Steps:
 1. {{STEP_1}}
 2. {{STEP_2}}

## Commands (Client)

### help
> [!NOTE]
> Display the command usage

> Server answers with `ENQ <tag> ETB ESC <line1> ETB ... EOT`
```
help
```

### {{COMMAND}}
> [!NOTE]
> {{COMMAND_PURPOSE}}

> Server answers with `ENQ <tag> ETB ACK EOT`
>
> Errors: `ENQ <tag> ETB NAK <code> ETB <message> EOT` ({{COMMAND_ERRORS}})
```
{{COMMAND}} "{{ARGUMENT}}"
```

## Commands (Server)

### help
> [!NOTE]
> Display the command usage
```
help
```

## Server Data Storage
> [!WARNING]
> {{STORAGE_WARNING}}

Storage structure:
```bash
{{STORAGE_DIR}}/
└── {{FILE}}.data
```

### Separator

| Byte (hexa)  | Usage                     |
| ------------ | ------------------------- |
| GS  (`0x1D`) | End of a data record      |
| ETB (`0x17`) | Information separator     |

Separator layers:
```
file   -> record GS record GS ...
record -> info ETB info ...
```

### Files Content
{{FILE}}.data (one field per line, `:` only shows where the bytes are):
```
{{FIELD_1}} ETB
{{FIELD_2}} ETB
... GS
```
