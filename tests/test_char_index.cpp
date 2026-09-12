// 输入：无。
// 输出：退出码 0 = 全绿，1 = 有失败（ctest 靠这个判定）。
// 预期行为：覆盖公开接口的正常路径、边界（空串 / ASCII 特化 / 2·3·4 字节字符 / 溢出翻转）
//           与错误路径（at / index_at 越界抛 std::out_of_range；T 太窄抛 std::length_error）。
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "chridx/char_index.hpp"
#include "check.hpp"

static_assert(std::is_same_v<
                  std::iterator_traits<chridx::char_index<>::const_iterator>::iterator_category,
                  std::random_access_iterator_tag>,
              "char_index::const_iterator 必须是随机访问迭代器");
static_assert(std::is_same_v<chridx::char_index<>::size_type, std::size_t>);

namespace {

template <typename T, typename = void> struct has_plus : std::false_type {};
template <typename T>
struct has_plus<T, std::void_t<decltype(std::declval<T>() + std::ptrdiff_t{1})>>
    : std::true_type {};

}

static_assert(has_plus<chridx::char_index<>::const_iterator>::value,
              "const_iterator 应该支持 it + n");
static_assert(!has_plus<chridx::char_index<>::const_bidirectional_iterator>::value,
              "const_bidirectional_iterator 不应该支持 it + n，它只有 ++ / --");
static_assert(std::is_same_v<
                  std::iterator_traits<chridx::char_index<>::const_bidirectional_iterator>::iterator_category,
                  std::bidirectional_iterator_tag>,
              "const_bidirectional_iterator 必须是双向迭代器");

static_assert(std::is_constructible_v<chridx::char_index<>, std::string_view>,
              "string_view 是唯一该被接受的构造实参");
static_assert(std::is_constructible_v<chridx::char_index<>, std::string_view&>,
              "string_view 左值也该被接受");
static_assert(!std::is_constructible_v<chridx::char_index<>, std::string&&>,
              "std::string 右值不该被接受：隐式转 view 会指向已销毁的临时串");
static_assert(!std::is_constructible_v<chridx::char_index<>, std::string&>,
              "std::string 左值也不该被接受：请显式写 std::string_view{...}");
static_assert(!std::is_constructible_v<chridx::char_index<>, const char*>,
              "const char* 不该被接受：请显式写 std::string_view{...}");
static_assert(!std::is_constructible_v<chridx::char_index<>, const char[6]>,
              "字符串字面量不该被直接接受");

namespace {

// 输入：count —— 想要几个字符
// 输出：由 count 个「4 字节 emoji」拼成的串
// 预期行为：每个字符给累计偏移贡献 3 字节，所以第 86 个字符必然撞上 u8 偏移上限
//           （3 × 86 = 258 > 255），拿它稳定地测「溢出翻转」这条路径
std::string repeated_emoji(std::size_t count) {
    std::string text;
    text.reserve(count * 4);
    for (std::size_t i = 0; i < count; ++i) {
        text += "💚";
    }
    return text;
}

// 输入：一段文本
// 输出：是否全部是 ASCII
// 预期行为：任何 > 0x7F 的字节都算非 ASCII —— 用它断言「异常消息用英文」
bool is_ascii(const std::string& text) {
    for (const char c : text) {
        if (static_cast<unsigned char>(c) > 0x7Fu) {
            return false;
        }
    }
    return true;
}

// 输入：fn —— 一个会抛异常的调用
// 输出：捕获到的 what() 文本；没抛就返回空串（调用方的 CHECK 会因此失败）
// 预期行为：只接 std::exception 家族，别的异常照常往外冒
template <typename F> std::string catch_what(F&& fn) {
    try {
        fn();
    } catch (const std::exception& e) {
        return std::string(e.what());
    }
    return std::string{};
}

}

// 输入：无
// 输出：无
// 预期行为：空串没有字符、begin() == end()、任何下标都越界
TEST("空串：size() 为 0、empty() 为真、首尾迭代器相等") {
    const chridx::char_index<> idx{std::string_view{""}};
    CHECK_EQ(idx.size(), std::size_t{0});
    CHECK(idx.empty());
    CHECK(idx.begin() == idx.end());
    CHECK_THROWS(idx.at(0), std::out_of_range);
    CHECK_THROWS(idx.index_at(0), std::out_of_range);
}

