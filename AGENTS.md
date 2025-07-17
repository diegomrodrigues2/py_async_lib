# AGENTS Guide – py_async_lib

This file consolidates **practical instructions for maintainers, contributors and automated agents** on how to navigate and break down work in the `diegomrodrigues2/py_async_lib` repository *entirely from the command line* with the GitHub CLI (**`gh`**).

> **Why a dedicated guide?**
> • Consistent task decomposition keeps our async library evolving in small, reviewable steps.
> • Using `gh` eliminates context‑switching and makes it easy to automate maintenance via scripts or CI.

---

## 1. Setup

1. **Install the CLI**

```bash
# macOS
brew install gh
# Debian/Ubuntu
sudo apt install gh
```

2. **Authenticate** using the token stored in `$GITHUB_TOKEN`:

```bash
GITHUB_TOKEN=... gh auth status              # confirm login
# To explicitly log in with the token:
gh auth login --with-token <<< "$GITHUB_TOKEN"
```

3. **Tell `gh` which repo we are in** (optional shortcut):

```bash
export GH_REPO=diegomrodrigues2/py_async_lib   # fish/zsh syntax: set -xg GH_REPO …
```

---

## 2. Issue Taxonomy

| Type                 | Label(s)                             | Purpose & Conventions                                                                                                                              |
| -------------------- | ------------------------------------ | -------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Epic / Parent**    | `epic`                               | High‑level feature or refactor. Must contain a **task‑list** turned into linked issues (see § 4).                                                  |
| **Task / Sub‑issue** | `task` (default) + optional `area:*` | Incremental, self‑contained slice of work that “unblocks” the epic. Body **always starts** with `Parent: #<epic‑number>` so we can query it later. |
| **Spike / Research** | `spike`                              | Time‑boxed investigation. Same linking rules as tasks.                                                                                             |

Additional labels such as `bug`, `docs`, `infra`, etc. can be combined.

---

## 3. Listing Issues

### 3.1 All open items

```bash
gh issue list            # defaults to --state open
```

### 3.2 Filter by label or assignee

```bash
gh issue list --label epic          # all epics
gh issue list --author "@me"        # what you opened
```

### 3.3 Show tasks of a given Epic

```bash
EPIC=42
gh issue list --label task   --search "is:open in:body #$EPIC"
```

The `in:body` qualifier finds every sub‑issue whose body references the parent number.

---

## 4. Creating Sub‑issues

### 4.1 Interactive (recommended)

Inside the shell:

```bash
EPIC=42
TITLE="Async DNS resolver – socket reuse"
gh issue create   --title "$TITLE"   --body "Parent: #$EPIC

### Goal
Reuse the same UDP socket across resolution requests to cut down on FD usage.

### Definition of done
- [ ] Unit tests passing
- [ ] Benchmarks show ≤ 5 µs overhead per lookup"   --label task
```

### 4.2 Non‑interactive / scripted

```bash
gh issue create -t "task:… " -b "$(cat task.md)" -l task -l area:dns
```

### 4.3 Link the new issue back to the Epic automatically

Add a check‑list item in the parent issue’s body:

```markdown
- [ ] #<new‑issue‑number>
```

GitHub will resolve the reference and display cross‑links in both directions.

---

## 5. Updating and Tracking Progress

| Action                    | Command                                                 |
| ------------------------- | ------------------------------------------------------- |
| View an issue             | `gh issue view <number>`                                |
| Claim a task              | `gh issue edit <num> --add-assignee @me`                |
| Change labels / milestone | `gh issue edit <num> --add-label docs --milestone v2.0` |
| Comment or paste worklog  | `gh issue comment <num> --body "…"`                     |
| Close when done           | `gh issue close <num> -c "Implemented in #PR_NO"`       |

---

## 6. Handy Search Shortcuts

```bash
alias epic='gh issue list --label epic'
alias mytasks='gh issue list --label task --assignee "@me"'
alias orphan='gh issue list --label task --search "is:open -in:body Parent"'
```

---

## 7. Working With Scripts & CI

The same commands can be embedded in CI jobs to:

* Fail the build if an Epic has open blocking tasks.
* Post automatic comments with benchmark results (`gh issue comment …`).
* Open tracking issues programmatically via `gh issue create` in release pipelines.

For deeper automation reach out to the GitHub REST/GraphQL APIs through `gh api …`.

---

## 8. Keep It Tidy

* One pull request **per** task.
* Keep descriptions concise but actionable.
* Close tasks promptly and check them off in the Epic list so progress bars stay meaningful.

---

## 9. Context from Existing Issues

The repository currently tracks the step-by-step implementation of an asyncio-like event loop in C. Open issues (`gh issue list --repo diegomrodrigues2/py_async_lib`) outline each milestone:

1. **#1** – Prototype of a micro event loop purely in Python.
2. **#2** – Start the C skeleton: object structure and build config.
3. **#3** – Implement `call_soon` and `run_forever` for a CPU-bound scheduler.
4. **#4** – Add timers via `call_later` and a heap-based timeout queue.
5. **#5** – Integrate real I/O using `epoll`.
6. **#6** – Handle non-blocking writes and back-pressure.
7. **#7** – Support task cancellation and timeouts.
8. **#8** – Manage OS signals and subprocesses.
9. **#9** – Work toward becoming a fully fledged event loop compatible with `asyncio`.
10. **#10** – Profiling and final optimizations.
11. **#11** – Heap-based timers and `call_later`.
12. **#12** – Thread-safe callbacks (implemented).
13. **#13** – `run_in_executor` and async DNS resolution (implemented).
14. **#14** – High-level stream abstractions (implemented).

Referencing these issues in commit messages and pull requests helps maintain traceability as each feature is implemented.

Happy hacking! 🐙
