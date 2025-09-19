#!/usr/bin/env python3
import json
import argparse
import sys
import traceback
from typing import Any, Dict, List
import os
import subprocess as sp


# --- 既存のクラス (変更なし) ---
class ValidationError(Exception):
    """カスタムの検証エラー例外"""

    pass


def ref_to_body(type_name: str):
    no_ref = type_name.removesuffix("Ref")
    if no_ref == "Identifier" or no_ref == "String":
        body_name = "String"
    else:
        body_name = no_ref + "Body"
    return body_name


def isArrayLikeObject(data: Any, field_map: dict):
    return isinstance(data, list) and not (
        set(field_map.keys()) - set({"len", "container"})
    )


class SchemaValidator:
    # (このクラスの実装は前回のものから変更ありません)
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

    def validate(
        self, data: Dict[str, Any], root_struct_name: str, rough: set[str] = set()
    ):
        """
        検証プロセスのエントリーポイント
        """
        if root_struct_name not in self.structs:
            raise ValidationError(
                f"ルート構造体 '{root_struct_name}' がスキーマに見つかりません。"
            )
        self._validate_object(
            data, root_struct_name, path=root_struct_name, rough=rough
        )

    def _validate_object(self, data: Any, struct_name: str, path: str, rough: set[str]):
        """
        オブジェクト（辞書）を再帰的に検証します。
        """
        struct_def = self.structs[struct_name]
        field_map = {f["name"]: f for f in struct_def["fields"]}
        if not isinstance(data, dict):
            # もし配列ならばlenとcontainerに読み替える
            if isArrayLikeObject(data, field_map):
                data = {"len": len(data), "container": data}
            else:
                raise ValidationError(
                    f"パス '{path}' はオブジェクトであるべきですが、型が異なります。{type(data)}"
                )

        active_field_names: set[str]
        subset_used = False
        if struct_name in self.subsets:
            kind_value = data.get("kind")
            if kind_value is None:
                raise ValidationError(
                    f"パス '{path}' は 'kind' フィールドを持つべきですが、見つかりません。 (有効な値: {",".join(self.subsets[struct_name].keys())})"
                )
            if kind_value not in self.subsets[struct_name]:
                raise ValidationError(
                    f"パス '{path}' の 'kind' の値 '{kind_value}' は不正です。(有効な値: {",".join(self.subsets[struct_name].keys())})"
                )
            active_field_names = set(self.subsets[struct_name][kind_value])
            subset_used = True
            path = f"{path}({kind_value})"
        else:
            active_field_names = set(field_map.keys())

        extra_fields = set(data.keys()) - active_field_names
        if extra_fields:
            raise ValidationError(
                f"パス '{path}' に不正なフィールドがあります: {', '.join(extra_fields)}"
            )

        for field_name in sorted(list(active_field_names)):
            field_def = field_map[field_name]
            is_present = field_name in data
            is_pointer = (field_name in rough) or (
                not subset_used and field_def.get("is_pointer", False)
            )

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
                            f"パス '{new_path}' は配列であるべきですが、型が異なります。{type(value)}"
                        )
                    for i, item in enumerate(value):
                        self._validate_value(
                            item, field_type, f"{new_path}[{i}]", rough
                        )
                else:
                    self._validate_value(value, field_type, new_path, rough)

    def _validate_value(self, value: Any, type_name: str, path: str, rough: set[str]):
        """
        与えられた型に基づいて個々の値を検証します。
        """
        if type_name in self.structs:
            self._validate_object(value, type_name, path, rough)
        elif type_name in self.enums:
            valid_members = self.enums[type_name]["members_set"]
            if not isinstance(value, str) or value not in valid_members:
                raise ValidationError(
                    f"パス '{path}' の値 '{value}' は Enum '{type_name}' のメンバーではありません。"
                    f" (有効な値: {', '.join(sorted(list(valid_members)))})"
                )
        elif type_name.endswith("Ref") or type_name == "AnyRef":
            if isinstance(value, dict):
                body_name = ref_to_body(type_name)
                if body_name in self.structs:
                    self._validate_object(
                        value, body_name, f"{path}->{body_name}", rough
                    )
                else:
                    raise ValidationError(
                        f"パス '{path}' の型 '{type_name}' はオブジェクトを持つことができません。"
                    )
            elif not isinstance(value, int):
                raise ValidationError(
                    f"パス '{path}' はオブジェクトまたは整数であるべきですが、型が異なります。{type(value)}"
                )
        elif type_name in ("std::uint8_t", "std::uint64_t", "Varint"):
            if not isinstance(value, int):
                raise ValidationError(
                    f"パス '{path}' は整数であるべきですが、型が異なります。{type(value)}"
                )
        elif type_name == "bool":
            if not isinstance(value, bool):
                raise ValidationError(
                    f"パス '{path}' はブール値であるべきですが、型が異なります。{type(value)}"
                )
        elif type_name == "std::string":
            if not isinstance(value, str):
                raise ValidationError(
                    f"パス '{path}' は文字列であるべきですが、型が異なります。{type(value)}"
                )
        else:
            raise ValidationError(
                f"パス '{path}' のスキーマで定義されている型 '{type_name}' は不明な型です。"
            )


