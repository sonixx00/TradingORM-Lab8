#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <sqlite3.h>
#include <sqlite_orm/sqlite_orm.h>

using namespace std;
using namespace sqlite_orm;

// ===============================
// 1. DATA MODELS
// ===============================

struct Product {
    int id;
    string name;
    double price;
    int quantity;
};

struct Order {
    int id;
    int productId;
    int quantity;
    string orderDate;
};

// ===============================
// 2. ORM DATABASE
// ===============================

auto createStorage(const string& filename) {
    return make_storage(filename,
        make_table("Products",
            make_column("id", &Product::id,
                primary_key().autoincrement()),
            make_column("name", &Product::name),
            make_column("price", &Product::price),
            make_column("quantity", &Product::quantity)
        ),
        make_table("Orders",
            make_column("id", &Order::id,
                primary_key().autoincrement()),
            make_column("productId", &Order::productId),
            make_column("quantity", &Order::quantity),
            make_column("orderDate", &Order::orderDate),
            foreign_key(&Order::productId)
            .references(&Product::id)
        )
    );
}

using Storage = decltype(createStorage("trading.db"));

// ===============================
// 3. REPOSITORY INTERFACE
// ===============================

class IProductRepository {
public:
    virtual int add(const Product& product) = 0;
    virtual vector<Product> getAll() = 0;
    virtual Product getById(int id) = 0;
    virtual void update(const Product& product) = 0;
    virtual void remove(int id) = 0;

    virtual ~IProductRepository() = default;
};

// ===============================
// 4. PRODUCT REPOSITORY
// ===============================

class ProductRepository : public IProductRepository {
private:
    Storage& storage;

public:
    explicit ProductRepository(Storage& db)
        : storage(db) {
    }

    int add(const Product& product) override {
        return storage.insert(product);
    }

    vector<Product> getAll() override {
        return storage.get_all<Product>();
    }

    Product getById(int id) override {
        return storage.get<Product>(id);
    }

    void update(const Product& product) override {
        storage.update(product);
    }

    void remove(int id) override {
        storage.remove<Product>(id);
    }
};

// ===============================
// 5. ORDER DATA ACCESS
// ===============================

class OrderRepository {
private:
    Storage& storage;

public:
    explicit OrderRepository(Storage& db)
        : storage(db) {
    }

    int add(const Order& order) {
        return storage.insert(order);
    }

    vector<Order> getAll() {
        return storage.get_all<Order>();
    }
};

// ===============================
// 6. OUTPUT FUNCTIONS
// ===============================

void printProducts(IProductRepository& repository) {
    cout << "\n=== PRODUCTS ===\n";

    for (const auto& p : repository.getAll()) {
        cout << "ID: " << p.id
            << " | Name: " << p.name
            << " | Price: " << p.price
            << " | Quantity: " << p.quantity
            << endl;
    }
}

void printOrders(OrderRepository& repository) {
    cout << "\n=== ORDERS ===\n";

    for (const auto& o : repository.getAll()) {
        cout << "ID: " << o.id
            << " | Product ID: " << o.productId
            << " | Quantity: " << o.quantity
            << " | Date: " << o.orderDate
            << endl;
    }
}

// ===============================
// 7. SQLITE C API
// ===============================

void demonstrateRawSQLite(const string& filename) {
    cout << "\n=== SQLITE C API ===\n";

    sqlite3* db = nullptr;

    if (sqlite3_open(filename.c_str(), &db) != SQLITE_OK) {
        cout << "Cannot open database\n";

        if (db) {
            sqlite3_close(db);
        }

        return;
    }

    const char* sql =
        "SELECT COUNT(*) FROM Products;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(
        db, sql, -1, &stmt, nullptr) == SQLITE_OK) {

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            cout << "Products count (C API): "
                << sqlite3_column_int(stmt, 0)
                << endl;
        }
    }
    else {
        cout << "SQL preparation error\n";
    }

    if (stmt) {
        sqlite3_finalize(stmt);
    }

    sqlite3_close(db);
}

// ===============================
// 8. MAIN PROGRAM
// ===============================

