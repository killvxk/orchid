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


#ifndef ORCHID_BUFFER_HPP
#define ORCHID_BUFFER_HPP

#include <deque>
#include <functional>
#include <iostream>
#include <list>

#include <asio.hpp>

#include <boost/endian/conversion.hpp>
#include <boost/mp11/tuple.hpp>
#include <boost/multiprecision/cpp_int.hpp>

#include "error.hpp"
#include "trace.hpp"

namespace orc {

using boost::multiprecision::uint128_t;
using boost::multiprecision::uint256_t;

class Region;
class Beam;

class Buffer {
  public:
    virtual bool each(const std::function<bool (const uint8_t *, size_t)> &code) const = 0;

    virtual size_t size() const;

    virtual bool empty() const {
        return size() == 0;
    }

    size_t copy(uint8_t *data, size_t size) const;

    size_t copy(char *data, size_t size) const {
        return copy(reinterpret_cast<uint8_t *>(data), size);
    }

    std::string str() const;
    std::string hex() const;
};

std::ostream &operator <<(std::ostream &out, const Buffer &buffer);

template <typename Type_, typename Enable_ = void>
struct Cast;

template <typename Type_>
struct Cast<Type_, typename std::enable_if<std::is_arithmetic<Type_>::value>::type> {
    static auto Load(const uint8_t *data, size_t size) {
        orc_assert(size == sizeof(Type_));
        return boost::endian::big_to_native(*reinterpret_cast<const Type_ *>(data));
    }
};

template <unsigned Bits_, boost::multiprecision::cpp_integer_type Sign_, boost::multiprecision::cpp_int_check_type Check_>
struct Cast<boost::multiprecision::number<boost::multiprecision::backends::cpp_int_backend<Bits_, Bits_, Sign_, Check_, void>>, typename std::enable_if<Bits_ % 8 == 0>::type> {
    static auto Load(const uint8_t *data, size_t size) {
        orc_assert(size == Bits_ / 8);
        boost::multiprecision::number<boost::multiprecision::backends::cpp_int_backend<Bits_, Bits_, Sign_, Check_, void>> value;
        boost::multiprecision::import_bits(value, std::reverse_iterator(data + size), std::reverse_iterator(data), 8, false);
        return value;
    }
};

class Region :
    public Buffer
{
  public:
    virtual const uint8_t *data() const = 0;
    size_t size() const override = 0;

    bool each(const std::function<bool (const uint8_t *, size_t)> &code) const override {
        return code(data(), size());
    }

    uint8_t operator [](size_t index) const {
        return data()[index];
    }

    operator asio::const_buffer() const {
        return asio::const_buffer(data(), size());
    }

    template <typename Type_>
    Type_ num() const {
        return Cast<Type_>::Load(data(), size());
    }

    unsigned nib(size_t index) const {
        orc_assert((index >> 1) < size());
        auto value(data()[index >> 1]);
        if ((index & 0x1) == 0)
            return value >> 4;
        else
            return value & 0xf;
    }
};

template <typename Type_ = uint8_t>
class Span {
  protected:
    Type_ *data_;
    size_t size_;

  public:
    Span() = default;

    Span(const Span &span) :
        data_(span.data()),
        size_(span.size())
    {
    }

    Span(Type_ *data, size_t size) :
        data_(data),
        size_(size)
    {
    }

    Type_ *data() const {
        return data_;
    }

    size_t size() const {
        return size_;
    }

    template <typename Cast_>
    Cast_ &cast(size_t offset = 0) {
        static_assert(sizeof(Type_) == 1);
        orc_assert_(size() >= offset + sizeof(Cast_), "orc_assert(" << size() << " {size()} >= " << offset << " {offset} + " << sizeof(Cast_) << " {sizeof(" << typeid(Cast_).name() << ")})");
        return *reinterpret_cast<Cast_ *>(data() + offset);
    }

    Span &operator +=(size_t offset) {
        orc_assert(size_ >= offset);
        data_ += offset;
        size_ -= offset;
        return *this;
    }

    Span &operator ++() {
        return *this += 1;
    }

