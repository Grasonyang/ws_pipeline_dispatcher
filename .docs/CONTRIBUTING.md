# Contributing to ws_pipeline_dispatcher

Thank you for your interest in contributing to `ws_pipeline_dispatcher`! This document guides contributors through our development process, expectations, and best practices.

## Project Philosophy

Before you start contributing, please understand the core UNIX principles that `ws_pipeline_dispatcher` follows:

- **Single Responsibility**: Each applet (`stream_merge`, `log_parse`, `clip_store`) has one job
- **Composition over Complexity**: Tools are designed to be combined via pipes, not monolithic
- **Stream Discipline**: stdout only carries structured data; stderr only carries diagnostics
- **Minimal Dependencies**: Pure C implementation for resource-constrained embedded environments

When contributing, respect this philosophy. Avoid adding bloated features or breaking UNIX principles.

---

## 1. What Kind of Help Do We Need?

We welcome contributions in the following areas:

### New Features & Functions
- When edge devices (ESP32) or data sources support new capabilities, implement new functions
- Extend `stream_merge`, `log_parse`, or `clip_store` with additional features
- Add support for new data formats or protocols

### Documentation & Examples
- Improve and update the README and documentation
- Provide example code and usage patterns
- Fix typos or clarify unclear sections
- Expand the `.docs/` directory with architectural explanations

### Bug Reports & Fixes
- Report issues you encounter while using the tools
- Fix bugs in existing code
- Improve error handling and edge case coverage
- Fix documentation errors and inconsistencies

### Performance & Optimization
- Profile and optimize hot paths
- Reduce memory footprint for embedded environments
- Improve streaming data processing performance

### Testing Improvements
- Add more comprehensive unit tests
- Write integration tests for complex scenarios
- Test on different platforms and edge cases

### Finding Issues to Contribute To

We use GitHub labels to help contributors find work that matches their interests. **New contributors should start with these labels:**

- **`good first issue`** ⭐ - Great starting points for newcomers, usually small scope and low difficulty
- **`help wanted`** - Areas where we especially need assistance, higher priority
- **`bug`** - Known issues that need fixing
- **`documentation`** - Documentation improvements, no deep technical knowledge needed
- **`enhancement`** - Feature requests and improvements
- **`question`** - Design questions and discussions

**How to find issues:**
```bash
1. Go to the "Issues" tab on GitHub
2. Use the "Labels" filter on the left sidebar
3. Select "good first issue" or "help wanted"
4. Read the issue description and ask questions in comments if unsure
```

Don't worry about asking for clarification! Maintainers are happy to guide new contributors.

---

## 2. Development Conventions & Code Style

To maintain code consistency and readability, please follow these conventions.

### Development Environment Setup

#### Prerequisites

- **C Compiler**: GCC or Clang (C11 or later)
- **POSIX Environment**: Linux, macOS, or WSL2
- **Build Tools**: GNU Make, standard POSIX utilities (sh, grep, tail)
- **Optional**: `valgrind` for memory leak detection, `clang-format` for code formatting

#### Building & Compilation

```bash
# Clone the repository
git clone <repo-url>
cd ws_pipeline_dispatcher

# Compile all applets
make

# Compile with debug symbols, no optimization (recommended for development)
make clean
CFLAGS="-g -O0 -Wall -Wextra" make

# Compile and run tests
make test

# Run end-to-end smoke tests
make smoke
```

Build outputs go to `build/`, test artifacts to `.test_tmp/`.

### C Code Style

#### 1. Indentation & Formatting
- Use 4 spaces for indentation (not tabs)
- Line length: target 100 characters, hard limit 120
- Use auto-formatting tools like `clang-format`:
  ```bash
  clang-format -i applets/*.c lib/*.c
  ```
  Or `astyle`:
  ```bash
  astyle --style=bsd --indent=spaces=4 --pad-oper --pad-header applets/*.c lib/*.c
  ```

#### 2. Naming Conventions
- Use `snake_case` for functions and variables: `read_metadata_sidecar`, `buffer_size`
- Use `SCREAMING_SNAKE_CASE` for constants and macros: `MAX_BUFFER_SIZE`, `SENTINEL_MARKER`
- Prefix internal static functions with `_`: `_parse_metadata`, `_validate_record`
- Use meaningful names; avoid abbreviations unless universal (e.g., `ts_ms` for timestamp milliseconds)

#### 3. Function Design
- Keep functions under 100 lines where practical
- Document public functions with comments above them:
  ```c
  // Reads metadata from sidecar file and populates buffer.
  // Returns count of valid records on success, -1 on I/O error.
  int read_metadata_sidecar(const char *path, struct metadata_buf *buf);
  ```
- Use `static` for file-local functions

