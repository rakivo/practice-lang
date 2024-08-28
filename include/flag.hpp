// Copyright 2024 Mark Tyrkba <marktyrkba456@gmail.com>

// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:

// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#pragma once

#include <string>
#include <ostream>
#include <optional>
#include <type_traits>

template <class T>
struct Flag {
    const char *shrt, *lng;
    const char *help = "[EMPTY]";
    bool mandatory = false;

    constexpr Flag(const char *shrt, const char *lng)
        : shrt(shrt),
          lng(lng)
    {};

    constexpr Flag(const char *shrt, const char *lng, const char *help)
        : shrt(shrt),
          lng(lng),
          help(help)
    {};

    constexpr Flag(const char *shrt, const char *lng, bool mandatory)
        : shrt(shrt),
          lng(lng),
          mandatory(mandatory)
    {};

    constexpr Flag(const char *shrt, const char *lng, const char *help, bool mandatory)
        : shrt(shrt),
          lng(lng),
          help(help),
          mandatory(mandatory)
    {};

    constexpr Flag(const char *shrt, const char *lng, bool mandatory, const char *help)
        : shrt(shrt),
          lng(lng),
          help(help),
          mandatory(mandatory)
    {};

    ~Flag() = default;
};

template <class T>
std::ostream&
operator<<(std::ostream &os, const Flag<T> &flag);

// Template for integer types: `int`, `int32_t`, `uint32_t`, etc...
template <class T, class = void>
struct is_constructible_from_int : std::false_type {};

template <class T>
struct is_constructible_from_int<
    T,
    std::void_t<decltype(T { std::declval<int>() })>
> : std::true_type {};

struct FlagParser {
    size_t argc;
    const char *const *const argv;

    constexpr FlagParser(int argc, const char *const *argv)
        : argc(argc),
          argv(argv)
    {};

    [[noreturn]] static void
    mandatory_flag_hasnt_been_passed(const char *shrt, const char *lng);

    [[noreturn]] static void
    flag_hasnt_been_passed(const char *shrt, const char *lng);

    [[noreturn]] static void
    failed_to_parse_flag_to_int(const char *shrt, const char *lng, const char *v);

    [[noreturn]] static void
    failed_to_parse_flag_to_float(const char *shrt, const char *lng, const char *v);

    std::optional<const char *>
    parse_str(const char *shrt, const char *lng) const noexcept;

    inline const char *
    parse_str_or_exit(const char *shrt, const char *lng) const noexcept;


    template <class T>
    bool
    passed(const Flag<T> &flag) const noexcept;

    inline std::optional<const char *>
    parse(const Flag<const char *> &flag) const noexcept;

    inline const char *
    parse(const Flag<const char *> &flag, const char * def) const noexcept;

    inline std::optional<std::string>
    parse(const Flag<std::string> &flag) const noexcept;

    inline std::string
    parse(const Flag<std::string> &flag, std::string def) const noexcept;

    template <class T>
    inline std::enable_if_t<std::is_floating_point<T>::value, std::optional<T>>
    parse(const Flag<T> &flag) const noexcept;

    template <class T>
    inline std::enable_if_t<std::is_floating_point<T>::value, T>
    parse(const Flag<T> &flag, T def) const noexcept;

    template <class T>
    inline std::optional<std::enable_if_t<is_constructible_from_int<T>::value, T>>
    parse(const Flag<T> &flag) const noexcept;

    template <class T>
    inline std::enable_if_t<is_constructible_from_int<T>::value, T>
    parse(const Flag<T> &flag, T def) const noexcept;
};

#ifdef FLAG_IMPLEMENTATION

#include <cstdlib>
#include <iomanip>
#include <cstring>
#include <iostream>

template <class T>
std::ostream&
operator<<(std::ostream &os, const Flag<T> &flag)
{
    const int HELP_WIDTH = 24;

    os << std::right
       << "[" << flag.shrt << ", " << flag.lng << "]"
       << std::setw(HELP_WIDTH - (std::strlen(flag.shrt) + std::strlen(flag.lng) + 4))
       << " " << flag.help;

    return os;
}

[[noreturn]] void
FlagParser::mandatory_flag_hasnt_been_passed(const char *shrt, const char *lng)
{
    std::cerr << "Mandatory flag `" << shrt
              << "` or `"           << lng
              << "` hasn't been passed" << std::endl;

    std::exit(1);
}

[[noreturn]] void
FlagParser::flag_hasnt_been_passed(const char *shrt, const char *lng)
{
    std::cerr << "Flag `" << shrt
              << "` or `" << lng
              << "` hasn't been passed" << std::endl;

    std::exit(1);
}

[[noreturn]] void
FlagParser::failed_to_parse_flag_to_int(const char *shrt, const char *lng, const char *v)
{
    std::cerr << "Failed to parse `" << v << "` to integer"
              << ", value of the `"  << shrt
              << "` or `"            << lng << "` flag." << std::endl;

    std::exit(1);
}

[[noreturn]] void
FlagParser::failed_to_parse_flag_to_float(const char *shrt, const char *lng, const char *v)
{
    std::cerr << "Failed to parse `" << v << "` to float"
              << ", value of the `"  << shrt
              << "` or `"            << lng << "` flag." << std::endl;

    std::exit(1);
}

