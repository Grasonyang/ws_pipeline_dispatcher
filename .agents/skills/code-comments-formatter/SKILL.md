---
name: code-comments-formatter
description: Add comprehensive English comments to C/C++ code files. Automatically adds Doxygen-style documentation for public functions in headers, explanatory comments for internal/static functions in implementations, and inline comments for complex logic. Supports any codebase, not limited to lib/. Apply this skill when you need to improve code readability with clear, standardized documentation.
---

# Code Comments Formatter Skill

Adds standardized English comments to C/C++ source files following best practices for library and application code.

## Overview

This skill automates the process of adding clear, well-structured comments to C/C++ code. It:

- Adds **Doxygen-style documentation** for public functions in header files
- Provides **clear explanations** for static/internal functions in implementation files
- Inserts **inline comments** for complex logic and edge cases
- Documents **structures, enums, and type definitions**
- Explains **design decisions** and constraints
- Maintains **consistent English style** across the codebase

## When to Use

- **Starting a new library**: Document all public APIs before or after implementation
- **Refactoring code**: Add comments while improving code clarity
- **Code review preparation**: Ensure all functions are well-documented
- **Onboarding**: Help new team members understand the codebase
- **Library maintenance**: Update or improve existing documentation

Trigger this skill when the user asks to:
- "Add comments to this file"
- "Write documentation for these functions"
- "Improve code readability with comments"
- "Document this library/module"

## Core Principles

### 1. Location-Based Documentation

| Code Element | File | Style |
|-------------|------|-------|
| Public functions | .h (header) | Doxygen format with @param, @return, @brief |
| Static functions | .c (implementation) | Simplified with purpose and key logic |
| Macros/Constants | .h (header) | Single-line or brief block comments |
| Structs/Enums | .h (header) | Member-by-member documentation |
| Complex logic | .c (implementation) | Inline comments explaining why, not what |

### 2. Doxygen Format for Public Functions

```c
/**
 * @brief One-sentence description of what the function does.
 * 
 * Detailed explanation of the function's behavior, edge cases,
 * and any important implementation notes.
 * 
 * @param param1 Description of first parameter
 * @param param2 Description of second parameter
 * @return Description of return value and error conditions
 * @retval 0 Success
 * @retval -1 Failure with errno set
 * @note Important constraints or side effects
 * @see related_function()
 */
type function_name(type param1, type param2);
```

### 3. Implementation Comments

For static helper functions, use a simpler format:

```c
/**
 * Brief description of what this helper does.
 * Explain its role in the larger algorithm or workflow.
 */
static void helper_function(...) {
    /* For complex sections, add inline comments explaining the approach */
    if (condition) {
        /* Why this check matters */
        ...
    }
}
```

### 4. Inline Comment Guidelines

**Good inline comments explain:**
- Why a check is needed (not that a check exists)
- Assumptions being made (e.g., NULL-terminated, size constraints)
- Non-obvious performance or correctness considerations
- Error handling decisions

**Example:**
```c
/* Preserve original error before cleanup touches errno */
int saved = errno;
close(fd);
errno = saved;
return -1;
```

**Not just:**
```c
/* Save errno */
int saved = errno;  // This is obvious from the code!
```

### 5. Structure and Enum Documentation

```c
/**
 * Descriptor for extracting a field from JSON data.
 * Used by json_parse() to specify how to extract and convert values.
 */
typedef struct {
    const char *key;      /* JSON object key name */
    json_type_t type;     /* Target data type (STRING, INT64, etc.) */
    void *out;            /* Output buffer to write to */
    size_t out_size;      /* Buffer size in bytes (0 for pointer types) */
    int required;         /* 1 if field must exist, 0 if optional */
} json_field_t;
```

## Workflow

### Step 1: Identify Files and Scope
- Determine which files need comments
- Separate .h files (declarations) from .c files (definitions)
- Identify public vs. internal APIs

### Step 2: Document Public API (Header Files)
- Add Doxygen-style comments for all public functions
- Document structures, enums, and important macros
- Include @param, @return, @note tags where relevant

### Step 3: Document Implementation (Source Files)
- Add brief comments for static/internal functions
- Add inline comments for complex logic
- Explain non-obvious design decisions

### Step 4: Review and Validate
- Check for completeness (all functions documented)
- Verify correct grammar and spelling
- Ensure consistency with project style
- Validate that comments add clarity without being obvious

## Language and Style Guidelines

