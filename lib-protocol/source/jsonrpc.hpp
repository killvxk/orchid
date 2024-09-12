/* Orchid - WebRTC P2P VPN Market (on Ethereum)
 * Copyright (C) 2017-2020  The Orchid Authors
*/

/* GNU Affero General Public License, Version 3 {{{ */
/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.

 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
**/
/* }}} */


#ifndef ORCHID_JSONRPC_HPP
#define ORCHID_JSONRPC_HPP

#include "address.hpp"
#include "signed.hpp"

namespace orc {

typedef Beam Bytes;
typedef Brick<32> Bytes32;

template <typename Type_, typename Enable_ = void>
struct Coded;

template <bool Sign_, size_t Size_, typename Type_>
struct Numeric;

template <size_t Size_, typename Type_>
struct Numeric<false, Size_, Type_> {
    static const bool dynamic_ = false;

    static Type_ Decode(Window &window) {
        window.Zero(32 - Size_);
        Brick<Size_> brick;
        window.Take(brick);
        return brick.template num<Type_>();
    }

    static void Encode(Builder &builder, const Type_ &value) {
        builder += Number<uint256_t>(value);
    }

    static void Size(size_t &offset, const Type_ &value) {
        offset += 32;
    }
};

// XXX: these conversions only just barely work
template <size_t Size_, typename Type_>
struct Numeric<true, Size_, Type_> {
    static const bool dynamic_ = false;

    static Type_ Decode(Window &window) {
        // XXX: verify size of type
        Brick<32> brick;
        window.Take(brick);
        return brick.template num<uint256_t>().convert_to<Type_>();
    }

    static void Encode(Builder &builder, const Type_ &value) {
        builder += Number<uint256_t>(Complement(value), signbit(value) ? 0xff : 0x00);
    }

    static void Size(size_t &offset, const Type_ &value) {
        offset += 32;
    }
};

template <typename Type_>
struct Coded<Type_, typename std::enable_if_t<std::is_unsigned_v<Type_>>> :
    public Numeric<false, sizeof(Type_), Type_>
{
    static void Name(std::ostringstream &signature) {
        signature << "uint" << std::dec << sizeof(Type_) * 8;
    }
};

template <typename Type_>
struct Coded<Type_, typename std::enable_if_t<std::is_signed_v<Type_>>> :
    public Numeric<true, sizeof(Type_), Type_>
{
    static void Name(std::ostringstream &signature) {
        signature << "int" << std::dec << sizeof(Type_) * 8;
    }
};

template <size_t Bits_, boost::multiprecision::cpp_int_check_type Check_>
struct Coded<boost::multiprecision::number<boost::multiprecision::backends::cpp_int_backend<Bits_, Bits_, boost::multiprecision::unsigned_magnitude, Check_, void>>, typename std::enable_if_t<Bits_ % 8 == 0>> :
    public Numeric<false, (Bits_ >> 3), boost::multiprecision::number<boost::multiprecision::backends::cpp_int_backend<Bits_, Bits_, boost::multiprecision::unsigned_magnitude, Check_, void>>>
{
    static void Name(std::ostringstream &signature) {
        signature << "uint" << std::dec << Bits_;
    }
};

template <size_t Bits_, boost::multiprecision::cpp_int_check_type Check_>
struct Coded<boost::multiprecision::number<boost::multiprecision::backends::cpp_int_backend<Bits_, Bits_, boost::multiprecision::signed_magnitude, Check_, void>>, typename std::enable_if_t<Bits_ % 8 == 0>> :
    public Numeric<true, (Bits_ >> 3), boost::multiprecision::number<boost::multiprecision::backends::cpp_int_backend<Bits_, Bits_, boost::multiprecision::signed_magnitude, Check_, void>>>
{
    static void Name(std::ostringstream &signature) {
        signature << "int" << std::dec << Bits_;
    }
};

template <>
struct Coded<Address, void> {
    static const bool dynamic_ = false;

    static void Name(std::ostringstream &signature) {
        signature << "address";
    }

    static Address Decode(Window &window) {
        return Coded<uint160_t>::Decode(window);
    }

    static void Encode(Builder &builder, const Address &value) {
        return Coded<uint160_t>::Encode(builder, value.num());
    }

    static void Size(size_t &offset, const Address &value) {
        offset += 32;
    }
};

template <>
struct Coded<bool, void> {
    static const bool dynamic_ = false;

    static void Name(std::ostringstream &signature) {
        signature << "bool";
    }

