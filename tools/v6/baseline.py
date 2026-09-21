"""Read-only V6 source-pin and portable-core boundary verification (stdlib only)."""
import argparse
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
FORBIDDEN = re.compile(
    r'QueueVoicePhrase|OnVoicePhrase|abortCloudReply|VoiceSessionPort|'
    r'#\s*include\s*[<"](?:lvgl|esp_|freertos/|driver/|mbedtls/)'
)
INCLUDES = re.compile(r'^\s*#\s*include\s*"([^"]+)"', re.MULTILINE)


def git(path, *args):
    return subprocess.check_output(
        ['git', '-C', str(path), *args], text=True, encoding='utf-8',
        stderr=subprocess.PIPE).strip()


def verify_checkout(path, sha):
    if git(path, 'rev-parse', 'HEAD') != sha:
        raise ValueError(f'{path}: source SHA differs from lock')
    if git(path, 'status', '--porcelain', '--untracked-files=all'):
        raise ValueError(f'{path}: source checkout is dirty')


def verify_core(root):
    core = root / 'firmware/main'
    cmake = (root / 'integration/v6/learning_core/CMakeLists.txt').read_text(encoding='utf-8')
    sources = re.findall(r'"\$\{CORE\}/([^"\n]+\.cpp)"', cmake)
    if not sources:
        raise ValueError('No explicit learning core sources')
    pending = [core / source for source in sources]
    visited = set()
    while pending:
        path = pending.pop().resolve()
        if not path.is_relative_to(core.resolve()):
            raise ValueError(f'Core dependency escapes portable tree: {path}')
        if path in visited:
            continue
        visited.add(path)
        source = path.read_text(encoding='utf-8-sig')
        # Ignore prose comments, inspect code and include directives only.
        source = re.sub(r'/\*.*?\*/|//[^\n]*', '', source, flags=re.DOTALL)
        if FORBIDDEN.search(source):
            raise ValueError(f'Legacy voice or platform dependency: {path}')
        for include in INCLUDES.findall(source):
            local = path.parent / include
            target = local if local.is_file() else core / include
            if not target.is_file():
                raise ValueError(f'Unresolved core include: {path}: {include}')
            pending.append(target)
    return len(sources), len(visited)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--xiaozhi', type=Path)
    parser.add_argument('--idf', type=Path)
    parser.add_argument('--metalio', type=Path)
    args = parser.parse_args()
    lock = json.loads((ROOT / 'integration/v6/baseline.lock.json').read_text(encoding='utf-8'))
    sources, closure = verify_core(ROOT)
    result = {'core_sources': sources, 'core_dependency_files': closure, 'checkouts': {}}
    for name in ('xiaozhi', 'idf', 'metalio'):
        path = getattr(args, name)
        if path is None:
            result['checkouts'][name] = 'NOT_CHECKED'
            continue
        verify_checkout(path, lock['sources'][name]['sha'])
        result['checkouts'][name] = 'PIN_AND_CLEAN_TREE_PASS'
    result['device_build'] = 'NOT_RUN'
    result['hardware'] = 'NOT_RUN'
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