int main() {
    try {
        // Use a separate demo database.
        // The previous trading.db is not modified.
        const string dbName = "trading_lab8.db";

        auto storage = createStorage(dbName);
        storage.on_open = [](sqlite3* db) {
            sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
            };
        storage.sync_schema();

        // Enable foreign key checks.
       

        ProductRepository productRepo(storage);
        OrderRepository orderRepo(storage);

        IProductRepository& repository = productRepo;

        // ===============================
        // 9. INITIAL DATA
        // ===============================

        cout << "=== DATABASE INITIALIZATION ===\n";

        // Clear old demo records.
        storage.remove_all<Order>();
        storage.remove_all<Product>();

        vector<Product> products = {
            {-1, "Laptop", 25000, 5},
            {-1, "Mouse", 500, 20},
            {-1, "Keyboard", 1200, 10},
            {-1, "Monitor", 7000, 8},
            {-1, "Printer", 4500, 6}
        };

        vector<int> productIds;

        for (const auto& p : products) {
            productIds.push_back(repository.add(p));
        }

        // Add 15 related orders.
        for (int i = 0; i < 15; i++) {
            Order order{
                -1,
                productIds[i % 5],
                i + 1,
                "2026-11-16"
            };

            orderRepo.add(order);
        }

        cout << "5 products added successfully.\n";
        cout << "15 orders added successfully.\n";

        printProducts(repository);
        printOrders(orderRepo);

        // ===============================
        // 10. CRUD OPERATIONS
        // ===============================

        cout << "\n=== CRUD OPERATIONS ===\n";

        // CREATE
        int testId = repository.add(
            { -1, "Test Product", 100, 3 }
        );

        cout << "CREATE: Product added, ID = "
            << testId << endl;

        // READ
        Product testProduct = repository.getById(testId);

        cout << "READ: "
            << testProduct.name << endl;

        // UPDATE
        testProduct.price = 150;
        repository.update(testProduct);

        cout << "UPDATE: New price = "
            << repository.getById(testId).price
            << endl;

        // DELETE
        repository.remove(testId);

        cout << "DELETE: Test product removed\n";

        // ===============================
        // 11. FILTERING
        // ===============================

        cout << "\n=== FILTER: PRICE > 1000 ===\n";

        auto filtered = storage.get_all<Product>(
            where(c(&Product::price) > 1000.0)
        );

        for (const auto& p : filtered) {
            cout << p.name
                << " | Price: " << p.price
                << endl;
        }

        // ===============================
        // 12. SORTING AND LIMIT
        // ===============================

        cout << "\n=== SORT BY PRICE DESC, LIMIT 3 ===\n";

        auto sorted = storage.get_all<Product>(
            order_by(&Product::price).desc(),
            limit(3)
        );

        for (const auto& p : sorted) {
            cout << p.name
                << " | Price: " << p.price
                << endl;
        }

        // ===============================
        // 13. AGGREGATION
        // ===============================

        cout << "\n=== AGGREGATION ===\n";

        cout << "Products count: "
            << storage.count<Product>()
            << endl;

        auto totalQuantity = storage.sum(&Product::quantity);

        if (totalQuantity) {
            cout << "Total quantity: "
                << *totalQuantity << endl;
        }

        // ===============================
        // 14. GROUPING
        // ===============================

        cout << "\n=== GROUP BY PRODUCT ===\n";

        auto grouped = storage.select(
            columns(
                &Order::productId,
                count(&Order::id)
            ),
            group_by(&Order::productId)
        );

        for (const auto& row : grouped) {
            cout << "Product ID: "
                << get<0>(row)
                << " | Orders: "
                << get<1>(row)
                << endl;
        }

        // ===============================
        // 15. JOIN
        // ===============================

        cout << "\n=== JOIN PRODUCTS AND ORDERS ===\n";

        auto joined = storage.select(
            columns(
                &Product::name,
                &Order::id,
                &Order::quantity,
                &Order::orderDate
            ),
            inner_join<Order>(
                on(c(&Product::id) == &Order::productId)
            )
        );

        for (const auto& row : joined) {
            cout << "Product: " << get<0>(row)
                << " | Order ID: " << get<1>(row)
                << " | Quantity: " << get<2>(row)
                << " | Date: " << get<3>(row)
                << endl;
        }

        // ===============================
        // 16. SUCCESSFUL TRANSACTION
        // ===============================

        cout << "\n=== TRANSACTION COMMIT ===\n";

        int firstId = productIds[0];

        int before =
            repository.getById(firstId).quantity;

        bool committed = storage.transaction([&]() {
            Product p = repository.getById(firstId);

            p.quantity -= 1;
            repository.update(p);

            orderRepo.add({
                -1,
                firstId,
                1,
                "2026-11-16"
                });

            return true;
            });

        int after =
            repository.getById(firstId).quantity;

        cout << "Before: " << before << endl;
        cout << "After: " << after << endl;
        cout << "COMMIT: "
            << (committed ? "SUCCESS" : "FAILED")
            << endl;

        // ===============================
        // 17. ROLLBACK TRANSACTION
        // ===============================

        cout << "\n=== TRANSACTION ROLLBACK ===\n";

        before =
            repository.getById(firstId).quantity;

        try {
            storage.transaction([&]() {
                Product p = repository.getById(firstId);

                p.quantity -= 1;
                repository.update(p);

                // Invalid foreign key causes an error.
                orderRepo.add({
                    -1,
                    999999,
                    1,
                    "2026-11-16"
                    });

                return true;
                });
        }
        catch (const exception& e) {
            cout << "Expected error: "
                << e.what() << endl;
        }

        after =
            repository.getById(firstId).quantity;

        cout << "Before: " << before << endl;
        cout << "After: " << after << endl;

        if (before == after) {
            cout << "ROLLBACK SUCCESSFUL\n";
        }
        else {
            cout << "ROLLBACK CHECK FAILED\n";
        }

        // ===============================
        // 18. WITHOUT TRANSACTION
        // ===============================

        cout << "\n=== WITHOUT TRANSACTION ===\n";

        before =
            repository.getById(firstId).quantity;

        Product p = repository.getById(firstId);

        p.quantity -= 1;
        repository.update(p);

        try {
            orderRepo.add({
                -1,
                999999,
                1,
                "2026-11-16"
                });
        }
        catch (const exception& e) {
            cout << "Expected error: "
                << e.what() << endl;
        }

        after =
            repository.getById(firstId).quantity;

        cout << "Before: " << before << endl;
        cout << "After: " << after << endl;

        cout << "Without transaction, first change remains.\n";

        // ===============================
        // 19. RAW API COMPARISON
        // ===============================

        demonstrateRawSQLite(dbName);

        cout << "\n=== ORM API ===\n";

        cout << "Products count (ORM): "
            << storage.count<Product>()
            << endl;

        // ===============================
        // 20. FINAL RESULTS
        // ===============================

        cout << "\n=== FINAL DATABASE STATE ===\n";

        printProducts(repository);

        cout << "\nTotal products: "
            << storage.count<Product>()
            << endl;

        cout << "Total orders: "
            << storage.count<Order>()
            << endl;

        cout << "\nLABORATORY WORK COMPLETED\n";

        return 0;
    }
    catch (const exception& e) {
        cerr << "\nERROR: "
            << e.what() << endl;

        return 1;
    }
}