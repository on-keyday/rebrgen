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

    struct Env {
        size_t object_id = 0;
        size_t new_id() {
            return object_id++;
        }
        Value self;
        std::map<std::uint64_t, std::string> bytes_arrays;
        std::map<std::uint64_t, std::pair<std::string*, std::uint64_t>> index_access_arrays;

        std::map<std::uint64_t, std::map<std::string, std::shared_ptr<Value>>> object_refs;

        std::uint64_t register_object_ref() {
            auto id = new_id();
            object_refs[id] = std::map<std::string, std::shared_ptr<Value>>();
            return id;
        }

        void init_self() {
            auto id = register_object_ref();
            self = Value(ValueKind::OBJECT_REF, id);
        }

        std::uint64_t register_index_access_array(std::string* base, std::uint64_t index) {
            auto id = new_id();
            index_access_arrays[id] = std::make_pair(base, index);
            return id;
        }

        std::string* index_access_base(const std::shared_ptr<Value>& v) {
            if (v->get_kind() == ValueKind::BYTES_ARRAY_ZERO) {
                auto size = v->get_value();
                auto id = new_id();
                bytes_arrays[id].resize(size, 0);
                *v = Value(ValueKind::BYTES_ARRAY, id);
                return &bytes_arrays[id];
            }
            if (v->get_kind() == ValueKind::BYTES_ARRAY) {
                auto id = v->get_value();
                auto it = bytes_arrays.find(id);
                if (it != bytes_arrays.end()) {
                    return &it->second;
                }
            }
            return nullptr;
        }

        std::vector<std::shared_ptr<Scope>> scopes;

        Env() {
            push_scope();
        }

        void push_scope() {
            scopes.emplace_back(std::make_shared<Scope>());
        }

        void pop_scope() {
            scopes.pop_back();
        }

        std::shared_ptr<Value> get_variable(std::string_view name) {
            for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
                auto& scope = *it;
                auto found = scope->variables.find(std::string(name));
                if (found != scope->variables.end()) {
                    return found->second;
                }
            }
            return nullptr;
        }

        void set_variable(std::string_view name, std::shared_ptr<Value> value) {
            if (scopes.empty()) {
                throw std::runtime_error("no scope available");
            }
            scopes.back()->variables[std::string(name)] = value;
        }

        void define_variable(std::string_view name, Value val) {
            if (scopes.empty()) {
                throw std::runtime_error("no scope available");
            }
            scopes.back()->variables[std::string(name)] = std::make_shared<Value>(val);
        }

        void define_reference(std::string_view name, std::shared_ptr<Value> ref) {
            if (scopes.empty()) {
                throw std::runtime_error("no scope available");
            }
            scopes.back()->variables[std::string(name)] = ref;
        }
    };

    inline ebmgen::expected<std::uint64_t> Value::get_int(Env& env) const {
        if (kind == ValueKind::INT) {
            return value;
        }
        if (kind == ValueKind::INDEX_ACCESS) {
            auto it = env.index_access_arrays.find(value);
            if (it != env.index_access_arrays.end()) {
                auto& [base, index] = it->second;
                if (base && index < base->size()) {
                    return static_cast<std::uint8_t>((*base)[index]);
                }
                return ebmgen::unexpect_error("index access out of bounds");
            }
            return ebmgen::unexpect_error("invalid index access value");
        }
        return ebmgen::unexpect_error("cannot convert to int from value kind");
    }

    inline ebmgen::expected<std::map<std::string, std::shared_ptr<Value>>*> Value::as_object_ref(Env& env) {
        if (kind != ValueKind::OBJECT_REF) {
            return ebmgen::unexpect_error("not an object reference value");
        }
        auto it = env.object_refs.find(value);
        if (it != env.object_refs.end()) {
            return &it->second;
        }
        return ebmgen::unexpect_error("invalid object reference value");
    }

}  // namespace ebm2rmw