    uint8_t operator [](size_t index) const {
        return data_[index];
    }

    void copy(size_t offset, const Buffer &data) {
        orc_assert(offset <= size_);
        data.each([&](const uint8_t *data, size_t size) {
            orc_assert(size_ - offset >= size);
            memcpy(data_ + offset, data, size);
            offset += size;
            return true;
        });
    }
};

class Range final :
    public Span<const uint8_t>
{
  public:
    using Span<const uint8_t>::Span;

    Range() = default;

    Range(const Region &region) :
        Span(region.data(), region.size())
    {
    }

    Range(const char *data, size_t size) :
        Span(reinterpret_cast<const uint8_t *>(data), size)
    {
    }

    Range &operator =(const Region &region) {
        data_ = region.data();
        size_ = region.size();
        return *this;
    }

    operator asio::const_buffer() const {
        return asio::const_buffer(data(), size());
    }
};

class Subset final :
    public Region
{
  private:
    const Range range_;

  public:
    Subset(const Range &range) :
        range_(range)
    {
    }

    Subset(const uint8_t *data, size_t size) :
        range_(data, size)
    {
    }

    Subset(const char *data, size_t size) :
        range_(data, size)
    {
    }

    Subset(const std::string &data) :
        Subset(data.data(), data.size())
    {
    }

    Subset(const Region &region) :
        Subset(region.data(), region.size())
    {
    }

    const uint8_t *data() const override {
        return range_.data();
    }

    size_t size() const override {
        return range_.size();
    }
};

template <typename Data_>
class Strung final :
    public Region
{
  private:
    const Data_ data_;

  public:
    explicit Strung(Data_ data) :
        data_(std::move(data))
    {
    }

    const uint8_t *data() const override {
        return reinterpret_cast<const uint8_t *>(data_.data());
    }

    size_t size() const override {
        return data_.size();
    }
};

template <size_t Size_>
class Data :
    public Region
{
  protected:
    std::array<uint8_t, Size_> data_;

  public:
    Data() = default;

    Data(const void *data, size_t size) {
        copy(data, size);
    }

    Data(const Region &region) :
        Data(region.data(), region.size())
    {
    }

    Data(const std::array<uint8_t, Size_> &data) :
        data_(data)
    {
    }

    void copy(const void *data, size_t size) {
        orc_assert(size == Size_);
        memcpy(data_.data(), data, Size_);
    }

    Data &operator =(const Region &region) {
        copy(region.data(), region.size());
        return *this;
    }

    Data &operator =(const Range &range) {
        copy(range.data(), range.size());
        return *this;
    }

    const uint8_t *data() const override {
        return data_.data();
    }

    uint8_t *data() {
        return data_.data();
    }

    size_t size() const override {
        return Size_;
    }

    bool operator <(const Data<Size_> &rhs) const {
        return data_ < rhs.data_;
    }
};

template <size_t Size_>
class Brick final :
    public Data<Size_>
{
  public:
    static const size_t Size = Size_;

  public:
    using Data<Size_>::Data;
    using Data<Size_>::operator =;

    Brick() = default;

    Brick(const std::string &data) :
        Brick(data.data(), data.size())
    {
    }

    explicit constexpr Brick(std::initializer_list<uint8_t> list) noexcept {
        std::copy(list.begin(), list.end(), this->data_.begin());
    }

    Brick(const Brick &rhs) :
        Data<Size_>(rhs.data_)
    {
    }

    uint8_t &operator [](size_t index) {
        return this->data_[index];
    }

    template <size_t Clip_>
    typename std::enable_if<Clip_ <= Size_, Brick<Clip_>>::type Clip() {
        Brick<Clip_> value;
        for (size_t i(0); i != Clip_; ++i)
            value[i] = this->data_[i];
        return value;
    }
};

template <typename Type_, bool Arithmetic_ = std::is_arithmetic<Type_>::value>
class Number;

template <typename Type_>
class Number<Type_, true> final :
    public Region
{
  private:
    Type_ value_;

  public:
    Number() = default;

    constexpr Number(Type_ value) noexcept :
        value_(boost::endian::native_to_big(value))
    {
    }

    Number(const Brick<sizeof(Type_)> &brick) :
        Number(brick.template num<Type_>())
    {
    }

    operator Type_() const {
        return boost::endian::big_to_native(value_);
    }

    const uint8_t *data() const override {
        return reinterpret_cast<const uint8_t *>(&value_);
    }

    uint8_t *data() {
        return reinterpret_cast<uint8_t *>(&value_);
    }

    size_t size() const override {
        return sizeof(Type_);
    }
};

template <unsigned Bits_, boost::multiprecision::cpp_integer_type Sign_, boost::multiprecision::cpp_int_check_type Check_>
class Number<boost::multiprecision::number<boost::multiprecision::backends::cpp_int_backend<Bits_, Bits_, Sign_, Check_, void>>, false> final :
    public Data<(Bits_ >> 3)>
{
  public:
    using Data<(Bits_ >> 3)>::Data;

    Number(boost::multiprecision::number<boost::multiprecision::backends::cpp_int_backend<Bits_, Bits_, Sign_, Check_, void>> value, uint8_t pad = 0) {
        for (auto i(boost::multiprecision::export_bits(value, this->data_.rbegin(), 8, false)), e(this->data_.rend()); i != e; ++i)
            *i = pad;
    }

    Number(const std::string &value) :
        Number(boost::multiprecision::number<boost::multiprecision::backends::cpp_int_backend<Bits_, Bits_, Sign_, Check_, void>>(value))
    {
    }
};

class Beam :
    public Region
{
  private:
    size_t size_;
    uint8_t *data_;

    void destroy() {
        delete [] data_;
    }

  public:
    Beam() :
        size_(0),
        data_(nullptr)
    {
    }

    Beam(size_t size) :
        size_(size),
        data_(new uint8_t[size_])
    {
    }

    Beam(const void *data, size_t size) :
        Beam(size)
    {
        memcpy(data_, data, size_);
    }

    Beam(const std::string &data) :
        Beam(data.data(), data.size())
    {
    }

    Beam(const Buffer &buffer);

    Beam(Beam &&rhs) noexcept :
        size_(rhs.size_),
        data_(rhs.data_)
    {
        rhs.size_ = 0;
        rhs.data_ = nullptr;
    }

    Beam(const Beam &rhs) = delete;

    virtual ~Beam() {
        destroy();
    }

    Beam &operator =(const Beam &) = delete;

    Beam &operator =(Beam &&rhs) noexcept {
        destroy();
        size_ = rhs.size_;
        data_ = rhs.data_;
        rhs.size_ = 0;
        rhs.data_ = nullptr;
        return *this;
    }

    const uint8_t *data() const override {
        return data_;
    }

    uint8_t *data() {
        return data_;
    }

    size_t size() const override {
        return size_;
    }

    Span<uint8_t> span() {
        return {data(), size()};
    }

    Subset subset(size_t offset, size_t size) const {
        orc_assert(offset <= size_);
        orc_assert(size_ - offset >= size);
        return {data_ + offset, size};
    }

    uint8_t &operator [](size_t index) {
        return data_[index];
    }
};

Beam Bless(const std::string &data);

template <typename Data_>
inline bool operator ==(const Beam &lhs, const std::string &rhs) {
    auto size(lhs.size());
    return size == rhs.size() && memcmp(lhs.data(), rhs.data(), size) == 0;
}

template <typename Data_>
inline bool operator ==(const Beam &lhs, const Strung<Data_> &rhs) {
    auto size(lhs.size());
    return size == rhs.size() && memcmp(lhs.data(), rhs.data(), size) == 0;
}

template <size_t Size_>
inline bool operator ==(const Beam &lhs, const Brick<Size_> &rhs) {
    auto size(lhs.size());
    return size == rhs.size() && memcmp(lhs.data(), rhs.data(), size) == 0;
}

inline bool operator ==(const Beam &lhs, const Range &rhs) {
    auto size(lhs.size());
    return size == rhs.size() && memcmp(lhs.data(), rhs.data(), size) == 0;
}

inline bool operator ==(const Beam &lhs, const Beam &rhs) {
    auto size(lhs.size());
    return size == rhs.size() && memcmp(lhs.data(), rhs.data(), size) == 0;
}

bool operator ==(const Beam &lhs, const Buffer &rhs);

template <typename Buffer_>
inline bool operator !=(const Beam &lhs, const Buffer_ &rhs) {
    return !(lhs == rhs);
}

class Nothing final :
    public Region
{
  public:
    const uint8_t *data() const override {
        return nullptr;
    }

    size_t size() const override {
        return 0;
    }
};

template <typename... Buffer_>
class Knot final :
    public Buffer
{
  private:
    const std::tuple<const Buffer_ &...> buffers_;

  public:
    Knot(const Buffer_ &...buffers) :
        buffers_(buffers...)
    {
    }

    bool each(const std::function<bool (const uint8_t *, size_t)> &code) const override {
        bool value(true);
        boost::mp11::tuple_for_each(buffers_, [&](const auto &buffer) {
            value &= buffer.each(code);
        });
        return value;
    }
};

template <typename... Buffer_>
auto Tie(Buffer_ &&...buffers) {
    return Knot<Buffer_...>(std::forward<Buffer_>(buffers)...);
}

class Sequence final :
    public Buffer
{
  private:
    size_t count_;
    std::unique_ptr<Range[]> ranges_;

    class Iterator {
      private:
        const Range *range_;

      public:
        Iterator(const Range *range) :
            range_(range)
        {
        }

        const Range &operator *() const {
            return *range_;
        }

        const Range *operator ->() const {
            return range_;
        }

        Iterator &operator ++() {
            ++range_;
            return *this;
        }

        bool operator !=(const Iterator &rhs) const {
            return range_ != rhs.range_;
        }
    };

  public:
    Sequence(const Buffer &buffer) :
        count_([&]() {
            size_t count(0);
            buffer.each([&](const uint8_t *data, size_t size) {
                ++count;
                return true;
            });
            return count;
        }()),

        ranges_(new Range[count_])
    {
        auto i(ranges_.get());
        buffer.each([&](const uint8_t *data, size_t size) {
            *(i++) = Range(data, size);
            return true;
        });
    }

    Sequence(Sequence &&sequence) noexcept :
        count_(sequence.count_),
        ranges_(std::move(sequence.ranges_))
    {
    }

    Sequence(const Sequence &sequence) :
        count_(sequence.count_),
        ranges_(new Range[count_])
    {
        auto old(sequence.ranges_.get());
        std::copy(old, old + count_, ranges_.get());
    }

    Iterator begin() const {
        return ranges_.get();
    }

    Iterator end() const {
        return ranges_.get() + count_;
    }

    bool each(const std::function<bool (const uint8_t *, size_t)> &code) const override {
        for (auto i(begin()), e(end()); i != e; ++i)
            if (!code(i->data(), i->size()))
                return false;
        return true;
    }
};

template <size_t Size_>
class Pad :
    public Data<Size_>
{
  public:
    Pad() {
        this->data_.fill(0);
    }
};

class Window :
    public Buffer
{
  private:
    size_t count_;
    std::unique_ptr<Range[]> ranges_;

    const Range *range_;
    size_t offset_;

  public:
    Window() :
        count_(0),
        range_(nullptr),
        offset_(0)
    {
    }

    Window(const Buffer &buffer) :
        count_([&]() {
            size_t count(0);
            buffer.each([&](const uint8_t *data, size_t size) {
                ++count;
                return true;
            });
            return count;
        }()),

        ranges_(new Range[count_]),

        range_(ranges_.get()),
        offset_(0)
    {
        auto i(ranges_.get());
        buffer.each([&](const uint8_t *data, size_t size) {
            *(i++) = Range(data, size);
            return true;
        });
    }

    Window(const Range &range) :
        count_(1),
        ranges_(new Range[count_]),
        range_(ranges_.get()),
        offset_(0)
    {
        ranges_.get()[0] = range;
    }

    Window(const Window &window) :
        Window([](const Buffer &buffer) -> const Buffer & {
            return buffer;
        }(window))
    {
    }

    Window(Window &&rhs) = default;
    Window &operator =(Window &&rhs) = default;

    bool each(const std::function<bool (const uint8_t *, size_t)> &code) const override {
        auto here(range_);
        auto rest(ranges_.get() + count_ - here);
        if (rest == 0)
            return true;

        size_t i;
        if (offset_ == 0)
            i = 0;
        else {
            i = 1;
            if (!code(here->data() + offset_, here->size() - offset_))
                return false;
        }

        for (; i != rest; ++i)
            if (!code(here[i].data(), here[i].size()))
                return false;

        return true;
    }

    void Stop() {
        orc_assert(empty());
    }

    void Take(uint8_t *data, size_t size) {
        Beam beam(size);

        auto &here(range_);
        auto &step(offset_);

        auto rest(ranges_.get() + count_ - here);

        for (auto need(size); need != 0; step = 0, ++here, --rest) {
            orc_assert(rest != 0);

            auto size(here->size() - step);
            if (size == 0)
                continue;

            if (need < size) {
                memcpy(data, here->data() + step, need);
                step += need;
                break;
            }

            memcpy(data, here->data() + step, size);
            data += size;
            need -= size;
        }
    }

    void Take(std::string &data) {
        Take(reinterpret_cast<uint8_t *>(data.data()), data.size());
    }

    uint8_t Take() {
        uint8_t value;
        Take(&value, 1);
        return value;
    }

    template <size_t Size_>
    void Take(Brick<Size_> &value) {
        Take(value.data(), value.size());
    }

    template <typename Type_>
    void Take(Number<Type_> &value) {
        Take(value.data(), value.size());
    }

    Beam Take(size_t size) {
        Beam beam(size);
        Take(beam.data(), beam.size());
        return beam;
    }

    void Skip(size_t size) {
        auto data(Take(size));
        for (size_t i(0); i != size; ++i)
            orc_assert(data[i] == 0);
    }

    template <size_t Size_>
    void Take(Pad<Size_> &value) {
        Skip(Size_);
    }
};

class Rest final :
    public Window
{
  private:
    Beam data_;

  public:
    Rest() = default;

    Rest(Window &&window, Beam &&data) :
        Window(std::move(window)),
        data_(std::move(data))
    {
    }
};


class Builder :
    public Buffer
{
  private:
    std::list<Beam> ranges_;

  public:
    bool each(const std::function<bool (const uint8_t *, size_t)> &code) const override {
        for (const auto &range : ranges_)
            if (!code(range.data(), range.size()))
                return false;
        return true;
    }

    void operator +=(const Buffer &buffer) {
        ranges_.emplace_back(buffer);
    }

    void operator +=(Beam &&beam) {
        ranges_.emplace_back(std::move(beam));
    }
};


template <size_t Index_, typename Next_, typename Enable_, typename... Taking_>
struct Taking;

template <size_t Index_, typename... Taking_>
struct Taker;

template <size_t Index_, size_t Size_, typename... Taking_>
struct Taking<Index_, Pad<Size_>, void, Taking_...> final {
template <typename Tuple_, typename Buffer_>
static void Take(Tuple_ &tuple, Window &window, Buffer_ &&buffer) {
    window.Skip(Size_);
    return Taker<Index_, Taking_...>::Take(tuple, window, std::forward<Buffer_>(buffer));
} };

template <size_t Index_, size_t Size_, typename... Taking_>
struct Taking<Index_, Brick<Size_>, void, Taking_...> final {
template <typename Tuple_, typename Buffer_>
static void Take(Tuple_ &tuple, Window &window, Buffer_ &&buffer) {
    window.Take(std::get<Index_>(tuple));
    return Taker<Index_ + 1, Taking_...>::Take(tuple, window, std::forward<Buffer_>(buffer));
} };

template <size_t Index_, typename Type_, typename... Taking_>
struct Taking<Index_, Number<Type_>, void, Taking_...> final {
template <typename Tuple_, typename Buffer_>
static void Take(Tuple_ &tuple, Window &window, Buffer_ &&buffer) {
    window.Take(std::get<Index_>(tuple));
    return Taker<Index_ + 1, Taking_...>::Take(tuple, window, std::forward<Buffer_>(buffer));
} };

template <size_t Index_, unsigned Bits_, boost::multiprecision::cpp_integer_type Sign_, boost::multiprecision::cpp_int_check_type Check_, typename... Taking_>
struct Taking<Index_, boost::multiprecision::number<boost::multiprecision::backends::cpp_int_backend<Bits_, Bits_, Sign_, Check_, void>>, typename std::enable_if<Bits_ % 8 == 0>::type, Taking_...> final {
template <typename Tuple_, typename Buffer_>
static void Take(Tuple_ &tuple, Window &window, Buffer_ &&buffer) {
    Brick<Bits_ / 8> brick;
    window.Take(brick);
    std::get<Index_>(tuple) = brick.template num<boost::multiprecision::number<boost::multiprecision::backends::cpp_int_backend<Bits_, Bits_, Sign_, Check_, void>>>();
    return Taker<Index_ + 1, Taking_...>::Take(tuple, window, std::forward<Buffer_>(buffer));
} };

template <size_t Index_, typename Next_, typename... Taking_>
struct Taking<Index_, Next_, typename std::enable_if<std::is_arithmetic<Next_>::value>::type, Taking_...> {
template <typename Tuple_, typename Buffer_>
static void Take(Tuple_ &tuple, Window &window, Buffer_ &&buffer) {
    Brick<sizeof(Next_)> brick;
    window.Take(brick);
    std::get<Index_>(tuple) = brick.template num<Next_>();
    return Taker<Index_ + 1, Taking_...>::Take(tuple, window, std::forward<Buffer_>(buffer));
} };

template <size_t Index_>
struct Taking<Index_, Window, void> {
template <typename Tuple_, typename Buffer_>
static void Take(Tuple_ &tuple, Window &window, Buffer_ &&buffer) {
    static_assert(!std::is_rvalue_reference<Buffer_ &&>::value);
    std::get<Index_>(tuple) = std::move(window);
} };

template <size_t Index_>
struct Taking<Index_, Rest, void> {
template <typename Tuple_>
static void Take(Tuple_ &tuple, Window &window, Beam &&buffer) {
    std::get<Index_>(tuple) = Rest(std::move(window), std::move(buffer));
} };

template <size_t Index_>
struct Taking<Index_, Beam, void> {
template <typename Tuple_>
static void Take(Tuple_ &tuple, Window &window, Beam &&buffer) {
    std::get<Index_>(tuple) = Beam(window);
} };

template <size_t Index_, typename Next_, typename... Taking_>
struct Taker<Index_, Next_, Taking_...> {
template <typename Tuple_, typename Buffer_>
static void Take(Tuple_ &tuple, Window &window, Buffer_ &&buffer) {
    return Taking<Index_, Next_, void, Taking_...>::Take(tuple, window, std::forward<Buffer_>(buffer));
} };

template <size_t Index_>
struct Taker<Index_> {
template <typename Tuple_, typename Buffer_>
static void Take(Tuple_ &tuple, Window &window, Buffer_ &&buffer) {
    window.Stop();
} };

template <typename Tuple_, typename... Taking_>
struct Taken;

template <typename Tuple_, typename Type_, typename... Taking_>
struct Taken<Tuple_, Type_, Taking_...> {
    typedef typename Taken<decltype(std::tuple_cat(Tuple_(), std::tuple<Type_>())), Taking_...>::type type;
};

template <typename Tuple_, size_t Size_, typename... Taking_>
struct Taken<Tuple_, Pad<Size_>, Taking_...> {
    typedef typename Taken<Tuple_, Taking_...>::type type;
};

template <typename Tuple_>
struct Taken<Tuple_> {
    typedef Tuple_ type;
};

template <typename... Taking_, typename Buffer_>
auto Take(Buffer_ &&buffer) {
    typename Taken<std::tuple<>, Taking_...>::type tuple;
    Window window(buffer);
    Taker<0, Taking_...>::Take(tuple, window, std::forward<Buffer_>(buffer));
    return tuple;
}


}

#endif//ORCHID_BUFFER_HPP
