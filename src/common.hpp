/*license*/
#pragma once
#include <error/error.h>
#include <helper/expected.h>
#include <format>
namespace brgen::ast {}

namespace rebgn {
    namespace ast = brgen::ast;
    using Error = futils::error::Error<>;

    template <typename T>
    using expected = futils::helper::either::expected<T, futils::error::Error<>>;

    template <typename... Args>
    Error error(std::format_string<Args...> fmt, Args&&... args) {
        return futils::error::StrError{
            std::format(fmt, std::forward<Args>(args)...),
        };
    }

    template <typename... Args>
    futils::helper::either::unexpected<Error> unexpect_error(std::format_string<Args...> fmt, Args&&... args) {
        return futils::helper::either::unexpected(error(fmt, std::forward<Args>(args)...));
    }

    futils::helper::either::unexpected<Error> unexpect_error(Error&& err) {
        return futils::helper::either::unexpected(std::forward<decltype(err)>(err));
    }

}  // namespace rebgn