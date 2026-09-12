# chridx

C++17 的 UTF-8 字符索引：把「第 n 个字符（Unicode 标量值）」映射到「第几个字节」，

不把字符串展开成 `std::vector<char32_t>` 也能做到均摊 O(1) 查询。

## 结构

```
chridx/
├─ CMakeLists.txt                简单 CMake：一个头文件库 + 一个测试可执行
├─ LICENSE                       MIT
├─ include/chridx/char_index.hpp chridx::char_index：偏移表 + 溢出表实现，全在头里
└─ tests/
   ├─ check.hpp                  极简断言宏（无第三方依赖）
   └─ test_char_index.cpp        测试入口 + 用例
```

## 构建与测试

需要 CMake ≥ 3.26 和支持 C++17 的编译器。

```powershell
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

仓库里的 `CMakeUserPresets.json`（本机 preset，不入库）已经配好 MSYS2 UCRT64 + Ninja：

```powershell
cmake --preset default
cmake --build --preset default
ctest --preset default
```

## 用法

```cpp
#include "chridx/char_index.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

int main() {
    std::string text = "a💚b中é";

    // 构造只接受 std::string_view（这段内存不归索引，生命周期由调用方负责）
    chridx::char_index<> idx{std::string_view{text}};

    std::cout << idx.size() << "\n";   // 5 个字符
    std::cout << idx[1] << "\n";       // 💚 —— 切片是原串的子视图，不拷贝
    std::cout << idx(2) << "\n";       // 5  —— 第 3 个字符的首字节下标

    // 越界：at / index_at 抛 std::out_of_range；[] / () 不检查（同 STL，属 UB）
    try {
        idx.at(99);
    } catch (const std::out_of_range& e) {
        std::cout << e.what() << "\n";
    }

    // 只读随机访问迭代器：range-for 直接拿到每个字符的切片
    for (std::string_view ch : idx) {
        std::cout << ch;
    }
    std::cout << "\n";

    // 只支持 ++ / -- 的双向迭代器：顺序遍历每步 O(1)
    for (auto it = idx.bidirectional_begin(); it != idx.bidirectional_end(); ++it) {
        std::cout << *it;
    }
    std::cout << "\n";

    // 尾后迭代器往回走：-- 一次就是最后一个字符
    auto it = idx.bidirectional_end();
    --it;
    std::cout << *it << "\n";          // é

    // 模板参数换溢出点计数的宽度：窄 T 省内存，装不下时构造抛 std::length_error
    chridx::char_index<std::uint8_t> narrow{std::string_view{"a💚b"}};
    std::cout << narrow(2) << "\n";    // 5
    return 0;
}
```

输出（实测）：

```
5
💚
5
chridx::at: char index 99 is out of range (size() == 5)
a💚b中é
a💚b中é
é
5
```

## 设计要点

- UTF-8 每个字符最多 4 字节，所以「字节下标 − 字符下标」这个偏移单调不减、每步最多 +3。
  于是可以用一张 `std::uint8_t` 偏移表 + 一张溢出表来表达，不必存全量 `std::size_t`。
- 偏移累计超过 255 时记一次「翻转」，查询时按翻转次数补回 `255 × n`。
- 纯 ASCII 可以特化：字符数 == 字节数，表全空，字节下标直接等于字符下标。
- 接口分四路：`operator[]` / `at()` 返回该字符的**字节切片**（`std::string_view`），
  `operator()` / `index_at()` 返回**首字节下标**；每对都是「不检查（越界 UB）+ 检查（抛
  `std::out_of_range`）」的 STL 惯例组合。
- 构造只接受 `std::string_view`（左值/右值都行）：`std::string`、`const char*`、字符串字面量
  统统被编译期挡住，必须显式写 `std::string_view{...}`  
- 另提供两种**只读迭代器**：随机访问的 `const_iterator`（`begin` / `end` / `cbegin` /
  `cend`，解引用得到切片，可直接 `for (std::string_view ch : idx)`），以及只给 `++` / `--`
  的 `const_bidirectional_iterator`（`bidirectional_begin` / `bidirectional_end`）——它把「溢出点条数」存在迭代器自己
  身上，顺序遍历每步 O(1)，代价是不给 `it + n`。

## 参考资料

本仓库是**独立实现**：只借鉴算法思路，不搬运上游源码。

- 算法思路来源（Rust crate）：<https://crates.io/crates/char_index>
- 仓库：<https://github.com/unicode-entropy/char_index>

## License

MIT，见 [LICENSE](LICENSE)。
