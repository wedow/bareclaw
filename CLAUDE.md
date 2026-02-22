# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What This Is

BareClaw is a minimal agentic runtime written in x86-64 FASM (Flat Assembler). It implements a chat loop with an OpenAI-compatible LLM API and can execute shell commands as tools. The binary is ~14KB, statically linked, with zero libc dependency — direct Linux syscalls only. External runtime deps: `curl` (HTTP), `/bin/sh` (tool execution).

## Build & Test

```bash
make          # build bareclaw binary via fasm
make test     # build and run all tests
make clean    # remove binaries
```

Requires `fasm` (Flat Assembler 1.73+). Tests are individual assembly binaries in `test/` — each includes the source modules it tests and uses macros from `src/test_macros.inc`.

## Architecture

Single compilation unit: `src/main.asm` includes all `.inc` modules. No separate object files or linking step.

**Core flow:**
1. `main.asm` — CLI arg parsing, REPL loop, session init
2. `agent.inc` — Agent turn loop: build request → POST to API (via curl) → parse response → execute tool calls or return content. Max 200 turns, exponential backoff on errors, message compaction at 40 messages.
3. `session.inc` — JSONL file persistence in `~/.bareclaw/sessions/<ID>.jsonl`. Each line is a complete JSON message. Supports create/open/append/load/compact.
4. `json.inc` — Hand-written cursor-based JSON scanner for OpenAI response format + request builder with string escaping. Not a general-purpose parser.
5. `config.inc` — Reads env vars: `BARECLAW_API_KEY` (required), `BARECLAW_MODEL`, `BARECLAW_ENDPOINT`, `BARECLAW_SKILLS_DIR`, `BARECLAW_SESSIONS_DIR`
6. `run_capture.inc` — Fork+pipe+execve to capture subprocess stdout/stderr
7. `arena.inc` — 64MB mmap bump allocator, 8-byte aligned, no free
8. `strings.inc` — str_len, str_eq, str_copy, str_starts_with, mem_copy, etc.
9. `syscalls.inc` — Linux x86-64 syscall wrappers (read, write, open, fork, execve, mmap, nanosleep, etc.)

**Calling convention:** System V AMD64 ABI (rdi, rsi, rdx for args; rax for return).

## Config (env vars)

```
BARECLAW_API_KEY       # required — API key
BARECLAW_MODEL         # default: anthropic/claude-haiku-4.5
BARECLAW_ENDPOINT      # default: https://openrouter.ai/api/v1/chat/completions
BARECLAW_SKILLS_DIR    # default: ~/.bareclaw/skills
BARECLAW_SESSIONS_DIR  # default: ~/.bareclaw/sessions
```