### English Standards
- Use **clear, concise English** - assume readers are non-native speakers
- Prefer **active voice**: "Allocates memory for..." vs "Memory is allocated"
- Use **imperative mood** for functions: "Returns the count" not "Returns the count"
- Avoid **jargon** unless explained or unavoidable

### Tone
- Professional and factual
- Friendly but not casual
- Focus on **what** and **why**, not implementation details

### Common Patterns

| Pattern | Example |
|---------|---------|
| Allocation | "Returns newly allocated memory; caller must free with free()" |
| Ownership | "Caller owns the returned file descriptor; must close it" |
| Constraints | "len must be <= SIZE_MAX - 1 to avoid overflow" |
| Error cases | "Returns -1 on failure and sets errno" |
| Side effects | "Modifies src buffer in place; does not allocate" |

## Examples

### Before
```c
int parse_json(const char *line, const json_field_t *fields, size_t count) {
    if (line == NULL || fields == NULL) {
        return -1;
    }
    
    for (size_t i = 0; i < count; ++i) {
        const json_field_t *f = &fields[i];
        if (f->key == NULL || f->out == NULL) {
            return -1;
        }
        
        char *token = find_token(line, f->key);
        if (token == NULL) {
            if (f->required) {
                return -1;
            }
            continue;
        }
        
        convert_token(token, f->type, f->out, f->out_size);
        free(token);
    }
    
    return 0;
}
```

### After
```c
/**
 * @brief Parse a JSON line and extract specified fields.
 * 
 * Searches the JSON line for each field descriptor and converts
 * the extracted values to the target type. Handles optional fields
 * gracefully by continuing, but fails on required fields.
 * 
 * @param line JSON string (no validation of structure performed)
 * @param fields Array of field descriptors specifying what to extract
 * @param count Number of fields in the array
 * @return 0 on success, -1 if any required field is missing or conversion fails
 * 
 * @note Caller retains ownership of all input buffers
 * @see json_field_t for field descriptor structure
 */
int parse_json(const char *line, const json_field_t *fields, size_t count) {
    /* Validate inputs to prevent NULL dereference */
    if (line == NULL || fields == NULL) {
        return -1;
    }
    
    /* Process each field descriptor */
    for (size_t i = 0; i < count; ++i) {
        const json_field_t *f = &fields[i];
        
        /* Verify descriptor integrity */
        if (f->key == NULL || f->out == NULL) {
            return -1;
        }
        
        /* Extract the raw token value from JSON */
        char *token = find_token(line, f->key);
        
        /* Handle missing optional fields gracefully */
        if (token == NULL) {
            if (f->required) {
                return -1;  /* Required field missing */
            }
            continue;  /* Skip optional field */
        }
        
        /* Convert token to target type and store in output buffer */
        convert_token(token, f->type, f->out, f->out_size);
        free(token);
    }
    
    return 0;
}
```

## Implementation Notes

### Function Documentation
- Always document the public API in .h files
- Keep implementation comments concise and focused
- Use comments to explain **why**, not **what** (the code shows what it does)

### Error Handling
- Document all error conditions and return values
- Explain errno behavior if relevant
- Note which resources must be freed by caller

### Memory Management
- Always document memory ownership
- Note if function allocates memory
- Clarify if caller must free resources

### Performance Considerations
- Document time/space complexity for critical functions
- Note any assumptions needed for efficiency
- Explain non-obvious optimizations

## Quality Checklist

Before completing the skill application:

- [ ] All public functions in .h files have Doxygen documentation
- [ ] All static functions in .c files have brief comments
- [ ] Complex logic sections have explaining comments
- [ ] All parameters are documented
- [ ] Return values and error conditions are explained
- [ ] Structure/enum members are documented
- [ ] Grammar and spelling are correct
- [ ] Comments add value (no obvious statements)
- [ ] Documentation matches actual code behavior
- [ ] Consistent style throughout

## Related Skills

- **git-commit-creator**: Create structured commit messages for documentation updates
- **skill-creator**: Optimize this skill further based on feedback

## Troubleshooting

**Q: How much detail should comments have?**
A: Enough so someone unfamiliar with the code can understand the function's purpose and how to use it correctly, but not so much that comments become maintenance burden.

**Q: What about over-documentation?**
A: Comments like "// increment i" are noise. Focus on **why** decisions were made and important constraints.

**Q: Should I document obvious code?**
A: No. Document the "why" and "what for", not "what" (code shows that). Exception: non-obvious optimizations or critical error checks.

---

**Created:** 2026-05-25  
**Version:** 1.0  
**Author:** Code Documentation Standards
