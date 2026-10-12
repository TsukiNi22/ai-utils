/**************************************************************\
Edition:
##  @date 13/10/2026 by @author Tsukini

File Name:
##  @file VoiceEngines.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "cluster/VoiceEngines.hpp"
#include "cluster/Tools.hpp"
#include "cluster/Auth.hpp"
#include <filesystem>

/* functions */
_cold const std::vector<cluster::VoiceEngine>& cluster::voice_engines(void)
{
    // free or free-tier only; the APIs: their key stored by claude-cluster auth login <provider>, read by the voice tools
    static const std::vector<cluster::VoiceEngine> engines = {
        {.id = "parakeet", .kind = "listen", .name = "NVIDIA Parakeet TDT 0.6B v3 (int8, sherpa-onnx)", .local = true,
            .languages = "25 European languages (fr, en, de, es, it, pt, nl, pl, ru, uk...), each piece of a sentence its own one",
            .strengths = "the best local accuracy here, French and English mixed, live text (~every second), offline and private",
            .limits = "~1 s after a sentence on this CPU, 640 MB, no Asian / Arabic language",
            .free = "unlimited (local)", .link = "https://huggingface.co/nvidia/parakeet-tdt-0.6b-v3",
            .folder = "models/sherpa-onnx-nemo-parakeet-tdt-0.6b-v3-int8", .install = "voice-models install parakeet"},
        {.id = "whisper", .kind = "listen", .name = "OpenAI Whisper small (int8, sherpa-onnx)", .local = true,
            .languages = "99 languages, mixed inside a sentence",
            .strengths = "the widest language coverage offline",
            .limits = "slower (3-6 s per sentence on this CPU, live text every 3 s or more), less accurate than Parakeet on French / English",
            .free = "unlimited (local)", .link = "https://github.com/openai/whisper",
            .folder = "models/sherpa-onnx-whisper-small", .install = "voice-models install whisper"},
        {.id = "kroko-fr", .kind = "listen", .name = "Kroko French streaming zipformer (sherpa-onnx)", .local = true,
            .languages = "French only",
            .strengths = "true streaming: the text word by word, the fastest, 68 MB",
            .limits = "French only (English words garbled), less accurate",
            .free = "unlimited (local)", .link = "https://huggingface.co/Banafo/Kroko-ASR",
            .folder = "models/sherpa-onnx-streaming-zipformer-fr-kroko-2025-08-06", .install = "voice-models install kroko-fr"},
        {.id = "groq", .kind = "listen", .name = "Whisper large-v3-turbo on Groq", .local = false, .provider = "groq",
            .languages = "99 languages ([voice] listen_language to force one)",
            .strengths = "the large Whisper model: the best accuracy, ~0.3-0.8 s per sentence, no load on this computer",
            .limits = "internet, the audio sent to Groq, no live text (one request per sentence), a failure: the local model",
            .free = "2 h of audio an hour, 8 h a day, ~20 requests a minute (each one counted 10 s at least)",
            .link = "https://console.groq.com/docs/speech-to-text", .keyLink = "https://console.groq.com/keys"},
        {.id = "deepgram", .kind = "listen", .name = "Deepgram Nova-3", .local = false, .provider = "deepgram",
            .languages = "40+ languages; multi (default): French, English, German, Spanish, Italian, Portuguese, Dutch, Russian, Hindi, Japanese mixed",
            .strengths = "switches language inside a sentence, ~0.3-0.6 s, numbers / punctuation formatted",
            .limits = "internet, the audio sent to Deepgram, no live text (one request per sentence), a failure: the local model",
            .free = "$200 of credit at the sign-up, no card, no expiry (~350-400 h of speech)",
            .link = "https://developers.deepgram.com/docs/models-languages-overview", .keyLink = "https://console.deepgram.com"},
        {.id = "piper", .kind = "say", .name = "Piper", .local = true,
            .languages = "~40 languages, several voices each",
            .strengths = "instant (< 0.2 s), light, offline; the fallback of the other engines",
            .limits = "synthetic, flat voices, no emotion",
            .free = "unlimited (local)",
            .voices = "[voice] voice_fr / voice_en (tom-medium, en_GB-alan-medium...): voice-voices list, samples on the link",
            .link = "https://rhasspy.github.io/piper-samples/", .folder = "piper/piper", .install = "voice-voices set <voice> (downloads it)"},
        {.id = "kokoro", .kind = "say", .name = "Kokoro 82M (sherpa-onnx)", .local = true,
            .languages = "English (US / UK, 28 voices), French (1), Spanish, Italian, Portuguese, Hindi, Japanese, Chinese",
            .strengths = "much more natural than Piper, offline, Apache 2.0; the answer read sentence by sentence",
            .limits = "~0.8x real time on this CPU (a sentence ~1 s, ~4 s when its server starts), one French voice, no emotion, "
                "~600 MB of memory while loaded (its server ends after 15 min unused)",
            .free = "unlimited (local)",
            .voices = "ff_siwis (fr), af_heart / am_michael (en-us), bf_emma / bm_george (en-gb), ef_dora (es), if_sara (it), pf_dora (pt): "
                "voice-tts --list kokoro",
            .link = "https://huggingface.co/hexgrad/Kokoro-82M", .folder = "models/kokoro-multi-lang-v1_0", .install = "voice-models install kokoro"},
        {.id = "elevenlabs", .kind = "say", .name = "ElevenLabs Flash v2.5", .local = false, .provider = "elevenlabs",
            .languages = "32 languages",
            .strengths = "very natural, the largest voice library, ~0.3 s",
            .limits = "internet, the text sent, small free quota, its free audio not for commercial use; quota spent: Piper",
            .free = "10k credits a month (~20k characters with Flash, ~20 min of speech)",
            .voices = "a voice id (default: George, multilingual): voice-tts --list elevenlabs",
            .link = "https://elevenlabs.io/docs/api-reference/text-to-speech/stream", .keyLink = "https://elevenlabs.io/app/settings/api-keys"},
        {.id = "cartesia", .kind = "say", .name = "Cartesia Sonic 3.5", .local = false, .provider = "cartesia",
            .languages = "42 languages",
            .strengths = "the fastest measured (~0.2 s), very natural, speed and emotion controls",
            .limits = "internet, the text sent, the free credits used once (not monthly), not for commercial use; spent: Piper",
            .free = "20k credits (1 per character) once, then $5 a month for 100k",
            .voices = "a voice id (default: the first voice of the language): voice-tts --list cartesia",
            .link = "https://docs.cartesia.ai/api-reference/tts/bytes", .keyLink = "https://play.cartesia.ai/keys"},
    };
    return engines;
}

