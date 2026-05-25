# log_parse

Structured log parser, JSONL filter, and lightweight counter.

## Parse Mode

```text
log_parse --regex <pattern> --fields a,b,c --format json|csv
```

- Reads lines from stdin.
- Applies POSIX extended regex.
- Maps capture groups to field names.
- Writes JSON Lines or CSV to stdout.
- Regex capture values are treated as strings.

## Filter Mode

```text
log_parse --filter <expr>
```

- Reads JSON Lines from stdin.
- Keeps records with matching top-level string/number/bool field.
- Used in pipeline as `log_parse --filter type=clip`.

Supported filter expressions:

| Operator | Meaning | Example |
| --- | --- | --- |
| `=` | equals | `type=clip` |
| `!=` | not equals | `type!=heartbeat` |
| `>` | numeric greater-than | `ts>1747065600` |
| `~` | contains substring | `path~/clips/` |

JSONL mode passes matching lines through unchanged by default.

## Count Mode

```text
log_parse --filter type=clip --format count
log_parse --regex <pattern> --fields a,b,c --filter type=clip --format count
```

- Counts records that pass the filter.
- Writes one decimal count to stdout.
- Works in both regex mode and JSONL filter mode.

## Format Rules

- `--format json` and `--format csv` require `--regex`.
- `--format count` works with or without `--regex`.
- JSONL mode without `--format count` is pass-through only; it does not convert JSONL to CSV.

## Limitations

- JSONL filters inspect top-level fields only.
- `>` requires numeric values.
- `~` compares the scalar field text representation.

## Stream Rule

- stdout: data only.
- stderr: diagnostics only.
