#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <stdexcept>

namespace dm::test {

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

class Registry {
public:
    static Registry& instance() {
        static Registry r;
        return r;
    }
    void add(const std::string& name, std::function<void()> fn) {
        tests_.push_back({name, fn});
    }
    int runAll() {
        int pass = 0, fail = 0;
        for (auto& t : tests_) {
            try {
                t.fn();
                std::cout << "[PASS] " << t.name << "\n";
                pass++;
            } catch (const std::exception& e) {
                std::cout << "[FAIL] " << t.name << " — " << e.what() << "\n";
                fail++;
            }
        }
        std::cout << "\n=== " << pass << " 通过, " << fail << " 失败 ===\n";
        return fail == 0 ? 0 : 1;
    }
private:
    std::vector<TestCase> tests_;
};

} // namespace dm::test

#define TEST(name) \
    static void test_##name(); \
    static struct Reg_##name { \
        Reg_##name() { dm::test::Registry::instance().add(#name, test_##name); } \
    } reg_##name; \
    static void test_##name()

#define EXPECT_TRUE(x) do { if (!(x)) throw std::runtime_error("EXPECT_TRUE failed: " #x); } while(0)
#define EXPECT_EQ(a, b) do { if (!((a) == (b))) throw std::runtime_error("EXPECT_EQ failed: " #a " != " #b); } while(0)
