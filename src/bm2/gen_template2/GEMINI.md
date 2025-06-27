# gen_template2 Status Summary

## 0. Before you read

documents in [.clinerules](../../../.clinerules) explains project overview.
[refactoring plan](../../../docs/refactoring_plan.md) and [complexity reduction plan](../../../docs/complexity_reduction_plan.md) is need to be read why this project is needed.

## 1. Current Status

`gen_template2` is located in `src/bm2/gen_template2/`. The core components are:

- **`flags.hpp`**: Defines configuration structures (`CommandLineOptions`, `LanguageConfig`, `DocumentationData`) for better separation of concerns.
- **`ast.hpp`**: Implements the Abstract Syntax Tree (AST) for representing generated C++ code. It includes:
  - Base nodes: `AstNode`, `Statement`, `Expression`.
  - Concrete nodes: `Literal`, `BinaryOpExpr`, `UnaryOpExpr`, `VariableDecl`, `FunctionCall`, `ReturnStatement`, `IfStatement`, `SwitchStatement`, `CaseStatement`, `Block`, `ExpressionStatement`, `LoopStatement`.
  - `AstRenderer`: A visitor pattern-based renderer that converts AST nodes into formatted C++ code strings, handling indentation.
- **`code_gen_map.hpp`**: Contains the core logic for mapping `rebgn::AbstractOp` (from `brgen`'s intermediate representation) to corresponding AST nodes.
  - It defines central dispatch functions (`generate_expression_from_code` and `generate_statement_from_code`) that take a `bm2::Context` object and a `code_idx` to recursively build the AST.
  - It correctly resolves references within the `rebgn::BinaryModule` using `ctx.ident_index_table` and `ctx.ident_table`.
- **`main.cpp`**: Serves as a test harness. It constructs a dummy `rebgn::BinaryModule` and `bm2::Context` to simulate input data, then calls the AST generation and rendering functions.

## 2. Current Capabilities

The current implementation of `gen_template2` can successfully generate C++ code for the following `rebgn::AbstractOp` types, as demonstrated by `main.cpp`'s output:

- **`IMMEDIATE_INT`**: Generates integer literals (e.g., `123`).
- **`IMMEDIATE_STRING`**: Generates string literals (e.g., `"test_string"`), correctly resolving the identifier to its string value.
- **`BINARY`**: Generates binary expressions (e.g., `(10 + 20)`), correctly resolving left and right operand references.
- **`UNARY`**: Generates unary expressions (e.g., `-5`), correctly resolving the operand reference.
- **`IF`**, **`ELIF`**, **`ELSE`**: Generates `if-else` and nested `if-elif-else` control flow structures, correctly parsing the linear `rebgn::Code` sequence into a hierarchical AST.
- **`LOOP_INFINITE`** and **`LOOP_CONDITION`**: Generates `while` loops (e.g., `while (true) { ... }` or `while (condition) { ... }`).
- **`RETURN_TYPE`**: Generates basic `return;` statements.
- **`DEFINE_VARIABLE`**: Generates variable declarations with initializers (e.g., `int var_789 = 123;`), resolving variable names and initializer expressions.

## 3. Current Challenges and Next Steps

The project is at a critical juncture, with the following immediate challenges and future development areas:

- **Debugging Execution Crash**: The `main.cpp` test harness currently crashes after the "Immediate String Test" when the full test suite is enabled. This is the most pressing issue and needs to be debugged to identify the exact cause of the runtime error. This will likely involve more granular logging or further simplification of test cases to isolate the failing component.
- **Comprehensive `rebgn::Code` to AST Mapping**: Only a subset of `rebgn::AbstractOp` types are currently handled. The remaining operations need corresponding AST nodes and generator functions to fully support the `brgen` DSL.
- **Hook System Integration**: The original `gen_template` relies heavily on a C++-based hook system for language-specific customizations. A new, more structured, and potentially safer (e.g., declarative or scripting-language based) hook system needs to be designed and integrated with the AST generation process. This is crucial for maintaining the flexibility of `rebrgen`.
- **Error Handling**: Implement robust error handling mechanisms for unimplemented `AbstractOp` types, invalid `rebgn::BinaryModule` structures, and other potential issues during AST generation.
- **Advanced Code Formatting**: Enhance the `AstRenderer` to support more advanced code formatting options (e.g., configurable brace styles, line wrapping, comment handling) to produce idiomatic code for various target languages.
- **Testing Framework**: Develop a more comprehensive and automated testing framework to validate the generated code against expected outputs for a wide range of `brgen` DSL inputs. This is essential for ensuring the correctness and stability of `gen_template2`.
- **Integration with `bmgen`**: Once `gen_template2` is stable and feature-complete, it needs to be integrated into the `rebrgen` build process, replacing the original `gen_template`.

## 4. How to Debug `gen_template2`

Debugging code generation can be challenging due to the layers of abstraction. Here's a systematic approach to identify and resolve issues in `gen_template2`:

### 4.1. Pinpointing the Crash/Error Location

- **Incremental Testing**: As demonstrated in recent interactions, simplify your `main.cpp` test cases. Start with the most basic `AbstractOp` types (e.g., `IMMEDIATE_INT`, `IMMEDIATE_STRING`) and gradually introduce more complex ones (`BINARY`, `UNARY`, `IF`, `LOOP`). This helps isolate the specific `AbstractOp` or code path causing the issue.
- **Print Statements**: Since a full debugger might not always be available or easy to attach in a CLI environment, strategically place `std::cerr` or `std::cout` statements within your generator functions (`generate_immediate_int`, `generate_binary_op`, `generate_if_statement`, etc.) and dispatch functions (`generate_expression_from_code`, `generate_statement_from_code`). Print:
  - The `code_idx` being processed.
  - The `AbstractOp` type (`to_string(code.op)`).
  - The values of `ref`, `left_ref`, `right_ref`, `ident`, `int_value`, `bop`, `uop` (and their `.value()` if they are `std::optional`).
  - Entry and exit points of functions.
- **Conditional Debugging**: Use `flags.cmd_options.debug` (if implemented) to enable/disable verbose debug output, preventing clutter during normal operation.

### 4.2. Inspecting Data Structures

- **`rebgn::BinaryModule` (`bm`)**: Verify the contents of `bm.code` (the linear sequence of `rebgn::Code` objects). Ensure the `op` types, and `ref` values are as expected for your test case.
- **`bm2::Context` (`ctx`)**: This is crucial for reference resolution. Inspect:
  - `ctx.ident_table`: Ensure that `ident` values (e.g., from `IMMEDIATE_STRING` or `DEFINE_VARIABLE`) are correctly mapped to their string representations.
  - `ctx.ident_index_table`: Confirm that `ident` values are correctly mapped to their corresponding indices in `bm.code`. Incorrect mappings here will lead to crashes when trying to access `bm.code[index]`.
  - The `bm2::Context` constructor is responsible for populating these tables from `bm.identifiers.refs` and `bm.ident_indexes.refs`. Ensure your dummy `BinaryModule` correctly populates these `refs`.

### 4.3. Tracing AST Generation and Structure

- **Step-by-Step AST Construction**: Follow the execution flow through `generate_expression_from_code` and `generate_statement_from_code`. For each `AbstractOp`, verify that the correct generator function is called and that it correctly extracts data from the `rebgn::Code` object.
- **Intermediate AST Inspection**: Before calling `renderer.render()`, you can add print statements to inspect the structure of the generated `std::unique_ptr<Statement>` or `std::unique_ptr<Expression>`. For example, you could add a simple `to_string()` method to your AST nodes for debugging purposes, or recursively print their types and values.

### 4.4. Verifying Rendered Output

- **Compare with Expected Output**: Once the program runs without crashing, carefully compare the generated C++ code with your expected output. Pay close attention to:
  - **Syntax**: Are parentheses, semicolons, and keywords correct?
  - **Indentation**: Does the code follow standard C++ formatting?
  - **Logic**: Does the generated code accurately reflect the intended logic of the `AbstractOp` sequence, especially for control flow (if/else, loops)?
  - **Resolved References**: Are `ref` values correctly translated into variable names or other expressions?

By systematically applying these debugging techniques, you can effectively pinpoint and resolve issues within `gen_template2`'s code generation process.

## 5. Debugging Command Log

This section logs the specific commands executed during the debugging process to identify and resolve issues.

### Initial Setup and Build Attempts

- **Initial build attempt (after refactoring Flags struct)**:

  ```bash
  python script/build.py native Debug
  ```

  _Result_: Build failed due to `Flags` not inheriting `HelpOption` and `parse_or_err` expecting `flags.help`.

- **Build attempt (after fixing Flags inheritance)**:
  ```bash
  python script/build.py native Debug
  ```
  _Result_: Build failed due to linker errors (`rebgn::Code` constructor/destructor undefined).

### Resolving Linker Errors

- **Manual intervention**: User manually edited `CMakeLists.txt` to link `bm` library.

- **Build attempt (after manual CMakeLists.txt edit)**:
  ```bash
  python script/build.py native Debug
  ```
  _Result_: Build successful.

### Debugging Runtime Crashes (Accessing Empty Optionals)

- **First run attempt (after successful build)**:

  ```bash
  C:/workspace/shbrgen/rebrgen/tool/gen_template2.exe --lang test
  ```

  _Result_: Program crashed with empty output and exit code 3. Suspected missing DLL or environment issue.

- **Second run attempt (absolute path)**:

  ```bash
  C:/workspace/shbrgen/rebrgen/tool/gen_template2.exe --lang test
  ```

  _Result_: Same crash. Confirmed not a path issue.

- **Third run attempt (no arguments)**:

  ```bash
  C:/workspace/shbrgen/rebrgen/tool/gen_template2.exe
  ```

  _Result_: Same crash. Still pointed to environment or missing DLL.

- **Manual intervention**: User indicated they fixed the environment issue.

- **Run attempt (after environment fix)**:

  ```bash
  C:/workspace/shbrgen/rebrgen/tool/gen_template2.exe --lang test
  ```

  _Result_: Program ran, but output was incomplete (stopped after "Immediate String Test"). Exit code 3. Indicated crash due to accessing empty `std::optional`.

- **Build attempt (after setting `code.op` before setters)**:

  ```bash
  python script/build.py native Debug
  ```

  _Result_: Build successful.

- **Run attempt (after setting `code.op` before setters)**:

  ```bash
  C:/workspace/shbrgen/rebrgen/tool/gen_template2.exe --lang test
  ```

  _Result_: Program ran, but output was incomplete (stopped after "Immediate String Test"). Exit code 3. Still indicated crash due to accessing empty `std::optional`.

- **Build attempt (after fixing `Varint` access and `to_string` calls)**:

  ```bash
  python script/build.py native Debug
  ```

  _Result_: Build successful.

- **Run attempt (after fixing `Varint` access and `to_string` calls)**:

  ```bash
  C:/workspace/shbrgen/rebrgen/tool/gen_template2.exe --lang test
  ```

  _Result_: Program ran, but output was incomplete (stopped after "Immediate String Test"). Exit code 3. Still indicated crash due to accessing empty `std::optional`.

- **Build attempt (after fixing `std::unique_ptr` typo in `code_gen_map.hpp`)**:

  ```bash
  python script/build.py native Debug
  ```

  _Result_: Build successful.

- **Run attempt (after fixing `std::unique_ptr` typo)**:

  ```bash
  C:/workspace/shbrgen/rebrgen/tool/gen_template2.exe --lang test
  ```

  _Result_: Program ran, but output was incomplete (stopped after "Immediate String Test"). Exit code 3. Still indicated crash due to accessing empty `std::optional`.

- **Build attempt (after fixing `bm2::Context` and `futils::binary::writer` initialization)**:

  ```bash
  python script/build.py native Debug
  ```

  _Result_: Build successful.

- **Run attempt (after fixing `bm2::Context` and `futils::binary::writer` initialization)**:

  ```bash
  C:/workspace/shbrgen/rebrgen/tool/gen_template2.exe --lang test
  ```

  _Result_: Program ran, but output was incomplete (stopped after "Immediate String Test"). Exit code 3. Still indicated crash due to accessing empty `std::optional`.

- **Build attempt (after fixing `rebgn::Context` namespace and `unordered_map` access)**:

  ```bash
  python script/build.py native Debug
  ```

  _Result_: Build successful.

- **Run attempt (after fixing `rebgn::Context` namespace and `unordered_map` access)**:

  ```bash
  C:/workspace/shbrgen/rebrgen/tool/gen_template2.exe --lang test
  ```

  _Result_: Program ran, but output was incomplete (stopped after "Immediate String Test"). Exit code 3. Still indicated crash due to accessing empty `std::optional`.

- **Build attempt (after simplifying `main.cpp` to only include Immediate Int and String tests)**:

  ```bash
  python script/build.py native Debug
  ```

  _Result_: Build successful.

- **Run attempt (after simplifying `main.cpp`)**:

  ```bash
  C:/workspace/shbrgen/rebrgen/tool/gen_template2.exe --lang test
  ```

  _Result_: Program ran, but output was incomplete (stopped after "Immediate String Test"). Exit code 3. Still indicated crash due to accessing empty `std::optional`.

- **Manual intervention**: User indicated they fixed the issue.

- **Run attempt (after manual fix)**:
  ```bash
  C:/workspace/shbrgen/rebrgen/tool/gen_template2.exe --lang test
  ```
  _Result_: Program ran successfully, producing all expected output. Exit code 0.
