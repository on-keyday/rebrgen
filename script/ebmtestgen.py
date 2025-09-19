#!/usr/bin/env python3
import json
import argparse
import sys
from typing import Any, Dict, List


class ValidationError(Exception):
    """カスタムの検証エラー例外"""

    pass


class SchemaValidator:
    """
    JSONオブジェクトが指定されたスキーマに準拠しているかを検証するクラス
    """

    def __init__(self, schema: Dict[str, Any]):
        """
        スキーマを前処理して、高速なルックアップのための辞書を準備します。
        """
        self.schema = schema
        # 構造体定義を名前でマッピング
        self.structs: Dict[str, Dict] = {
            s["name"]: s for s in schema.get("structs", [])
        }
        # Enum定義を名前でマッピングし、メンバー名をセットとして保持
        self.enums: Dict[str, Dict] = {e["name"]: e for e in schema.get("enums", [])}
        for name, enum_def in self.enums.items():
            enum_def["members_set"] = {m["name"] for m in enum_def["members"]}
        # subset_infoを構造体名とkindでマッピング
        self.subsets: Dict[str, Dict] = {
            s["name"]: {
                item["kind"]: item["fields"] for item in s["available_field_per_kind"]
            }
            for s in schema.get("subset_info", [])
        }

    def validate(self, data: Dict[str, Any], root_struct_name: str):
        """
        検証プロセスのエントリーポイント

        Args:
            data: 検証対象のJSONデータ
            root_struct_name: スキーマ内のルートとなる構造体名

        Raises:
            ValidationError: 検証に失敗した場合
        """
        if root_struct_name not in self.structs:
            raise ValidationError(
                f"ルート構造体 '{root_struct_name}' がスキーマに見つかりません。"
            )
        self._validate_object(data, root_struct_name, path=root_struct_name)

    def _validate_object(self, data: Any, struct_name: str, path: str):
        """
        オブジェクト（辞書）を再帰的に検証します。
        """
        if not isinstance(data, dict):
            raise ValidationError(
                f"パス '{path}' はオブジェクトであるべきですが、型が異なります。"
            )

        struct_def = self.structs[struct_name]
        field_map = {f["name"]: f for f in struct_def["fields"]}

        # このオブジェクトで許可されるフィールドのセットを決定する
        active_field_names: set[str]
        subset_used = False
        if struct_name in self.subsets:
            # subset_info に基づいて動的にフィールドを決定
            kind_value = data.get("kind")
            if kind_value is None:
                raise ValidationError(
                    f"パス '{path}' は 'kind' フィールドを持つべきですが、見つかりません。"
                )
            if kind_value not in self.subsets[struct_name]:
                raise ValidationError(
                    f"パス '{path}' の 'kind' の値 '{kind_value}' は不正です。"
                )
            active_field_names = set(self.subsets[struct_name][kind_value])
            subset_used = True
            path = f"{path}({kind_value})"
        else:
            # 通常の構造体
            active_field_names = set(field_map.keys())

        # スキーマで許可されていない余分なフィールドがないかチェック
        extra_fields = set(data.keys()) - active_field_names
        if extra_fields:
            raise ValidationError(
                f"パス '{path}' に不正なフィールドがあります: {', '.join(extra_fields)}"
            )

        # 許可された各フィールドについて、必須項目の存在チェックと型の検証を行う
        for field_name in active_field_names:
            field_def = field_map[field_name]
            is_present = field_name in data
            is_pointer = not subset_used and field_def.get("is_pointer", False)

            # is_pointer: false のフィールドは必須
            if not is_pointer and not is_present:
                raise ValidationError(
                    f"パス '{path}' に必須フィールド '{field_name}' がありません。"
                )

            if is_present:
                value = data[field_name]
                field_type = field_def["type"]
                is_array = field_def.get("is_array", False)
                new_path = f"{path}.{field_name}"

                if is_array:
                    if not isinstance(value, list):
                        raise ValidationError(
                            f"パス '{new_path}' は配列であるべきですが、型が異なります。"
                        )
                    for i, item in enumerate(value):
                        self._validate_value(item, field_type, f"{new_path}[{i}]")
                else:
                    self._validate_value(value, field_type, new_path)

    def _validate_value(self, value: Any, type_name: str, path: str):
        """
        与えられた型に基づいて個々の値を検証します。
        """
        if type_name in self.structs:
            self._validate_object(value, type_name, path)
        elif type_name in self.enums:
            valid_members = self.enums[type_name]["members_set"]
            if not isinstance(value, str) or value not in valid_members:
                raise ValidationError(
                    f"パス '{path}' の値 '{value}' は Enum '{type_name}' のメンバーではありません。"
                    f" (有効な値: {', '.join(sorted(list(valid_members)))})"
                )
        elif type_name.endswith("Ref"):
            if isinstance(value, dict):
                body_name = type_name.removesuffix("Ref") + "Body"
                self._validate_object(value, body_name, f"{path}->{body_name}")
            elif not isinstance(value, int):
                raise ValidationError(
                    f"パス '{path}' はオブジェクトまたは整数であるべきですが、型が異なります。"
                )
        elif type_name in ("std::uint8_t", "std::uint64_t", "Varint"):
            if not isinstance(value, int):
                raise ValidationError(
                    f"パス '{path}' は整数であるべきですが、型が異なります。"
                )
        elif type_name == "bool":
            if not isinstance(value, bool):
                raise ValidationError(
                    f"パス '{path}' はブール値であるべきですが、型が異なります。"
                )
        elif type_name == "std::string":
            if not isinstance(value, str):
                raise ValidationError(
                    f"パス '{path}' は文字列であるべきですが、型が異なります。"
                )
        else:
            raise ValidationError(
                f"パス '{path}' のスキーマで定義されている型 '{type_name}' は不明な型です。"
            )


import os
import subprocess as sp


def execute(command, env, capture=True) -> bytes:
    passEnv = os.environ.copy()
    if env is not None:
        passEnv.update(env)
    if capture:
        return sp.check_output(command, env=passEnv, stderr=sys.stderr)
    else:
        return sp.check_call(command, env=passEnv, stdout=sys.stdout, stderr=sys.stderr)


def main():
    """
    CLIのエントリーポイント
    """
    parser = argparse.ArgumentParser(
        description="JSONデータが指定されたスキーマに準拠しているかを検証します。"
    )
    parser.add_argument("json_data", help="検証対象のJSONファイル (B)")
    parser.add_argument("struct_name", help="検証の起点となるルート構造体名")
    args = parser.parse_args()

    schema_json = json.loads(
        execute(
            ["./tool/ebmcodegen", "--mode", "spec-json"],
            None,
            True,
        )
    )

    try:
        with open(args.json_data, "r", encoding="utf-8") as f:
            data_json = json.load(f)
    except FileNotFoundError as e:
        print(f"エラー: ファイルが見つかりません: {e.filename}", file=sys.stderr)
        sys.exit(1)
    except json.JSONDecodeError as e:
        print(f"エラー: JSONの解析に失敗しました: {e}", file=sys.stderr)
        sys.exit(1)

    try:
        validator = SchemaValidator(schema_json)
        validator.validate(data_json, args.struct_name)
        print("✅ 検証に成功しました。JSONデータはスキーマに準拠しています。")
    except ValidationError as e:
        print(f"❌ 検証に失敗しました:\n{e}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"予期せぬエラーが発生しました: {e}", file=sys.stderr)
        import traceback

        print(traceback.format_exc(), file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
