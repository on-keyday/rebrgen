## 4. コード生成ロジック (ビジターフック)

### 4.1 ビジターフックの概要
ビジターフックは、C++コードスニペットを含む`.hpp`ファイルです。これはクラス定義では**ありません**。代わりに、`ebmcodegen`によって動的に生成される関数のリテラルボディです。`ebmcodegen`は`visit_Statement_WRITE_DATA(...)`のような関数を生成し、あなたの`.hpp`ファイル (例: `visitor/Statement_WRITE_DATA.hpp`) の内容は、その関数のボディに直接`#include`されます。関数、したがってあなたのフックコードは、`expected<Result>`型の値を**返さなければなりません**。`Result`構造体は、生成されたコード文字列をその`value`メンバーに保持します。

### 4.2 `ebmtemplate.py`によるビジターフックの管理
`script/ebmtemplate.py`スクリプトは、`ebmcodegen`をラップし、ビジターフックのライフサイクルを簡素化します。
-   **`python script/ebmtemplate.py`**: ヘルプを表示します。
-   **`python script/ebmtemplate.py interactive`**: スクリプト機能のインタラクティブガイドを開始します。
-   **`python script/ebmtemplate.py <template_target>`**: フックファイルの概要を標準出力に出力します。
-   **`python script/ebmtemplate.py <template_target> <lang>`**: `src/ebmcg/ebm2<lang>/visitor/`に新しいフックファイル (`.hpp`) を生成します。
-   **`python script/ebmtemplate.py update <lang>`**: 指定された言語の既存のすべてのフックの自動生成されたコメントブロック (利用可能な変数リスト) を更新します。
-   **`python script/ebmtemplate.py test`**: 利用可能なすべてのテンプレートターゲットの生成をテストします。
-   **`python script/ebmtemplate.py list <lang>`**: 指定された[lang]ディレクトリ内のすべての定義済みテンプレートをリストします。

### 4.3 ビジターフック実装のワークフロー
1.  **ターゲットの検索**: 処理したいEBMノードを決定します。`tool/ebmcodegen --mode hooklist`を使用して、利用可能なフックをリストします。
2.  **ファイルの作成**: `python script/ebmtemplate.py <template_target> <lang>`を使用してフックファイルを生成します。
3.  **ロジックの実装**: 新しく作成されたファイル (`src/ebmcg/ebm2<lang>/visitor/<template_target>.hpp`) を開きます。`/*here to write the hook*/`コメントをC++実装に置き換えます。
4.  **ビルドと検証**: フックを実装した後、`python script/unictest.py` (またはデバッグ用に`python script/unictest.py --print-stdout`) を実行して、変更をビルドおよび検証します。これにより、ビルドプロセスがトリガーされ、`unictest`フレームワークで定義されたテストが実行されます。`unictest.py`は、適切な場合に`--debug-unimplemented`フラグをコードジェネレーター実行可能ファイルに自動的に渡します。また、ビルドシステムが新しいファイルを認識するように、フックファイルの作成時に`src/ebmcg/ebm2<lang>/main.cpp`の最終更新時間を更新することを忘れないでください。

### 4.4 ビジターフックAPIリファレンス

ビジターフックは、`ebmcodegen`によって生成されるC++関数の本体として機能します。これらのフック内で利用可能な主要なAPI要素を以下に示します。

#### 4.4.1 戻り値の型と`Result`構造体

すべてのビジターフックは`expected<Result>`型の値を返さなければなりません。

-   **`expected<T>`**: エラーが発生する可能性のある操作の結果を表す型です。成功した場合は`T`型の値を保持し、失敗した場合はエラー情報を保持します。`ebmgen`プロジェクトでは、エラーハンドリングのために`MAYBE`マクロと組み合わせて使用されます。
-   **`Result`構造体**:
    -   `CodeWriter value`: 生成されたコードを保持する`CodeWriter`オブジェクトです。`CodeWriter`は、インデントなどを考慮しながらコードを効率的に構築するためのユーティリティです。フックの目的は、この`value`メンバーにターゲット言語のコードを構築することです。

#### 4.4.2 利用可能な変数

各ビジターフックは、訪問しているEBMノードのフィールドに対応する変数に直接アクセスできます。これらの変数は、`ebmtemplate.py <template_target> <lang>`でフックファイルを生成した際に、自動生成されるコメントブロックにリストされます。

