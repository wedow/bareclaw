# BareClaw

Minimal agentic runtime in x86-64 FASM. No libc, no linker — just syscalls.

Skill markdown + LLM + shell + loop = autonomous agent.

> **Warning:** This executes arbitrary shell commands with no sandboxing. The LLM decides what to run and the runtime runs it.

## Comparison

|                | BareClaw | [SubZeroClaw](https://github.com/jmlago/subzeroclaw) | [NullClaw](https://github.com/nullclaw/nullclaw) | [ZeroClaw](https://github.com/zeroclaw-labs/zeroclaw) | [OpenClaw](https://github.com/openclaw/openclaw) |
|----------------|----------|--------------|-----------|-----------|-----------|
| Language       | x86-64 FASM   | C             | Zig           | Rust          | TypeScript    |
| Source         | ~2,900 lines  | ~380 lines    | ~45,000       | ~15,000       | ~430,000      |
| Binary         | 14 KB         | 54 KB         | 678 KB        | 3.4 MB        | 80+ MB        |
| RAM (runtime)  | ~2 MB         | ~2 MB         | ~1 MB         | < 5 MB        | 80-120 MB     |
| Startup        | < 1 ms        | < 1 ms        | < 8 ms        | < 10 ms       | > 500 ms      |
| Dependencies   | curl¹         | curl, cJSON   | libc (static) | ~100 crates   | ~800 npm      |
| Sessions       | persistent    | log-only      | SQLite        | SQLite        | PostgreSQL    |

¹ Runtime only — zero linked libraries, no libc.

## Features

- **Persistent sessions** — JSONL files in `~/.bareclaw/sessions/`, resume with `-s ID`
- **Lazy skill loading** — drop markdown in `~/.bareclaw/skills/`, indexed at startup
- **Message compaction** — keeps last 39 messages + system prompt when history grows
- **Retry with backoff** — exponential backoff on API errors, up to 5 retries
- **64 MB arena allocator** — single mmap, bump allocation; ~2 MB RSS in practice
- **One tool: shell** — `fork` + `execve` + pipe capture, no adapter layer

## Usage

```
bareclaw                      # new session, REPL
bareclaw "prompt"             # new session, one-shot
bareclaw -s ID                # resume session, REPL
bareclaw -s ID "prompt"       # resume session, one-shot
```

## Build

```
make          # build
make test     # run tests
make clean    # remove binaries
```

Requires: `fasm` (Flat Assembler 1.73+), `curl` (runtime).

## Config

All via environment variables:

| Variable | Default |
|----------|---------|
| `BARECLAW_API_KEY` | *(required)* |
| `BARECLAW_MODEL` | `minimax/minimax-m2.5` |
| `BARECLAW_ENDPOINT` | `https://openrouter.ai/api/v1/chat/completions` |
| `BARECLAW_SKILLS_DIR` | `~/.bareclaw/skills` |
| `BARECLAW_SESSIONS_DIR` | `~/.bareclaw/sessions` |

## License

MIT
