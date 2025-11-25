#include <memory>
#include "ebmgen/common.hpp"

namespace ebm2rmw {
    enum class ValueKind {
        INT,
        BYTES_ARRAY_ZERO,
        BYTES_ARRAY,
        INDEX_ACCESS,
        OBJECT_REF,
    };
    struct Env;
    struct Value {
       private:
        ValueKind kind = ValueKind::INT;
        std::uint64_t value = 0;

       public:
        Value() = default;
        Value(ValueKind k, std::uint64_t v)
            : kind(k), value(v) {}
        Value(std::uint64_t v)
            : kind(ValueKind::INT), value(v) {}
        ValueKind get_kind() const {
            return kind;
        }

        std::uint64_t get_value() const {
            return value;
        }

        ebmgen::expected<std::uint64_t> get_int(Env& env) const;

        ebmgen::expected<std::map<std::string, std::shared_ptr<Value>>*> as_object_ref(Env& env);
    };

    struct Scope {
        std::map<std::string, std::shared_ptr<Value>> variables;
    };

}  // namespace ebm2rmw