/* Orchid - WebRTC P2P VPN Market (on Ethereum)
 * Copyright (C) 2017-2019  The Orchid Authors
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

#include <string>

#include <json/json.h>

#include "buffer.hpp"
#include "crypto.hpp"
#include "http.hpp"
#include "locator.hpp"
#include "task.hpp"

namespace orc {

// XXX: none of this is REMOTELY efficient

typedef boost::multiprecision::number<boost::multiprecision::cpp_int_backend<160, 160, boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void>> uint160_t;

class Nested {
  protected:
    bool scalar_;
    mutable std::string value_;
    mutable std::vector<Nested> array_;

  private:
    static void enc(std::string &data, unsigned length);
    static void enc(std::string &data, unsigned length, uint8_t offset);

  public:
    Nested() :
        scalar_(false)
    {
    }

    Nested(bool scalar, std::string value, std::vector<Nested> array) :
        scalar_(scalar),
        value_(std::move(value)),
        array_(std::move(array))
    {
    }

    Nested(uint8_t value) :
        scalar_(true),
        value_(1, char(value))
    {
    }

    Nested(std::string value) :
        scalar_(true),
        value_(std::move(value))
    {
    }

    Nested(const char *value) :
        Nested(std::string(value))
    {
    }

    Nested(const Buffer &buffer) :
        Nested(buffer.str())
    {
    }

    Nested(std::initializer_list<Nested> list) :
        scalar_(false)
    {
        for (auto nested(list.begin()); nested != list.end(); ++nested)
            array_.emplace_back(nested->scalar_, std::move(nested->value_), std::move(nested->array_));
    }

    Nested(Nested &&rhs) :
        scalar_(rhs.scalar_),
        value_(std::move(rhs.value_)),
        array_(std::move(rhs.array_))
    {
    }

    bool scalar() const {
        return scalar_;
    }

    size_t size() const {
        orc_assert(!scalar_);
        return array_.size();
    }

    const Nested &operator [](unsigned i) const {
        orc_assert(!scalar_);
        orc_assert(i < size());
        return array_[i];
    }

    Subset buf() const {
        orc_assert(scalar_);
        return Subset(value_);
    }

    const std::string &str() const {
        orc_assert(scalar_);
        return value_;
    }

    void enc(std::string &data) const;
};

std::ostream &operator <<(std::ostream &out, const Nested &value);

class Explode final :
    public Nested
{
  public:
    Explode(Window &window);
    Explode(Window &&window);
};

std::string Implode(Nested value);

class Argument final {
  private:
    mutable Json::Value value_;

  public:
    Argument(Json::Value value) :
        value_(value)
    {
    }

    template <unsigned Bits_, boost::multiprecision::cpp_int_check_type Check_>
    Argument(const boost::multiprecision::number<boost::multiprecision::backends::cpp_int_backend<Bits_, Bits_, boost::multiprecision::unsigned_magnitude, Check_, void>> &value) :
        value_("0x" + value.str(0, std::ios::hex))
    {
    }

    Argument(const char *value) :
        value_(value)
    {
    }

    Argument(const std::string &value) :
        value_(value)
    {
    }

    Argument(const Buffer &buffer) :
        value_(buffer.hex())
    {
    }

    Argument(std::initializer_list<Argument> args) :
        value_(Json::arrayValue)
    {
        int index(0);
        for (auto arg(args.begin()); arg != args.end(); ++arg)
            value_[index++] = std::move(arg->value_);
    }

    template <typename Type_>
    Argument(const std::vector<Type_> &args) :
        value_(Json::arrayValue)
    {
        int index(0);
        for (auto arg(args.begin()); arg != args.end(); ++arg)
            value_[index] = Argument(arg->value_);
    }

    Argument(std::map<std::string, Argument> args) :
        value_(Json::objectValue)
    {
        for (auto arg(args.begin()); arg != args.end(); ++arg)
            value_[std::move(arg->first)] = std::move(arg->second);
    }

    operator Json::Value &&() && {
        return std::move(value_);
    }
};

typedef std::map<std::string, Argument> Map;

class Proven final {
  private:
    uint256_t balance_;

  public:
    Proven(Json::Value value) :
        balance_(value["balance"].asString())
    {
    }

    const uint256_t &Balance() {
        return balance_;
    }
};

class Address :
    public uint160_t
{
  public:
    using uint160_t::uint160_t;

    Address(uint160_t &&value) :
        uint160_t(std::move(value))
    {
    }
};

typedef Beam Bytes;
typedef Brick<32> Bytes32;

template <typename Type_, typename Enable_ = void>
struct Coded;

inline void Encode(Builder &builder) {
}

template <typename Type_, typename... Args_>
inline void Encode(Builder &builder, const Type_ &value, const Args_ &...args) {
    Coded<Type_>::Encode(builder, value);
    Encode(builder, args...);
}

template <bool Sign_, size_t Size_, typename Type_>
struct Numeric;

template <size_t Size_, typename Type_>
struct Numeric<false, Size_, Type_> {
    static Type_ Decode(Window &window) {
        window.Skip(32 - Size_);
        Brick<Size_> brick;
        window.Take(brick);
        return brick.template num<Type_>();
    }

    static void Encode(Builder &builder, const Type_ &value) {
        builder += Number<uint256_t>(value);
    }
};

// XXX: these conversions only just barely work
template <size_t Size_, typename Type_>
struct Numeric<true, Size_, Type_> {
    static Type_ Decode(Window &window) {
        Brick<32> brick;
        window.Take(brick);
        return brick.template num<uint256_t>().convert_to<Type_>();
    }

    static void Encode(Builder &builder, const Type_ &value) {
        builder += Number<uint256_t>(value, signbit(value) ? 0xff : 0x00);
    }
};

template <typename Type_>
struct Coded<Type_, typename std::enable_if<std::is_unsigned<Type_>::value>::type> :
    public Numeric<false, sizeof(Type_), Type_>
{
    static void Name(std::ostringstream &signature) {
        signature << "uint" << std::dec << sizeof(Type_) * 8;
    }
};

template <typename Type_>
struct Coded<Type_, typename std::enable_if<std::is_signed<Type_>::value>::type> :
    public Numeric<true, sizeof(Type_), Type_>
{
    static void Name(std::ostringstream &signature) {
        signature << "int" << std::dec << sizeof(Type_) * 8;
    }
};

template <unsigned Bits_, boost::multiprecision::cpp_int_check_type Check_>
struct Coded<boost::multiprecision::number<boost::multiprecision::backends::cpp_int_backend<Bits_, Bits_, boost::multiprecision::unsigned_magnitude, Check_, void>>, typename std::enable_if<Bits_ % 8 == 0>::type> :
    public Numeric<false, (Bits_ >> 3), boost::multiprecision::number<boost::multiprecision::backends::cpp_int_backend<Bits_, Bits_, boost::multiprecision::unsigned_magnitude, Check_, void>>>
{
    static void Name(std::ostringstream &signature) {
        signature << "uint" << std::dec << Bits_;
    }
};

/*template <unsigned Bits_, boost::multiprecision::cpp_int_check_type Check_>
struct Coded<boost::multiprecision::number<boost::multiprecision::backends::cpp_int_backend<Bits_, Bits_, boost::multiprecision::signed_magnitude, Check_, void>>, typename std::enable_if<Bits_ % 8 == 0>::type> :
    public Numeric<true, (Bits_ >> 3), boost::multiprecision::number<boost::multiprecision::backends::cpp_int_backend<Bits_, Bits_, boost::multiprecision::signed_magnitude, Check_, void>>>
{
    static void Name(std::ostringstream &signature) {
        signature << "int" << std::dec << Bits_;
    }
};*/

