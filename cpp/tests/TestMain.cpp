#include "TestHarness.h"

struct TestCase {
    std::string name;
    std::function<bool()> testFunc;
};

std::vector<TestCase>& GetTestRegistry() {
    static std::vector<TestCase> registry;
    return registry;
}

void RegisterTest(const std::string& name, std::function<bool()> func) {
    GetTestRegistry().push_back({ name, func });
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::cout << "========================================" << std::endl;
    std::cout << " Running Step Recorder Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;

    int passed = 0;
    int failed = 0;

    for (const auto& tc : GetTestRegistry()) {
        std::cout << "[ RUN      ] " << tc.name << std::endl;
        bool ok = false;
        try {
            ok = tc.testFunc();
        } catch (const std::exception& ex) {
            std::cerr << "Exception: " << ex.what() << std::endl;
            ok = false;
        } catch (...) {
            std::cerr << "Unknown exception" << std::endl;
            ok = false;
        }

        if (ok) {
            std::cout << "[       OK ] " << tc.name << std::endl;
            ++passed;
        } else {
            std::cout << "[  FAILED  ] " << tc.name << std::endl;
            ++failed;
        }
    }

    std::cout << "========================================" << std::endl;
    std::cout << " Tests run: " << (passed + failed)
              << ", Passed: " << passed
              << ", Failed: " << failed << std::endl;
    std::cout << "========================================" << std::endl;

    return (failed == 0) ? 0 : 1;
}
