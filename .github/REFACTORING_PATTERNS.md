## Pattern: Parse/Execute Separation

**Use when**: Refactoring command parsing functions for testability

**Template**:
```c
// 1. Define data structure
struct ParseMyCommandData {
  char *arg1;
  int flags;
  // ... fields for all parsed components
};

// 2. Define array if multiple items
ARRAY_HEAD(ParseMyCommandArray, struct ParseMyCommandData *);

// 3. Create parser (returns bool, takes Buffer *err)
static bool parse_mycommand_args(struct Command *cmd, struct Buffer *line,
                                  struct Buffer *err, struct ParseMyCommandArray *array);

// 4. Create executor (performs action)
static enum CommandResult parse_mycommand_exec(struct Command *cmd,
                                                 struct ParseMyCommandArray *array,
                                                 struct Buffer *err);

// 5. Create cleanup helpers
static void parse_mycommand_data_free(struct ParseMyCommandData **ptr);
static void parse_mycommand_array_free(struct ParseMyCommandArray *array);

// 6. Update original function to orchestrate
enum CommandResult parse_mycommand(struct Buffer *buf, struct Buffer *s,
                                    intptr_t data, struct Buffer *err) {
  struct ParseMyCommandArray args = ARRAY_HEAD_INITIALIZER;
  
  if (!parse_mycommand_args(cmd, s, err, &args))
    goto done;
    
  rc = parse_mycommand_exec(cmd, &args, err);
  
done:
  parse_mycommand_array_free(&args);
  return rc;
}

// 7. Write comprehensive tests for parse_mycommand_args()
```

**Examples in codebase**:  `parse_unbind()`, `parse_mailboxes()`, `parse_folder_hook()`

