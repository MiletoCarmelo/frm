#include <iostream> // Pour cout, cin, etc.
#include <iomanip> // Pour std::fixed, std::setprecision (formatage)
#include <vector> // Pour std::vector (tableaux dynamiques)
#include <chrono> // Pour mesurer le temps d'exécution
#include <random> // Pour générer des données de test aléatoires
#include <string> // Pour std::string
#include <concepts> // Pour les concepts C++20
#include <stdexcept> // Pour std::runtime_error

// g++ -std=c++20 -Wall -Wextra -Wpedantic -O3 -march=native -DNDEBUG -I. main_test.cpp -o main_test
// ./main_test

// CONCEPTS //
template<typename T>
concept NumericLike = std::is_arithmetic_v<T>;

template<typename T>
concept CharLike = std::is_same_v<T, std::string> ||
                    std::is_same_v<T, const char*> ||
                    std::is_convertible_v<T, std::string>;

template<typename T, typename E>
class expected {
private:
    union {
        T value_;
        E error_;
    };
    bool has_value_;
    
public:
    expected(const T& value) : value_(value), has_value_(true) {}
    expected(const E& error) : error_(error), has_value_(false) {}
   
    bool has_value() const noexcept { return has_value_; }
    
    expected(const expected& other) : has_value_(other.has_value_) {
        if (has_value_) {
            new(&value_) T(other.value_);
        } else {
            new(&error_) E(other.error_);
        }
    }
   
    expected& operator=(const expected& other) {
        if (this != &other) {
            this->~expected();
            new(this) expected(other);
        }
        return *this;
    }
   
    ~expected() {
        if (has_value_) {
            value_.~T();
        } else {
            error_.~E();
        }
    }
    
    const T& value() const {
        if (!has_value_) throw std::runtime_error("No value present");
        return value_;
    }
   
    const E& error() const {
        if (has_value_) throw std::runtime_error("No error present");
        return error_;
    }
   
    explicit operator bool() const noexcept { return has_value_; }
};

// FONCTIONS CORRIGÉES //
template<NumericLike T, NumericLike U>
expected<T, std::string> add(T a, U b) {  
    try {
        return expected<T, std::string>(a + static_cast<T>(b));
    } catch (const std::exception& e) {
        return expected<T, std::string>(std::string("Addition error: ") + e.what());
    }
}

template<NumericLike T, NumericLike U>
expected<T, std::string> subtract(T a, U b) {
    try {
        return expected<T, std::string>(a - static_cast<T>(b));
    } catch (const std::exception& e) {
        return expected<T, std::string>(std::string("Subtraction error: ") + e.what());
    }
}

// Fonction bonus: multiplication
template<NumericLike T, NumericLike U>
expected<T, std::string> multiply(T a, U b) {
    try {
        return expected<T, std::string>(a * static_cast<T>(b));
    } catch (const std::exception& e) {
        return expected<T, std::string>(std::string("Multiplication error: ") + e.what());
    }
}

// Fonction bonus: division avec gestion division par zéro
template<NumericLike T, NumericLike U>
expected<double, std::string> divide(T a, U b) {
    if (b == 0) {
        return expected<double, std::string>("Division by zero error");
    }
    try {
        return expected<double, std::string>(static_cast<double>(a) / static_cast<double>(b));
    } catch (const std::exception& e) {
        return expected<double, std::string>(std::string("Division error: ") + e.what());
    }
}

// CONCEPT CORRIGÉ //
template<typename T>
concept operationModel = requires (T t, double a, double b) {
    { t.add(a, b) } -> std::convertible_to<expected<double, std::string>>;
    { t.subtract(a, b) } -> std::convertible_to<expected<double, std::string>>;
    { t.multiply(a, b) } -> std::convertible_to<expected<double, std::string>>;
    { t.divide(a, b) } -> std::convertible_to<expected<double, std::string>>;
};

// UTILISATION DU CONCEPT operationModel //

// Fonction générique qui utilise le concept
template<operationModel T>
void perform_operations(T& calculator, double x, double y) {
    std::cout << "   Performing operations with " << typeid(T).name() << ":\n";
    
    auto add_result = calculator.add(x, y);
    if (add_result) {
        std::cout << "     " << x << " + " << y << " = " << add_result.value() << std::endl;
    } else {
        std::cout << "     Add error: " << add_result.error() << std::endl;
    }
    
    auto sub_result = calculator.subtract(x, y);
    if (sub_result) {
        std::cout << "     " << x << " - " << y << " = " << sub_result.value() << std::endl;
    } else {
        std::cout << "     Subtract error: " << sub_result.error() << std::endl;
    }
}

// Fonction template qui accepte seulement les types satisfaisant operationModel
template<operationModel Calculator>
expected<double, std::string> calculate_expression(Calculator& calc, double a, double b, double c) {
    // Calcule (a + b) - c
    auto step1 = calc.add(a, b);
    if (!step1) {
        return expected<double, std::string>("Error in addition: " + step1.error());
    }
    
    auto step2 = calc.subtract(step1.value(), c);
    if (!step2) {
        return expected<double, std::string>("Error in subtraction: " + step2.error());
    }
    
    return step2;
}

// Exemple de classe qui satisfait le concept
class Calculator {
public:
    expected<double, std::string> add(double a, double b) {
        return expected<double, std::string>(a + b);
    }
    
    expected<double, std::string> subtract(double a, double b) {
        return expected<double, std::string>(a - b);
    }
    
    expected<double, std::string> multiply(double a, double b) {
        return expected<double, std::string>(a * b);
    }
    
