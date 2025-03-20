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
## function `eval`
### ACCESS
#### left_eval_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of left operand
#### left_eval
Type: EvalResult
Initial Value: eval(ctx.ref(left_eval_ref), ctx)
Description: left operand
#### right_ident_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of right operand
#### right_ident
Type: string
Initial Value: ctx.ident(right_ident_ref)
Description: identifier of right operand
### ADDRESS_OF
#### target_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of target object
#### target
Type: EvalResult
Initial Value: eval(ctx.ref(target_ref), ctx)
Description: target object
### ARRAY_SIZE
#### vector_eval_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of array
#### vector_eval
Type: EvalResult
Initial Value: eval(ctx.ref(vector_eval_ref), ctx)
Description: array
### ASSIGN
#### left_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of assign target
#### right_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of assign source
#### ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of previous assignment or phi or definition
### BEGIN_COND_BLOCK
#### ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of expression
### BINARY
#### op
Type: rebgn::BinaryOp
Initial Value: code.bop().value()
Description: binary operator
#### left_eval_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of left operand
#### left_eval
Type: EvalResult
Initial Value: eval(ctx.ref(left_eval_ref), ctx)
Description: left operand
#### right_eval_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of right operand
#### right_eval
Type: EvalResult
Initial Value: eval(ctx.ref(right_eval_ref), ctx)
Description: right operand
#### opstr
Type: string
Initial Value: to_string(op)
Description: binary operator string
### CAST
#### type_ref
Type: Varint
Initial Value: code.type().value()
Description: reference of cast target type
#### type
Type: string
Initial Value: type_to_string(ctx,type_ref)
Description: cast target type
#### from_type_ref
Type: Varint
Initial Value: code.from_type().value()
Description: reference of cast source type
#### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of cast source value
#### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: cast source value
### DECLARE_VARIABLE
#### ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of expression
### DEFINE_PARAMETER
#### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of variable value
#### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of variable value
### DEFINE_VARIABLE
#### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of variable value
#### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of variable value
### DEFINE_VARIABLE_REF
#### ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of expression
### FIELD_AVAILABLE
#### left_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of field (maybe null)
#### right_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of condition
#### left_eval
Type: EvalResult
Initial Value: eval(ctx.ref(left_ref), ctx)
Description: field
#### right_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of condition
### IMMEDIATE_CHAR
#### char_code
Type: uint64_t
Initial Value: code.int_value()->value()
Description: immediate char code
### IMMEDIATE_INT
#### value
Type: uint64_t
Initial Value: code.int_value()->value()
Description: immediate value
### IMMEDIATE_INT64
#### value
Type: uint64_t
Initial Value: *code.int_value64()
Description: immediate value
### IMMEDIATE_STRING
#### str
Type: string
Initial Value: ctx.string_table[code.ident().value().value()]
Description: immediate string
### IMMEDIATE_TYPE
#### type_ref
Type: Varint
Initial Value: code.type().value()
Description: reference of immediate type
#### type
Type: string
Initial Value: type_to_string(ctx,type_ref)
Description: immediate type
### INDEX
#### left_eval_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of indexed object
#### left_eval
Type: EvalResult
Initial Value: eval(ctx.ref(left_eval_ref), ctx)
Description: indexed object
#### right_eval_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of index
#### right_eval
Type: EvalResult
Initial Value: eval(ctx.ref(right_eval_ref), ctx)
Description: index
### IS_LITTLE_ENDIAN
#### fallback
Type: Varint
Initial Value: code.fallback().value()
Description: reference of fallback expression
### NEW_OBJECT
#### type_ref
Type: Varint
Initial Value: code.type().value()
Description: reference of object type
#### type
Type: string
Initial Value: type_to_string(ctx,type_ref)
Description: object type
### OPTIONAL_OF
#### target_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of target object
#### target
Type: EvalResult
Initial Value: eval(ctx.ref(target_ref), ctx)
Description: target object
#### type_ref
Type: Varint
Initial Value: code.type().value()
Description: reference of type of optional (not include optional)
#### type
Type: string
Initial Value: type_to_string(ctx,type_ref)
Description: type of optional (not include optional)
### PHI
#### ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of expression
### PROPERTY_INPUT_PARAMETER
#### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of variable value
#### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of variable value
### UNARY
#### op
Type: rebgn::UnaryOp
Initial Value: code.uop().value()
Description: unary operator
#### target_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of target
#### target
Type: EvalResult
Initial Value: eval(ctx.ref(target_ref), ctx)
Description: target
#### opstr
Type: string
Initial Value: to_string(op)
Description: unary operator string
## function `inner_function`
### APPEND
#### vector_eval_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of vector (not temporary)
#### vector_eval
Type: EvalResult
Initial Value: eval(ctx.ref(vector_eval_ref), ctx)
Description: vector (not temporary)
#### new_element_eval_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of new element
#### new_element_eval
Type: EvalResult
Initial Value: eval(ctx.ref(new_element_eval_ref), ctx)
Description: new element
### ASSERT
#### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of assertion condition
#### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: assertion condition
### ASSIGN
#### left_eval_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of assignment target
#### left_eval
Type: EvalResult
Initial Value: eval(ctx.ref(left_eval_ref), ctx)
Description: assignment target
#### right_eval_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of assignment source
#### right_eval
Type: EvalResult
Initial Value: eval(ctx.ref(right_eval_ref), ctx)
Description: assignment source
### BACKWARD_INPUT
#### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of backward offset to move (in byte)
#### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: backward offset to move (in byte)
### BACKWARD_OUTPUT
#### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of backward offset to move (in byte)
#### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: backward offset to move (in byte)
### BEGIN_DECODE_PACKED_OPERATION
#### fallback
Type: Varint
Initial Value: code.fallback().value()
Description: reference of fallback operation
#### inner_range
Type: string
Initial Value: ctx.range(fallback)
Description: range of fallback operation
### BEGIN_DECODE_SUB_RANGE
#### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of sub range length or vector expression
#### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: sub range length or vector expression
#### sub_range_type
Type: SubRangeType
Initial Value: code.sub_range_type().value()
Description: sub range type (byte_len or replacement)
### BEGIN_ENCODE_PACKED_OPERATION
#### fallback
Type: Varint
Initial Value: code.fallback().value()
Description: reference of fallback operation
#### inner_range
Type: string
Initial Value: ctx.range(fallback)
Description: range of fallback operation
### BEGIN_ENCODE_SUB_RANGE
#### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of sub range length or vector expression
#### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: sub range length or vector expression
#### sub_range_type
Type: SubRangeType
Initial Value: code.sub_range_type().value()
Description: sub range type (byte_len or replacement)
### CALL_DECODE
#### func_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of function
#### func_belong
Type: Varint
Initial Value: ctx.ref(func_ref).belong().value()
Description: reference of function belong
#### func_belong_name
Type: string
Initial Value: type_accessor(ctx.ref(func_belong), ctx)
Description: function belong name
#### func_name
Type: string
Initial Value: ctx.ident(func_ref)
Description: identifier of function
#### obj_eval_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of `this` object
#### obj_eval
Type: EvalResult
Initial Value: eval(ctx.ref(obj_eval_ref), ctx)
Description: `this` object
#### inner_range
Type: string
Initial Value: ctx.range(func_ref)
Description: range of function call range
### CALL_ENCODE
#### func_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of function
#### func_belong
Type: Varint
Initial Value: ctx.ref(func_ref).belong().value()
Description: reference of function belong
#### func_belong_name
Type: string
Initial Value: type_accessor(ctx.ref(func_belong), ctx)
Description: function belong name
#### func_name
Type: string
Initial Value: ctx.ident(func_ref)
Description: identifier of function
#### obj_eval_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of `this` object
#### obj_eval
Type: EvalResult
Initial Value: eval(ctx.ref(obj_eval_ref), ctx)
Description: `this` object
#### inner_range
Type: string
Initial Value: ctx.range(func_ref)
Description: range of function call range
### CASE
#### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of condition
#### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: condition
### CHECK_UNION
#### union_member_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of current union member
#### union_ref
Type: Varint
Initial Value: ctx.ref(union_member_ref).belong().value()
Description: reference of union
#### union_field_ref
Type: Varint
Initial Value: ctx.ref(union_ref).belong().value()
Description: reference of union field
#### union_member_index
Type: uint64_t
Initial Value: ctx.ref(union_member_ref).int_value()->value()
Description: current union member index
#### union_member_ident
Type: string
Initial Value: ctx.ident(union_member_ref)
Description: identifier of union member
#### union_ident
Type: string
Initial Value: ctx.ident(union_ref)
Description: identifier of union
#### union_field_ident
Type: EvalResult
Initial Value: eval(ctx.ref(union_field_ref), ctx)
Description: union field
#### check_type
Type: rebgn::UnionCheckAt
Initial Value: code.check_at().value()
Description: union check location
### DECLARE_VARIABLE
#### ident_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of variable
#### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of variable
#### init_ref
Type: Varint
Initial Value: ctx.ref(code.ref().value()).ref().value()
Description: reference of variable initialization
#### init
Type: EvalResult
Initial Value: eval(ctx.ref(init_ref), ctx)
Description: variable initialization
#### type_ref
Type: Varint
Initial Value: ctx.ref(code.ref().value()).type().value()
Description: reference of variable
#### type
Type: string
Initial Value: type_to_string(ctx,type_ref)
Description: variable
### DECODE_INT
#### fallback
Type: Varint
Initial Value: code.fallback().value()
Description: reference of fallback operation
#### inner_range
Type: string
Initial Value: ctx.range(fallback)
Description: range of fallback operation
#### bit_size
Type: uint64_t
Initial Value: code.bit_size()->value()
Description: bit size of element
#### endian
Type: rebgn::Endian
Initial Value: code.endian().value()
Description: endian of element
#### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of element
#### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: element
### DECODE_INT_VECTOR
#### fallback
Type: Varint
Initial Value: code.fallback().value()
Description: reference of fallback operation
#### inner_range
Type: string
Initial Value: ctx.range(fallback)
Description: range of fallback operation
#### bit_size
Type: uint64_t
Initial Value: code.bit_size()->value()
Description: bit size of element
#### endian
Type: rebgn::Endian
Initial Value: code.endian().value()
Description: endian of element
#### vector_value_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of vector
#### vector_value
Type: EvalResult
Initial Value: eval(ctx.ref(vector_value_ref), ctx)
Description: vector
#### size_value_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of size
#### size_value
Type: EvalResult
Initial Value: eval(ctx.ref(size_value_ref), ctx)
Description: size
### DECODE_INT_VECTOR_FIXED
#### fallback
Type: Varint
Initial Value: code.fallback().value()
Description: reference of fallback operation
#### inner_range
Type: string
Initial Value: ctx.range(fallback)
Description: range of fallback operation
#### bit_size
Type: uint64_t
Initial Value: code.bit_size()->value()
Description: bit size of element
#### endian
Type: rebgn::Endian
Initial Value: code.endian().value()
Description: endian of element
#### vector_value_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of vector
#### vector_value
Type: EvalResult
Initial Value: eval(ctx.ref(vector_value_ref), ctx)
Description: vector
#### size_value_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of size
#### size_value
Type: EvalResult
Initial Value: eval(ctx.ref(size_value_ref), ctx)
Description: size
### DECODE_INT_VECTOR_UNTIL_EOF
#### fallback
Type: Varint
Initial Value: code.fallback().value()
Description: reference of fallback operation
#### inner_range
Type: string
Initial Value: ctx.range(fallback)
Description: range of fallback operation
#### bit_size
Type: uint64_t
Initial Value: code.bit_size()->value()
Description: bit size of element
#### endian
Type: rebgn::Endian
Initial Value: code.endian().value()
Description: endian of element
#### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of element
#### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: element
### DEFINE_FUNCTION
#### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of function
#### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of function
#### type
Type: std::optional<std::string>
Initial Value: std::nullopt
Description: function return type
#### belong_name
Type: std::optional<std::string>
Initial Value: std::nullopt
Description: function belong name
### DEFINE_VARIABLE
#### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of variable
#### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of variable
#### init_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of variable initialization
#### init
Type: EvalResult
Initial Value: eval(ctx.ref(init_ref), ctx)
Description: variable initialization
#### type_ref
Type: Varint
Initial Value: code.type().value()
Description: reference of variable
#### type
Type: string
Initial Value: type_to_string(ctx,type_ref)
Description: variable
### DYNAMIC_ENDIAN
#### fallback
Type: Varint
Initial Value: code.fallback().value()
Description: reference of fallback operation
#### inner_range
Type: string
Initial Value: ctx.range(fallback)
Description: range of fallback operation
### ELIF
#### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of condition
#### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: condition
### ENCODE_INT
#### fallback
Type: Varint
Initial Value: code.fallback().value()
Description: reference of fallback operation
#### inner_range
Type: string
Initial Value: ctx.range(fallback)
Description: range of fallback operation
#### bit_size
Type: uint64_t
Initial Value: code.bit_size()->value()
Description: bit size of element
#### endian
Type: rebgn::Endian
Initial Value: code.endian().value()
Description: endian of element
#### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of element
#### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: element
### ENCODE_INT_VECTOR
#### fallback
Type: Varint
Initial Value: code.fallback().value()
Description: reference of fallback operation
#### inner_range
Type: string
Initial Value: ctx.range(fallback)
Description: range of fallback operation
#### bit_size
Type: uint64_t
Initial Value: code.bit_size()->value()
Description: bit size of element
#### endian
Type: rebgn::Endian
Initial Value: code.endian().value()
Description: endian of element
#### vector_value_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of vector
#### vector_value
Type: EvalResult
Initial Value: eval(ctx.ref(vector_value_ref), ctx)
Description: vector
#### size_value_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of size
#### size_value
Type: EvalResult
Initial Value: eval(ctx.ref(size_value_ref), ctx)
Description: size
### ENCODE_INT_VECTOR_FIXED
#### fallback
Type: Varint
Initial Value: code.fallback().value()
Description: reference of fallback operation
#### inner_range
Type: string
Initial Value: ctx.range(fallback)
Description: range of fallback operation
#### bit_size
Type: uint64_t
Initial Value: code.bit_size()->value()
Description: bit size of element
#### endian
Type: rebgn::Endian
Initial Value: code.endian().value()
Description: endian of element
#### vector_value_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of vector
#### vector_value
Type: EvalResult
Initial Value: eval(ctx.ref(vector_value_ref), ctx)
Description: vector
#### size_value_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of size
#### size_value
Type: EvalResult
Initial Value: eval(ctx.ref(size_value_ref), ctx)
Description: size
### END_DECODE_PACKED_OPERATION
#### fallback
Type: Varint
Initial Value: code.fallback().value()
Description: reference of fallback operation
#### inner_range
Type: string
Initial Value: ctx.range(fallback)
Description: range of fallback operation
### END_ENCODE_PACKED_OPERATION
#### fallback
Type: Varint
Initial Value: code.fallback().value()
Description: reference of fallback operation
#### inner_range
Type: string
Initial Value: ctx.range(fallback)
Description: range of fallback operation
### EXHAUSTIVE_MATCH
#### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of condition
#### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: condition
### EXPLICIT_ERROR
#### param
Type: Param
Initial Value: code.param().value()
Description: error message parameters
#### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(param.refs[0]), ctx)
Description: error message
### IF
#### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of condition
#### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: condition
### INC
#### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of increment target
#### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: increment target
### LENGTH_CHECK
#### vector_eval_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of vector to check
#### vector_eval
Type: EvalResult
Initial Value: eval(ctx.ref(vector_eval_ref), ctx)
Description: vector to check
#### size_eval_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of size to check
#### size_eval
Type: EvalResult
Initial Value: eval(ctx.ref(size_eval_ref), ctx)
Description: size to check
### LOOP_CONDITION
#### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of condition
#### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: condition
### MATCH
#### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of condition
#### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: condition
### PEEK_INT_VECTOR
#### fallback
Type: Varint
Initial Value: code.fallback().value()
Description: reference of fallback operation
#### inner_range
Type: string
Initial Value: ctx.range(fallback)
Description: range of fallback operation
#### bit_size
Type: uint64_t
Initial Value: code.bit_size()->value()
Description: bit size of element
#### endian
Type: rebgn::Endian
Initial Value: code.endian().value()
Description: endian of element
#### vector_value_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of vector
#### vector_value
Type: EvalResult
Initial Value: eval(ctx.ref(vector_value_ref), ctx)
Description: vector
#### size_value_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of size
#### size_value
Type: EvalResult
Initial Value: eval(ctx.ref(size_value_ref), ctx)
Description: size
### RET
#### ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of return value
#### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(ref), ctx)
Description: return value
### SWITCH_UNION
#### union_member_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of current union member
#### union_ref
Type: Varint
Initial Value: ctx.ref(union_member_ref).belong().value()
Description: reference of union
#### union_field_ref
Type: Varint
Initial Value: ctx.ref(union_ref).belong().value()
Description: reference of union field
#### union_member_index
Type: uint64_t
Initial Value: ctx.ref(union_member_ref).int_value()->value()
Description: current union member index
#### union_member_ident
Type: string
Initial Value: ctx.ident(union_member_ref)
Description: identifier of union member
#### union_ident
Type: string
Initial Value: ctx.ident(union_ref)
Description: identifier of union
#### union_field_ident
Type: EvalResult
Initial Value: eval(ctx.ref(union_field_ref), ctx)
Description: union field
## function `inner_block`
### DECLARE_BIT_FIELD
#### ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of bit field
#### ident
Type: string
Initial Value: ctx.ident(ref)
Description: identifier of bit field
#### inner_range
Type: string
Initial Value: ctx.range(ref)
Description: range of bit field
#### type_ref
Type: Varint
Initial Value: ctx.ref(ref).type().value()
Description: reference of bit field type
#### type
Type: string
Initial Value: type_to_string(ctx,type_ref)
Description: bit field type
### DECLARE_ENUM
#### ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of ENUM
#### inner_range
Type: string
Initial Value: ctx.range(code.ref().value())
Description: range of ENUM
### DECLARE_FORMAT
#### ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of FORMAT
#### inner_range
Type: string
Initial Value: ctx.range(code.ref().value())
Description: range of FORMAT
### DECLARE_FUNCTION
#### ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of FUNCTION
#### inner_range
Type: string
Initial Value: ctx.range(code.ref().value())
Description: range of FUNCTION
### DECLARE_PROPERTY
#### ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of PROPERTY
#### inner_range
Type: string
Initial Value: ctx.range(code.ref().value())
Description: range of PROPERTY
### DECLARE_STATE
#### ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of STATE
#### inner_range
Type: string
Initial Value: ctx.range(code.ref().value())
Description: range of STATE
### DECLARE_UNION
#### ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of UNION
#### inner_range
Type: string
Initial Value: ctx.range(ref)
Description: range of UNION
### DECLARE_UNION_MEMBER
#### ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of UNION_MEMBER
#### inner_range
Type: string
Initial Value: ctx.range(ref)
Description: range of UNION_MEMBER
### DEFINE_ENUM
#### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of enum
#### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of enum
#### base_type_ref
Type: StorageRef
Initial Value: code.type().value()
Description: type reference of enum base type
#### base_type
Type: std::optional<std::string>
Initial Value: std::nullopt
Description: enum base type
### DEFINE_ENUM_MEMBER
#### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of enum member
#### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of enum member
#### evaluated_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of enum member value
#### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: enum member value
#### enum_ident_ref
Type: Varint
Initial Value: code.belong().value()
Description: reference of enum
#### enum_ident
Type: string
Initial Value: ctx.ident(enum_ident_ref)
Description: identifier of enum
### DEFINE_FIELD
#### type
Type: string
Initial Value: type_to_string(ctx,code.type().value())
Description: field type
#### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of field
#### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of field
### DEFINE_FORMAT
#### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of format
#### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of format
### DEFINE_PROPERTY_GETTER
#### func
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of function
#### inner_range
Type: string
Initial Value: ctx.range(func)
Description: range of function
### DEFINE_PROPERTY_SETTER
#### func
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of function
#### inner_range
Type: string
Initial Value: ctx.range(func)
Description: range of function
### DEFINE_STATE
#### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of format
#### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of format
### DEFINE_UNION
#### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of union
#### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of union
### DEFINE_UNION_MEMBER
#### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of format
#### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of format
## function `add_parameter`
### DEFINE_PARAMETER
#### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of parameter
#### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of parameter
#### type_ref
Type: Varint
Initial Value: code.type().value()
Description: reference of parameter type
#### type
Type: string
Initial Value: type_to_string(ctx,type_ref)
Description: parameter type
### PROPERTY_INPUT_PARAMETER
#### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of parameter
#### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of parameter
#### type_ref
Type: Varint
Initial Value: code.type().value()
Description: reference of parameter type
#### type
Type: string
Initial Value: type_to_string(ctx,type_ref)
Description: parameter type
### STATE_VARIABLE_PARAMETER
#### ident_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of state variable
#### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of state variable
#### type_ref
Type: Varint
Initial Value: ctx.ref(ident_ref).type().value()
Description: reference of state variable type
#### type
Type: string
Initial Value: type_to_string(ctx,type_ref)
Description: state variable type
## function `add_call_parameter`
### PROPERTY_INPUT_PARAMETER
#### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of parameter
#### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of parameter
### STATE_VARIABLE_PARAMETER
#### ident_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of state variable
#### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of state variable
## function `type_to_string`
### ARRAY
#### base_type
Type: string
Initial Value: type_to_string_impl(ctx, s, bit_size, index + 1)
Description: base type
#### is_byte_vector
Type: bool
Initial Value: index + 1 < s.storages.size() && s.storages[index + 1].type == rebgn::StorageType::UINT && s.storages[index + 1].size().value().value() == 8
Description: is byte vector
#### length
Type: uint64_t
Initial Value: storage.size()->value()
Description: array length
### ENUM
#### ident_ref
Type: Varint
Initial Value: storage.ref().value()
Description: reference of enum
#### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of enum
### FLOAT
#### size
Type: uint64_t
Initial Value: storage.size()->value()
Description: bit size
### INT
#### size
Type: uint64_t
Initial Value: storage.size()->value()
Description: bit size
### OPTIONAL
#### base_type
Type: string
Initial Value: type_to_string_impl(ctx, s, bit_size, index + 1)
Description: base type
### PTR
#### base_type
Type: string
Initial Value: type_to_string_impl(ctx, s, bit_size, index + 1)
Description: base type
### RECURSIVE_STRUCT_REF
#### ident_ref
Type: Varint
Initial Value: storage.ref().value()
Description: reference of recursive struct
#### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of recursive struct
### STRUCT_REF
#### ident_ref
Type: Varint
Initial Value: storage.ref().value()
Description: reference of struct
#### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of struct
### UINT
#### size
Type: uint64_t
Initial Value: storage.size()->value()
Description: bit size
### VARIANT
#### ident_ref
Type: Varint
Initial Value: storage.ref().value()
Description: reference of variant
#### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of variant
#### types
Type: std::vector<std::string>
Initial Value: {}
Description: variant types
### VECTOR
#### base_type
Type: string
Initial Value: type_to_string_impl(ctx, s, bit_size, index + 1)
Description: base type
#### is_byte_vector
Type: bool
Initial Value: index + 1 < s.storages.size() && s.storages[index + 1].type == rebgn::StorageType::UINT && s.storages[index + 1].size().value().value() == 8
Description: is byte vector
## function `field_accessor`
### DEFINE_BIT_FIELD
#### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of BIT_FIELD
#### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of BIT_FIELD
#### belong
Type: Varint
Initial Value: code.belong().value()
Description: reference of belong
#### is_member
Type: bool
Initial Value: belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM
Description: is member of a struct
### DEFINE_FIELD
#### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of FIELD
#### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of FIELD
#### belong
Type: Varint
Initial Value: code.belong().value()
Description: reference of belong
#### is_member
Type: bool
Initial Value: belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM
Description: is member of a struct
#### belong_eval
Type: string
Initial Value: field_accessor(ctx.ref(belong), ctx)
Description: belong eval
### DEFINE_PROPERTY
#### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of PROPERTY
#### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of PROPERTY
#### belong
Type: Varint
Initial Value: code.belong().value()
Description: reference of belong
#### is_member
Type: bool
Initial Value: belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM
Description: is member of a struct
#### belong_eval
Type: string
Initial Value: field_accessor(ctx.ref(belong), ctx)
Description: belong eval
### DEFINE_UNION
#### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of UNION
#### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of UNION
#### belong
Type: Varint
Initial Value: code.belong().value()
Description: reference of belong
#### is_member
Type: bool
Initial Value: belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM
Description: is member of a struct
#### belong_eval
Type: string
Initial Value: field_accessor(ctx.ref(belong), ctx)
Description: belong eval
### DEFINE_UNION_MEMBER
#### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of UNION_MEMBER
#### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of UNION_MEMBER
#### belong
Type: Varint
Initial Value: code.belong().value()
Description: reference of belong
#### is_member
Type: bool
Initial Value: belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM
Description: is member of a struct
#### union_member_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of union member
#### union_ref
Type: Varint
Initial Value: belong
Description: reference of union
#### union_field_ref
Type: Varint
Initial Value: ctx.ref(union_ref).belong().value()
Description: reference of union field
#### union_field_belong
Type: Varint
Initial Value: ctx.ref(union_field_ref).belong().value()
Description: reference of union field belong
## function `type_accessor`
### DEFINE_BIT_FIELD
#### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of BIT_FIELD
#### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of BIT_FIELD
#### belong
Type: Varint
Initial Value: code.belong().value()
Description: reference of belong
#### is_member
Type: bool
Initial Value: belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM
Description: is member of a struct
### DEFINE_FIELD
#### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of FIELD
#### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of FIELD
#### belong
Type: Varint
Initial Value: code.belong().value()
Description: reference of belong
#### is_member
Type: bool
Initial Value: belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM
Description: is member of a struct
#### belong_eval
Type: string
Initial Value: type_accessor(ctx.ref(belong),ctx)
Description: field accessor
### DEFINE_FORMAT
#### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of FORMAT
#### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of FORMAT
### DEFINE_PROPERTY
#### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of PROPERTY
#### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of PROPERTY
#### belong
Type: Varint
Initial Value: code.belong().value()
Description: reference of belong
#### is_member
Type: bool
Initial Value: belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM
Description: is member of a struct
#### belong_eval
Type: string
Initial Value: type_accessor(ctx.ref(belong),ctx)
Description: field accessor
### DEFINE_STATE
#### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of STATE
#### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of STATE
### DEFINE_UNION
#### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of UNION
#### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of UNION
#### belong
Type: Varint
Initial Value: code.belong().value()
Description: reference of belong
#### is_member
Type: bool
Initial Value: belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM
Description: is member of a struct
#### belong_eval
Type: string
Initial Value: type_accessor(ctx.ref(belong),ctx)
Description: field accessor
### DEFINE_UNION_MEMBER
#### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of UNION_MEMBER
#### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of UNION_MEMBER
#### belong
Type: Varint
Initial Value: code.belong().value()
Description: reference of belong
#### is_member
Type: bool
Initial Value: belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM
Description: is member of a struct
#### union_member_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of union member
#### union_ref
Type: Varint
Initial Value: belong
Description: reference of union
#### union_field_ref
Type: Varint
Initial Value: ctx.ref(union_ref).belong().value()
Description: reference of union field
#### union_field_belong
Type: Varint
Initial Value: ctx.ref(union_field_ref).belong().value()
Description: reference of union field belong
#### belong_eval
Type: string
Initial Value: type_accessor(ctx.ref(union_field_belong),ctx)
Description: field accessor