template <>
struct Coded<Address, void> {
    static void Name(std::ostringstream &signature) {
        signature << "address";
    }

    static Address Decode(Window &window) {
        return Coded<uint160_t>::Decode(window);
    }

    static void Encode(Builder &builder, const Address &value) {
        return Coded<uint160_t>::Encode(builder, value);
    }
};

template <>
struct Coded<Beam, void> {
    static void Name(std::ostringstream &signature) {
        signature << "bytes";
    }

    static Beam Decode(Window &window) {
        auto size(Coded<uint256_t>::Decode(window).convert_to<size_t>());
        auto data(window.Take(size));
        window.Skip(31 - (size + 31) % 32);
        return data;
    }

    static void Encode(Builder &builder, const Beam &data) {
        auto size(data.size());
        Coded<uint256_t>::Encode(builder, size);
        builder += data;
        Beam pad(31 - (size + 31) % 32);
        memset(pad.data(), 0, pad.size());
        builder += std::move(pad);
    }
};

// XXX: provide a more complete implementation

template <typename Type_>
struct Coded<std::vector<Type_>, void> {
    static void Name(std::ostringstream &signature) {
        Coded<Type_>::Name(signature);
        signature << "[]";
    }

    static void Encode(Builder &builder, const std::vector<Type_> &values) {
        Coded<uint256_t>::Encode(builder, values.size());
        for (const auto &value : values)
            Coded<Type_>::Encode(builder, value);
    }
};

template <>
struct Coded<std::tuple<uint256_t, Bytes>, void> {
    static std::tuple<uint256_t, Bytes> Decode(Window &window) {
        std::tuple<uint256_t, Bytes> value;
        std::get<0>(value) = Coded<uint256_t>::Decode(window);
        orc_assert(Coded<uint256_t>::Decode(window) == 0x40);
        std::get<1>(value) = Coded<Bytes>::Decode(window);
        return value;
    }
};