    static bool Decode(Window &window) {
        auto value(Coded<uint8_t>::Decode(window));
        if (value == 0)
            return false;
        orc_assert(value == 1);
        return true;
    }

    static void Encode(Builder &builder, const bool &value) {
        return Coded<uint8_t>::Encode(builder, value ? 1 : 0);
    }

    static void Size(size_t &offset, const bool &value) {
        offset += 32;
    }
};

template <size_t Size_>
struct Coded<Brick<Size_>, typename std::enable_if_t<Size_ == 32>> {
    static const bool dynamic_ = false;

    static void Name(std::ostringstream &signature) {
        signature << "bytes" << std::dec << Size_;
    }

    static Bytes32 Decode(Window &window) {
        Brick<Size_> value;
        window.Take(value);
        return value;
    }

    static void Encode(Builder &builder, const Brick<Size_> &data) {
        builder += data;
    }

    static void Size(size_t &offset, const Brick<Size_> &data) {
        offset += Size_;
    }
};

template <>
struct Coded<Beam, void> {
    static const bool dynamic_ = true;

    static void Name(std::ostringstream &signature) {
        signature << "bytes";
    }

    static size_t Pad(size_t size) {
        return 31 - (size + 31) % 32;
    }

    static Beam Decode(Window &window) {
        auto size(Coded<uint256_t>::Decode(window).convert_to<size_t>());
        auto data(window.Take(size));
        window.Zero(Pad(size));
        return data;
    }

    static void Encode(Builder &builder, const Buffer &data) {
        auto size(data.size());
        Coded<uint256_t>::Encode(builder, size);
        builder += data;
        builder.append(Pad(size), 0);
    }

    static void Size(size_t &offset, const Buffer &data) {
        auto size(data.size());
        offset += 32 + size + Pad(size);
    }
};

template <>
struct Coded<std::string, void> {
    static const bool dynamic_ = true;

    static void Name(std::ostringstream &signature) {
        signature << "string";
    }

    static size_t Pad(size_t size) {
        return 31 - (size + 31) % 32;
    }

    static std::string Decode(Window &window) {
        auto size(Coded<uint256_t>::Decode(window).convert_to<size_t>());
        std::string data;
        data.resize(size);
        window.Take(data);
        window.Zero(Pad(size));
        return data;
    }

    static void Encode(Builder &builder, const std::string &data) {
        auto size(data.size());
        Coded<uint256_t>::Encode(builder, size);
        builder += Subset(data);
        builder.append(Pad(size), 0);
    }

    static void Size(size_t &offset, const std::string &data) {
        offset += 32 + data.size() + Pad(data.size());
    }
};

// XXX: provide a more complete implementation

template <typename Type_>
struct Coded<std::vector<Type_>, void> {
    static const bool dynamic_ = true;

    static void Name(std::ostringstream &signature) {
        Coded<Type_>::Name(signature);
        signature << "[]";
    }

    static std::vector<Type_> Decode(Window &window) {
        orc_assert(!Coded<Type_>::dynamic_);
        std::vector<Type_> values;
        const auto size(Coded<uint256_t>::Decode(window));
        for (auto i(0); i != size; ++i)
            values.emplace_back(Coded<Type_>::Decode(window));
        return values;
    }

    static void Encode(Builder &builder, const std::vector<Type_> &values) {
        Coded<uint256_t>::Encode(builder, values.size());

        if (Coded<Type_>::dynamic_) {
            size_t offset(values.size() * 32);
            for (const auto &value : values) {
                Coded<uint256_t>::Encode(builder, offset);
                Coded<Type_>::Size(offset, value);
            }
        }

        for (const auto &value : values)
            Coded<Type_>::Encode(builder, value);
    }

    static void Size(size_t &offset, const std::vector<Type_> &values) {
        offset += 32;
        if (Coded<Type_>::dynamic_)
            offset += values.size() * 32;
        for (const auto &value : values)
            Coded<Type_>::Size(offset, value);
    }
};

template <size_t Index_, typename... Args_>
struct Tupled;

template <size_t Index_>
struct Tupled<Index_> final {
    static void Name(std::ostringstream &signature) {
    }

    template <typename Tuple_>
    static void Head(Window &window, Tuple_ &tuple) {
    }

    template <typename Tuple_>
    static void Tail(Window &window, Tuple_ &tuple) {
    }

    template <typename Tuple_>
    static void Head(Builder &builder, Tuple_ &tuple, size_t offset) {
    }