// 输入：无
// 输出：无
// 预期行为：纯 ASCII 走「零额外分配」那条特化，字节下标 == 字符下标
TEST("纯 ASCII：切片是单字节、字节下标等于字符下标") {
    const chridx::char_index<> idx{std::string_view{"abc"}};
    CHECK_EQ(idx.size(), std::size_t{3});
    CHECK(!idx.empty());
    CHECK_EQ(idx[0], std::string_view{"a"});
    CHECK_EQ(idx.at(1), std::string_view{"b"});
    CHECK_EQ(idx[2], std::string_view{"c"});
    CHECK_EQ(idx(0), std::size_t{0});
    CHECK_EQ(idx(2), std::size_t{2});
    CHECK_EQ(idx.index_at(2), std::size_t{2});
}

// 输入：无
// 输出：无
// 预期行为：多字节字符整块取出来（切片长度 = 该字符的 UTF-8 长度），下标逐字符累计偏移
TEST("多字节：切片是整个字符、字节下标累计偏移") {
    const chridx::char_index<> idx{std::string_view{"a💚b"}};
    CHECK_EQ(idx.size(), std::size_t{3});
    CHECK_EQ(idx[0], std::string_view{"a"});
    CHECK_EQ(idx[1], std::string_view{"💚"});
    CHECK_EQ(idx[2], std::string_view{"b"});
    CHECK_EQ(idx.at(1).size(), std::size_t{4});
    CHECK_EQ(idx(0), std::size_t{0});
    CHECK_EQ(idx(1), std::size_t{1});
    CHECK_EQ(idx(2), std::size_t{5});
    CHECK_EQ(idx.index_at(2), std::size_t{5});
}

// 输入：无
// 输出：无
// 预期行为：2 / 3 / 4 字节字符混排时偏移逐字符累加（不是按最长字符一刀切）
TEST("多字节：2 / 3 / 4 字节字符混排") {
    const std::string text = "é中💚";
    CHECK_EQ(text.size(), std::size_t{9});

    const chridx::char_index<> idx{std::string_view{text}};
    CHECK_EQ(idx.size(), std::size_t{3});
    CHECK_EQ(idx[0], std::string_view{"é"});
    CHECK_EQ(idx[1], std::string_view{"中"});
    CHECK_EQ(idx[2], std::string_view{"💚"});
    CHECK_EQ(idx(1), std::size_t{2});
    CHECK_EQ(idx(2), std::size_t{5});
}

// 输入：无
// 输出：无
// 预期行为：累计偏移超过 u8 上限后要记「溢出点」，翻转点前后都算得对
TEST("溢出翻转：100 个 4 字节字符，翻转点前后字节下标都对") {
    const std::string text = repeated_emoji(100);
    CHECK_EQ(text.size(), std::size_t{400});

    const chridx::char_index<> idx{std::string_view{text}};
    CHECK_EQ(idx.size(), std::size_t{100});
    for (const std::size_t i :
         {std::size_t{0}, std::size_t{85}, std::size_t{86}, std::size_t{99}}) {
        CHECK_EQ(idx(i), i * 4);
        CHECK_EQ(idx[i], std::string_view{"💚"});
    }
}

// 输入：无
// 输出：无
// 预期行为：跨越多个溢出点（翻转表里有 5 条记录）时依旧正确
TEST("溢出翻转：500 个 4 字节字符，跨多个溢出点仍然正确") {
    const std::string text = repeated_emoji(500);
    const chridx::char_index<> idx{std::string_view{text}};
    CHECK_EQ(idx.size(), std::size_t{500});
    for (const std::size_t i : {std::size_t{170}, std::size_t{171}, std::size_t{255},
                                std::size_t{256}, std::size_t{341}, std::size_t{426},
                                std::size_t{499}}) {
        CHECK_EQ(idx(i), i * 4);
    }
}

