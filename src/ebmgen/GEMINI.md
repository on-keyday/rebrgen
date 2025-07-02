### `ebmgen` Project Overview and Progress: A Journey Towards a Superior Intermediate Representation

This document serves as a comprehensive overview of the `ebmgen` project, detailing its purpose, design philosophy, current status, and future direction. It is intended for anyone (including a new AI instance) seeking to understand the intricacies of `rebrgen`'s compiler architecture and the role `ebmgen` plays within it.

#### 1. The `rebrgen` Ecosystem: Context is Key

`rebrgen` is a code generation project built around `brgen`, a Domain Specific Language (DSL) for defining binary data formats. At its core, `rebrgen` takes a `brgen` Abstract Syntax Tree (AST) and transforms it into executable code (structs, encoders, decoders) in various programming languages (C++, Python, etc.).

The original architecture involved:

- **`brgen`:** Parses the `.bgn` DSL files into an AST.
- **`bmgen`:** A compiler frontend that consumed the `brgen` AST and produced an Intermediate Representation (IR) called `BinaryModule`.
- **`BinaryModule`:** A binary-serialized IR (defined in `src/bm/binary_module.bgn`).
- **`bm2{lang}` tools:** Language-specific backends that read the `BinaryModule` and generated code for their respective target languages.

#### 2. The Problem: Limitations of the Original `BinaryModule`

Our initial analysis revealed significant challenges with the original `BinaryModule` design, which propagated complexity throughout the entire `rebrgen` pipeline:

- **Overly Granular and Prescriptive `AbstractOp`s:** The `BinaryModule`'s `AbstractOp` enum contained a vast number of low-level operations (e.g., `ENCODE_INT`, `DECODE_INT_VECTOR_FIXED`). These operations, while accurately reflecting core binary format semantics (like integer encoding/decoding with specific bit sizes and endianness), were highly prescriptive. They dictated the _exact bit-level serialization logic_ within the IR itself. This design was a deliberate trade-off, prioritizing implementation simplicity for `bmgen` and `bm2*` backends. By providing concrete serialization logic (e.g., through `ENCODE_INT`'s `fallback` block mechanism), it allowed backends to generate simple, direct expressions. However, this approach inadvertently prevented backends from leveraging potentially more optimized, idiomatic, and language-native integer encoding/decoding functions (e.g., Go's `binary.BigEndian.AppendUint16()` or Rust's `x.to_be_bytes()`). This trade-off between implementation ease and the ability to utilize language-specific, optimized logic hindered higher-level optimizations and idiomatic code generation across diverse target languages.
- **Implicit Control Flow:** Control flow (like `if` or `loop`) was represented by `BEGIN_X`/`END_X` markers in a linear instruction stream. This required both `bmgen` and `bm2{lang}` backends to reconstruct the program's structure, leading to redundant analysis and potential errors.
- **Ambiguous References:** Generic `Varint` IDs were used for all references within the IR, lacking semantic meaning. This made the IR hard to read, debug, and process without constant contextual lookups.
- **Verbose Type Handling:** The `Storage` format for type definitions was complex, leading to boilerplate code in generated C++ bindings and runtime overhead.

#### 3. The Solution: Introducing the `ExtendedBinaryModule` (`ebm`) and `ebmgen`

To address these limitations, the `ebmgen` project was initiated with the goal of creating a **superior Intermediate Representation (IR)** called `ExtendedBinaryModule` (EBM), defined in `src/ebm/extended_binary_module.bgn`. `ebmgen` itself is the new compiler frontend responsible for generating this EBM from the `brgen` AST.

The core philosophy is to create an IR that:

- **Preserves Core Binary Semantics:** For a binary format definition language, details like endianness, bit size, and specific data layouts are _not_ platform-specific implementation details. They are **fundamental, semantic properties of the binary format itself, as defined within the DSL.** The EBM _must_ explicitly capture these.
- **Is Language-Agnostic at its Core:** While preserving binary semantics, the EBM should abstract away programming language-specific implementation details. This allows `bm2{lang}` backends to generate idiomatic and efficient code for diverse paradigms (imperative, functional, assembly, etc.).
- **Enables Advanced Optimizations:** The EBM's structure should facilitate sophisticated compiler analyses and optimizations within `ebmgen` before being passed to the backends.