    template <typename Tuple_>
    static void Tail(Builder &builder, Tuple_ &tuple) {
    }

    template <typename Tuple_>
    static void Head(size_t &offset, Tuple_ &tuple) {
    }

    template <typename Tuple_>
    static void Tail(size_t &offset, Tuple_ &tuple) {
    }
};

template <size_t Index_, typename Next_, typename... Rest_>
struct Tupled<Index_, Next_, Rest_...> final {
    static void Name(std::ostringstream &signature) {
        if (Index_ != 0)
            signature << ",";
        Coded<Next_>::Name(signature);
        Tupled<Index_ + 1, Rest_...>::Name(signature);
    }

    template <typename Tuple_>
    static void Head(Window &window, Tuple_ &tuple) {
        if (!Coded<Next_>::dynamic_)
            std::get<Index_>(tuple) = Coded<Next_>::Decode(window);
        else
            Coded<uint256_t>::Decode(window);
        Tupled<Index_ + 1, Rest_...>::Head(window, tuple);
    }

    template <typename Tuple_>
    static void Tail(Window &window, Tuple_ &tuple) {
        if (Coded<Next_>::dynamic_)
            std::get<Index_>(tuple) = Coded<Next_>::Decode(window);
        Tupled<Index_ + 1, Rest_...>::Tail(window, tuple);
    }

    template <typename Tuple_>
    static void Head(Builder &builder, Tuple_ &tuple, size_t offset) {
        if (!Coded<Next_>::dynamic_)
            Coded<Next_>::Encode(builder, std::get<Index_>(tuple));
        else {
            Coded<uint256_t>::Encode(builder, offset);
            Coded<Next_>::Size(offset, std::get<Index_>(tuple));
        }
        Tupled<Index_ + 1, Rest_...>::Head(builder, tuple, offset);
    }

    template <typename Tuple_>
    static void Tail(Builder &builder, Tuple_ &tuple) {
        if (Coded<Next_>::dynamic_)
            Coded<Next_>::Encode(builder, std::get<Index_>(tuple));
        Tupled<Index_ + 1, Rest_...>::Tail(builder, tuple);
    }

    template <typename Tuple_>
    static void Head(size_t &offset, Tuple_ &tuple) {
        if (!Coded<Next_>::dynamic_)
            Coded<Next_>::Size(offset, std::get<Index_>(tuple));
        else
            offset += 32;
        Tupled<Index_ + 1, Rest_...>::Head(offset, tuple);
    }

    template <typename Tuple_>
    static void Tail(size_t &offset, Tuple_ &tuple) {
        if (Coded<Next_>::dynamic_)
            Coded<Next_>::Size(offset, std::get<Index_>(tuple));
        Tupled<Index_ + 1, Rest_...>::Tail(offset, tuple);
    }
};

template <typename... Args_>
struct Coded<std::tuple<Args_...>, void> {
    typedef Tupled<0, std::decay_t<Args_>...> Tupled_;

    static const bool dynamic_ = (... || Coded<std::decay_t<Args_>>::dynamic_);

    static void Name(std::ostringstream &signature) {
        signature << "(";
        Tupled_::Name(signature);
        signature << ")";
    }

    static std::tuple<Args_...> Decode(Window &window) {
        std::tuple<Args_...> value;
        Tupled_::Head(window, value);
        Tupled_::Tail(window, value);
        return value;
    }

    static std::tuple<Args_...> Decode(const Beam &data) {
        Window window(data);
        auto value(Decode(window));
        window.Stop();
        return value;
    }

    static void Encode(Builder &builder, const std::tuple<Args_...> &data) {
        size_t offset(0);
        Tupled_::Head(offset, data);
        Tupled_::Head(builder, data, offset);
        Tupled_::Tail(builder, data);
    }

    static void Size(size_t &offset, const std::tuple<Args_...> &value) {
        Tupled_::Head(offset, value);
        Tupled_::Tail(offset, value);
    }
};

template <typename... Args_>
struct Coder {
    typedef std::tuple<const Args_ &...> Tuple;

    static void Encode(Builder &builder, const Args_ &...args) {
        return Coded<Tuple>::Encode(builder, Tuple(args...));
    }

    static Builder Encode(const Args_ &...args) {
        Builder builder;
        Coded<Tuple>::Encode(builder, Tuple(args...));
        return builder;
    }
};

}

#endif//ORCHID_JSONRPC_HPP
