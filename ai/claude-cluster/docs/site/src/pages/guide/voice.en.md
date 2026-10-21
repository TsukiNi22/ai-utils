+++
title = "Voice"
summary = "Talk to the global session: listening modes, confirmation, words, vocabulary, engines and the wake phrase."
section = "guide"
order = 50
keywords = "voice microphone push to talk wake word ok claude speech listen say piper kokoro whisper parakeet engines vocabulary"
+++

The voice is optional and local by default: `[voice] enabled = true`. It uses the tools `voice-listen` (speech to text, sherpa-onnx) and `voice-say` (text to speech, Piper); the palette action *Voice: what is missing* lists what to set up (tools, enrolment, voices, models, API keys).

## Listening modes {#modes}

| Mode (`[voice] mode`) | Listening | Prompt |
|---|---|---|
| `push` | push-to-talk: `F5` starts, `F5` again stops | every sentence heard while listening |
| `auto` | always; `F5` pauses / resumes | every sentence heard |
| `wake` | always | only after the wake phrase (`wake_word = "ok claude"`): *ok claude, list the sessions* sends it; the phrase alone arms the listening for `wake_seconds` |

`--voice auto|wake|push` sets it for one run. The speech model loads at start (about 10 s); `only_me = true` accepts only your enrolled voice (`voice-enroll`).

## Confirmation {#confirm}

What is heard shows live in the voice bar, then waits `confirm_seconds` (3) with its target and a countdown, with the buttons **Send now**, **Drop**, **Correct** (or Enter, Esc, Ctrl+C). Say a **send word** (*envoie*, *valide*, *go*, *send*) or a **cancel word** (*annule*, *stop*, *cancel*) alone and it acts at once. The words are in `voice-words.txt` of the config folder (settings *Voice > Voice words*), one per line under `[cancel]` and `[send]`. `<session>: ...` addresses one session by its exact name.

The global session receives the text marked `[voice]` and asks again when a word looks misheard or the request is ambiguous.

## Vocabulary {#vocabulary}

Proper names that recognition bends are written back before the sentence is shown: `vocabulary.txt` (settings *Voice > Vocabulary*) maps a name to the ways it is heard (`libutils = lib utils, lib you tils`). The names of your sessions, the projects of the map of your computer and your own names are added by themselves.

## Answers aloud {#speech}

The global session's answers are spoken as a short summary in their language (`speak`, `summary_chars`). `F6` or `Ctrl+C` stops it. `say_engine` picks the engine (local `piper`, `kokoro`, or the free-tier `elevenlabs` / `cartesia`; a failing engine falls back to Piper) and `say_voices` the voice per language.

## Engines {#engines}

`listen_model` chooses the recogniser: local `parakeet` (French, English, mixed), `whisper` (99 languages, slower), `kroko-fr` (French only, live, fastest), or the free-tier APIs `groq` and `deepgram` (a key stored by `claude-cluster auth login groq`). `claude-cluster engines` lists each engine with its limits, free use and whether it is ready.

## The web page and the phone {#web}

The web page uses the microphone of the browser (https is required): *tap to talk* or *hands-free*, each sentence recognised by the speech model of the computer. See [Interfaces](page:interfaces).

## Wake phrase while closed {#service}

`claude-cluster service enable` installs a user systemd unit whose light detector hears the wake phrase and opens claude-cluster with the voice on. Nothing heard is kept: the audio goes through a ring of 0.5 s that is overwritten in place. See [Settings](page:settings#service).