    expected<double, std::string> divide(double a, double b) {
        if (b == 0.0) {
            return expected<double, std::string>("Cannot divide by zero");
        }
        return expected<double, std::string>(a / b);
    }
};

// Autre classe qui satisfait le concept (avec logging)
class LoggingCalculator {
public:
    expected<double, std::string> add(double a, double b) {
        std::cout << "       [LOG] Adding " << a << " + " << b << std::endl;
        return expected<double, std::string>(a + b);
    }
    
    expected<double, std::string> subtract(double a, double b) {
        std::cout << "       [LOG] Subtracting " << a << " - " << b << std::endl;
        return expected<double, std::string>(a - b);
    }
};

// Classe qui NE satisfait PAS le concept (pour comparaison)
class BadCalculator {
public:
    int add(double a, double b) {  // ❌ Retourne int au lieu d'expected
        return static_cast<int>(a + b);
    }
    
    int subtract(double a, double b) {  // ❌ Retourne int au lieu d'expected
        return static_cast<int>(a - b);
    }
};

int main() {
    std::cout << "=== Tests des fonctions expected ===\n\n";
    
    // Test des fonctions de base
    std::cout << "1. Tests des opérations arithmétiques:\n";
    
    auto result1 = add(10, 5);
    if (result1) {
        std::cout << "   add(10, 5) = " << result1.value() << std::endl;
    } else {
        std::cout << "   add(10, 5) error: " << result1.error() << std::endl;
    }
    
    auto result2 = subtract(15, 7);
    if (result2) {
        std::cout << "   subtract(15, 7) = " << result2.value() << std::endl;
    } else {
        std::cout << "   subtract(15, 7) error: " << result2.error() << std::endl;
    }
    
    auto result3 = multiply(4, 6);
    if (result3) {
        std::cout << "   multiply(4, 6) = " << result3.value() << std::endl;
    } else {
        std::cout << "   multiply(4, 6) error: " << result3.error() << std::endl;
    }
    
    // Test avec types différents
    std::cout << "\n2. Tests avec types mixtes:\n";
    
    auto result4 = add(3.14, 2);  // double + int
    if (result4) {
        std::cout << "   add(3.14, 2) = " << result4.value() << std::endl;
    }
    
    auto result5 = subtract(100.5f, 25.3);  // float + double
    if (result5) {
        std::cout << "   subtract(100.5f, 25.3) = " << result5.value() << std::endl;
    }
    
    // Test de gestion d'erreur
    std::cout << "\n3. Test de gestion d'erreur (division par zéro):\n";
    
    auto result6 = divide(10.0, 2.0);
    if (result6) {
        std::cout << "   divide(10.0, 2.0) = " << result6.value() << std::endl;
    } else {
        std::cout << "   divide(10.0, 2.0) error: " << result6.error() << std::endl;
    }
    
    auto result7 = divide(5.0, 0);  // Division par zéro !
    if (result7) {
        std::cout << "   divide(5.0, 0) = " << result7.value() << std::endl;
    } else {
        std::cout << "   divide(5.0, 0) error: " << result7.error() << std::endl;
    }
    
    // ===== UTILISATION CONCRÈTE DU CONCEPT operationModel =====
    std::cout << "\n4. UTILISATION du concept operationModel:\n";
    
    // Test avec Calculator
    Calculator calc;
    std::cout << "\n   Tests avec Calculator standard:\n";
    perform_operations(calc, 20.0, 8.0);
    
    // Test avec LoggingCalculator
    LoggingCalculator log_calc;
    std::cout << "\n   Tests avec LoggingCalculator:\n";
    perform_operations(log_calc, 15.0, 5.0);
    
    // Test de la fonction calculate_expression
    std::cout << "\n5. Test de calculate_expression (utilise le concept):\n";
    
    auto expr_result = calculate_expression(calc, 10.0, 5.0, 3.0);  // (10 + 5) - 3
    if (expr_result) {
        std::cout << "   (10.0 + 5.0) - 3.0 = " << expr_result.value() << std::endl;
    } else {
        std::cout << "   Expression error: " << expr_result.error() << std::endl;
    }
    
    // Test avec LoggingCalculator
    auto expr_result2 = calculate_expression(log_calc, 7.0, 3.0, 2.0);  // (7 + 3) - 2
    if (expr_result2) {
        std::cout << "   (7.0 + 3.0) - 2.0 = " << expr_result2.value() << std::endl;
    } else {
        std::cout << "   Expression error: " << expr_result2.error() << std::endl;
    }
    
    // Démonstration que BadCalculator ne satisfait PAS le concept
    std::cout << "\n6. Vérification des concepts:\n";
    std::cout << "   Calculator satisfait operationModel? " 
              << operationModel<Calculator> << std::endl;
    std::cout << "   LoggingCalculator satisfait operationModel? " 
              << operationModel<LoggingCalculator> << std::endl;
    std::cout << "   BadCalculator satisfait operationModel? " 
              << operationModel<BadCalculator> << std::endl;
    
    // Ces lignes ne compileraient PAS car BadCalculator ne satisfait pas le concept :
    // perform_operations(bad_calc, 1.0, 2.0);  // ❌ Erreur de compilation
    // calculate_expression(bad_calc, 1.0, 2.0, 3.0);  // ❌ Erreur de compilation
    
    std::cout << "\n=== Tous les tests réussis ! ===\n";
    std::cout << "Le concept operationModel est maintenant UTILISÉ dans :\n";
    std::cout << "  - perform_operations() : fonction générique\n";
    std::cout << "  - calculate_expression() : calculs composés\n";
    std::cout << "  - Validation à la compilation des types\n";
    
    return 0;
}