// 输入：无
// 输出：无
// 预期行为：只读随机访问迭代器的距离 / 移动 / 解引用 / 下标 / 比较全套可用
TEST("迭代器：距离 / 移动 / 解引用 / 下标 / 比较") {
    const chridx::char_index<> idx{std::string_view{"a💚b"}};
    const auto first = idx.begin();
    const auto last = idx.end();

    CHECK_EQ(static_cast<std::size_t>(std::distance(first, last)), std::size_t{3});
    CHECK_EQ(last - first, std::ptrdiff_t{3});
    CHECK_EQ(*first, std::string_view{"a"});
    CHECK_EQ(*(first + 1), std::string_view{"💚"});
    CHECK_EQ(*(1 + first), std::string_view{"💚"});
    CHECK_EQ(first[2], std::string_view{"b"});
    CHECK_EQ(*(last - 1), std::string_view{"b"});

    auto it = first;
    CHECK_EQ(*++it, std::string_view{"💚"});
    CHECK_EQ(*it++, std::string_view{"💚"});
    CHECK_EQ(*it, std::string_view{"b"});
    CHECK_EQ(*--it, std::string_view{"💚"});
    CHECK_EQ(*it--, std::string_view{"💚"});
    CHECK_EQ(*it, std::string_view{"a"});
    it += 2;
    CHECK_EQ(*it, std::string_view{"b"});
    it -= 2;
    CHECK_EQ(*it, std::string_view{"a"});

    CHECK(first < last);
    CHECK(last > first);
    CHECK(first <= first);
    CHECK(first >= first);
    CHECK(first != last);
    CHECK(idx.cbegin() == first);
    CHECK(idx.cend() == last);
}

// 输入：无
// 输出：无
// 预期行为：range-for 直接遍历，每个元素是该字符的切片，拼回去等于原串
TEST("迭代器：range-for 遍历出的切片拼回原串") {
    const std::string original = "a💚b中é";
    CHECK_EQ(original.size(), std::size_t{11});

    const chridx::char_index<> idx{std::string_view{original}};
    std::string joined;
    std::size_t count = 0;
    for (const std::string_view ch : idx) {
        joined += std::string(ch);
        ++count;
    }
    CHECK_EQ(joined, original);
    CHECK_EQ(count, idx.size());
}

// 输入：无
// 输出：无
// 预期行为：at / index_at 越界抛 std::out_of_range，最后一个合法下标不抛
TEST("错误路径：at / index_at 越界抛 std::out_of_range") {
    const chridx::char_index<> idx{std::string_view{"a💚b"}};
    CHECK_THROWS(idx.at(3), std::out_of_range);
    CHECK_THROWS(idx.index_at(3), std::out_of_range);
    CHECK_THROWS(idx.at(999), std::out_of_range);
    CHECK_EQ(idx.at(2), std::string_view{"b"});
    CHECK_EQ(idx.index_at(2), std::size_t{5});
}

// 输入：无
// 输出：无
// 预期行为：T 太窄（uint8_t）装不下溢出点下标时抛 std::length_error，而不是悄悄写坏
TEST("错误路径：模板参数 T 装不下溢出点时抛 std::length_error") {
    CHECK_THROWS(chridx::char_index<std::uint8_t>{std::string_view{repeated_emoji(500)}}, std::length_error);

    const chridx::char_index<std::uint8_t> narrow{std::string_view{"a💚b"}};
    CHECK_EQ(narrow.size(), std::size_t{3});
    CHECK_EQ(narrow(2), std::size_t{5});
    CHECK_EQ(narrow[1], std::string_view{"💚"});
}

// 输入：无
// 输出：无
// 预期行为：换 T 实例化后语义一致（模板参数只影响存储宽度）
TEST("模板参数：换成 uint32_t 后行为一致") {
    const chridx::char_index<std::uint32_t> idx{std::string_view{"a💚b"}};
    CHECK_EQ(idx.size(), std::size_t{3});
    CHECK_EQ(idx(1), std::size_t{1});
    CHECK_EQ(idx.index_at(2), std::size_t{5});
    CHECK_EQ(idx[2], std::string_view{"b"});
}

