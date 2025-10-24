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
4.  **ビルドと検証**: フックを実装した後、`python script/unictest.py` (またはデバッグ用に`python script/unictest.py --print-stdout`) を実行して、変更をビルドおよび検証します。これにより、ビルドプロセスがトリガーされ、`unictest`フレームワークで定義されたテストが実行されます。また、ビルドシステムが新しいファイルを認識するように、フックファイルの作成時に`src/ebmcg/ebm2<lang>/main.cpp`の最終更新時間を更新することを忘れないでください。

### 4.4 DSL (ドメイン固有言語) for ビジターフック
`ebmcodegen`ツールは、ビジターフックを記述するためのドメイン固有言語 (DSL) をサポートしており、コード生成ロジックをより簡潔で読みやすい方法で定義できます。このDSLは、`--mode dsl`および`--dsl-file=FILE`で呼び出されたときに`ebmcodegen`によって処理されます。

**DSL構文の概要:**
DSLは、特別なマーカーを使用してC++コード、EBMノード処理、および制御フロー構造を混在させます。
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
