/*license*/
#pragma once
#include <error/error.h>
#include <helper/expected.h>
#include <format>
#include <ebm/extended_binary_module.hpp>
namespace brgen::ast {}

namespace ebmgen {
    namespace ast = brgen::ast;
    using Error = futils::error::Error<std::allocator<char>, std::string>;

    template <typename T>
    using expected = futils::helper::either::expected<T, Error>;

#define FORMAT_ARG std::format_string<Args...>
    template <typename... Args>
    Error error(FORMAT_ARG fmt, Args&&... args) {
        return futils::error::StrError{
            std::format(fmt, std::forward<Args>(args)...),
        };
    }

    template <typename... Args>
    futils::helper::either::unexpected<Error> unexpect_error(FORMAT_ARG fmt, Args&&... args) {
        return futils::helper::either::unexpected(error(fmt, std::forward<Args>(args)...));
    }

    inline futils::helper::either::unexpected<Error> unexpect_error(Error&& err) {
        return futils::helper::either::unexpected(std::forward<decltype(err)>(err));
    }

    constexpr auto none = Error();

    constexpr ebm::Varint null_varint = ebm::Varint();

    constexpr expected<ebm::Varint> varint(std::uint64_t n) {
        ebm::Varint v;
        if (n < 0x40) {
            v.prefix(0);
            v.value(n);
        }
        else if (n < 0x4000) {
            v.prefix(1);
            v.value(n);
        }
        else if (n < 0x40000000) {
            v.prefix(2);
            v.value(n);
        }
        else if (n < 0x4000000000000000) {
            v.prefix(3);
            v.value(n);
        }
        else {
            return unexpect_error("Invalid varint value: {}", n);
        }
        return v;
    }

    template <class T>
    void append(T& container, const typename decltype(T::container)::value_type& value) {
        container.container.push_back(value);
        container.len = varint(container.container.size()).value();
    }
}  // namespace ebmgen