_cold const cluster::VoiceEngine* cluster::voice_engine(const std::string& kind, const std::string& id)
{
    for (const cluster::VoiceEngine& engine: cluster::voice_engines())
        if (engine.kind == kind && engine.id == id) return &engine;
    return nullptr;
}

_cold std::vector<std::string> cluster::voice_engine_lines(const cluster::VoiceEngine& engine)
{
    std::vector<std::string> lines = {engine.name + (engine.local ? " - local" : " - API, its key: claude-cluster auth login " + engine.provider)};

    lines.push_back("Languages: " + engine.languages);
    lines.push_back("+ " + engine.strengths);
    lines.push_back("- " + engine.limits);
    lines.push_back("Free: " + engine.free);
    if (!engine.voices.empty()) lines.push_back("Voices: " + engine.voices);
    if (engine.local && !engine.install.empty())
        lines.push_back(cluster::voice_engine_installed(engine) ? std::string("Installed") : "Not installed: " + engine.install);
    lines.push_back(engine.link);
    if (!engine.keyLink.empty()) lines.push_back("Key: " + engine.keyLink);
    return lines;
}

_cold std::string cluster::voice_engine_help(const std::string& kind, const std::string& id)
{
    if (id.empty()) return "The choice of voice-models (parakeet when installed, else kroko-fr).";
    const cluster::VoiceEngine* engine = cluster::voice_engine(kind, id);
    if (engine == nullptr) return "Unknown engine.";
    std::string text;
    for (const std::string& line: cluster::voice_engine_lines(*engine))
        text += (text.empty() ? "" : "\n") + line;
    return text;
}

_cold cluster::Json cluster::voice_engines_json(const cluster::Config& config)
{
    const cluster::Auth auth(config);
    cluster::Json list = cluster::Json::array();

    for (const cluster::VoiceEngine& engine: cluster::voice_engines()) {
        bool ready = cluster::voice_engine_installed(engine);
        std::string state = engine.local ? (ready ? "installed" : "not installed: " + engine.install) : "";
        if (const std::optional<cluster::Provider> provider = engine.provider.empty() ? std::nullopt : auth.find(engine.provider)) {
            const auto [stored, detail] = auth.status(*provider);
            ready = stored;
            state = stored ? detail : "no key: claude-cluster auth login " + engine.provider + " (" + engine.keyLink + ")";
        }
        const std::string used = engine.kind == "listen" ? config.voice.listenModel : (config.voice.sayEngine.empty() ? "piper" : config.voice.sayEngine);
        list.push_back({{"id", engine.id}, {"kind", engine.kind == "listen" ? "speech to text" : "text to speech"}, {"name", engine.name},
            {"local", engine.local}, {"ready", ready}, {"state", state}, {"used", used == engine.id}, {"languages", engine.languages},
            {"strengths", engine.strengths}, {"limits", engine.limits}, {"free", engine.free}, {"voices", engine.voices},
            {"link", engine.link}, {"key_link", engine.keyLink},
            {"setting", engine.kind == "listen" ? "[voice] listen_model = \"" + engine.id + "\"" : "[voice] say_engine = \"" + engine.id + "\""}});
    }
    return list;
}
