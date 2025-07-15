### `ebmgen` Project Overview

This document provides a concise overview of the `ebmgen` project, its purpose, and how to work with it.

#### 1. Project Goal: A Better Code Generator

The `rebrgen` project is a part of `brgen` project. `brgen` is project that converts binary format definitions from a custom DSL (`.bgn` files) into source code for various languages (C++, Python, etc.).
The goal for `rebrgen` project is making code generator more easier and standardized way.
To achieve this goal, we are trying two way now; building IR and writing code-generator-generator

This subproject, `ebmgen`, is a part of IR project. This is successor of `bmgen` and `BinaryModule(bm)`.
It converts the`brgen` Abstract Syntax Tree (AST) into a superior Intermediate Representation (IR) called the \*\*`ExtendedBinaryModule` (EBM)\*\*.

#### 2. The EBM: A Superior Intermediate Representation

The EBM (`src/ebm/extended_binary_module.bgn`) was designed to replace a previous, less flexible IR. Its key features are:

- **Graph-Based Structure:** The EBM is not a flat list of instructions. It's a graph where `Statements` and `Expressions` are stored in centralized tables and referenced by unique IDs (`StatementRef`, `ExpressionRef`). This allows for deduplication and makes the program structure explicit.
- **Structured Control Flow:** `if`, `loop`, and `match` constructs are represented with dedicated formats, making control flow easy to analyze.
- **Higher-Level Abstraction:** The IR focuses on _what_ to do (e.g., `READ_DATA`), leaving the "how" to the language-specific backends. This allows for more idiomatic and optimized code generation.
- **Preservation of Semantics:** Critical binary format details like endianness and bit size are preserved as fundamental properties within the IR.

#### 3. Current Development Status

Your role is code analyzer.

- you should read ebm/ and ebmgen/ as current project directory.
- you should read bmgen/ as old project directory.
- you should provide advice for refactoring based on current best practice and code knowledge
- you should help debug

Because this code is under development, there are code states that appear to be inconsistent. Always seek human assistance to identify possible inconsistencies. Never make arbitrary decisions.
This is a very important instruction. I repeat: Never make arbitrary decisions. This is a very important instruction.
When analyzing code, always keep a critical eye on it, as we are not looking for familiarity.

#### 4. Building and Running `ebmgen`

To build the `ebmgen` executable, navigate to the root of the `rebrgen` directory and use the `script/build.py` script:

```bash
python script/build.py native Debug
```

This command will build the project in `Debug` mode for your native platform. The `ebmgen` executable will be located at `tool/ebmgen.exe` (on Windows) or `tool/ebmgen` (on Linux/macOS) relative to the `rebrgen` root directory.

Once built, you can run `ebmgen` by providing an input `brgen` AST JSON file and specifying an output EBM file.

```bash
./tool/ebmgen -i <path/to/input.json> -o <path/to/output.ebm>
```

Replace `<path/to/input.json>` with the absolute path to your `brgen` AST JSON file and `<path/to/output.ebm>` with the desired absolute path for the generated EBM file. Currently, you might use `./save/simple.json` as input and `./save/out.ebm` as output.

Also there are a command at `src/ebm/ebm.ps1`. it generates `src/ebm/extended_binary_module.hpp` and `src/ebm/extended_binary_module.cpp` from `src/ebm/extended_binary_module.bgn`
