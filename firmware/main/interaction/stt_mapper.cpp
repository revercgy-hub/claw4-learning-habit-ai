// claw4/firmware/main/interaction/stt_mapper.cpp
// Pure MVP phrase table (WB-LEARNING-V4 P14). No AI provider, no ASR.
//
// Matching order is deliberate:
//   1) exact canonical phrases (after trim + ASCII lower-case), then
//   2) keyword fallback scanned ACTION-words first, QUERY-words second, so
//      "完成任务" routes to CompleteTask (never hijacked by the 任务 query
//      keyword) while "今天进度" still reaches QueryTodayProgress.
//
// Canonical MVP phrase table (extend only with an explicit task-pack change):
//   StartTask           开始任务 / 开始学习 / 开始专注 / 开始
//   PauseTask           暂停任务 / 暂停学习 / 暂停
//   ResumeTask          继续任务 / 继续学习 / 继续 / 恢复任务 / 恢复学习 / 恢复
//   CompleteTask        完成任务 / 完成学习 / 完成 / 结束任务 / 结束学习 / 结束
//   SkipTask            跳过任务 / 跳过 / 放弃任务 / 放弃
//   QueryTodayTasks     今天任务 / 今天的任务 / 今天有什么任务 / 任务列表 / 今天作业
//   QueryCurrentTask    当前任务 / 在学什么 / 现在在学什么 / 现在学什么
//   QueryRemainingTime  剩余时间 / 还剩多久 / 还要多久 / 还有多久
//   QueryTodayProgress  今天进度 / 今日进度 / 进度
#include "interaction/stt_mapper.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <vector>

namespace claw4 {
namespace interaction {

namespace {

struct Exact {
  CommandKind kind;
  const char* phrase;
};

struct KeywordRule {
  CommandKind kind;
  const char* keywords[3];
};

// Full-phrase matches take priority over keyword fallback.
const Exact kExact[] = {
    {CommandKind::StartTask, "开始任务"},
    {CommandKind::StartTask, "开始学习"},
    {CommandKind::StartTask, "开始专注"},
    {CommandKind::StartTask, "开始"},
    {CommandKind::PauseTask, "暂停任务"},
    {CommandKind::PauseTask, "暂停学习"},
    {CommandKind::PauseTask, "暂停"},
    {CommandKind::ResumeTask, "继续任务"},
    {CommandKind::ResumeTask, "继续学习"},
    {CommandKind::ResumeTask, "继续"},
    {CommandKind::ResumeTask, "恢复任务"},
    {CommandKind::ResumeTask, "恢复学习"},
    {CommandKind::ResumeTask, "恢复"},
    {CommandKind::CompleteTask, "完成任务"},
    {CommandKind::CompleteTask, "完成学习"},
    {CommandKind::CompleteTask, "完成"},
    {CommandKind::CompleteTask, "结束任务"},
    {CommandKind::CompleteTask, "结束学习"},
    {CommandKind::CompleteTask, "结束"},
    {CommandKind::SkipTask, "跳过任务"},
    {CommandKind::SkipTask, "跳过"},
    {CommandKind::SkipTask, "放弃任务"},
    {CommandKind::SkipTask, "放弃"},
    {CommandKind::QueryTodayTasks, "今天任务"},
    {CommandKind::QueryTodayTasks, "今天的任务"},
    {CommandKind::QueryTodayTasks, "今天有什么任务"},
    {CommandKind::QueryTodayTasks, "任务列表"},
    {CommandKind::QueryTodayTasks, "今天作业"},
    {CommandKind::QueryCurrentTask, "当前任务"},
    {CommandKind::QueryCurrentTask, "在学什么"},
    {CommandKind::QueryCurrentTask, "现在在学什么"},
    {CommandKind::QueryCurrentTask, "现在学什么"},
    {CommandKind::QueryRemainingTime, "剩余时间"},
    {CommandKind::QueryRemainingTime, "还剩多久"},
    {CommandKind::QueryRemainingTime, "还要多久"},
    {CommandKind::QueryRemainingTime, "还有多久"},
    {CommandKind::QueryTodayProgress, "今天进度"},
    {CommandKind::QueryTodayProgress, "今日进度"},
    {CommandKind::QueryTodayProgress, "进度"},
};

// Keyword fallback, scanned in array order: ACTION words before QUERY words.
const KeywordRule kKeywords[] = {
    {CommandKind::StartTask, {"开始", "", ""}},
    {CommandKind::PauseTask, {"暂停", "", ""}},
    {CommandKind::ResumeTask, {"继续", "恢复", ""}},
    {CommandKind::CompleteTask, {"完成", "结束", ""}},
    {CommandKind::SkipTask, {"跳过", "放弃", ""}},
    {CommandKind::QueryTodayProgress, {"进度", "", ""}},
    {CommandKind::QueryRemainingTime, {"多久", "剩余", "还剩"}},
    {CommandKind::QueryCurrentTask, {"在学", "当前", ""}},
    {CommandKind::QueryTodayTasks, {"任务", "作业", ""}},
};

std::string normalize(const std::string& raw) {
  std::size_t b = 0;
  std::size_t e = raw.size();
  while (b < e && std::isspace(static_cast<unsigned char>(raw[b]))) ++b;
  while (e > b && std::isspace(static_cast<unsigned char>(raw[e - 1]))) --e;
  std::string s = raw.substr(b, e - b);
  for (char& c : s) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  }
  return s;
}

bool contains(const std::string& s, const char* needle) {
  return needle[0] != '\0' && s.find(needle) != std::string::npos;
}

}  // namespace

SttMapping mapVoicePhrase(const std::string& phrase) {
  const std::string s = normalize(phrase);
  if (s.empty()) return {};

  for (const auto& rule : kExact) {
    if (s == rule.phrase) return {true, rule.kind};
  }
  for (const auto& rule : kKeywords) {
    for (const char* kw : rule.keywords) {
      if (contains(s, kw)) return {true, rule.kind};
    }
  }
  return {};  // unknown phrase (e.g. free chat) -> recognized=false
}

}  // namespace interaction
}  // namespace claw4
