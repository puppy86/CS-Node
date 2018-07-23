#pragma once
#ifndef _CREDITS_CSDB_AMOUNT_H_INCLUDED_
#define _CREDITS_CSDB_AMOUNT_H_INCLUDED_

#include <cinttypes>
#include <type_traits>
#include <limits>
#include <iostream>
#include <string>
#include <stdexcept>

#include "internal/math128ce.h"

namespace csdb {
	//A class for storing the amount of currency
  class Amount;
}
template<char ...s> constexpr csdb::Amount operator "" _c ();

namespace csdb {
namespace priv {
class obstream;
class ibstream;
} // namespace priv

#pragma pack(push, 1)
class Amount
{
public:
  static constexpr const uint64_t AMOUNT_MAX_FRACTION = 1000000000000000000ULL;

public:
  inline constexpr Amount() = default;
  inline constexpr Amount(int32_t value) noexcept : integral_(value) {}
  //The constructor from the integer part and the correct positive fractional part
  inline constexpr Amount(int32_t integral, uint64_t numerator, uint64_t denominator = 100);
  Amount(double value);

private:
  inline constexpr Amount(const int32_t integral, const uint64_t fraction, std::nullptr_t) noexcept :
    integral_(integral), fraction_(fraction) {}
  inline constexpr Amount(const internal::uint128_t& value, bool divide) noexcept;
  inline constexpr Amount(const internal::uint128_t::division64_result& value) noexcept;
  static inline constexpr uint64_t _check_fraction(const internal::uint128_t& fraction);

  // Obtaining values
public:
  inline constexpr int32_t integral() const noexcept {return integral_;}
  inline constexpr uint64_t fraction() const noexcept {return fraction_;}
  inline constexpr int32_t round() const noexcept;
  inline constexpr double to_double() const noexcept;
  inline constexpr operator int32_t() const noexcept {return round();}
  inline constexpr operator double() const noexcept {return to_double();}

  // Comparison
public:
  inline constexpr bool operator ==(const Amount& other) const noexcept;
  inline constexpr bool operator !=(const Amount& other) const noexcept;
  inline constexpr bool operator <(const Amount& other) const noexcept;
  inline constexpr bool operator >(const Amount& other) const noexcept;
  inline constexpr bool operator <=(const Amount& other) const noexcept;
  inline constexpr bool operator >=(const Amount& other) const noexcept;

  // Arithmetic operations
public:
  inline constexpr Amount operator -() const noexcept;

  inline constexpr Amount operator +(const Amount& other) const noexcept;
  inline constexpr Amount operator +(const int32_t other) const noexcept;
  inline Amount operator +(double other) const;

  inline constexpr Amount operator -(const Amount& other) const  noexcept;
  inline constexpr Amount operator -(int32_t other) const  noexcept;
  inline Amount operator -(double other) const;

  inline constexpr Amount operator *(const Amount& other) const noexcept;
  inline constexpr Amount operator *(const int32_t other) const noexcept;
  inline Amount operator *(double other) const;


  inline constexpr Amount operator /(const int32_t other) const;

  inline Amount& operator +=(const Amount& other) noexcept;
  inline Amount& operator +=(int32_t other) noexcept;
  inline Amount& operator +=(double other);

  inline Amount& operator -=(const Amount& other) noexcept;
  inline Amount& operator -=(int32_t other) noexcept;
  inline Amount& operator -=(double other);

  inline Amount& operator *=(const Amount& other) noexcept;
  inline Amount& operator *=(int32_t other) noexcept;
  inline Amount& operator *=(double other);


  inline Amount& operator /=(int32_t other);


  ::std::string to_string(size_t min_decimal_places = 2) const noexcept;

  // Serialization
public:
  void put(priv::obstream&) const;
  bool get(priv::ibstream&);

private:
  int32_t integral_ = 0;
  uint64_t fraction_ = 0;