#### 4. Error Handling
- Check **all** system call return values, never assume success
- Use perror-style error logging to stderr
- Use meaningful exit codes:
  - `0` for success
  - `1` for general errors
  - `2` for usage errors (bad arguments)
  - Other codes for applet-specific failures

- Example:
  ```c
  if (read(fd, buf, n) < 0) {
      fprintf(stderr, "error: read failed on %s: %s\n", filename, strerror(errno));
      return -1;
  }
  ```

#### 5. Memory Management
- Free all allocated memory before returning from functions
- Use `calloc()` to zero-initialize structures where appropriate
- **Required**: Test all code with valgrind for memory leaks:
  ```bash
  valgrind --leak-check=full ./build/applet_name <args>
  ```

### Header Files

- Keep `.h` files in the `lib/` directory
- Use include guards:
  ```c
  #ifndef LIB_FOO_H
  #define LIB_FOO_H
  // ...
  #endif
  ```
- Document public APIs
- Avoid `#include` cycles; use forward declarations if needed

### Shell Script Conventions (Tests)

- Shebang: `#!/bin/sh` (POSIX, not bash)
- Safety: `set -eu` (error on unset variables, exit on first error)
- Cleanup trap: `trap 'rm -rf "$TMP_DIR"' EXIT`
- Local variables lowercase, environment variables UPPERCASE
- Quote all variable expansions: `"$var"`, not `$var`

## Stream Discipline & Logging (CRITICAL)

This is a **critical** part of our UNIX philosophy and **must be followed**:

### stdout - Data Output Only
- Reserved for structured output (JSON Lines format)
- **Absolutely no** progress messages, debug info, or logs
- One record per line, consistent format
- Example (from `stream_merge`):
  ```json
  {"type":"clip","session_id":"sess_1","complete":true,"offset":0,"length":128,"ts":1000}
  ```

### stderr - Diagnostics Only
- Reserved for diagnostic logs, warnings, and errors
- Use `stream_logger` helpers: `logger_warn(fmt, ...)`, `logger_error(fmt, ...)`
- Include context (filename, record number, operation type)
- Example:
  ```c
  logger_warn("skipping invalid JSON line %d: %s", line_num, line);
  logger_error("failed to open sidecar file: %s", strerror(errno));
  ```

## Testing

### Running Tests

```bash
# Run all unit and integration tests
make test

# Run applet-specific test (replace APP)
tests/test_APP.sh

# Run end-to-end smoke tests
make smoke

# Check for memory leaks (required)
valgrind --leak-check=full ./build/stream_merge <args>
valgrind --leak-check=full ./build/log_parse <args>
valgrind --leak-check=full ./build/clip_store <args>
```

### Writing Tests

**Shell Integration Tests** (`tests/test_*.sh`):

- One test file per major applet or component
- Use `set -eu` for safety
- Create temporary directories with `mktemp -d`, clean with trap
- Use helper functions:
  ```sh
  check_eq "test name" "expected" "actual"
  check_contains "test name" "substring" "haystack"
  ```
- Test normal paths, **error cases**, and **edge cases**
- Example:
  ```bash
  #!/bin/sh
  set -eu
  TMP_DIR=$(mktemp -d)
  trap 'rm -rf "$TMP_DIR"' EXIT
  
  # Normal test case
  echo '{"id":1,"msg":"test"}' | ./build/log_parse --filter id=1 > "$TMP_DIR/out"
  check_eq "filter id=1" '{"id":1,"msg":"test"}' "$(cat $TMP_DIR/out)"
  
  # Error case
  echo 'malformed' | ./build/log_parse --filter id=1 2>"$TMP_DIR/err"
  check_contains "error message" "error" "$(cat $TMP_DIR/err)"
  ```

**C Unit Tests**:

- Compile with `-g` and run under `valgrind`
- Test helper functions in `lib/` independently
- Example:
  ```c
  void test_buffer_append() {
      struct buffer buf = {0};
      buffer_append(&buf, "test", 4);
      assert(buf.len == 4);
      free(buf.data);
  }
  ```

### Test Coverage Goals

- Core logic (parsing, filtering, storage) should have >80% test coverage
- Error paths (I/O failures, malformed input) should be explicitly tested
- Race conditions in concurrent write scenarios should be validated

### Git Commit Message Format

All commits should follow this **standardized format**:

```
[action]: [description]
```

**Action types:**
- `init` - Project initialization
- `feat` - New feature or applet
- `fix` - Bug fix
- `docs` - Documentation changes only
- `style` - Code formatting (no logic change)
- `refactor` - Code restructure (no feature change)
- `test` - Test addition or fix
- `chore` - Build system, tools, dependencies

**Examples:**
```
[feat]: add regex parsing to log_parse
[fix]: handle EOF in stream_merge sidecar drain
[docs]: update API documentation in .docs/
[test]: add continuity break test for stream_merge
[chore]: update Makefile to include valgrind targets
```

