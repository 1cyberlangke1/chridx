// 输入：一段 UTF-8 文本（std::string_view，不拥有）。构造只认 std::string_view：
//       std::string（左值/右值）、const char*、字符串字面量都被编译期挡住，必须显式
//       写出 std::string_view{...} —— 悬垂索引从根上不可能，调用点也都写明「内存不归我」。
// 输出：按字符下标取「该字符的字节切片」（[] / at）、取「该字符首字节下标」（() / index_at），
//       外加两种只读迭代器：随机访问的 begin / end（可用于 range-for），
//       和只给 ++ / -- 的 bidirectional_begin / bidirectional_end（顺序遍历每步 O(1)）。
// 预期行为：构造 O(n)，查询不放溢出点时 O(1)、放了也只是 O(log n)；
//           [] / () 不检查（越界是 UB，同 STL），at() / index_at() 检查（越界抛
//           std::out_of_range）；T 太窄装不下溢出点下标时构造抛 std::length_error。
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace chridx {

template <typename T = std::size_t> class char_index {
  static_assert(std::is_unsigned<T>::value,
                "chridx: template parameter T must be an unsigned integer type");

public:
  using size_type = std::size_t;

  class const_iterator {
  public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = std::string_view;
    using difference_type = std::ptrdiff_t;
    using pointer = void;
    using reference = std::string_view;

    // 输入：无
    // 输出：一个「空」迭代器（不可解引用，赋过值才能用）
    // 预期行为：STL 要求迭代器默认可构造，不抛异常
    const_iterator() noexcept = default;

    // 输入：无
    // 输出：当前字符的字节切片
    // 预期行为：不检查；等于 end() 时解引用属未定义行为（同 STL）
    reference operator*() const noexcept { return (*owner_)[index_]; }

    // 输入：n —— 相对当前位置的字符偏移（可为负）
    // 输出：*(*this + n)，即偏移后那个字符的字节切片
    // 预期行为：不检查；越出 [begin, end) 属未定义行为
    reference operator[](difference_type n) const noexcept { return *(*this + n); }

    const_iterator& operator++() noexcept {
      ++index_;
      return *this;
    }
    const_iterator operator++(int) noexcept {
      const_iterator old = *this;
      ++index_;
      return old;
    }
    const_iterator& operator--() noexcept {
      --index_;
      return *this;
    }
    const_iterator operator--(int) noexcept {
      const_iterator old = *this;
      --index_;
      return old;
    }

    // 输入：n —— 可正可负的字符偏移
    // 输出：移动后的自己
    // 预期行为：不检查边界；下标是无符号的，所以负偏移走减法那一路
    const_iterator& operator+=(difference_type n) noexcept {
      if (n >= 0) {
        index_ += static_cast<size_type>(n);
      } else {
        index_ -= static_cast<size_type>(-n);
      }
      return *this;
    }
    const_iterator& operator-=(difference_type n) noexcept { return *this += -n; }

    friend const_iterator operator+(const_iterator it, difference_type n) noexcept {
      return it += n;
    }
    friend const_iterator operator+(difference_type n, const_iterator it) noexcept {
      return it += n;
    }
    friend const_iterator operator-(const_iterator it, difference_type n) noexcept {
      return it -= n;
    }
    friend difference_type operator-(const_iterator lhs, const_iterator rhs) noexcept {
      return static_cast<difference_type>(lhs.index_) - static_cast<difference_type>(rhs.index_);
    }

    friend bool operator==(const_iterator lhs, const_iterator rhs) noexcept {
      return lhs.index_ == rhs.index_;
    }
    friend bool operator!=(const_iterator lhs, const_iterator rhs) noexcept {
      return !(lhs == rhs);
    }
    friend bool operator<(const_iterator lhs, const_iterator rhs) noexcept {
      return lhs.index_ < rhs.index_;
    }
    friend bool operator<=(const_iterator lhs, const_iterator rhs) noexcept {
      return !(rhs < lhs);
    }
    friend bool operator>(const_iterator lhs, const_iterator rhs) noexcept {
      return rhs < lhs;
    }
    friend bool operator>=(const_iterator lhs, const_iterator rhs) noexcept {
      return !(lhs < rhs);
    }

  private:
    const char_index* owner_ = nullptr;
    size_type index_ = 0;

    // 输入：owner —— 所属的索引对象；index —— 字符下标
    // 输出：指向该位置的迭代器
    // 预期行为：私有构造，只有外层 char_index 能造（begin / end 是唯一入口）。
    const_iterator(const char_index* owner, size_type index) noexcept
        : owner_(owner), index_(index) {}
    friend class char_index;
  };

  class const_bidirectional_iterator {
  public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = std::string_view;
    using difference_type = std::ptrdiff_t;
    using pointer = void;
    using reference = std::string_view;

    // 输入：无
    // 输出：一个「空」迭代器（不可解引用，赋过值才能用）
    // 预期行为：STL 要求迭代器默认可构造，不抛异常
    const_bidirectional_iterator() noexcept = default;

    // 输入：无
    // 输出：当前字符的字节切片
    // 预期行为：O(1) —— 直接用自己维护的排名，不重新二分；等于 end() 时解引用是 UB
    reference operator*() const noexcept { return owner_->slice_of_(index_, rank_); }

    // 输入：无
    // 输出：前进一个字符后的自己
    // 预期行为：O(1)；只有正好跨过一个溢出点时才把排名 +1
    const_bidirectional_iterator& operator++() noexcept {
      ++index_;
      if (rank_ < owner_->overflow_count.size() &&
          static_cast<size_type>(owner_->overflow_count[rank_]) <= index_) {
        ++rank_;
      }
      return *this;
    }
    const_bidirectional_iterator operator++(int) noexcept {
      const_bidirectional_iterator old = *this;
      ++(*this);
      return old;
    }

    // 输入：无
    // 输出：后退一个字符后的自己
    // 预期行为：O(1)，规则与 ++ 对称；对 begin() 做 -- 属未定义行为（同 STL）
    const_bidirectional_iterator& operator--() noexcept {
      --index_;
      if (rank_ > 0 && static_cast<size_type>(owner_->overflow_count[rank_ - 1]) > index_) {
        --rank_;
      }
      return *this;
    }
    const_bidirectional_iterator operator--(int) noexcept {
      const_bidirectional_iterator old = *this;
      --(*this);
      return old;
    }

    friend bool operator==(const_bidirectional_iterator lhs, const_bidirectional_iterator rhs) noexcept {
      return lhs.index_ == rhs.index_;
    }
    friend bool operator!=(const_bidirectional_iterator lhs, const_bidirectional_iterator rhs) noexcept {
      return !(lhs == rhs);
    }

  private:
    const char_index* owner_ = nullptr;
    size_type index_ = 0;
    size_type rank_ = 0;

    // 输入：owner + 起点下标 + 该下标的排名
    // 输出：指向该位置的迭代器
    // 预期行为：私有构造，只有外层 char_index 能造（bidirectional_begin / bidirectional_end 是入口）
    const_bidirectional_iterator(const char_index* owner, size_type index, size_type rank) noexcept
        : owner_(owner), index_(index), rank_(rank) {}
    friend class char_index;
  };

  // 输入：text —— 要被索引的 UTF-8 串（不拷贝，调用方保证它活得比本对象久）
  // 输出：索引对象
  // 预期行为：构造 O(n)；纯 ASCII 时内部不额外分配；
  //           T 装不下溢出点下标时抛 std::length_error（换更宽的 T）
  explicit char_index(std::string_view text) : str_view(text) {
    size_type char_count = 0;
    for (const char ch : text) {
      if ((static_cast<unsigned char>(ch) & 0xC0u) != 0x80u) {
        ++char_count;
      }
    }
    size_ = char_count;
    if (char_count == text.size()) {
      return;
    }

    offset.reserve(char_count);
    size_type overflows = 0;
    for (size_type char_idx = 0, byte_idx = 0; byte_idx < text.size(); ++char_idx) {
      size_type residual = byte_idx - char_idx - overflows * 255u;
      if (residual > 255u) {
        if (char_idx > static_cast<size_type>(std::numeric_limits<T>::max())) {
          throw std::length_error(
              "chridx: overflow marker index doesn't fit in template parameter T "
              "(use a wider T)");
        }
        overflow_count.push_back(static_cast<T>(char_idx));
        ++overflows;
        residual -= 255u;
      }
      offset.push_back(static_cast<std::uint8_t>(residual));
      byte_idx += utf8_len_(static_cast<unsigned char>(text[byte_idx]));
    }
  }

  char_index(const std::string&) = delete;
  char_index(std::string&&) = delete;
  char_index(const char*) = delete;

  // 输入：无
  // 输出：指向第 0 个字符的迭代器 / 指向「第 size() 个字符」的尾后迭代器
  // 预期行为：只读、随机访问；end() 不可解引用
  const_iterator begin() const noexcept { return const_iterator(this, 0); }
  const_iterator end() const noexcept { return const_iterator(this, size_); }
  const_iterator cbegin() const noexcept { return begin(); }
  const_iterator cend() const noexcept { return end(); }

  // 输入：无
  // 输出：++/-- 版迭代器的首位置 / 尾后位置
  // 预期行为：只读、双向；适合顺序遍历（每步 O(1)），不适合随机跳——要用随机访问
  //           就换 begin / end 那套
  const_bidirectional_iterator bidirectional_begin() const noexcept { return const_bidirectional_iterator(this, 0, 0); }
  const_bidirectional_iterator bidirectional_end() const noexcept {
    return const_bidirectional_iterator(this, size_, overflow_count.size());
  }
  const_bidirectional_iterator bidirectional_cbegin() const noexcept { return bidirectional_begin(); }
  const_bidirectional_iterator bidirectional_cend() const noexcept { return bidirectional_end(); }

  // 输入：无
  // 输出：字符（Unicode 标量值）个数
  // 预期行为：O(1)，不抛异常
  size_type size() const noexcept { return size_; }

  // 输入：无
  // 输出：是否没有字符（等价于 size() == 0）
  // 预期行为：O(1)，不抛异常
  bool empty() const noexcept { return size_ == 0; }

  // 输入：char_idx —— 从 0 开始的字符下标
  // 输出：该字符在原文里的字节切片（UTF-8 下 1~4 字节，是原串的子视图、不拷贝）
  //       例："a💚b" 的 [1] == "💚"、[0] == "a"、[2] == "b"
  // 预期行为：不检查；char_idx >= size() 属未定义行为
  //           （与 std::string_view::operator[] 同约定）
  std::string_view operator[](size_type char_idx) const noexcept {
    return slice_of_(char_idx, rank_at_(char_idx));
  }

  // 输入：char_idx —— 从 0 开始的字符下标
  // 输出：同 operator[]，即该字符的字节切片
  // 预期行为：越界抛 std::out_of_range，消息里带越界的下标和当前 size()
  std::string_view at(size_type char_idx) const {
    if (char_idx >= size_) {
      throw_out_of_range_("chridx::at", char_idx);
    }
    return (*this)[char_idx];
  }

  // 输入：char_idx —— 从 0 开始的字符下标
  // 输出：该字符首个字节在原文里的字节下标（要按字节裁剪、配合 substr 时用这个）
  // 预期行为：不检查；char_idx >= size() 属未定义行为
  size_type operator()(size_type char_idx) const noexcept {
    return byte_of_(char_idx, rank_at_(char_idx));
  }

  // 输入：char_idx —— 从 0 开始的字符下标
  // 输出：同 operator()，即该字符首字节的字节下标
  // 预期行为：越界抛 std::out_of_range，消息里带越界的下标和当前 size()
  size_type index_at(size_type char_idx) const {
    if (char_idx >= size_) {
      throw_out_of_range_("chridx::index_at", char_idx);
    }
    return (*this)(char_idx);
  }

private:
  // 输入：func —— 抛异常的方法名；char_idx —— 越界的字符下标
  // 输出：无（总是抛）
  // 预期行为：统一在一处拼「人话」错误信息，两个检查版共用。
  //           消息用英文 —— 库的调用方不一定看中文，STL 自己的异常也是英文
  [[noreturn]] void throw_out_of_range_(const char* func, size_type char_idx) const {
    throw std::out_of_range(std::string(func) + ": char index " + std::to_string(char_idx) +
                            " is out of range (size() == " + std::to_string(size_) + ")");
  }

  // 输入：char_idx —— 字符下标
  // 输出：溢出点表里 ≤ char_idx 的条数（也就是「排名」）
  // 预期行为：表为空（纯 ASCII / 空串）直接返回 0，不白做一次二分；否则 O(log n)。
  //           比较放在 size_type 里做 —— T 可能比它窄，先转成 T 再比会在 char_idx
  //           很大时被截断，静默算错。
  size_type rank_at_(size_type char_idx) const noexcept {
    if (overflow_count.empty()) {
      return 0;
    }
    return static_cast<size_type>(
        std::upper_bound(overflow_count.begin(), overflow_count.end(), char_idx,
                         [](size_type value, T stored) {
                           return value < static_cast<size_type>(stored);
                         }) -
        overflow_count.begin());
  }

  // 输入：char_idx + 该下标的排名（由 rank_at_ 现算，或双向迭代器自己维护）
  // 输出：该字符首字节的字节下标
  // 预期行为：O(1)；不检查越界（调用方保证 rank 与 char_idx 配套）
  size_type byte_of_(size_type char_idx, size_type rank) const noexcept {
    if (offset.empty()) {
      return char_idx;
    }
    return static_cast<size_type>(offset[char_idx]) + 255u * rank + char_idx;
  }

  // 输入：char_idx + 该下标的排名
  // 输出：该字符在原文里的字节切片
  // 预期行为：O(1)；不检查越界
  std::string_view slice_of_(size_type char_idx, size_type rank) const noexcept {
    const size_type first = byte_of_(char_idx, rank);
    return str_view.substr(first, utf8_len_(static_cast<unsigned char>(str_view[first])));
  }

  // 输入：UTF-8 字符的首字节
  // 输出：该字符占几个字节（1~4）
  // 预期行为：只看首字节前缀，不做合法性校验（调用方保证原文是合法 UTF-8）
  static size_type utf8_len_(unsigned char lead) noexcept {
    if (lead < 0x80u) {
      return 1;
    }
    if (lead < 0xE0u) {
      return 2;
    }
    if (lead < 0xF0u) {
      return 3;
    }
    return 4;
  }

  std::string_view str_view;
  size_type size_ = 0;
  std::vector<std::uint8_t> offset;
  std::vector<T> overflow_count;
};

}
