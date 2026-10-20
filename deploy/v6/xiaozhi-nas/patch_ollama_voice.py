#!/usr/bin/env python3
"""Apply a narrow, source-hash-guarded XiaoZhi v0.9.6 Ollama voice patch."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import sys


EXPECTED_SHA256 = "0ce07df5f7f72b9e3a4ae4086eb62b4cc94f75ac5526ddda41a6600f335b6aa6"


def patch(path: Path) -> None:
    original = path.read_bytes()
    actual_sha256 = hashlib.sha256(original).hexdigest()
    if actual_sha256 != EXPECTED_SHA256:
        raise ValueError(
            f"refusing Ollama provider source: SHA-256 {actual_sha256} "
            f"does not match expected {EXPECTED_SHA256}"
        )

    source = original.decode("utf-8").replace("\r\n", "\n")
    replacements = [
        (
            """        self.client = OpenAI(
            base_url=self.base_url,
            api_key="ollama",  # Ollama doesn't need an API key but OpenAI client requires one
        )""",
            """        self.client = OpenAI(
            base_url=self.base_url,
            api_key="ollama",  # Ollama doesn't need an API key but OpenAI client requires one
            timeout=20.0,
            max_retries=0,
        )""",
            1,
        ),
        (
            """        # 检查是否是qwen3模型
        self.is_qwen3 = self.model_name and self.model_name.lower().startswith("qwen3")""",
            """        # 检查是否是qwen3模型
        self.is_qwen3 = self.model_name and self.model_name.lower().startswith("qwen3")
        self.is_qwen35 = self.model_name and self.model_name.lower().startswith("qwen3.5")""",
            1,
        ),
        (
            """        # 如果是qwen3模型，在用户最后一条消息中添加/no_think指令
        if self.is_qwen3:
            # 复制对话列表，避免修改原始对话
            dialogue_copy = dialogue.copy()

            # 找到最后一条用户消息
            for i in range(len(dialogue_copy) - 1, -1, -1):
                if dialogue_copy[i]["role"] == "user":
                    # 在用户消息前添加/no_think指令
                    dialogue_copy[i]["content"] = (
                        "/no_think " + dialogue_copy[i]["content"]
                    )
                    logger.bind(tag=TAG).debug(f"为qwen3模型添加/no_think指令")
                    break

            # 使用修改后的对话
            dialogue = dialogue_copy""",
            """        # 老版Qwen3保留/no_think提示；Qwen3.5通过API参数关闭思考。
        if self.is_qwen3 and not self.is_qwen35:
            import copy

            dialogue = copy.deepcopy(dialogue)
            for i in range(len(dialogue) - 1, -1, -1):
                if dialogue[i]["role"] == "user":
                    dialogue[i]["content"] = "/no_think " + dialogue[i]["content"]
                    logger.bind(tag=TAG).debug("为旧版qwen3模型添加/no_think指令")
                    break""",
            2,
        ),
        (
            """        responses = self.client.chat.completions.create(
            model=self.model_name, messages=dialogue, stream=True
        )""",
            """        request_options = {"temperature": 0.3, "max_tokens": 128}
        if self.is_qwen3:
            request_options["reasoning_effort"] = "none"
        responses = self.client.chat.completions.create(
            model=self.model_name,
            messages=dialogue,
            stream=True,
            **request_options,
        )""",
            1,
        ),
        (
            """        stream = self.client.chat.completions.create(
            model=self.model_name,
            messages=dialogue,
            stream=True,
            tools=functions,
        )""",
            """        request_options = {"temperature": 0.3, "max_tokens": 128}
        if self.is_qwen3:
            request_options["reasoning_effort"] = "none"
        stream = self.client.chat.completions.create(
            model=self.model_name,
            messages=dialogue,
            stream=True,
            tools=functions,
            **request_options,
        )""",
            1,
        ),
    ]

    for before, after, expected_count in replacements:
        count = source.count(before)
        if count != expected_count:
            raise ValueError(
                f"refusing unexpected Ollama provider layout: anchor count "
                f"{count}, expected {expected_count}"
            )
        source = source.replace(before, after)

    path.write_bytes(source.encode("utf-8"))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("provider", type=Path)
    args = parser.parse_args()
    try:
        patch(args.provider)
    except (OSError, UnicodeError, ValueError) as exc:
        print(f"Ollama provider patch failed: {exc}", file=sys.stderr)
        return 1
    print(f"Patched {args.provider}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