// 输入：无
// 输出：无
// 预期行为：异常消息用英文（库的使用者不一定看中文），并且带上越界下标和当前 size()
TEST("错误路径：异常消息是英文，且带上越界下标与 size()") {
    const chridx::char_index<> idx{std::string_view{"a💚b"}};

    const std::string at_msg = catch_what([&idx] { idx.at(7); });
    CHECK(!at_msg.empty());
    CHECK(is_ascii(at_msg));
    CHECK(at_msg.find("chridx::at") != std::string::npos);
    CHECK(at_msg.find("out of range") != std::string::npos);
    CHECK(at_msg.find("7") != std::string::npos);
    CHECK(at_msg.find("size() == 3") != std::string::npos);

    const std::string index_msg = catch_what([&idx] { idx.index_at(9); });
    CHECK(!index_msg.empty());
    CHECK(is_ascii(index_msg));
    CHECK(index_msg.find("chridx::index_at") != std::string::npos);
    CHECK(index_msg.find("out of range") != std::string::npos);

    const std::string length_msg =
        catch_what([] { chridx::char_index<std::uint8_t>{std::string_view{repeated_emoji(500)}}; });
    CHECK(!length_msg.empty());
    CHECK(is_ascii(length_msg));
    CHECK(length_msg.find("T") != std::string::npos);
}

// 输入：无
// 输出：无
// 预期行为：++ 走到底拿到的切片序列，跟随机访问版逐个取的一致
TEST("双向迭代器：++ 遍历与随机访问版结果一致") {
    const std::string text = "a💚b中é";
    const chridx::char_index<> idx{std::string_view{text}};

    std::size_t i = 0;
    for (auto it = idx.bidirectional_begin(); it != idx.bidirectional_end(); ++it, ++i) {
        CHECK_EQ(*it, idx[i]);
    }
    CHECK_EQ(i, idx.size());
}

// 输入：无
// 输出：无
// 预期行为：-- 能一路退回起点；跨过溢出点后往回走，排名也得跟着退回去
TEST("双向迭代器：-- 退回起点，跨溢出点也算得对") {
    const std::string text = repeated_emoji(100);
    const chridx::char_index<> idx{std::string_view{text}};
    CHECK_EQ(idx.size(), std::size_t{100});

    auto it = idx.bidirectional_begin();
    for (std::size_t step = 0; step < 90; ++step) {
        ++it;
    }
    CHECK_EQ(*it, std::string_view{"💚"});

    std::size_t left = 90;
    while (it != idx.bidirectional_begin()) {
        --it;
        --left;
        CHECK_EQ(*it, std::string_view{"💚"});
        CHECK_EQ(*it, idx[left]);
    }
    CHECK_EQ(left, std::size_t{0});
}

// 输入：无
// 输出：无
// 预期行为：前缀 ++/-- 返回自己、后缀返回旧值；首尾迭代器可比
TEST("双向迭代器：前后缀语义与首尾比较") {
    const chridx::char_index<> idx{std::string_view{"a💚b"}};
    auto it = idx.bidirectional_begin();

    CHECK_EQ(*it++, std::string_view{"a"});
    CHECK_EQ(*it, std::string_view{"💚"});
    CHECK_EQ(*it--, std::string_view{"💚"});
    CHECK_EQ(*it, std::string_view{"a"});
    CHECK(it != idx.bidirectional_end());

    ++it;
    ++it;
    ++it;
    CHECK(it == idx.bidirectional_end());
    CHECK(idx.bidirectional_cbegin() == idx.bidirectional_begin());
    CHECK(idx.bidirectional_cend() == idx.bidirectional_end());
}

// 输入：无
// 输出：无
// 预期行为：500 个 4 字节字符顺序走一遍（跨 5 个溢出点），每一步切片都对
TEST("双向迭代器：长串顺序遍历跨多个溢出点") {
    const std::string text = repeated_emoji(500);
    const chridx::char_index<> idx{std::string_view{text}};
    std::size_t count = 0;
    for (auto it = idx.bidirectional_begin(); it != idx.bidirectional_end(); ++it) {
        CHECK_EQ(*it, std::string_view{"💚"});
        ++count;
    }
    CHECK_EQ(count, std::size_t{500});
}

// 输入：无
// 输出：无
// 预期行为：纯 ASCII（没有溢出表，排名恒为 0）和空串也能用
TEST("双向迭代器：纯 ASCII 与空串") {
    const chridx::char_index<> ascii{std::string_view{"abc"}};
    std::string joined;
    for (auto it = ascii.bidirectional_begin(); it != ascii.bidirectional_end(); ++it) {
        joined += std::string(*it);
    }
    CHECK_EQ(joined, std::string{"abc"});

    const chridx::char_index<> empty{std::string_view{""}};
    CHECK(empty.bidirectional_begin() == empty.bidirectional_end());
}

int main() { return ::chridx_test::run_all(); }