  // Templates for a constexpr literal operator.
private:
  template<uint64_t m, char d, char ...s> struct amount_fraction;
  template<char d, char ...s> struct amount_full;
  template<char ...s> friend Amount constexpr ::operator "" _c ();
};
#pragma pack(pop)

static_assert(std::is_trivially_copyable<Amount>::value,
              "Invalid csdb::Amount definition.");

inline constexpr Amount::Amount(int32_t integral, uint64_t numerator, uint64_t denominator) :
  integral_(integral),
  fraction_(_check_fraction((internal::uint128_t::mul(numerator, AMOUNT_MAX_FRACTION)
                             + (denominator / 2)).div(denominator).quotient_))
{}

inline constexpr Amount::Amount(const internal::uint128_t& value, bool divide) noexcept :
  Amount(((0 == value.lo_) && (0 == value.hi_))
         ? internal::uint128_t::division64_result{{0,0},0}
         : divide
           ? value.div(AMOUNT_MAX_FRACTION).quotient_.div(AMOUNT_MAX_FRACTION)
           : value.div(AMOUNT_MAX_FRACTION))
{}

inline constexpr Amount::Amount(const internal::uint128_t::division64_result& value) noexcept :
  integral_(static_cast<int32_t>(value.quotient_.lo_)),
  fraction_(value.remainder_)
{}

inline constexpr uint64_t Amount::_check_fraction(const internal::uint128_t& fraction)
{
  return ((0 != fraction.hi_) || (AMOUNT_MAX_FRACTION <= fraction.lo_))
      ? throw std::invalid_argument("Amount::Amount(): Invalid fraction part.")
      : fraction.lo_;
}

inline constexpr int32_t Amount::round() const noexcept
{
  return (fraction_ < (AMOUNT_MAX_FRACTION / 2ULL)) ? integral_ : (integral_ + 1);
}

inline constexpr double Amount::to_double() const noexcept
{
  return static_cast<double>(integral_)
      + (static_cast<double>(fraction_) / static_cast<double>(AMOUNT_MAX_FRACTION));
}

inline constexpr bool Amount::operator ==(const Amount& other) const noexcept
{
  return (integral_ == other.integral_) && (fraction_ == other.fraction_);
}

inline constexpr bool Amount::operator !=(const Amount& other) const noexcept
{
  return !this->operator ==(other);
}

inline constexpr bool Amount::operator <(const Amount& other) const noexcept
{
  return (integral_ < other.integral_) ? true :
         (integral_ > other.integral_) ? false :
         (fraction_ < other.fraction_);
}

inline constexpr bool Amount::operator >(const Amount& other) const noexcept
{
  return (integral_ > other.integral_) ? true :
         (integral_ < other.integral_) ? false :
         (fraction_ > other.fraction_);
}

inline constexpr bool Amount::operator <=(const Amount& other) const noexcept
{
  return !this->operator >(other);
}

inline constexpr bool Amount::operator >=(const Amount& other) const noexcept
{
  return !this->operator <(other);
}

inline constexpr Amount Amount::operator -() const noexcept
{
  return (0 == fraction_)
      ? Amount(-integral_)
      : Amount(-integral_ - 1, AMOUNT_MAX_FRACTION - fraction_, nullptr);
}

inline Amount& Amount::operator +=(const Amount& other) noexcept
{
  integral_ += other.integral_;
  fraction_ += other.fraction_;
  if (fraction_ > AMOUNT_MAX_FRACTION) {
    ++integral_;
    fraction_ -= AMOUNT_MAX_FRACTION;
  }
  return *this;
}

inline Amount& Amount::operator +=(int32_t other) noexcept
{
  integral_ += other;
  return *this;
}

inline Amount& Amount::operator +=(double other)
{
  return operator +=(Amount(other));
}

inline Amount& Amount::operator -=(const Amount& other) noexcept
{
  integral_ -= other.integral_;
  if (other.fraction_ > fraction_) {
    --integral_;
    fraction_ += (AMOUNT_MAX_FRACTION - other.fraction_);
  } else {
    fraction_ -= other.fraction_;
  }
  return *this;
}

inline Amount& Amount::operator -=(int32_t other) noexcept
{
  integral_ -= other;
  return *this;
}

inline Amount& Amount::operator -=(double other)
{
  return operator -=(Amount(other));
}

inline constexpr Amount Amount::operator +(const Amount& other) const noexcept
{
  return (AMOUNT_MAX_FRACTION <= (fraction_ + other.fraction_))
      ? Amount(integral_ + other.integral_ + 1, fraction_ + other.fraction_ - AMOUNT_MAX_FRACTION, nullptr)
      : Amount(integral_ + other.integral_, fraction_ + other.fraction_, nullptr);
}

inline constexpr Amount Amount::operator +(const int32_t other) const noexcept
{
  return Amount(integral_ + other, fraction_, nullptr);
}

inline Amount Amount::operator +(double other) const
{
  return this->operator +(Amount(other));
}

inline constexpr Amount Amount::operator -(const Amount& other) const  noexcept
{
  return (fraction_ < other.fraction_)
      ? Amount(integral_ - other.integral_ - 1, fraction_ + AMOUNT_MAX_FRACTION - other.fraction_, nullptr)
      : Amount(integral_ - other.integral_, fraction_ - other.fraction_, nullptr);
}

inline constexpr Amount Amount::operator -(int32_t other) const  noexcept
{
  return Amount(integral_ - other, fraction_, nullptr);
}

inline Amount Amount::operator -(double other) const
{
  return this->operator -(Amount(other));
}

inline constexpr Amount Amount::operator *(const Amount& other) const noexcept
{
  return (0 > integral_)
      ? this->operator -().operator *(other).operator -()
      : (0 > other.integral_)
        ? this->operator *(-other).operator -()
        : Amount(static_cast<int32_t>(integral_ * other.integral_))
          + Amount(internal::uint128_t::mul(fraction_, other.integral_), false)
          + Amount(internal::uint128_t::mul(other.fraction_, integral_), false)
          + Amount(internal::uint128_t::mul(fraction_, other.fraction_), true);
}

inline constexpr Amount Amount::operator *(const int32_t other) const noexcept
{
  return (0 > other)
      ? this->operator *(-other).operator -()
      : Amount(integral_ * other) + Amount(internal::uint128_t::mul(fraction_, other), false);
}

inline Amount Amount::operator *(double other) const
{
  return this->operator *(Amount(other));
}

inline Amount& Amount::operator *=(const Amount& other) noexcept
{
  (*this) = this->operator *(other);
  return *this;
}

inline Amount& Amount::operator *=(int32_t other) noexcept
{
  (*this) = this->operator *(other);
  return *this;
}

inline Amount& Amount::operator *=(double other)
{
  return this->operator *=(Amount(other));
}

inline constexpr Amount Amount::operator /(const int32_t other) const
{
  return (0 == other)
      ? throw std::overflow_error("Amount division by zero")
      : (0 > other)
        ? this->operator /(-other).operator -()
        : (1 == other)
          ? (*this)
          : (0 > integral_)
            ? this->operator-().operator /(other).operator -()
            : Amount(internal::uint128_t::mul(integral_, AMOUNT_MAX_FRACTION).div(other).quotient_, false)
              + Amount(0, fraction_ / other, nullptr);
}

inline Amount& Amount::operator /=(int32_t other)
{
  (*this) = this->operator /(other);
  return *this;
}

template<uint64_t m, char d, char ...s>
struct Amount::amount_fraction
{
  static constexpr const uint64_t value = ((d - '0') * m) + amount_fraction<m / 10, s...>::value;
};

template<uint64_t m, char d>
struct Amount::amount_fraction<m, d>
{
  static constexpr const uint64_t value = ((d - '0') * m);
};

template<char d, char ...s>
struct Amount::amount_full
{
  static constexpr const uint64_t integral = amount_full<s...>::integral + amount_full<s...>::multiplier * (d - '0');
  static constexpr const uint64_t multiplier = amount_full<s...>::multiplier * 10;
  static constexpr const uint64_t fraction = amount_full<s...>::fraction;
  static constexpr const Amount value() {return Amount{static_cast<int32_t>(integral), fraction, nullptr};}
};

template<char ...s>
struct Amount::amount_full<'.', s...>
{
  static constexpr const uint64_t integral = 0;
  static constexpr const uint64_t multiplier = 1;
  static constexpr const uint64_t fraction = amount_fraction<AMOUNT_MAX_FRACTION / 10ULL, s...>::value;
  static constexpr const Amount value() {return Amount{static_cast<int32_t>(integral), fraction, nullptr};}
};

} // namespace csdb

inline constexpr csdb::Amount operator +(const int32_t a, const csdb::Amount& b)
{
  return b + a;
}

inline csdb::Amount operator +(double a, const csdb::Amount& b)
{
  return b + a;
}

inline constexpr csdb::Amount operator -(int32_t a, const csdb::Amount& b) noexcept
{
  return (-b) + a;
}

inline csdb::Amount operator -(double a, const csdb::Amount& b)
{
  return csdb::Amount(a) - b;
}

inline constexpr csdb::Amount operator *(const int32_t a, const csdb::Amount& b)
{
  return b * a;
}

inline csdb::Amount operator *(double a, const csdb::Amount& b)
{
  return b * a;
}

inline constexpr csdb::Amount operator "" _c (unsigned long long value)
{
  return csdb::Amount(static_cast<int32_t>(value));
}

template<char ...s>
inline constexpr csdb::Amount operator "" _c ()
{
  return csdb::Amount::amount_full<s...>::value();
}

inline ::std::ostream& operator << (::std::ostream& os, const csdb::Amount& value)
{
  return (os << value.to_string());
}

#endif // _CREDITS_CSDB_AMOUNT_H_INCLUDED_