std::optional<const char *>
FlagParser::parse_str(const char *shrt, const char *lng) const noexcept
{
    for (size_t i = 0; i < argc; i++) {
        if (0 == std::strcmp(argv[i], shrt) or
            0 == std::strcmp(argv[i], lng))
        {
            if (i + 1 >= argc) return std::nullopt;
            return argv[i + 1];
        } else if (0 == std::strncmp(argv[i], shrt, std::strlen(shrt))) {
            return argv[i] + std::strlen(shrt) + 1;
        } else if (0 == std::strncmp(argv[i], lng,  std::strlen(lng))) {
            return argv[i] + std::strlen(lng) + 1;
        }
    }

    return std::nullopt;
}

inline const char *
FlagParser::parse_str_or_exit(const char *shrt, const char *lng) const noexcept
{
    const auto ret = parse_str(shrt, lng);
    if (ret == std::nullopt) flag_hasnt_been_passed(shrt, lng);
    return *ret;
}

template <class T>
bool
FlagParser::passed(const Flag<T> &flag) const noexcept
{
    for (size_t i = 0; i < argc; i++) {
        if ((0 == std::strcmp(argv[i], flag.shrt) or
             0 == std::strcmp(argv[i], flag.lng)) or
            (0 == std::strncmp(argv[i], flag.shrt, std::strlen(flag.shrt)) or
             0 == std::strncmp(argv[i], flag.lng,  std::strlen(flag.lng))))
        {
            return true;
        }
    }

    return false;
}

inline std::optional<const char *>
FlagParser::parse(const Flag<const char *> &flag) const noexcept
{
    const auto ret = parse_str(flag.shrt, flag.lng);
    if (ret == std::nullopt) {
        if (flag.mandatory) mandatory_flag_hasnt_been_passed(flag.shrt, flag.lng);
        return std::nullopt;
    }
    return *ret;
}

inline const char *
FlagParser::parse(const Flag<const char *> &flag, const char *def) const noexcept
{
    const auto ret_ = parse_str(flag.shrt, flag.lng);
    if (ret_ == std::nullopt) {
        if (flag.mandatory) mandatory_flag_hasnt_been_passed(flag.shrt, flag.lng);
        return def;
    }
    return *ret_;
}

inline std::optional<std::string>
FlagParser::parse(const Flag<std::string> &flag) const noexcept
{
    const auto ret = parse_str(flag.shrt, flag.lng);
    if (ret == std::nullopt) {
        if (flag.mandatory) mandatory_flag_hasnt_been_passed(flag.shrt, flag.lng);
        return std::nullopt;
    }
    return std::string(*ret);
}

inline std::string
FlagParser::parse(const Flag<std::string> &flag, std::string def) const noexcept
{
    const auto ret_ = parse_str(flag.shrt, flag.lng);
    if (ret_ == std::nullopt) {
        if (flag.mandatory) mandatory_flag_hasnt_been_passed(flag.shrt, flag.lng);
        return def;
    }
    return std::string(*ret_);
}

template <class T>
inline std::enable_if_t<std::is_floating_point<T>::value, std::optional<T>>
FlagParser::parse(const Flag<T> &flag) const noexcept
{
    const auto ret = parse_str(flag.shrt, flag.lng);
    if (ret == std::nullopt) {
        if (flag.mandatory) mandatory_flag_hasnt_been_passed(flag.shrt, flag.lng);
        return std::nullopt;
    }

    try {
        return static_cast<T>(std::stof(*ret));
    } catch (...) {
        return std::nullopt;
    }
}

template <class T>
inline std::enable_if_t<std::is_floating_point<T>::value, T>
FlagParser::parse(const Flag<T> &flag, T def) const noexcept
{
    const auto ret_ = parse_str(flag.shrt, flag.lng);
    if (ret_ == std::nullopt) {
        if (flag.mandatory) mandatory_flag_hasnt_been_passed(flag.shrt, flag.lng);
        return def;
    }

    const auto ret = *ret_;
    try {
        return static_cast<T>(std::stof(ret));
    } catch (...) {
        failed_to_parse_flag_to_float(flag.shrt, flag.lng, ret);
    }
}

template <class T>
inline std::optional<std::enable_if_t<is_constructible_from_int<T>::value, T>>
FlagParser::parse(const Flag<T> &flag) const noexcept
{
    const auto ret = parse_str(flag.shrt, flag.lng);
    if (ret == std::nullopt) {
        if (flag.mandatory) mandatory_flag_hasnt_been_passed(flag.shrt, flag.lng);
        return std::nullopt;
    }

    try {
        return static_cast<T>(std::stoi(*ret));
    } catch (...) {
        return std::nullopt;
    }
}

template <class T>
inline std::enable_if_t<is_constructible_from_int<T>::value, T>
FlagParser::parse(const Flag<T> &flag, T def) const noexcept
{
    const auto ret_ = parse_str(flag.shrt, flag.lng);
    if (ret_ == std::nullopt) {
        if (flag.mandatory) mandatory_flag_hasnt_been_passed(flag.shrt, flag.lng);
        return def;
    }

    const auto ret = *ret_;
    try {
        return static_cast<T>(std::stoi(ret));
    } catch (...) {
        failed_to_parse_flag_to_int(flag.shrt, flag.lng, ret);
    }
}

#endif // FLAG_IMPLEMENTATION
