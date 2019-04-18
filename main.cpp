#include <functional>
#include <iostream>
#include <tuple>
#include <vector>
#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

using namespace std;

class TransactionManager {
  private:
    struct Transaction {

        using redo_function_t = std::function<void()>;
        using undo_function_t = std::function<void()>;
        using operation = std::tuple<redo_function_t, undo_function_t>;

        Transaction(std::string what) : mWhat(std::move(what)) {}

        Transaction(Transaction &&) = default;

        Transaction(Transaction &) = delete;

        Transaction &operator=(Transaction &&) = default;

        Transaction &operator=(Transaction &) = delete;

        void storeAndExecute(redo_function_t &&redo_f, undo_function_t &&undo_f) {
            redo_f();
            mOperations.emplace_back(operation{std::move(redo_f), std::move(undo_f)});
        }

        void redo() {
            for (auto it = mOperations.rbegin(); it != mOperations.rend(); ++it)
                get<0> (*it)();
        }

        void undo() {
            for (const auto &op : mOperations)
                get<1>(op)();
        }

        std::string mWhat;
        std::vector<operation> mOperations;
    };

  public:
    void begin(std::string what) {
        // Discard events from a different future
        if (mCurrent != mTimeLine.end())
            mTimeLine.erase(std::next(mCurrent), mTimeLine.end());

        // Start a new transaction, and put it at top of stack
        mTimeLine.emplace_back(Transaction(std::move(what)));
        mCurrent = std::prev(mTimeLine.end());
    }

    void commit() {
        // Nothing, really.
    }

    void rollback() {
        assert(mCurrent != mTimeLine.end());

        // Roll back current transaction
        mCurrent->redo();

        // Remove if from stack
        mTimeLine.pop_back();
    }

    void storeAndExecute(Transaction::redo_function_t &&do_f, Transaction::undo_function_t &&undo_f) {
        assert(mCurrent != mTimeLine.end());
        mCurrent->storeAndExecute(std::move(do_f), std::move(undo_f));
    }

    void undo() {
        assert(mCurrent != mTimeLine.end());
        mCurrent->undo();
        mCurrent--;
    }

    void redo() {
        assert(mCurrent != mTimeLine.end());
        ++mCurrent;

        assert(mCurrent != mTimeLine.end());
        mCurrent->redo();
    }

    size_t size() const { return mTimeLine.size(); }

    ssize_t lastIndex() const { return mCurrent - mTimeLine.begin(); }

  private:
    std::vector<Transaction> mTimeLine;
    std::vector<Transaction>::iterator mCurrent = mTimeLine.end();
};

struct item {
    std::string a;
    std::string b;
};

TEST_CASE("Undo redo test 1", "[single-file]") {
    item data{"Hello", "Other"};

    TransactionManager mgr;

    mgr.begin("Add string");
    mgr.storeAndExecute([&data, new_value = data.a + " World"]() { data.a = new_value; },
                        [&data, old_value = data.a]() { data.a = old_value; });
    mgr.commit();

    REQUIRE(data.a == "Hello World");
    REQUIRE(data.b == "Other");
    REQUIRE(mgr.size() == 1);
}

TEST_CASE("Undo redo test 2", "[single-file]") {
    item data{"Hello", "Other"};

    TransactionManager mgr;

    mgr.begin("Add string");
    mgr.storeAndExecute([&data, new_value = data.a + " World"]() { data.a = new_value; },
                        [&data, old_value = data.a]() { data.a = old_value; });
    mgr.commit();
    REQUIRE(data.a == "Hello World");
    REQUIRE(data.b == "Other");

    mgr.undo();
    REQUIRE(data.a == "Hello");
    REQUIRE(data.b == "Other");
    REQUIRE(mgr.size() == 1);
    REQUIRE(mgr.lastIndex() == -1);
}

TEST_CASE("Undo redo test 3", "[single-file]") {
    item data{"Hello", "Other"};

    TransactionManager mgr;

    mgr.begin("Add string");
    mgr.storeAndExecute([&data, new_value = data.a + " World"]() { data.a = new_value; },
                        [&data, old_value = data.a]() { data.a = old_value; });
    mgr.commit();

    REQUIRE(data.a == "Hello World");

    mgr.begin("Add another string");
    mgr.storeAndExecute([&data, new_value = data.a + "!"]() { data.a = new_value; },
                        [&data, old_value = data.a]() { data.a = old_value; });
    mgr.commit();

    REQUIRE(data.a == "Hello World!");
    REQUIRE(data.b == "Other");

    mgr.undo();
    REQUIRE(data.a == "Hello World");
    REQUIRE(data.b == "Other");

    mgr.undo();
    REQUIRE(data.a == "Hello");
    REQUIRE(data.b == "Other");

    mgr.redo();
    REQUIRE(data.a == "Hello World");
    REQUIRE(data.b == "Other");

    mgr.redo();
    REQUIRE(data.a == "Hello World!");
    REQUIRE(data.b == "Other");

    mgr.undo();
    REQUIRE(data.a == "Hello World");
    REQUIRE(data.b == "Other");

    mgr.undo();
    REQUIRE(data.a == "Hello");
    REQUIRE(data.b == "Other");

    REQUIRE(mgr.size() == 2);
    REQUIRE(mgr.lastIndex() == -1);
}

TEST_CASE("Undo redo test 4", "[single-file]") {
    item data{"Hello", "Other"};

    TransactionManager mgr;

    mgr.begin("Add string");
    mgr.storeAndExecute([&data, new_value = data.a + " World"]() { data.a = new_value; },
                        [&data, old_value = data.a]() { data.a = old_value; });
    mgr.commit();

    REQUIRE(data.a == "Hello World");
    REQUIRE(data.b == "Other");
    REQUIRE(mgr.lastIndex() == 0);

    mgr.begin("Add another string");
    mgr.storeAndExecute([&data, new_value = data.a + "!"]() { data.a = new_value; },
                        [&data, old_value = data.a]() { data.a = old_value; });
    mgr.commit();

    REQUIRE(data.a == "Hello World!");
    REQUIRE(data.b == "Other");

    mgr.undo();
    REQUIRE(data.a == "Hello World");
    REQUIRE(data.b == "Other");

    mgr.begin("Add another !");
    mgr.storeAndExecute([&data, new_value = data.a + "!!"]() { data.a = new_value; },
                        [&data, old_value = data.a]() { data.a = old_value; });
    mgr.commit();
    REQUIRE(data.a == "Hello World!!");
    REQUIRE(data.b == "Other");

    mgr.undo();
    REQUIRE(data.a == "Hello World");
    REQUIRE(data.b == "Other");

    mgr.undo();
    REQUIRE(data.a == "Hello");
    REQUIRE(data.b == "Other");

    REQUIRE(mgr.lastIndex() == -1);
}
