// ════════════════════════════════════════════════════════════════════════════
// tests/class_analyzer_test.cpp — Test suite for ClassAnalyzer.
//
// 7 test cases:
//   1. Fields set only in init are collected
//   2. Fields set in methods (not init) are collected
//   3. Fields set inside an if-branch conditional are collected
//   4. Fields set inside a for-loop body are collected
//   5. Fields set in both init and a method are deduplicated
//   6. Inherited fields from base class precede derived-class fields
//   7. Multi-level inheritance (A → B → C) is handled correctly
// ════════════════════════════════════════════════════════════════════════════

#include "lexer.h"
#include "parser.h"
#include "class_analyzer.h"
#include <iostream>
#include <string>
#include <vector>

using namespace visuall;

static int failures = 0;

static void expect(bool condition, const std::string& testName) {
    if (!condition) {
        std::cerr << "  FAIL: " << testName << "\n";
        failures++;
    } else {
        std::cout << "  PASS: " << testName << "\n";
    }
}

// Helper: lex + parse + run ClassAnalyzer, return the fields for className.
// Returns empty vector on parse error.
static std::vector<std::string>
analyzeFields(const std::string& src, const std::string& className) {
    try {
        Lexer lexer(src, "test.vsl");
        auto tokens = lexer.tokenize();
        Parser parser(tokens, "test.vsl");
        auto program = parser.parse();
        ClassAnalyzer ca;
        ca.analyze(*program);
        const auto& m = ca.classFields();
        auto it = m.find(className);
        if (it == m.end()) return {};
        return it->second;
    } catch (const std::exception& e) {
        std::cerr << "  [analyzeFields error] " << e.what() << "\n";
        return {};
    }
}

// ── 1. Fields set only in init ─────────────────────────────────────────────
static void test_initFields() {
    const std::string src = R"(
class Point:
	init(x: int, y: int):
		this.x = x
		this.y = y
)";
    auto fields = analyzeFields(src, "Point");
    expect(fields.size() == 2,  "1a. Two fields found from init");
    expect(!fields.empty() && fields[0] == "x", "1b. First field is 'x'");
    expect(fields.size() >= 2 && fields[1] == "y", "1c. Second field is 'y'");
}

// ── 2. Fields set in a method (not init) ───────────────────────────────────
static void test_methodFields() {
    const std::string src = R"(
class Dog:
	init():
		this.name = "default"
	define bark() -> void:
		this.sound = "woof"
)";
    auto fields = analyzeFields(src, "Dog");
    expect(fields.size() == 2, "2a. Two fields found (init + method)");
    bool hasName  = false;
    bool hasSound = false;
    for (const auto& f : fields) {
        if (f == "name")  hasName  = true;
        if (f == "sound") hasSound = true;
    }
    expect(hasName,  "2b. Field 'name' collected from init");
    expect(hasSound, "2c. Field 'sound' collected from method");
}

// ── 3. Fields set inside an if-branch conditional ──────────────────────────
static void test_conditionalFields() {
    const std::string src = R"(
class Animal:
	init(species: str):
		this.species = species
		if species == "cat":
			this.sound = "meow"
		else:
			this.sound = "generic"
)";
    auto fields = analyzeFields(src, "Animal");
    expect(fields.size() == 2, "3a. Two fields found (unconditional + conditional)");
    bool hasSpecies = false;
    bool hasSound   = false;
    for (const auto& f : fields) {
        if (f == "species") hasSpecies = true;
        if (f == "sound")   hasSound   = true;
    }
    expect(hasSpecies, "3b. Field 'species' collected");
    expect(hasSound,   "3c. Field 'sound' from conditional branch collected");
}

// ── 4. Fields set inside a for-loop body ───────────────────────────────────
static void test_loopFields() {
    const std::string src = R"(
class Counter:
	init():
		this.total = 0
		for i in range(10, 1, 1):
			this.last = i
)";
    auto fields = analyzeFields(src, "Counter");
    bool hasTotal = false;
    bool hasLast  = false;
    for (const auto& f : fields) {
        if (f == "total") hasTotal = true;
        if (f == "last")  hasLast  = true;
    }
    expect(hasTotal, "4a. Field 'total' collected");
    expect(hasLast,  "4b. Field 'last' inside for-loop body collected");
}

// ── 5. Duplicate this.field assignments deduplicated ──────────────────────
static void test_deduplication() {
    const std::string src = R"(
class Dup:
	init():
		this.x = 0
	define reset() -> void:
		this.x = 0
)";
    auto fields = analyzeFields(src, "Dup");
    expect(fields.size() == 1, "5. Duplicate field 'x' appears exactly once");
}

// ── 6. Inherited fields from base class precede derived-class fields ────────
static void test_inheritance() {
    const std::string src = R"(
class Base:
	init():
		this.x = 0
		this.y = 0

class Derived extends Base:
	init():
		this.z = 0
)";
    auto derivedFields = analyzeFields(src, "Derived");
    expect(derivedFields.size() == 3, "6a. Three total fields in Derived");
    expect(!derivedFields.empty() && derivedFields[0] == "x", "6b. Inherited 'x' is first");
    expect(derivedFields.size() >= 2 && derivedFields[1] == "y", "6c. Inherited 'y' is second");
    expect(derivedFields.size() >= 3 && derivedFields[2] == "z", "6d. Own 'z' is last");
}

// ── 7. Multi-level inheritance (A → B → C) ────────────────────────────────
static void test_multiLevelInheritance() {
    const std::string src = R"(
class A:
	init():
		this.a = 1

class B extends A:
	init():
		this.b = 2

class C extends B:
	init():
		this.c = 3
)";
    auto cFields = analyzeFields(src, "C");
    expect(cFields.size() == 3, "7a. Three total fields in C");
    expect(!cFields.empty() && cFields[0] == "a", "7b. 'a' from A is first");
    expect(cFields.size() >= 2 && cFields[1] == "b", "7c. 'b' from B is second");
    expect(cFields.size() >= 3 && cFields[2] == "c", "7d. 'c' from C is last");
}

// ── Public entry point ────────────────────────────────────────────────────
int runClassAnalyzerTests() {
    failures = 0;
    test_initFields();
    test_methodFields();
    test_conditionalFields();
    test_loopFields();
    test_deduplication();
    test_inheritance();
    test_multiLevelInheritance();
    return failures;
}
