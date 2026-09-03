// claw4/firmware/main/interaction/stt_mapper.h
// Deterministic spoken-phrase -> CommandKind mapping (V4 §7, WB-LEARNING-V4 P14).
// Portable C++17. NO ASR / LLM / TTS / wake-word: MVP routing is a pure
// lookup table over a fixed canonical phrase set (see stt_mapper.cpp).
#pragma once

#include <string>

#include "interaction/command.h"

namespace claw4 {
namespace interaction {

// Result of mapping one recognized utterance.
struct SttMapping {
  bool recognized = false;
  CommandKind kind = CommandKind::Unknown;
};

// Maps a normalized utterance to the best canonical command. The caller (host
// glue / device adapter) resolves the concrete target task/session AFTER this
// pure mapping — never inside it.
SttMapping mapVoicePhrase(const std::string& phrase);

}  // namespace interaction
}  // namespace claw4