template <typename Type_>
struct Result final {
    typedef uint256_t type;
};

class Endpoint final {
  private:
    const Locator locator_;

    auto Get(int index, Json::Value &storage, const uint256_t &key) {
        auto proof(storage[index]);
        orc_assert(uint256_t(proof["key"].asString()) == key);
        uint256_t value(proof["value"].asString());
        return value;
    }

    template <int Index_, typename Result_, typename... Args_>
    void Get(Result_ &result, Json::Value &storage) {
    }

    template <int Index_, typename Result_, typename... Args_>
    void Get(Result_ &result, Json::Value &storage, const uint256_t &key, Args_ &&...args) {
        std::get<Index_ + 1>(result) = Get(Index_, storage, key);
        Get<Index_ + 1>(result, storage, std::forward<Args_>(args)...);
    }

  public:
    Endpoint(Locator locator) :
        locator_(std::move(locator))
    {
    }

    task<Json::Value> operator ()(const std::string &method, Argument args);

    task<uint256_t> Block() {
        co_return uint256_t((co_await operator ()("eth_blockNumber", {})).asString());
    }

    template <typename... Args_>
    task<std::tuple<Proven, typename Result<Args_>::type...>> Get(const Argument &block, const uint256_t &account, Args_ &&...args) {
        auto proof(co_await operator ()("eth_getProof", {account, {std::forward<Args_>(args)...}, block}));
        std::tuple<Proven, typename Result<Args_>::type...> result(proof);
        Get<0>(result, proof["storageProof"], std::forward<Args_>(args)...);
        co_return result;
    }

    template <typename... Args_>
    task<std::tuple<Proven, std::vector<uint256_t>>> Get(const Argument &block, const uint256_t &account, const std::vector<uint256_t> &args) {
        auto proof(co_await operator ()("eth_getProof", {account, {std::forward<Args_>(args)...}, block}));
        std::tuple<Proven, std::vector<uint256_t>> result(proof);
        auto storage(proof["storageProof"]);
        for (unsigned i(0); i != args.size(); ++i)
            std::get<1>(result).emplace_back(Get(i, storage, args[i]));
        co_return result;
    }
};

template <typename Result_, typename... Args_>
class Selector final :
    public Region
{
  private:
    uint32_t value_;
    uint256_t gas_;

    template <bool Comma_, typename... Rest_>
    struct Args;

    template <bool Comma_, typename Next_, typename... Rest_>
    struct Args<Comma_, Next_, Rest_...> {
    static void Write(std::ostringstream &signature) {
        if (Comma_)
            signature << ',';
        Coded<Next_>::Name(signature);
        Args<true, Rest_...>::Write(signature);
    } };

    template <bool Comma_>
    struct Args<Comma_> {
    static void Write(std::ostringstream &signature) {
    } };

  public:
    Selector(uint32_t value, uint256_t gas = 90000) :
        value_(boost::endian::native_to_big(value)),
        gas_(gas)
    {
    }

    Selector(const std::string &name) :
        Selector([&]() {
            std::ostringstream signature;
            signature << name << '(';
            Args<false, Args_...>::Write(signature);
            signature << ')';
            std::cerr << signature.str() << std::endl;
            return Hash(Strung(signature.str())).Clip<4>().num<uint32_t>();
        }())
    {
    }

    const uint8_t *data() const override {
        return reinterpret_cast<const uint8_t *>(&value_);
    }

    size_t size() const override {
        return sizeof(value_);
    }

    task<Result_> Call(Endpoint &endpoint, const Argument &block, const Address &contract, const Args_ &...args) {
        Builder builder;
        Encode<Args_...>(builder, std::forward<const Args_>(args)...);
        auto data(Bless((co_await endpoint("eth_call", {Map{
            {"to", contract},
            {"gas", gas_},
            {"data", Tie(*this, builder)},
        }, block})).asString()));
        Window window(data);
        auto result(Coded<Result_>::Decode(window));
        window.Stop();
        co_return std::move(result);
    }

    task<uint256_t> Send(Endpoint &endpoint, const Address &from, const Address &contract, const Args_ &...args) {
        Builder builder;
        Encode<Args_...>(builder, std::forward<const Args_>(args)...);
        auto transaction(Bless((co_await endpoint("eth_sendTransaction", {Map{
            {"from", from},
            {"to", contract},
            {"gas", gas_},
            {"data", Tie(*this, builder)},
        }})).asString()).template num<uint256_t>());
        co_return std::move(transaction);
    }
};

}

#endif//ORCHID_JSONRPC_HPP