-   **例**: `Statement_WRITE_DATA`フックの場合、`io_data`や`endian`などの変数が利用可能です。
-   **変数の更新**: EBM構造 (`extended_binary_module.bgn`) が変更された場合は、`python script/ebmtemplate.py update <lang>`を実行して、既存のフックのコメントブロック内の変数リストを更新してください。

#### 4.4.3 ヘルパー関数とマクロ

ビジターフック内でコード生成ロジックを簡素化するために、いくつかのヘルパー関数とマクロが提供されています。

-   **`MAYBE(var, expr)`**: エラーハンドリングマクロ。`expr`が`expected`型の値を返し、それがエラーである場合に、現在の関数からエラーを伝播して早期リターンします。成功した場合は、`expr`の値を`var`に代入します。
    -   **使用例**: `MAYBE(sub_expr_str, visit_Expression(*this, some_expression_ref));`
-   **`visit_Expression(*this, expression_ref)`**: 指定された`ExpressionRef`に対応するEBM式を再帰的に訪問し、その結果として生成されたコードを返します。
-   **`visit_Statement(*this, statement_ref)`**: 指定された`StatementRef`に対応するEBMステートメントを再帰的に訪問し、その結果として生成されたコードを返します。
-   **`std::format`**: C++20で導入された文字列フォーマット機能。生成するコード文字列を整形するために使用できます。
-   **`to_string(enum_value)`**: EBMの列挙型 (例: `SizeUnit`, `Endian`) の値を対応する文字列表現に変換するために使用されます。

#### 4.4.4 `extended_binary_module.hpp`の参照

EBMの構造体、列挙型、および参照型 (`StatementRef`, `ExpressionRef`, `TypeRef`など) の正確な定義については、`src/ebm/extended_binary_module.hpp`ファイルを参照してください。このファイルは、`extended_binary_module.bgn`から自動生成されます。

### 4.5 DSL (ドメイン固有言語) for ビジターフック
`ebmcodegen`ツールは、ビジターフックを記述するためのドメイン固有言語 (DSL) をサポートしており、コード生成ロジックをより簡潔で読みやすい方法で定義できます。このDSLは、`--mode dsl`および`--dsl-file=FILE`で呼び出されたときに`ebmcodegen`によって処理されます。

**DSL構文の概要:**
DSLは、特別なマーカーを使用してC++コード、EBMノード処理、および制御フロー構造を混在させます。
DSLの具体的な使用例については、`src/ebmcg/ebm2python/dsl_sample/`を参照してください。
-   **`{% C++_CODE %}`**: C++リテラルコードを生成された出力に直接埋め込みます。(例: `{% int a = 0; %}`)
-   **`{{ C++_EXPRESSION }}`**: 結果が出力に書き込まれるC++式を埋め込みます。(例: `{{ a }} += 1;`)
-   **`{* EBM_NODE_EXPRESSION *}`**: `visit_Object`を呼び出し、生成された出力を書き込むことでEBMノード (例: `ExpressionRef`、`StatementRef`) を処理します。(例: `{* expr *}`)
-   **`{& IDENTIFIER_EXPRESSION &}`**: 識別子を取得し、それを出力に書き込みます。(例: `{& item_id &}`)
-   **`{! SPECIAL_MARKER !}`**: DSL自体内の高度な制御フローおよび変数定義に使用されます。これらのマーカー内のコンテンツは、ネストされたDSLによって解析されます。
    -   **`transfer_and_reset_writer`**: 現在の`CodeWriter`コンテンツを転送し、リセットするC++コードを生成します。
    -   **`for IDENT in (range(BEGIN, END, STEP) | COLLECTION)`**: C++ `for`ループを生成します。数値範囲とコレクションのイテレーションの両方をサポートします。
    -   **`endfor`**: `for`ループブロックを閉じます。
    -   **`if (CONDITION)` / `elif (CONDITION)` / `else`**: C++ `if`/`else if`/`else`ブロックを生成します。
    -   **`endif`**: `if`ブロックを閉じます。
    -   **`VARIABLE := VALUE`**: 生成されたコード内でC++変数を定義します。(例: `my_var := 42`)

これらのマーカーの外側のテキストはターゲット言語コードとして扱われ、出力に書き込まれる前にエスケープされます。DSLは、ソースの書式設定に基づいて自動インデントとデデントも処理します。
