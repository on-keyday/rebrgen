# Template Document
This document describes the variables that can be used in the code generator-generator hooks.
Code generator-generator hooks are pieces of C++ code that are inserted into the generator code.
## Words
### reference
A reference to a several object in the binary module.
They are represented as rebgn::Varint. they are not supposed to be used directly.
### type reference
A reference to a type in the binary module.
They are represented as rebgn::StorageRef. they are not supposed to be used directly.
### identifier
An identifier of a several object (e.g. function, variable, types, etc.)
They are represented as std::string. use them for generating code.
### EvalResult
result of eval() function. it contains the result of the expression evaluation.
### placeholder
in some case, code generator placeholder in `config.json` can be replaced with context specific value by envrionment variable like format.
for example, config name `int_type` can take two env map `BIT_SIZE` and `ALIGNED_BIT_SIZE`.
and use like below:
```json
{
    "int_type": "std::int${ALIGNED_BIT_SIZE}_t"
}
```
and if you set `ALIGNED_BIT_SIZE` to 32, then `int_type` will be `std::int32_t`.
specially, `$DOLLAR` or `${DOLLAR}` is reserved for `$` character.
## function `eval`
### ACCESS
#### Variables: 
##### left_eval_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of left operand
##### left_eval
Type: EvalResult
Initial Value: eval(ctx.ref(left_eval_ref), ctx)
Description: left operand
##### right_ident_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of right operand
##### right_ident
Type: string
Initial Value: ctx.ident(right_ident_ref)
Description: identifier of right operand
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `std::format("{}.{}", left_eval.result, right_ident)`
### ADDRESS_OF
#### Variables: 
##### target_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of target object
##### target
Type: EvalResult
Initial Value: eval(ctx.ref(target_ref), ctx)
Description: target object
#### Placeholder Mappings: 
##### address_of_placeholder
Name: VALUE
Mapped Value: `",target.result,"`
##### eval_result_text
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `futils::strutil::concat<std::string>("&",target.result,"")`
### ARRAY_SIZE
#### Variables: 
##### vector_eval_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of array
##### vector_eval
Type: EvalResult
Initial Value: eval(ctx.ref(vector_eval_ref), ctx)
Description: array
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `std::format("{}({})", vector_eval.result, "size")`
### ASSIGN
#### Variables: 
##### left_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of assign target
##### right_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of assign source
##### ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of previous assignment or phi or definition
#### Placeholder Mappings: 
##### eval_result_passthrough
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `eval(ctx.ref(ref), ctx)`
##### eval_result_passthrough
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `eval(ctx.ref(left_ref), ctx)`
### BEGIN_COND_BLOCK
#### Variables: 
##### ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of expression
#### Placeholder Mappings: 
##### eval_result_passthrough
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `eval(ctx.ref(ref), ctx)`
### BINARY
#### Variables: 
##### op
Type: rebgn::BinaryOp
Initial Value: code.bop().value()
Description: binary operator
##### left_eval_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of left operand
##### left_eval
Type: EvalResult
Initial Value: eval(ctx.ref(left_eval_ref), ctx)
Description: left operand
##### right_eval_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of right operand
##### right_eval
Type: EvalResult
Initial Value: eval(ctx.ref(right_eval_ref), ctx)
Description: right operand
##### opstr
Type: string
Initial Value: to_string(op)
Description: binary operator string
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `std::format("({} {} {})", left_eval.result, opstr, right_eval.result)`
### CALL
#### Variables: 
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `"/*Unimplemented CALL*/"`
### CALL_CAST
#### Variables: 
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `"/*Unimplemented CALL_CAST*/"`
### CAN_READ
#### Variables: 
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `"/*Unimplemented CAN_READ*/"`
### CAST
#### Variables: 
##### type_ref
Type: Varint
Initial Value: code.type().value()
Description: reference of cast target type
##### type
Type: string
Initial Value: type_to_string(ctx,type_ref)
Description: cast target type
##### from_type_ref
Type: Varint
Initial Value: code.from_type().value()
Description: reference of cast source type
##### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of cast source value
##### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: cast source value
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `std::format("({}){}", type, evaluated.result)`
### DECLARE_VARIABLE
#### Variables: 
##### ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of expression
#### Placeholder Mappings: 
##### eval_result_passthrough
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `eval(ctx.ref(ref), ctx)`
### DEFINE_CONSTANT
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of constant name
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of constant name
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `ident`
### DEFINE_FIELD
#### Variables: 
#### Placeholder Mappings: 
##### eval_result_passthrough
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `field_accessor(code,ctx)`
### DEFINE_PARAMETER
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of variable value
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of variable value
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `ident`
### DEFINE_PROPERTY
#### Variables: 
#### Placeholder Mappings: 
##### eval_result_passthrough
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `field_accessor(code,ctx)`
### DEFINE_VARIABLE
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of variable value
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of variable value
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `ident`
### DEFINE_VARIABLE_REF
#### Variables: 
##### ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of expression
#### Placeholder Mappings: 
##### eval_result_passthrough
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `eval(ctx.ref(ref), ctx)`
### EMPTY_OPTIONAL
#### Variables: 
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `"std::nullopt"`
### EMPTY_PTR
#### Variables: 
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `"nullptr"`
### FIELD_AVAILABLE
#### Variables: 
##### left_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of field (maybe null)
##### right_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of condition
##### left_eval
Type: EvalResult
Initial Value: eval(ctx.ref(left_ref), ctx)
Description: field
##### right_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of condition
#### Placeholder Mappings: 
##### eval_result_passthrough
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `eval(ctx.ref(right_ref), ctx)`
##### eval_result_passthrough
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `eval(ctx.ref(right_ref), ctx)`
### IMMEDIATE_CHAR
#### Variables: 
##### char_code
Type: uint64_t
Initial Value: code.int_value()->value()
Description: immediate char code
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `std::format("{}", char_code)`
### IMMEDIATE_FALSE
#### Variables: 
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `"false"`
### IMMEDIATE_INT
#### Variables: 
##### value
Type: uint64_t
Initial Value: code.int_value()->value()
Description: immediate value
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `std::format("{}", value)`
### IMMEDIATE_INT64
#### Variables: 
##### value
Type: uint64_t
Initial Value: *code.int_value64()
Description: immediate value
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `std::format("{}", value)`
### IMMEDIATE_STRING
#### Variables: 
##### str
Type: string
Initial Value: ctx.string_table[code.ident().value().value()]
Description: immediate string
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `std::format("\"{}\"", futils::escape::escape_str<std::string>(str,futils::escape::EscapeFlag::hex,futils::escape::no_escape_set(),futils::escape::escape_all()))`
### IMMEDIATE_TRUE
#### Variables: 
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `"true"`
### IMMEDIATE_TYPE
#### Variables: 
##### type_ref
Type: Varint
Initial Value: code.type().value()
Description: reference of immediate type
##### type
Type: string
Initial Value: type_to_string(ctx,type_ref)
Description: immediate type
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `type`
### INDEX
#### Variables: 
##### left_eval_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of indexed object
##### left_eval
Type: EvalResult
Initial Value: eval(ctx.ref(left_eval_ref), ctx)
Description: indexed object
##### right_eval_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of index
##### right_eval
Type: EvalResult
Initial Value: eval(ctx.ref(right_eval_ref), ctx)
Description: index
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `std::format("{}[{}]", left_eval.result, right_eval.result)`
### INPUT_BIT_OFFSET
#### Variables: 
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `"/*Unimplemented INPUT_BIT_OFFSET*/"`
### INPUT_BYTE_OFFSET
#### Variables: 
#### Placeholder Mappings: 
##### decode_offset
Name: DECODER
Mapped Value: `",ctx.r(),"`
##### eval_result_text
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `futils::strutil::concat<std::string>("",ctx.r(),".offset()")`
### IS_LITTLE_ENDIAN
#### Variables: 
##### fallback
Type: Varint
Initial Value: code.fallback().value()
Description: reference of fallback expression
#### Placeholder Mappings: 
##### eval_result_passthrough
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `eval(ctx.ref(fallback), ctx)`
##### eval_result_text
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `"std::endian::native == std::endian::little"`
### NEW_OBJECT
#### Variables: 
##### type_ref
Type: Varint
Initial Value: code.type().value()
Description: reference of object type
##### type
Type: string
Initial Value: type_to_string(ctx,type_ref)
Description: object type
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `std::format("{}()", type)`
### OPTIONAL_OF
#### Variables: 
##### target_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of target object
##### target
Type: EvalResult
Initial Value: eval(ctx.ref(target_ref), ctx)
Description: target object
##### type_ref
Type: Varint
Initial Value: code.type().value()
Description: reference of type of optional (not include optional)
##### type
Type: string
Initial Value: type_to_string(ctx,type_ref)
Description: type of optional (not include optional)
#### Placeholder Mappings: 
##### optional_of_placeholder
Name: TYPE
Mapped Value: `",type,"`
Name: VALUE
Mapped Value: `",target.result,"`
##### eval_result_text
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `futils::strutil::concat<std::string>("",target.result,"")`
### OUTPUT_BIT_OFFSET
#### Variables: 
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `"/*Unimplemented OUTPUT_BIT_OFFSET*/"`
### OUTPUT_BYTE_OFFSET
#### Variables: 
#### Placeholder Mappings: 
##### encode_offset
Name: ENCODER
Mapped Value: `",ctx.w(),"`
##### eval_result_text
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `futils::strutil::concat<std::string>("",ctx.w(),".offset()")`
### PHI
#### Variables: 
##### ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of expression
#### Placeholder Mappings: 
##### eval_result_passthrough
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `eval(ctx.ref(ref), ctx)`
### PROPERTY_INPUT_PARAMETER
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of variable value
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of variable value
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `ident`
### REMAIN_BYTES
#### Variables: 
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `"/*Unimplemented REMAIN_BYTES*/"`
### UNARY
#### Variables: 
##### op
Type: rebgn::UnaryOp
Initial Value: code.uop().value()
Description: unary operator
##### target_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of target
##### target
Type: EvalResult
Initial Value: eval(ctx.ref(target_ref), ctx)
Description: target
##### opstr
Type: string
Initial Value: to_string(op)
Description: unary operator string
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `std::format("({}{})", opstr, target.result)`
## function `inner_function`
### APPEND
#### Variables: 
##### vector_eval_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of vector (not temporary)
##### vector_eval
Type: EvalResult
Initial Value: eval(ctx.ref(vector_eval_ref), ctx)
Description: vector (not temporary)
##### new_element_eval_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of new element
##### new_element_eval
Type: EvalResult
Initial Value: eval(ctx.ref(new_element_eval_ref), ctx)
Description: new element
#### Placeholder Mappings: 
### ASSERT
#### Variables: 
##### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of assertion condition
##### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: assertion condition
#### Placeholder Mappings: 
### ASSIGN
#### Variables: 
##### left_eval_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of assignment target
##### left_eval
Type: EvalResult
Initial Value: eval(ctx.ref(left_eval_ref), ctx)
Description: assignment target
##### right_eval_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of assignment source
##### right_eval
Type: EvalResult
Initial Value: eval(ctx.ref(right_eval_ref), ctx)
Description: assignment source
#### Placeholder Mappings: 
### BACKWARD_INPUT
#### Variables: 
##### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of backward offset to move (in byte)
##### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: backward offset to move (in byte)
#### Placeholder Mappings: 
##### decode_backward
Name: DECODER
Mapped Value: `",ctx.r(),"`
Name: OFFSET
Mapped Value: `",evaluated.result,"`
### BACKWARD_OUTPUT
#### Variables: 
##### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of backward offset to move (in byte)
##### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: backward offset to move (in byte)
#### Placeholder Mappings: 
##### encode_backward
Name: ENCODER
Mapped Value: `",ctx.w(),"`
Name: OFFSET
Mapped Value: `",evaluated.result,"`
### BEGIN_DECODE_PACKED_OPERATION
#### Variables: 
##### fallback
Type: Varint
Initial Value: code.fallback().value()
Description: reference of fallback operation
##### inner_range
Type: string
Initial Value: ctx.range(fallback)
Description: range of fallback operation
#### Placeholder Mappings: 
### BEGIN_DECODE_SUB_RANGE
#### Variables: 
##### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of sub range length or vector expression
##### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: sub range length or vector expression
##### sub_range_type
Type: SubRangeType
Initial Value: code.sub_range_type().value()
Description: sub range type (byte_len or replacement)
#### Placeholder Mappings: 
### BEGIN_ENCODE_PACKED_OPERATION
#### Variables: 
##### fallback
Type: Varint
Initial Value: code.fallback().value()
Description: reference of fallback operation
##### inner_range
Type: string
Initial Value: ctx.range(fallback)
Description: range of fallback operation
#### Placeholder Mappings: 
### BEGIN_ENCODE_SUB_RANGE
#### Variables: 
##### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of sub range length or vector expression
##### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: sub range length or vector expression
##### sub_range_type
Type: SubRangeType
Initial Value: code.sub_range_type().value()
Description: sub range type (byte_len or replacement)
#### Placeholder Mappings: 
### CALL_DECODE
#### Variables: 
##### func_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of function
##### func_belong
Type: Varint
Initial Value: ctx.ref(func_ref).belong().value()
Description: reference of function belong
##### func_belong_name
Type: string
Initial Value: type_accessor(ctx.ref(func_belong), ctx)
Description: function belong name
##### func_name
Type: string
Initial Value: ctx.ident(func_ref)
Description: identifier of function
##### obj_eval_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of `this` object
##### obj_eval
Type: EvalResult
Initial Value: eval(ctx.ref(obj_eval_ref), ctx)
Description: `this` object
##### inner_range
Type: string
Initial Value: ctx.range(func_ref)
Description: range of function call range
#### Placeholder Mappings: 
### CALL_ENCODE
#### Variables: 
##### func_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of function
##### func_belong
Type: Varint
Initial Value: ctx.ref(func_ref).belong().value()
Description: reference of function belong
##### func_belong_name
Type: string
Initial Value: type_accessor(ctx.ref(func_belong), ctx)
Description: function belong name
##### func_name
Type: string
Initial Value: ctx.ident(func_ref)
Description: identifier of function
##### obj_eval_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of `this` object
##### obj_eval
Type: EvalResult
Initial Value: eval(ctx.ref(obj_eval_ref), ctx)
Description: `this` object
##### inner_range
Type: string
Initial Value: ctx.range(func_ref)
Description: range of function call range
#### Placeholder Mappings: 
### CASE
#### Variables: 
##### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of condition
##### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: condition
#### Placeholder Mappings: 
### CHECK_UNION
#### Variables: 
##### union_member_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of current union member
##### union_ref
Type: Varint
Initial Value: ctx.ref(union_member_ref).belong().value()
Description: reference of union
##### union_field_ref
Type: Varint
Initial Value: ctx.ref(union_ref).belong().value()
Description: reference of union field
##### union_member_index
Type: uint64_t
Initial Value: ctx.ref(union_member_ref).int_value()->value()
Description: current union member index
##### union_member_ident
Type: string
Initial Value: ctx.ident(union_member_ref)
Description: identifier of union member
##### union_ident
Type: string
Initial Value: ctx.ident(union_ref)
Description: identifier of union
##### union_field_ident
Type: EvalResult
Initial Value: eval(ctx.ref(union_field_ref), ctx)
Description: union field
##### check_type
Type: rebgn::UnionCheckAt
Initial Value: code.check_at().value()
Description: union check location
#### Placeholder Mappings: 
##### check_union_condition
Name: FIELD_IDENT
Mapped Value: `",union_field_ident.result,"`
Name: MEMBER_FULL_IDENT
Mapped Value: `",type_accessor(ctx.ref(union_member_ref),ctx),"`
Name: MEMBER_IDENT
Mapped Value: `",union_member_ident,"`
Name: MEMBER_INDEX
Mapped Value: `",futils::number::to_string<std::string>(union_member_index),"`
Name: UNION_FULL_IDENT
Mapped Value: `",type_accessor(ctx.ref(union_ref),ctx),"`
Name: UNION_IDENT
Mapped Value: `",union_ident,"`
##### check_union_fail_return_value
Name: FIELD_IDENT
Mapped Value: `",union_field_ident.result,"`
Name: MEMBER_FULL_IDENT
Mapped Value: `",type_accessor(ctx.ref(union_member_ref),ctx),"`
Name: MEMBER_IDENT
Mapped Value: `",union_member_ident,"`
Name: MEMBER_INDEX
Mapped Value: `",futils::number::to_string<std::string>(union_member_index),"`
Name: UNION_FULL_IDENT
Mapped Value: `",type_accessor(ctx.ref(union_ref),ctx),"`
Name: UNION_IDENT
Mapped Value: `",union_ident,"`
### DECLARE_VARIABLE
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of variable
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of variable
##### init_ref
Type: Varint
Initial Value: ctx.ref(code.ref().value()).ref().value()
Description: reference of variable initialization
##### init
Type: EvalResult
Initial Value: eval(ctx.ref(init_ref), ctx)
Description: variable initialization
##### type_ref
Type: Varint
Initial Value: ctx.ref(code.ref().value()).type().value()
Description: reference of variable
##### type
Type: string
Initial Value: type_to_string(ctx,type_ref)
Description: variable
#### Placeholder Mappings: 
### DECODE_INT
#### Variables: 
##### fallback
Type: Varint
Initial Value: code.fallback().value()
Description: reference of fallback operation
##### inner_range
Type: string
Initial Value: ctx.range(fallback)
Description: range of fallback operation
##### bit_size
Type: uint64_t
Initial Value: code.bit_size()->value()
Description: bit size of element
##### endian
Type: rebgn::Endian
Initial Value: code.endian().value()
Description: endian of element
##### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of element
##### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: element
#### Placeholder Mappings: 
### DECODE_INT_VECTOR
#### Variables: 
##### fallback
Type: Varint
Initial Value: code.fallback().value()
Description: reference of fallback operation
##### inner_range
Type: string
Initial Value: ctx.range(fallback)
Description: range of fallback operation
##### bit_size
Type: uint64_t
Initial Value: code.bit_size()->value()
Description: bit size of element
##### endian
Type: rebgn::Endian
Initial Value: code.endian().value()
Description: endian of element
##### vector_value_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of vector
##### vector_value
Type: EvalResult
Initial Value: eval(ctx.ref(vector_value_ref), ctx)
Description: vector
##### size_value_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of size
##### size_value
Type: EvalResult
Initial Value: eval(ctx.ref(size_value_ref), ctx)
Description: size
#### Placeholder Mappings: 
##### decode_bytes_op
Name: DECODER
Mapped Value: `",ctx.r(),"`
Name: LEN
Mapped Value: `",size_value.result,"`
Name: VALUE
Mapped Value: `",vector_value.result,"`
### DECODE_INT_VECTOR_FIXED
#### Variables: 
##### fallback
Type: Varint
Initial Value: code.fallback().value()
Description: reference of fallback operation
##### inner_range
Type: string
Initial Value: ctx.range(fallback)
Description: range of fallback operation
##### bit_size
Type: uint64_t
Initial Value: code.bit_size()->value()
Description: bit size of element
##### endian
Type: rebgn::Endian
Initial Value: code.endian().value()
Description: endian of element
##### vector_value_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of vector
##### vector_value
Type: EvalResult
Initial Value: eval(ctx.ref(vector_value_ref), ctx)
Description: vector
##### size_value_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of size
##### size_value
Type: EvalResult
Initial Value: eval(ctx.ref(size_value_ref), ctx)
Description: size
#### Placeholder Mappings: 
##### decode_bytes_op
Name: DECODER
Mapped Value: `",ctx.r(),"`
Name: LEN
Mapped Value: `",size_value.result,"`
Name: VALUE
Mapped Value: `",vector_value.result,"`
### DECODE_INT_VECTOR_UNTIL_EOF
#### Variables: 
##### fallback
Type: Varint
Initial Value: code.fallback().value()
Description: reference of fallback operation
##### inner_range
Type: string
Initial Value: ctx.range(fallback)
Description: range of fallback operation
##### bit_size
Type: uint64_t
Initial Value: code.bit_size()->value()
Description: bit size of element
##### endian
Type: rebgn::Endian
Initial Value: code.endian().value()
Description: endian of element
##### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of element
##### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: element
#### Placeholder Mappings: 
##### decode_bytes_until_eof_op
Name: DECODER
Mapped Value: `",ctx.r(),"`
Name: VALUE
Mapped Value: `",evaluated.result,"`
### DEFINE_CONSTANT
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of constant name
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of constant name
##### init_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of constant value
##### init
Type: EvalResult
Initial Value: eval(ctx.ref(init_ref), ctx)
Description: constant value
#### Placeholder Mappings: 
### DEFINE_FUNCTION
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of function
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of function
##### func_type
Type: rebgn::FunctionType
Initial Value: code.func_type().value()
Description: function type
##### type
Type: std::optional<std::string>
Initial Value: std::nullopt
Description: function return type
##### belong_name
Type: std::optional<std::string>
Initial Value: std::nullopt
Description: function belong name
#### Placeholder Mappings: 
### DEFINE_VARIABLE
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of variable
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of variable
##### init_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of variable initialization
##### init
Type: EvalResult
Initial Value: eval(ctx.ref(init_ref), ctx)
Description: variable initialization
##### type_ref
Type: Varint
Initial Value: code.type().value()
Description: reference of variable
##### type
Type: string
Initial Value: type_to_string(ctx,type_ref)
Description: variable
#### Placeholder Mappings: 
### DYNAMIC_ENDIAN
#### Variables: 
##### fallback
Type: Varint
Initial Value: code.fallback().value()
Description: reference of fallback operation
##### inner_range
Type: string
Initial Value: ctx.range(fallback)
Description: range of fallback operation
#### Placeholder Mappings: 
### ELIF
#### Variables: 
##### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of condition
##### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: condition
#### Placeholder Mappings: 
### ENCODE_INT
#### Variables: 
##### fallback
Type: Varint
Initial Value: code.fallback().value()
Description: reference of fallback operation
##### inner_range
Type: string
Initial Value: ctx.range(fallback)
Description: range of fallback operation
##### bit_size
Type: uint64_t
Initial Value: code.bit_size()->value()
Description: bit size of element
##### endian
Type: rebgn::Endian
Initial Value: code.endian().value()
Description: endian of element
##### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of element
##### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: element
#### Placeholder Mappings: 
### ENCODE_INT_VECTOR
#### Variables: 
##### fallback
Type: Varint
Initial Value: code.fallback().value()
Description: reference of fallback operation
##### inner_range
Type: string
Initial Value: ctx.range(fallback)
Description: range of fallback operation
##### bit_size
Type: uint64_t
Initial Value: code.bit_size()->value()
Description: bit size of element
##### endian
Type: rebgn::Endian
Initial Value: code.endian().value()
Description: endian of element
##### vector_value_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of vector
##### vector_value
Type: EvalResult
Initial Value: eval(ctx.ref(vector_value_ref), ctx)
Description: vector
##### size_value_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of size
##### size_value
Type: EvalResult
Initial Value: eval(ctx.ref(size_value_ref), ctx)
Description: size
#### Placeholder Mappings: 
##### encode_bytes_op
Name: ENCODER
Mapped Value: `",ctx.w(),"`
Name: LEN
Mapped Value: `",size_value.result,"`
Name: VALUE
Mapped Value: `",vector_value.result,"`
### ENCODE_INT_VECTOR_FIXED
#### Variables: 
##### fallback
Type: Varint
Initial Value: code.fallback().value()
Description: reference of fallback operation
##### inner_range
Type: string
Initial Value: ctx.range(fallback)
Description: range of fallback operation
##### bit_size
Type: uint64_t
Initial Value: code.bit_size()->value()
Description: bit size of element
##### endian
Type: rebgn::Endian
Initial Value: code.endian().value()
Description: endian of element
##### vector_value_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of vector
##### vector_value
Type: EvalResult
Initial Value: eval(ctx.ref(vector_value_ref), ctx)
Description: vector
##### size_value_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of size
##### size_value
Type: EvalResult
Initial Value: eval(ctx.ref(size_value_ref), ctx)
Description: size
#### Placeholder Mappings: 
##### encode_bytes_op
Name: ENCODER
Mapped Value: `",ctx.w(),"`
Name: LEN
Mapped Value: `",size_value.result,"`
Name: VALUE
Mapped Value: `",vector_value.result,"`
### END_DECODE_PACKED_OPERATION
#### Variables: 
##### fallback
Type: Varint
Initial Value: code.fallback().value()
Description: reference of fallback operation
##### inner_range
Type: string
Initial Value: ctx.range(fallback)
Description: range of fallback operation
#### Placeholder Mappings: 
### END_ENCODE_PACKED_OPERATION
#### Variables: 
##### fallback
Type: Varint
Initial Value: code.fallback().value()
Description: reference of fallback operation
##### inner_range
Type: string
Initial Value: ctx.range(fallback)
Description: range of fallback operation
#### Placeholder Mappings: 
### EXHAUSTIVE_MATCH
#### Variables: 
##### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of condition
##### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: condition
#### Placeholder Mappings: 
### EXPLICIT_ERROR
#### Variables: 
##### param
Type: Param
Initial Value: code.param().value()
Description: error message parameters
##### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(param.refs[0]), ctx)
Description: error message
#### Placeholder Mappings: 
### IF
#### Variables: 
##### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of condition
##### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: condition
#### Placeholder Mappings: 
### INC
#### Variables: 
##### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of increment target
##### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: increment target
#### Placeholder Mappings: 
### LENGTH_CHECK
#### Variables: 
##### vector_eval_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of vector to check
##### vector_eval
Type: EvalResult
Initial Value: eval(ctx.ref(vector_eval_ref), ctx)
Description: vector to check
##### size_eval_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of size to check
##### size_eval
Type: EvalResult
Initial Value: eval(ctx.ref(size_eval_ref), ctx)
Description: size to check
#### Placeholder Mappings: 
### LOOP_CONDITION
#### Variables: 
##### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of condition
##### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: condition
#### Placeholder Mappings: 
### MATCH
#### Variables: 
##### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of condition
##### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: condition
#### Placeholder Mappings: 
### PEEK_INT_VECTOR
#### Variables: 
##### fallback
Type: Varint
Initial Value: code.fallback().value()
Description: reference of fallback operation
##### inner_range
Type: string
Initial Value: ctx.range(fallback)
Description: range of fallback operation
##### bit_size
Type: uint64_t
Initial Value: code.bit_size()->value()
Description: bit size of element
##### endian
Type: rebgn::Endian
Initial Value: code.endian().value()
Description: endian of element
##### vector_value_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of vector
##### vector_value
Type: EvalResult
Initial Value: eval(ctx.ref(vector_value_ref), ctx)
Description: vector
##### size_value_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of size
##### size_value
Type: EvalResult
Initial Value: eval(ctx.ref(size_value_ref), ctx)
Description: size
#### Placeholder Mappings: 
##### peek_bytes_op
Name: DECODER
Mapped Value: `",ctx.r(),"`
Name: LEN
Mapped Value: `",size_value.result,"`
Name: VALUE
Mapped Value: `",vector_value.result,"`
### RESERVE_SIZE
#### Variables: 
##### vector_eval_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of vector
##### vector_eval
Type: EvalResult
Initial Value: eval(ctx.ref(vector_eval_ref), ctx)
Description: vector
##### size_eval_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of size
##### size_eval
Type: EvalResult
Initial Value: eval(ctx.ref(size_eval_ref), ctx)
Description: size
##### reserve_type
Type: rebgn::ReserveType
Initial Value: code.reserve_type().value()
Description: reserve vector type
#### Placeholder Mappings: 
##### reserve_size_static
Name: SIZE
Mapped Value: `",size_eval.result,"`
Name: VECTOR
Mapped Value: `",vector_eval.result,"`
##### reserve_size_dynamic
Name: SIZE
Mapped Value: `",size_eval.result,"`
Name: VECTOR
Mapped Value: `",vector_eval.result,"`
### RET
#### Variables: 
##### ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of return value
##### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(ref), ctx)
Description: return value
#### Placeholder Mappings: 
### SWITCH_UNION
#### Variables: 
##### union_member_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of current union member
##### union_ref
Type: Varint
Initial Value: ctx.ref(union_member_ref).belong().value()
Description: reference of union
##### union_field_ref
Type: Varint
Initial Value: ctx.ref(union_ref).belong().value()
Description: reference of union field
##### union_member_index
Type: uint64_t
Initial Value: ctx.ref(union_member_ref).int_value()->value()
Description: current union member index
##### union_member_ident
Type: string
Initial Value: ctx.ident(union_member_ref)
Description: identifier of union member
##### union_ident
Type: string
Initial Value: ctx.ident(union_ref)
Description: identifier of union
##### union_field_ident
Type: EvalResult
Initial Value: eval(ctx.ref(union_field_ref), ctx)
Description: union field
#### Placeholder Mappings: 
##### check_union_condition
Name: FIELD_IDENT
Mapped Value: `",union_field_ident.result,"`
Name: MEMBER_FULL_IDENT
Mapped Value: `",type_accessor(ctx.ref(union_member_ref),ctx),"`
Name: MEMBER_IDENT
Mapped Value: `",union_member_ident,"`
Name: MEMBER_INDEX
Mapped Value: `",futils::number::to_string<std::string>(union_member_index),"`
Name: UNION_FULL_IDENT
Mapped Value: `",type_accessor(ctx.ref(union_ref),ctx),"`
Name: UNION_IDENT
Mapped Value: `",union_ident,"`
##### switch_union
Name: FIELD_IDENT
Mapped Value: `",union_field_ident.result,"`
Name: MEMBER_FULL_IDENT
Mapped Value: `",type_accessor(ctx.ref(union_member_ref),ctx),"`
Name: MEMBER_IDENT
Mapped Value: `",union_member_ident,"`
Name: MEMBER_INDEX
Mapped Value: `",futils::number::to_string<std::string>(union_member_index),"`
Name: UNION_FULL_IDENT
Mapped Value: `",type_accessor(ctx.ref(union_ref),ctx),"`
Name: UNION_IDENT
Mapped Value: `",union_ident,"`
## function `inner_block`
### DECLARE_BIT_FIELD
#### Variables: 
##### ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of bit field
##### ident
Type: string
Initial Value: ctx.ident(ref)
Description: identifier of bit field
##### inner_range
Type: string
Initial Value: ctx.range(ref)
Description: range of bit field
##### type_ref
Type: Varint
Initial Value: ctx.ref(ref).type().value()
Description: reference of bit field type
##### type
Type: string
Initial Value: type_to_string(ctx,type_ref)
Description: bit field type
#### Placeholder Mappings: 
### DECLARE_ENUM
#### Variables: 
##### ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of ENUM
##### inner_range
Type: string
Initial Value: ctx.range(code.ref().value())
Description: range of ENUM
#### Placeholder Mappings: 
### DECLARE_FORMAT
#### Variables: 
##### ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of FORMAT
##### inner_range
Type: string
Initial Value: ctx.range(code.ref().value())
Description: range of FORMAT
#### Placeholder Mappings: 
### DECLARE_FUNCTION
#### Variables: 
##### ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of FUNCTION
##### inner_range
Type: string
Initial Value: ctx.range(code.ref().value())
Description: range of FUNCTION
##### func_type
Type: rebgn::FunctionType
Initial Value: ctx.ref(ref).func_type().value()
Description: function type
#### Placeholder Mappings: 
### DECLARE_PROPERTY
#### Variables: 
##### ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of PROPERTY
##### inner_range
Type: string
Initial Value: ctx.range(code.ref().value())
Description: range of PROPERTY
#### Placeholder Mappings: 
### DECLARE_STATE
#### Variables: 
##### ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of STATE
##### inner_range
Type: string
Initial Value: ctx.range(code.ref().value())
Description: range of STATE
#### Placeholder Mappings: 
### DECLARE_UNION
#### Variables: 
##### ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of UNION
##### inner_range
Type: string
Initial Value: ctx.range(ref)
Description: range of UNION
#### Placeholder Mappings: 
### DECLARE_UNION_MEMBER
#### Variables: 
##### ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of UNION_MEMBER
##### inner_range
Type: string
Initial Value: ctx.range(ref)
Description: range of UNION_MEMBER
#### Placeholder Mappings: 
### DEFINE_BIT_FIELD
#### Variables: 
##### type
Type: string
Initial Value: type_to_string(ctx,code.type().value())
Description: bit field type
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of bit field
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of bit field
##### belong
Type: Varint
Initial Value: code.belong().value()
Description: reference of belonging struct or bit field
#### Placeholder Mappings: 
### DEFINE_ENUM
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of enum
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of enum
##### base_type_ref
Type: StorageRef
Initial Value: code.type().value()
Description: type reference of enum base type
##### base_type
Type: std::optional<std::string>
Initial Value: std::nullopt
Description: enum base type
#### Placeholder Mappings: 
### DEFINE_ENUM_MEMBER
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of enum member
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of enum member
##### evaluated_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of enum member value
##### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: enum member value
##### enum_ident_ref
Type: Varint
Initial Value: code.belong().value()
Description: reference of enum
##### enum_ident
Type: string
Initial Value: ctx.ident(enum_ident_ref)
Description: identifier of enum
#### Placeholder Mappings: 
### DEFINE_FIELD
#### Variables: 
##### type
Type: string
Initial Value: type_to_string(ctx,code.type().value())
Description: field type
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of field
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of field
##### belong
Type: Varint
Initial Value: code.belong().value()
Description: reference of belonging struct or bit field
##### is_bit_field
Type: bool
Initial Value: belong.value()!=0&&ctx.ref(belong).op==rebgn::AbstractOp::DEFINE_BIT_FIELD
Description: is part of bit field
#### Placeholder Mappings: 
### DEFINE_FORMAT
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of format
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of format
#### Placeholder Mappings: 
### DEFINE_PROPERTY_GETTER
#### Variables: 
##### func
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of function
##### inner_range
Type: string
Initial Value: ctx.range(func)
Description: range of function
#### Placeholder Mappings: 
### DEFINE_PROPERTY_SETTER
#### Variables: 
##### func
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of function
##### inner_range
Type: string
Initial Value: ctx.range(func)
Description: range of function
#### Placeholder Mappings: 
### DEFINE_STATE
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of format
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of format
#### Placeholder Mappings: 
### DEFINE_UNION
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of union
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of union
#### Placeholder Mappings: 
### DEFINE_UNION_MEMBER
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of format
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of format
#### Placeholder Mappings: 
## function `add_parameter`
### DEFINE_PARAMETER
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of parameter
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of parameter
##### type_ref
Type: Varint
Initial Value: code.type().value()
Description: reference of parameter type
##### type
Type: string
Initial Value: type_to_string(ctx,type_ref)
Description: parameter type
#### Placeholder Mappings: 
### PROPERTY_INPUT_PARAMETER
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of parameter
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of parameter
##### type_ref
Type: Varint
Initial Value: code.type().value()
Description: reference of parameter type
##### type
Type: string
Initial Value: type_to_string(ctx,type_ref)
Description: parameter type
#### Placeholder Mappings: 
### STATE_VARIABLE_PARAMETER
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of state variable
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of state variable
##### type_ref
Type: Varint
Initial Value: ctx.ref(ident_ref).type().value()
Description: reference of state variable type
##### type
Type: string
Initial Value: type_to_string(ctx,type_ref)
Description: state variable type
#### Placeholder Mappings: 
## function `add_call_parameter`
### PROPERTY_INPUT_PARAMETER
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of parameter
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of parameter
#### Placeholder Mappings: 
### STATE_VARIABLE_PARAMETER
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of state variable
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of state variable
#### Placeholder Mappings: 
## function `type_to_string`
### ARRAY
#### Variables: 
##### base_type
Type: string
Initial Value: type_to_string_impl(ctx, s, bit_size, index + 1)
Description: base type
##### is_byte_vector
Type: bool
Initial Value: index + 1 < s.storages.size() && s.storages[index + 1].type == rebgn::StorageType::UINT && s.storages[index + 1].size().value().value() == 8
Description: is byte vector
##### length
Type: uint64_t
Initial Value: storage.size()->value()
Description: array length
#### Placeholder Mappings: 
##### array_type
Name: LENGTH
Mapped Value: `",futils::number::to_string<std::string>(length),"`
Name: TYPE
Mapped Value: `",base_type,"`
##### byte_array_type
Name: LEN
Mapped Value: ``
Name: LENGTH
Mapped Value: `",futils::number::to_string<std::string>(length),"`
Name: TYPE
Mapped Value: `",base_type,"`
### DEFINE_FUNCTION
#### Variables: 
##### func_type
Type: rebgn::FunctionType
Initial Value: code.func_type().value()
Description: function type
#### Placeholder Mappings: 
### ENUM
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: storage.ref().value()
Description: reference of enum
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of enum
#### Placeholder Mappings: 
### FLOAT
#### Variables: 
##### size
Type: uint64_t
Initial Value: storage.size()->value()
Description: bit size
##### aligned_size
Type: uint64_t
Initial Value: size < 32 ? 32 : 64
Description: aligned bit size
#### Placeholder Mappings: 
##### float_type
Name: ALIGNED_BIT_SIZE
Mapped Value: `",futils::number::to_string<std::string>(aligned_size),"`
Name: BIT_SIZE
Mapped Value: `",futils::number::to_string<std::string>(size),"`
### INT
#### Variables: 
##### size
Type: uint64_t
Initial Value: storage.size()->value()
Description: bit size
##### aligned_size
Type: uint64_t
Initial Value: size < 8 ? 8 : size < 16 ? 16 : size < 32 ? 32 : 64
Description: aligned bit size
#### Placeholder Mappings: 
##### int_type
Name: ALIGNED_BIT_SIZE
Mapped Value: `",futils::number::to_string<std::string>(aligned_size),"`
Name: BIT_SIZE
Mapped Value: `",futils::number::to_string<std::string>(size),"`
### OPTIONAL
#### Variables: 
##### base_type
Type: string
Initial Value: type_to_string_impl(ctx, s, bit_size, index + 1)
Description: base type
#### Placeholder Mappings: 
##### optional_type
Name: TYPE
Mapped Value: `",base_type,"`
### PTR
#### Variables: 
##### base_type
Type: string
Initial Value: type_to_string_impl(ctx, s, bit_size, index + 1)
Description: base type
#### Placeholder Mappings: 
##### pointer_type
Name: TYPE
Mapped Value: `",base_type,"`
### RECURSIVE_STRUCT_REF
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: storage.ref().value()
Description: reference of recursive struct
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of recursive struct
#### Placeholder Mappings: 
##### recursive_struct_type
Name: TYPE
Mapped Value: `",ident,"`
### STRUCT_REF
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: storage.ref().value()
Description: reference of struct
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of struct
#### Placeholder Mappings: 
### UINT
#### Variables: 
##### size
Type: uint64_t
Initial Value: storage.size()->value()
Description: bit size
##### aligned_size
Type: uint64_t
Initial Value: size < 8 ? 8 : size < 16 ? 16 : size < 32 ? 32 : 64
Description: aligned bit size
#### Placeholder Mappings: 
##### uint_type
Name: ALIGNED_BIT_SIZE
Mapped Value: `",futils::number::to_string<std::string>(aligned_size),"`
Name: BIT_SIZE
Mapped Value: `",futils::number::to_string<std::string>(size),"`
### VARIANT
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: storage.ref().value()
Description: reference of variant
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of variant
##### types
Type: std::vector<std::string>
Initial Value: {}
Description: variant types
#### Placeholder Mappings: 
### VECTOR
#### Variables: 
##### base_type
Type: string
Initial Value: type_to_string_impl(ctx, s, bit_size, index + 1)
Description: base type
##### is_byte_vector
Type: bool
Initial Value: index + 1 < s.storages.size() && s.storages[index + 1].type == rebgn::StorageType::UINT && s.storages[index + 1].size().value().value() == 8
Description: is byte vector
#### Placeholder Mappings: 
##### vector_type
Name: TYPE
Mapped Value: `",base_type,"`
## function `field_accessor`
### DEFINE_BIT_FIELD
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of BIT_FIELD
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of BIT_FIELD
##### belong
Type: Varint
Initial Value: code.belong().value()
Description: reference of belong
##### is_member
Type: bool
Initial Value: belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM
Description: is member of a struct
#### Placeholder Mappings: 
##### eval_result_passthrough
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `field_accessor(ctx.ref(belong),ctx)`
### DEFINE_FIELD
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of FIELD
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of FIELD
##### belong
Type: Varint
Initial Value: code.belong().value()
Description: reference of belong
##### is_member
Type: bool
Initial Value: belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM
Description: is member of a struct
##### belong_eval
Type: string
Initial Value: field_accessor(ctx.ref(belong), ctx)
Description: belong eval
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `std::format("{}.{}", belong_eval.result, ident)`
##### eval_result_text
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `ident`
### DEFINE_FORMAT
#### Variables: 
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `ctx.this_()`
### DEFINE_PROPERTY
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of PROPERTY
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of PROPERTY
##### belong
Type: Varint
Initial Value: code.belong().value()
Description: reference of belong
##### is_member
Type: bool
Initial Value: belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM
Description: is member of a struct
##### belong_eval
Type: string
Initial Value: field_accessor(ctx.ref(belong), ctx)
Description: belong eval
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `std::format("{}.{}", belong_eval.result, ident)`
##### eval_result_text
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `ident`
### DEFINE_STATE
#### Variables: 
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `ctx.this_()`
### DEFINE_UNION
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of UNION
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of UNION
##### belong
Type: Varint
Initial Value: code.belong().value()
Description: reference of belong
##### is_member
Type: bool
Initial Value: belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM
Description: is member of a struct
##### belong_eval
Type: string
Initial Value: field_accessor(ctx.ref(belong), ctx)
Description: belong eval
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `std::format("{}.{}", belong_eval.result, ident)`
##### eval_result_text
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `ident`
### DEFINE_UNION_MEMBER
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of UNION_MEMBER
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of UNION_MEMBER
##### belong
Type: Varint
Initial Value: code.belong().value()
Description: reference of belong
##### is_member
Type: bool
Initial Value: belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM
Description: is member of a struct
##### union_member_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of union member
##### union_ref
Type: Varint
Initial Value: belong
Description: reference of union
##### union_field_ref
Type: Varint
Initial Value: ctx.ref(union_ref).belong().value()
Description: reference of union field
##### union_field_belong
Type: Varint
Initial Value: ctx.ref(union_field_ref).belong().value()
Description: reference of union field belong
#### Placeholder Mappings: 
##### eval_result_passthrough
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `field_accessor(ctx.ref(union_field_ref),ctx)`
## function `type_accessor`
### DEFINE_BIT_FIELD
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of BIT_FIELD
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of BIT_FIELD
##### belong
Type: Varint
Initial Value: code.belong().value()
Description: reference of belong
##### is_member
Type: bool
Initial Value: belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM
Description: is member of a struct
#### Placeholder Mappings: 
### DEFINE_FIELD
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of FIELD
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of FIELD
##### belong
Type: Varint
Initial Value: code.belong().value()
Description: reference of belong
##### is_member
Type: bool
Initial Value: belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM
Description: is member of a struct
##### belong_eval
Type: string
Initial Value: type_accessor(ctx.ref(belong),ctx)
Description: field accessor
#### Placeholder Mappings: 
### DEFINE_FORMAT
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of FORMAT
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of FORMAT
#### Placeholder Mappings: 
### DEFINE_PROGRAM
#### Variables: 
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
Name: TEXT
Mapped Value: `std::format("{}{}{}","/*",to_string(code.op),"*/")`
### DEFINE_PROPERTY
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of PROPERTY
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of PROPERTY
##### belong
Type: Varint
Initial Value: code.belong().value()
Description: reference of belong
##### is_member
Type: bool
Initial Value: belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM
Description: is member of a struct
##### belong_eval
Type: string
Initial Value: type_accessor(ctx.ref(belong),ctx)
Description: field accessor
#### Placeholder Mappings: 
### DEFINE_STATE
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of STATE
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of STATE
#### Placeholder Mappings: 
### DEFINE_UNION
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of UNION
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of UNION
##### belong
Type: Varint
Initial Value: code.belong().value()
Description: reference of belong
##### is_member
Type: bool
Initial Value: belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM
Description: is member of a struct
##### belong_eval
Type: string
Initial Value: type_accessor(ctx.ref(belong),ctx)
Description: field accessor
#### Placeholder Mappings: 
### DEFINE_UNION_MEMBER
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of UNION_MEMBER
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of UNION_MEMBER
##### belong
Type: Varint
Initial Value: code.belong().value()
Description: reference of belong
##### is_member
Type: bool
Initial Value: belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM
Description: is member of a struct
##### union_member_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of union member
##### union_ref
Type: Varint
Initial Value: belong
Description: reference of union
##### union_field_ref
Type: Varint
Initial Value: ctx.ref(union_ref).belong().value()
Description: reference of union field
##### union_field_belong
Type: Varint
Initial Value: ctx.ref(union_field_ref).belong().value()
Description: reference of union field belong
##### belong_eval
Type: string
Initial Value: type_accessor(ctx.ref(union_field_belong),ctx)
Description: field accessor
#### Placeholder Mappings: 