#### 4. Key Design Principles of the `ExtendedBinaryModule` (EBM)

The `src/ebm/extended_binary_module.bgn` reflects these principles:

- **Graph-Based Structure via Explicit References:**
  - Instead of a linear instruction stream, EBM models the program as a graph. `Expression` and `Statement` are now top-level, centralized tables (`expressions`, `statements`) referenced by explicit `ExpressionRef` and `StatementRef` types.
  - **Significant Update:** Declarations like `MatchStatement.branches`, `StructDecl.fields`, `UnionDecl.members`, `FunctionDecl.params`, and `EnumDecl.members` now utilize `Block` formats instead of direct vectors. This allows for more flexible and complex content within these declarations, accommodating not just simple lists of items but also sequences of statements.
  - **Enhanced Referencing:** `ExpressionBody.id` now references `StatementRef` (the definition of an identifier) instead of `IdentifierRef`. Similarly, `FieldDecl.parent_struct`, `UnionMemberDecl.parent_union`, `BitFieldDecl.parent_format`, `PropertyDecl.parent_format`, and `FunctionDecl.parent_format` explicitly use `StatementRef` to point to their parent definition statements, strengthening hierarchical relationships.
  - **New Helper Formats:** Introduction of `Block` (for lists of `StatementRef`s), `Expressions` (for lists of `ExpressionRef`s), `LoopFlowControl` (for `break`/`continue` context), and `ErrorReport` (for structured error messages).
  - These changes allow for:
    - **Deduplication:** Common expressions/statements are stored once and referenced multiple times.
    - **Clearer Relationships:** Explicit references make the IR's structure easy to navigate and understand.
    - **Optimization Potential:** Facilitates graph-based analyses (e.g., data flow, control flow) and transformations.
- **Higher Abstraction of Operations:**
  - The `AbstractOp` enum is significantly reduced and more abstract. Operations like `READ_DATA`, `WRITE_DATA`, `SEEK_STREAM` replace many granular, instruction-like operations from the old IR. This pushes the "how to implement" details to the `bm2{lang}` backends, giving them flexibility.
  - `ExpressionOp` and `StatementOp` enums further refine the operation types for expressions and statements, respectively.
- **Structured Control Flow:**
  - EBM explicitly defines control flow using dedicated formats like `IfStatement`, `LoopStatement`, and `MatchStatement`. These formats directly encapsulate their conditions and `Block`s (lists of `StatementRef`s). This eliminates the ambiguity of `BEGIN_X`/`END_X` markers and makes the program structure explicit.
- **Preservation of Core Binary Semantics:**
  - Formats like `EndianExpr` (for endianness and signedness) and the detailed `Type` format (for bit sizes, array lengths, struct layouts) are integral parts of the EBM. These are not abstracted away because they are fundamental properties of the binary format being defined.
- **Modularity and Extensibility:**
  - Individual formats for declarations (`VariableDecl`, `FieldDecl`, `EnumDecl`, `StructDecl`, etc.) make the IR modular and easier to extend.
  - `reserved :u7` fields are included for future-proofing.

#### 5. Current Status of `ebmgen` Development

The `ebmgen` project is actively under development, focusing on the robust conversion of the `brgen` AST into the `ExtendedBinaryModule` (EBM).