**Commit message tips:**
- Use imperative mood: "add" not "added" or "adds"
- First line under 50 characters
- If more detail needed, blank line then detailed explanation
- Reference related issues: "fixes #123"

### Issue & Pull Request Templates

When creating issues or PRs, use the provided templates. They help us understand:
- **Issue template**: Bug reproduction steps, expected vs. actual behavior, environment details
- **Pull request template**: What changes you made, why, what testing you did

---

## 3. Development Workflow

This section walks through the complete process from start to finish. Following this workflow ensures smooth, efficient contributions.

### Step 1: Fork the Repository

First, create your own copy of the repository:

1. Go to [ws_pipeline_dispatcher GitHub repository](https://github.com/your-org/ws_pipeline_dispatcher)
2. Click the **"Fork"** button in the top-right corner
3. This creates a copy under your GitHub account (e.g., `your-username/ws_pipeline_dispatcher`)

**Why Fork?** Forking lets you work safely on your own repo without needing main repo permissions, and without affecting main repo development.

### Step 2: Clone Your Fork Locally

Clone your fork to your machine and set up upstream for syncing with the main repo:

```bash
# Clone YOUR fork (not the original repository)
git clone https://github.com/your-username/ws_pipeline_dispatcher.git
cd ws_pipeline_dispatcher

# Add the original repository as "upstream" for syncing
git remote add upstream https://github.com/original-org/ws_pipeline_dispatcher.git

# Verify you have both remotes
git remote -v
# origin    https://github.com/your-username/ws_pipeline_dispatcher.git (fetch)
# origin    https://github.com/your-username/ws_pipeline_dispatcher.git (push)
# upstream  https://github.com/original-org/ws_pipeline_dispatcher.git (fetch)
# upstream  https://github.com/original-org/ws_pipeline_dispatcher.git (push)
```

### Step 3: Create a Feature Branch

Create a branch for your work. Use descriptive names:

```bash
# Update main to latest first
git fetch upstream
git checkout main
git merge upstream/main

# Create your feature/fix branch
git checkout -b feat/my-new-feature
# Or for bug fixes:
git checkout -b fix/issue-123
```

**Branch naming conventions:**
- `feat/` - New features or applets
- `fix/` - Bug fixes
- `docs/` - Documentation updates
- `test/` - Test improvements
- `refactor/` - Code refactoring

### Step 4: Develop & Test

Develop your feature and thoroughly test locally:

```bash
# Make your changes
nano applets/stream_merge.c

# Build (with debug flags)
make clean
CFLAGS="-g -O0 -Wall -Wextra" make

# Run all tests (must pass)
make test
make smoke

# Check for memory leaks (required)
valgrind --leak-check=full ./build/stream_merge ...
valgrind --leak-check=full ./build/log_parse ...
valgrind --leak-check=full ./build/clip_store ...

# Verify code style
clang-format --dry-run -i applets/*.c lib/*.c
```

### Step 5: Commit Your Changes

Commit with descriptive messages following our format:

```bash
# View your changes
git status
git diff

# Stage changes
git add applets/stream_merge.c

# Commit with [action]: [description] format
git commit -m "[feat]: add window-size parameter to stream_merge"

# View your commits
git log --oneline -3
```

### Step 6: Push to Your Fork

Push your branch to your GitHub fork:

```bash
git push origin feat/my-new-feature
```

### Step 7: Create a Pull Request

1. Go to the [original repository](https://github.com/original-org/ws_pipeline_dispatcher)
2. You'll see a prompt to create a Pull Request from your fork
3. Click **"Compare & pull request"**
4. Fill in the PR template:
   - **Title**: Clear summary of changes (e.g., "Add regex support to log_parse")
   - **Description**: What and why (use template structure)
   - **Testing**: Describe tests you ran (`make test`, `make smoke`, valgrind, etc.)
   - **Related Issues**: Reference with `fixes #123` or `closes #456`

5. Click **"Create pull request"**

### Step 8: Address Code Review

Maintainers will review your PR:

- Respond to feedback promptly (usually within 24-48 hours)
- Make requested changes on the same branch
- Commit new changes (not forced overwrites; maintainers will squash before merge)
- Push new commits (PR updates automatically)

```bash
# Make changes based on review feedback
nano applets/stream_merge.c

# Test your changes
make test && make smoke
valgrind --leak-check=full ./build/stream_merge ...

# Commit and push
git add applets/stream_merge.c
git commit -m "[fix]: address review feedback on error handling"
git push origin feat/my-new-feature
```

### Step 9: Merge & Cleanup

Once approved and all tests pass:

1. Maintainer will merge your PR to `main`
2. Delete remote branch:
   ```bash
   git push origin --delete feat/my-new-feature
   ```
3. Delete local branch:
   ```bash
   git branch -d feat/my-new-feature
   ```
4. Update your local main:
   ```bash
   git fetch upstream
   git checkout main
   git merge upstream/main
   ```

### Syncing with Upstream (Keeping Your Fork Updated)

If your PR takes time or you want to stay updated with the main repository:

```bash
# Fetch latest from upstream
git fetch upstream

# Rebase your branch on latest upstream/main
git rebase upstream/main feat/my-new-feature

# Force push your updated branch (only to your own fork!)
git push origin --force-with-lease feat/my-new-feature
```

---

## Performance Considerations

- **Stream Processing**: Applets should process streaming data with constant memory (don't buffer entire input)
- **File I/O**: Use buffered I/O; avoid byte-at-a-time reads/writes
- **Large Files**: Use `mmap()` or sequential I/O for large `.bin` files, avoid random access
- **Profiling**: When adding new critical paths, profile with `perf record`:
  ```bash
  perf record -g ./build/stream_merge ...
  perf report
  ```

---

## Troubleshooting

### My PR has conflicts
```bash
# Fetch and rebase on latest
git fetch upstream
git rebase upstream/main
# Resolve conflicts in your editor (search for <<< === >>>)
git add .
git rebase --continue
git push origin --force-with-lease feat/my-new-feature
```

### I need to undo my last commit
```bash
git reset --soft HEAD~1  # Undo commit, keep changes (can recommit)
git reset --hard HEAD~1  # Undo commit and discard changes (use with caution)
```

### I want to update my fork to latest
```bash
git fetch upstream
git checkout main
git merge upstream/main
git push origin main
```

### I accidentally committed to main instead of a feature branch
```bash
# Undo commit, keep changes
git reset HEAD~1

# Create new branch; commit will follow you
git checkout -b fix/my-fix

# Return to main and reset to upstream
git checkout main
git reset --hard upstream/main
```

---

## Code Review Expectations

When your code is reviewed, expect feedback on:

- **Correctness**: Does it work as intended? Any logic errors?
- **Style**: Does it follow our code style guidelines?
- **Testing**: Is testing adequate? Does it cover edge cases?
- **Documentation**: Are there appropriate code comments? Are README/docs updated?
- **Performance**: Could it be more efficient? Does it introduce unnecessary memory usage?
- **UNIX Philosophy**: Does it follow UNIX principles? Stream discipline maintained?

**Code review is not criticism** - our goal is helping you write better code and ensuring project quality.

---

## Documentation & Comments

### Code Comments

- Comment the **why**, not the **what**
- Explain non-obvious design decisions
- Keep comments concise but meaningful
- Example:
  ```c
  // Continuity check: if sequence number jumps > 1, emit partial clip.
  // (Edge devices may drop packets; we don't reconstruct, we restart.)
  if (expected_seq != actual_seq) {
      emit_partial_clip();
  }
  ```

### README & Documentation

- Keep `README.md` high-level and user-focused
- Put design decisions in `.docs/`
- Put contract specs (JSON schema, CLI args) in `.docs/core/contract.md`
- Put applet implementation details in `.docs/applets/`
- Update compliance matrix (`.docs/core/compliance.md`) when adding requirements

---

## Reporting Issues

When reporting bugs, please provide:

- **Reproduction steps**: How to reproduce the issue (as detailed as possible)
- **Expected behavior**: What should happen
- **Actual behavior**: What actually happened
- **Sample input**: Minimal example input that causes the issue
- **Error messages**: Full stderr output
- **Environment**:
  - Operating system (Linux version, macOS version, etc.)
  - Compiler version (`gcc --version`)
  - Build flags (what CFLAGS did you compile with)

**Example issue title:**
```
[BUG] stream_merge crashes on empty metadata file
[BUG] log_parse --filter type=data skips valid records
```

---

## Questions?

- **Design questions?** Open an issue with the `question` label, explain your thoughts
- **Getting stuck?** Ask in PR or issue comments. We're happy to help!
- **Security issues?** 🔒 Email maintainers privately, don't open public issues
- **General feedback?** Open a discussion in GitHub Discussions

---

## Pre-Submission Checklist

Before submitting a PR, make sure:

- [ ] Code follows style guidelines (`clang-format`)
- [ ] All tests pass: `make test && make smoke`
- [ ] No memory leaks: `valgrind --leak-check=full`
- [ ] Stream discipline maintained (no logs in stdout)
- [ ] Documentation updated (README, .docs/, comments)
- [ ] Commit messages follow `[action]: [description]` format
- [ ] PR template filled completely
- [ ] Related issues referenced (if any)

---

## Acknowledgments

Thank you for contributing to `ws_pipeline_dispatcher`! Whether it's code, documentation, or bug reports, your help makes the project better.

**All contributors will be recognized in the project's CONTRIBUTORS.md file.**

---

*Last updated: 2024*
