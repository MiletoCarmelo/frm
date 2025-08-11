#include <iostream> // Pour cout, cin, etc.
#include <iomanip> // Pour std::fixed, std::setprecision (formatage)
#include <vector> // Pour std::vector (tableaux dynamiques)
#include <chrono> // Pour mesurer le temps d'exécution
#include <random> // Pour générer des données de test aléatoires
#include <string> // Pour std::string
#include <concepts> // Pour les concepts C++20

// g++ -std=c++20 -Wall -Wextra -Wpedantic -O3 -march=native -DNDEBUG -I. main_test.cpp -o main_test
// ./main_test

template<typename T>
concept Numeric = std::is_arithmetic_v<T>;

// Correction 1: Le concept String doit avoir un paramètre template
template<typename T>
concept String = std::is_same_v<T, std::string>;

// Exemple 1: Fonction template contrainte par le concept
template<Numeric T, String U>
T add(T a, U b) {
    return a + std::stoi(b);
}

int main() {
    // Correction 2: Utiliser une vraie string au lieu de 'hello' (qui est un const char*)
    // et une string qui peut être convertie en nombre
    std::cout << " add(5, \"3\") = " << add(5, std::string("3")) << std::endl;
    
    // Exemples supplémentaires qui fonctionnent :
    std::cout << " add(10, \"25\") = " << add(10, std::string("25")) << std::endl;
    std::cout << " add(7.5, \"15\") = " << add(7.5, std::string("15")) << std::endl;
    
    // Exemple avec une variable string
    std::string number_str = "42";
    std::cout << " add(100, number_str) = " << add(100, number_str) << std::endl;
    
    return 0;
}