# --- ここから新規追加 ---
class EqualityError(Exception):
    """カスタムの等価性比較エラー例外"""

    pass


class EqualityTester:
    """
    テスト対象(T1)とテストケース(T2)のJSONオブジェクトの等価性を検証するクラス。
    T1のRef型はebm_mapを用いて解決する。
    """

    def __init__(
        self,
        validator: SchemaValidator,
        ebm_map: Dict[int, Any] = None,
        rough: set[str] = False,
    ):
        self.validator = validator
        self.ebm_map = ebm_map if ebm_map is not None else {}
        self.rough_fields = rough

    def compare(self, target_obj: Any, case_obj: Any, struct_name: str):
        """比較プロセスのエントリーポイント"""
        self._compare_value(target_obj, case_obj, struct_name, path=struct_name)

    def _compare_value(self, t1_val: Any, t2_val: Any, type_name: str, path: str):
        """型に基づいて値を再帰的に比較する"""
        # 型が構造体の場合
        if type_name in self.validator.structs:
            self._compare_object(t1_val, t2_val, type_name, path)
            return

        # 型がRef系の場合 (特別ルール)
        if type_name.endswith("Ref"):
            self._compare_ref(t1_val, t2_val, type_name, path)
            return

        # その他のプリミティブ型やEnumの場合
        if t1_val != t2_val:
            raise EqualityError(
                f"パス '{path}' の値が異なります: T1=`{t1_val}`, T2=`{t2_val}`"
            )

    def _compare_ref(self, t1_ref: Any, t2_ref: Any, type_name: str, path: str):
        """Ref型を特別に比較する"""
        # T1のRefを解決する
        resolved_t1 = t1_ref
        if isinstance(t1_ref, int) and t1_ref in self.ebm_map:
            resolved_t1 = self.ebm_map[t1_ref]

        # T1(解決後)がオブジェクトの場合
        if isinstance(resolved_t1, dict):
            body_name = ref_to_body(type_name)
            if isinstance(t2_ref, dict):  # T2もオブジェクトなら再帰比較
                self._compare_object(
                    resolved_t1, t2_ref, body_name, f"{path}->{body_name}"
                )
            elif isinstance(t2_ref, int):  # T2が数値ならT1のIDと比較
                if t1_ref != t2_ref:
                    raise EqualityError(
                        f"パス '{path}' のIDが異なります: T1 ID=`{t1_ref}`, T2 ID=`{t2_ref}`"
                    )
            else:
                raise EqualityError(
                    f"パス '{path}' で型の不整合: T1はオブジェクトですが、T2は不正な型です。{type(resolved_t1)} vs {type(t2_ref)}"
                )
        # T1(解決後)が数値の場合
        elif isinstance(resolved_t1, int):
            if not isinstance(t2_ref, int):
                raise EqualityError(
                    f"パス '{path}' で型の不整合: T1は数値ですが、T2はオブジェクトです。{resolved_t1} vs {t2_ref}"
                )
            if resolved_t1 != t2_ref:
                raise EqualityError(
                    f"パス '{path}' のRef値が異なります: T1=`{resolved_t1}`, T2=`{t2_ref}`"
                )
        else:
            raise EqualityError(f"パス '{path}' のT1の値の型が不正です。")

    def _compare_object(self, t1_obj: Any, t2_obj: Any, struct_name: str, path: str):
        """オブジェクト（辞書）を再帰的に比較する"""
        # もし配列ならばlenとcontainerに読み替える
        struct_def = self.validator.structs[struct_name]
        field_map = {f["name"]: f for f in struct_def["fields"]}
        if isArrayLikeObject(t2_obj, field_map):
            t2_obj = {"len": len(t2_obj), "container": t2_obj}
        if not isinstance(t1_obj, dict) or not isinstance(t2_obj, dict):
            raise EqualityError(
                f"パス '{path}' のどちらかまたは両方がオブジェクトではありません。{type(t1_obj)} vs {type(t2_obj)}"
            )

        active_field_names: set[str]
        # subset_infoのハンドリング
        if struct_name in self.validator.subsets:
            t1_kind = t1_obj.get("kind")
            t2_kind = t2_obj.get("kind")
            if t1_kind != t2_kind:
                raise EqualityError(
                    f"パス '{path}' の 'kind' が異なります: T1=`{t1_kind}`, T2=`{t2_kind}`"
                )
            if t1_kind not in self.validator.subsets[struct_name]:
                raise EqualityError(
                    f"パス '{path}' の 'kind' '{t1_kind}' はスキーマに存在しません。"
                )
            active_field_names = set(self.validator.subsets[struct_name][t1_kind])
            path = f"{path}({t1_kind})"
        else:
            active_field_names = set(field_map.keys())

        # フィールドの比較
        for field_name in sorted(list(active_field_names)):
            field_def = field_map[field_name]
            t1_val = t1_obj.get(field_name)
            t2_val = t2_obj.get(field_name)

            new_path = f"{path}.{field_name}"
            field_type = field_def["type"]

            if (
                t1_val is not None
                and t2_val is None
                and (field_name in self.rough_fields)
            ):
                continue

            if field_def.get("is_array", False):
                if not isinstance(t1_val, list) or not isinstance(t2_val, list):
                    raise EqualityError(
                        f"パス '{new_path}' のどちらかが配列ではありません。"
                    )
                if len(t1_val) != len(t2_val):
                    raise EqualityError(
                        f"パス '{new_path}' の配列の長さが異なります: T1は{len(t1_val)}個, T2は{len(t2_val)}個"
                    )
                for i, (t1_item, t2_item) in enumerate(zip(t1_val, t2_val)):
                    self._compare_value(
                        t1_item, t2_item, field_type, f"{new_path}[{i}]"
                    )
            else:
                self._compare_value(t1_val, t2_val, field_type, new_path)


