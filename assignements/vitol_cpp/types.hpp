/*
 * types.hpp - Types et concepts modernes pour le Risk Engine
 * 
 * Ce fichier définit les types de base et les règles que notre code doit respecter.
 * C'est comme définir le "vocabulaire" de notre programme financier.
 */

#pragma once  // Évite que ce fichier soit inclus plusieurs fois

#include <concepts>  // Pour les "concepts" C++20 (nouvelles règles de types)
#include <ranges>    // Pour manipuler des collections de données facilement
#include <string>    // Pour utiliser std::string

// ===== CONCEPTS C++20 - RÈGLES POUR LES TYPES =====
/*
 * Les "concepts" sont comme des règles que les types doivent respecter.
 * C'est nouveau en C++20 et ça rend le code plus sûr et plus clair.
 */

// Concept 1: Un type "Numeric" doit être un nombre (int, double, float, etc.)
template<typename T>
concept Numeric = std::is_arithmetic_v<T>;
/*
 * Exemple d'utilisation:
 * - int → OK, c'est numérique
 * - double → OK, c'est numérique  
 * - std::string → ERREUR, ce n'est pas numérique
 */

// Concept 2: Un "PricingModel" doit avoir certaines fonctions
template<typename T>
concept PricingModel = requires(T t, double s, double k, double t_exp, double r, double vol) {
    // Le modèle DOIT avoir une fonction "price" qui retourne un double
    { t.price(s, k, t_exp, r, vol) } -> std::convertible_to<double>;
    // Le modèle DOIT avoir une fonction "delta" qui retourne un double  
    { t.delta(s, k, t_exp, r, vol) } -> std::convertible_to<double>;
};
/*
 * Cela garantit que tout "PricingModel" peut calculer:
 * - Un prix d'option
 * - Le Delta (sensibilité au prix de l'actif sous-jacent)
 * 
 * Si quelqu'un crée un modèle sans ces fonctions → ERREUR de compilation
 */

// Concept 3: Un conteneur qui permet l'accès rapide et connaît sa taille
template<typename Container>
concept RandomAccessRange = std::ranges::random_access_range<Container> && 
                           std::ranges::sized_range<Container>;
/*
 * Exemples qui marchent:
 * - std::vector<double> → OK
 * - std::array<int, 100> → OK
 * 
 * Exemples qui ne marchent pas:
 * - std::list<double> → ERREUR (pas d'accès rapide)
 */

// ===== GESTION D'ERREURS MODERNE =====
/*
 * Au lieu d'utiliser des exceptions (qui peuvent être lentes),
 * on utilise "expected" - un type qui contient SOIT un résultat SOIT une erreur.
 */

// Implémentation basique de std::expected (qui arrivera en C++23)
template<typename T, typename E>
class expected {
private:
    // Union = un espace mémoire qui peut contenir SOIT value_ SOIT error_
    union {
        T value_;  // Le résultat si tout va bien
        E error_;  // L'erreur si quelque chose ne va pas
    };
    bool has_value_;  // true = on a un résultat, false = on a une erreur

public:
    // Constructeur quand tout va bien - on stocke la valeur
    expected(const T& value) : value_(value), has_value_(true) {}
    
    // Constructeur quand il y a une erreur - on stocke l'erreur
    expected(const E& error) : error_(error), has_value_(false) {}
    
    // Fonction pour vérifier si on a un résultat ou une erreur
    bool has_value() const noexcept { return has_value_; }
    
    // Récupérer la valeur (attention: seulement si has_value() == true!)
    const T& value() const noexcept { return value_; }
    
    // Récupérer l'erreur (attention: seulement si has_value() == false!)
    const E& error() const noexcept { return error_; }
    
    // Destructeur - nettoie correctement la mémoire
    ~expected() {
        if (has_value_) {
            value_.~T();  // Détruit la valeur
        } else {
            error_.~E();  // Détruit l'erreur
        }
    }
    
    // On empêche la copie pour simplifier (en production, on l'implémenterait)
    expected(const expected&) = delete;
    expected& operator=(const expected&) = delete;
};

/*
 * Exemple d'utilisation:
 * 
 * expected<double, RiskError> result = calculate_price(100, 105, 0.25);
 * 
 * if (result.has_value()) {
 *     std::cout << "Prix: " << result.value() << std::endl;
 * } else {
 *     std::cout << "Erreur: " << (int)result.error() << std::endl;
 * }
 */

// Types d'erreurs possibles dans notre système de risque
enum class RiskError {
    INVALID_VOLATILITY,     // Volatilité négative ou nulle
    NEGATIVE_TIME,          // Temps jusqu'à expiration négatif
    INVALID_STRIKE,         // Prix d'exercice invalide
    COMPUTATION_FAILED,     // Échec de calcul général
    MISSING_MARKET_DATA     // Données de marché manquantes
};
/*
 * enum class = énumération moderne (C++11+)
 * Plus sûre que les enum classiques car les valeurs sont dans un namespace séparé
 */

// ===== STRUCTURE D'UNE POSITION FINANCIÈRE =====
/*
 * Une "Position" représente un contrat financier (option, forward, etc.)
 * que notre trader possède dans son portefeuille.
 */
struct Position {
    std::string instrument_id;  // Identifiant unique ("CALL_WTI_123")
    std::string underlying;     // Actif sous-jacent ("WTI", "BRENT", "NATGAS")
    double notional;           // Montant en $ ou nombre de contrats
    double strike;             // Prix d'exercice de l'option
    double maturity;           // Temps jusqu'à expiration (en années)
    bool is_call;             // true = Call option, false = Put option
    
    /*
     * Opérateur de comparaison automatique (C++20)
     * Permet de comparer deux positions facilement
     */
    auto operator<=>(const Position&) const = default;
    /*
     * Avec ça, on peut faire:
     * Position pos1, pos2;
     * if (pos1 == pos2) { ... }    // Égalité
     * if (pos1 < pos2) { ... }     // Comparaison
     */
    
    /*
     * [[nodiscard]] = attribut C++17 qui dit "n'ignorez pas le résultat!"
     * Si on appelle cette fonction et qu'on ignore le résultat,
     * le compilateur nous donne un avertissement.
     */
    [[nodiscard]] bool is_valid() const noexcept {
        return !underlying.empty() &&    // Doit avoir un sous-jacent
               notional != 0.0 &&        // Le montant ne peut pas être zéro
               strike > 0.0 &&           // Prix d'exercice doit être positif
               maturity >= 0.0;          // Maturité ne peut pas être négative
    }
    /*
     * noexcept = promet que cette fonction ne lance jamais d'exception
     * Cela aide le compilateur à optimiser le code
     */
};

/*
 * Exemple d'utilisation d'une Position:
 * 
 * Position my_option{
 *     .instrument_id = "CALL_WTI_2025_80",
 *     .underlying = "WTI",
 *     .notional = 1000000.0,  // 1 million de dollars
 *     .strike = 80.0,         // Prix d'exercice à 80$/baril
 *     .maturity = 0.25,       // 3 mois (0.25 année)
 *     .is_call = true         // Option d'achat
 * };
 * 
 * if (my_option.is_valid()) {
 *     std::cout << "Position valide!" << std::endl;
 * }
 */