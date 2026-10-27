"""Bounded sanitizer preflight and algorithm tests; never silently skip failure.

Example on this host:
python3 verified/tests/run_sanitizers.py --cxx /opt/homebrew/opt/llvm/bin/clang++ \
  --sysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk
"""
import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def run(command, timeout):
    print('+', ' '.join(map(str, command)), flush=True)
    try:
        result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True,
                                timeout=timeout)
    except subprocess.TimeoutExpired as exc:
        output = exc.stderr or b''
        if isinstance(output, bytes):
            output = output.decode(errors='replace')
        raise RuntimeError(f'timeout after {timeout}s; stderr: {output}') from exc
    print(result.stdout, end='', flush=True)
    print(result.stderr, end='', flush=True)
    if result.returncode:
        raise RuntimeError(f'exit code {result.returncode}')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cxx', default='c++')
    parser.add_argument('--sysroot')
    parser.add_argument('--sanitizers', default='address,undefined')
    parser.add_argument('--scale', action='store_true', help='include large/long-running boundary tests')
    args = parser.parse_args()
    flags = ['-std=c++17', '-O1', '-g', '-pthread',
             f'-fsanitize={args.sanitizers}', '-fno-sanitize-recover=all',
             '-fno-omit-frame-pointer']
    if args.sysroot:
        flags += ['-isysroot', args.sysroot]
    with tempfile.TemporaryDirectory(prefix='hgd-sanitizers-') as directory:
        directory = Path(directory)
        cases = [('sanitizer_smoke', False), ('test_intrinsic', False),
                 ('test_dynamic_boundaries', False), ('test_intrinsic', True),
                 ('test_dynamic_boundaries', True)]
        if args.scale:
            cases += [('test_scale_boundaries', False), ('test_scale_boundaries', True)]
        for test, reverse in cases:
            binary = directory / (test + ('_reverse' if reverse else ''))
            defines = (['-DHGD_REVERSE_REACH', '-DHGD_REVERSE_CSR',
                        '-DHGD_SPARSE_RESIDUAL_SYNC'] if reverse else [])
            run([args.cxx, *flags, *defines, str(ROOT/'verified/tests'/f'{test}.cpp'),
                 '-o', str(binary)], 120)
            command = [str(binary)]
            if test == 'test_dynamic_boundaries':
                command.append(str(directory/'batch.txt'))
            result = run(command, 10 if test == 'sanitizer_smoke' else 60)
            if test == 'sanitizer_smoke' and 'SANITIZER_MAIN_ENTERED' not in result.stderr:
                raise RuntimeError('preflight did not enter main')
    print('PASS: sanitizer preflight, intrinsic and dynamic boundary suites (SCC and CSR/sparse reverse variants)')


if __name__ == '__main__':
    try:
        main()
    except RuntimeError as exc:
        raise SystemExit(f'SANITIZER FAILED (not skipped): {exc}')