# --- 既存の関数 (変更なし) ---
def make_EBM_map(data: dict):
    map_target = ["identifiers", "strings", "types", "expressions", "statements"]
    result = {}
    for target in map_target:
        for element in data[target]:
            # RefのIDは数値なので、キーも数値に変換
            result[int(element["id"])] = element["body"]
    return result


def execute(command, env, capture=True, input=None) -> bytes:
    passEnv = os.environ.copy()
    if env is not None:
        passEnv.update(env)
    if capture:
        return sp.check_output(command, env=passEnv, stderr=sys.stderr, input=input)
    else:
        return sp.check_call(
            command, env=passEnv, stdout=sys.stdout, stderr=sys.stderr, input=input
        )


# --- main関数を修正 ---
def main():
    """CLIのエントリーポイント"""
    parser = argparse.ArgumentParser(
        description="JSONデータが指定されたスキーマに準拠しているかを検証します。"
    )
    parser.add_argument("json_data", help="検証対象のJSONファイル (T1の元データ)")
    parser.add_argument("struct_name", help="検証の起点となるルート構造体名")
    parser.add_argument(
        "--test-case", default=None, help="テストケースJSONファイル(T2)"
    )
    args = parser.parse_args()

    schema_json = json.loads(
        execute(["./tool/ebmcodegen", "--mode", "spec-json"], None, True)
    )

    try:
        with open(args.json_data, "r", encoding="utf-8") as f:
            data = f.read()
            data_json = json.loads(data)
    except FileNotFoundError as e:
        print(f"エラー: ファイルが見つかりません: {e.filename}", file=sys.stderr)
        sys.exit(1)
    except json.JSONDecodeError as e:
        print(f"エラー: JSONの解析に失敗しました: {e}", file=sys.stderr)
        sys.exit(1)

    try:
        validator = SchemaValidator(schema_json)
        print(
            f"🔬 ファイル '{args.json_data}' が '{args.struct_name}' スキーマに準拠しているか検証中..."
        )
        validator.validate(data_json, args.struct_name)
        print("✅ 検証成功: スキーマに準拠しています。")

        if args.test_case:
            # --- テストケース実行ロジック ---
            test_cases: list[str] = [args.test_case]
            if os.path.isdir(args.test_case):
                test_cases = os.listdir(args.test_case)
            for case in test_cases:
                print("-" * 20)
                with open(case, "r", encoding="utf-8") as f:
                    test_case_json = json.load(f)

                # 1. T1 (テスト対象) をjqで抽出
                target_t1 = json.loads(
                    execute(
                        ["jq", test_case_json["condition"]], None, True, data.encode()
                    )
                )

                # 2. T2 (テストケース) を取得
                case_t2 = test_case_json["case"]
                struct_to_compare = test_case_json["struct"]
                rough_field = set[str](test_case_json["rough"])

                # ---【復活させた検証部分 1/2】テストケース(T2)自体のスキーマ検証 ---
                print(
                    f"🔬 テストケース '{args.test_case}' の 'case' が '{struct_to_compare}' スキーマに準拠しているか検証中..."
                )
                validator.validate(case_t2, struct_to_compare, rough_field)
                print(
                    f"✅ 検証成功: テストケースはスキーマに準拠しています。{"(rough)" if test_case_json["rough"]else ""}"
                )

                # ---【復活させた検証部分 2/2】テスト対象(T1)のスキーマ検証 ---
                print(
                    f"🔬 テスト対象(T1)が '{struct_to_compare}' スキーマに準拠しているか検証中..."
                )
                validator.validate(target_t1, struct_to_compare)
                print("✅ 検証成功: テスト対象はスキーマに準拠しています。")

                # 3. ebm_mapを作成 (必要な場合)
                ebm_map = None
                if args.struct_name == "ExtendedBinaryModule":
                    ebm_map = make_EBM_map(data_json)

                print(f"🔬 テストケース '{args.test_case}' を用いて等価性を検証中...")
                print(
                    f"    - T1: '{args.json_data}' の '{test_case_json['condition']}' の結果"
                )
                print(f"    - T2: '{args.test_case}' の 'case' フィールド")
                print(f"    - テスト除外フィールド: {",".join(rough_field)}")

                # 4. EqualityTesterで比較
                tester = EqualityTester(validator, ebm_map, rough_field)
                tester.compare(target_t1, case_t2, struct_to_compare)

                print(
                    "✅ 等価性検証成功: テスト対象(T1)とテストケース(T2)は等しいです。"
                    + (
                        f"({",".join(rough_field)}を除く)"
                        if len(rough_field) != 0
                        else ""
                    )
                )

    except (ValidationError, EqualityError) as e:
        print(f"❌ 検証に失敗しました:\n{e}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"予期せぬエラーが発生しました: {e}", file=sys.stderr)
        print(traceback.format_exc(), file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
