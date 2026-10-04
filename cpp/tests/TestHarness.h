#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <functional>

void RegisterTest(const std::string& name, std::function<bool()> func);

#define TEST_CASE(name) \
    bool name##_impl(); \
    struct name##_registrar { \
        name##_registrar() { RegisterTest(#name, name##_impl); } \
    } name##_reg_instance; \
    bool name##_impl()

#define ASSERT_TRUE(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed: " #cond " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return false; \
        } \
    } while (false)

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_NE(a, b) ASSERT_TRUE((a) != (b))