- **EBM Definition Updated:** The `src/ebm/extended_binary_module.bgn` file has been significantly updated by manual intervention to incorporate `Block`s for various declarations and to use `StatementRef` for hierarchical relationships. After updating the `.bgn` file, run `src/ebm/ebm.ps1` to regenerate the corresponding C++ bindings (`extended_binary_module.hpp` and `extended_binary_module.cpp`), providing the necessary data structures for `ebmgen` and `bm2*` tools to interact with the new IR.
- **`ebmgen` Project Setup:** The basic project structure for `ebmgen` (including `main.cpp`, `convert.hpp`, `convert.cpp`, `converter.hpp`, `converter.cpp`) is in place, with appropriate namespaces (`ebmgen` for compiler logic, `ebm` for the IR).
- **Core `Converter` Implementation:** The `Converter` class is being developed to traverse the `brgen` AST and populate the `ExtendedBinaryModule`'s tables.
  - **Caching Mechanism:** A `visited_nodes` map has been introduced to cache `StatementRef`s for already converted `ast::Node`s, preventing redundant processing and ensuring a correct graph-based IR.
  - **Refactored Statement Conversion:** The `convert_statement` method now acts as a caching layer, calling `convert_statement_impl` for the actual conversion logic.
  - **Expression Conversion:** The converter currently supports the conversion of the following `brgen` AST expression types into `ebm::Expression` nodes:
    - `IntLiteral` (e.g., `10`, `0xFF`)
    - `BoolLiteral` (e.g., `true`, `false`)
    - `StrLiteral` (e.g., `"hello"`)
    - `TypeLiteral` (e.g., `u8`, `float32`)
    - `Ident` (now correctly references its defining `StatementRef`)
    - `Binary` operations (e.g., `a + b`, `x == y`)
    - `Unary` operations (e.g., `-x`, `!b`)
    - `Call` expressions (function calls)
    - `Index` expressions (array/vector indexing, e.g., `arr[i]`)
    - `MemberAccess` expressions (struct/object member access, e.g., `obj.field`)
    - `Cast` expressions (type casting, where the `CastType` is inferred during conversion based on source and destination types).
  - **Type Conversion:** The `convert_type` function now handles `ast::IntType`, `ast::BoolType`, `ast::FloatType`, `ast::IdentType`, `ast::ArrayType`, `ast::IntLiteralType`, `ast::StrLiteralType`, `ast::EnumType`, and `ast::StructType`.
  - **Statement Conversion:** The `convert_statement` method now supports `ast::Assert`, `ast::Return`, `ast::Break`, `ast::Continue`, `ast::If`, `ast::Loop`, `ast::Match`, `ast::Program`, `ast::Format` (as `StructDecl`), `ast::Enum`, `ast::Function`, `ast::Metadata`, and `ast::State`.
  - **Binary Serialization:** The `main.cpp` has been updated to correctly serialize the generated `ebm::ExtendedBinaryModule` to a binary output file.

#### 6. Crucial Learnings and `brgen` DSL Nuances

Throughout this development, understanding the specific syntax and behavior of the `brgen` DSL has been paramount:

- **`match` Expression Syntax:** The `match` keyword is used for discriminated unions. Cases are specified as `EnumName.MEMBER_NAME:`, followed by the fields relevant to that case.
- **No Optional (`?`) Syntax:** Unlike some IDLs, `brgen` does not use `?` for optional fields. All fields within a `format` are implicitly mandatory. Conditional presence of data must be handled semantically by the `ebmgen` compiler and `bm2{lang}` backends based on the `op`/`statement_kind` field.
- **Empty Match Arms (`..`):** Empty cases in `match` statements are denoted by `..`.
- **Enum Member Access:** Enum members are accessed using `EnumName.MEMBER_NAME` (e.g., `AbstractOp.LITERAL_INT`, `LoopType.WHILE`).
- **Implicit Type Information:** Unlike some languages where type casting is explicit in the AST, `brgen`'s AST does not always carry explicit `CastType` information for all conversions. `ebmgen` is responsible for inferring the appropriate `ebm::CastType` based on the source and destination types during the conversion process.
- **Bitwise Operations:** `brgen`'s AST uses `shl` and `shr` for bitwise left and right shifts, respectively, and `complement` for bitwise NOT. These are mapped to their corresponding `ebm::BinaryOp` and `ebm::UnaryOp` values during conversion.
- **`ast::Format` Body Contents:** It's now understood that `ast::Format` bodies can contain statements other than just `ast::Field` declarations (e.g., `assert` statements). The EBM's `StructDecl` has been updated to accommodate this by using a `Block` for its `fields`.

#### 7. Next Steps

The immediate next steps for `ebmgen` are:

1.  **Complete Statement Conversion:** Implement the conversion logic for any remaining `brgen::ast::Stmt` types into `ebm::Statement` nodes.
2.  **Refine `ast::Format` Body Handling:** Ensure all types of statements within `ast::Format` bodies are correctly converted and placed into the `StructDecl`'s `Block`.
3.  **Implement IR Optimizations:** Begin developing IR-to-IR transformation passes within `ebmgen` to optimize the EBM before it's consumed by the backends.

This new EBM design provides a robust and flexible foundation for `rebrgen` to generate high-quality, idiomatic code across a wide spectrum of programming languages, from low-level assembly to high-level functional paradigms.

#### 8. Building and Running `ebmgen`

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
