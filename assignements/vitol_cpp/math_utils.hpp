/*
 * math_utils.hpp - Utilitaires mathématiques haute performance
 * 
 * Ce fichier contient les fonctions mathématiques optimisées pour la finance quantitative.
 * L'objectif : calculer très rapidement les fonctions nécessaires au pricing d'options.
 */

#pragma once  // Évite l'inclusion multiple

#include <cmath>       // Pour sqrt, log, exp, abs
#include <numbers>     // Pour les constantes mathématiques (C++20)
#include <span>        // Pour manipuler des tableaux de façon sûre (C++20)
#include <algorithm>   // Pour std::transform
#include <execution>   // Pour les algorithmes parallèles (pas utilisé ici)

// ===== CLASSE D'UTILITAIRES MATHÉMATIQUES RAPIDES =====
/*
 * FastMath = une "boîte à outils" de fonctions mathématiques optimisées
 * Tout est "static" = on n'a pas besoin de créer un objet pour les utiliser
 */
class FastMath {
public:
    /*
     * CONSTANTES MATHÉMATIQUES PRÉ-CALCULÉES
     * =======================================
     * On les calcule une fois au moment de la compilation, pas à l'exécution
     * Cela rend les calculs plus rapides
     */
    
    // sqrt(2*π) ≈ 2.506628... (racine carrée de 2 fois pi)
    static constexpr double SQRT_2PI = 2.506628274631000502415765284811;
    
    // 1/sqrt(2*π) ≈ 0.398942... (inverse de la racine carrée de 2 fois pi)
    // Utilisé dans la fonction de densité normale
    static constexpr double INV_SQRT_2PI = 0.3989422804014326779399460599344;
    
    /*
     * constexpr = calculé au moment de la compilation
     * static = appartient à la classe, pas à une instance
     * Résultat : ces valeurs sont "gravées" dans le programme compilé
     */

    /*
     * FONCTION DE RÉPARTITION NORMALE (CDF)
     * ====================================== 
     * Cette fonction calcule P(X ≤ x) où X suit une loi normale standard
     * C'est ESSENTIEL pour le modèle Black-Scholes
     */
    [[nodiscard]] static double norm_cdf(double x) noexcept {
        /*
         * [[nodiscard]] = "n'ignorez pas le résultat de cette fonction!"
         * noexcept = "cette fonction ne lève jamais d'exception"
         */
        
        // Cas extrêmes : si x est très négatif ou très positif
        if (x < -8.0) return 0.0;  // Probabilité ≈ 0 pour x très négatif
        if (x > 8.0) return 1.0;   // Probabilité ≈ 1 pour x très positif
        
        /*
         * APPROXIMATION D'ABRAMOWITZ & STEGUN
         * ===================================
         * Au lieu d'utiliser une intégrale compliquée, on utilise une approximation
         * très précise mais beaucoup plus rapide à calculer
         */
        
        // Coefficients magiques de l'approximation (déterminés par des mathématiciens)
        const double a1 = 0.254829592;
        const double a2 = -0.284496736;
        const double a3 = 1.421413741;
        const double a4 = -1.453152027;
        const double a5 = 1.061405429;
        const double p = 0.3275911;
        
        // Gestion du signe (la fonction normale est symétrique)
        const double sign = (x >= 0) ? 1.0 : -1.0;
        
        // Transformation pour l'approximation
        x = std::abs(x) / std::numbers::sqrt2;  // sqrt2 = √2 (constante C++20)
        
        // Calcul de l'approximation (formule polynomiale)
        const double t = 1.0 / (1.0 + p * x);
        const double y = 1.0 - (((((a5 * t + a4) * t) + a3) * t + a2) * t + a1) * t * std::exp(-x * x);
        
        // Résultat final
        return 0.5 * (1.0 + sign * y);
        
        /*
         * Pourquoi cette approximation ?
         * - Précision : erreur < 1.5 × 10^-7
         * - Vitesse : ~10x plus rapide que l'intégrale exacte
         * - Stabilité numérique : pas de problème avec les nombres très grands/petits
         */
    }

    /*
     * FONCTION DE DENSITÉ NORMALE (PDF)
     * ==================================
     * Cette fonction calcule la "hauteur" de la courbe en cloche au point x
     * Utilisée pour calculer les "Greeks" (sensibilités) des options
     */
    [[nodiscard]] static double norm_pdf(double x) noexcept {
        /*
         * Formule : (1/√(2π)) × e^(-x²/2)
         * 
         * On utilise notre constante pré-calculée INV_SQRT_2PI
         * au lieu de recalculer 1/√(2π) à chaque fois
         */
        return std::exp(-0.5 * x * x) * INV_SQRT_2PI;
        
        /*
         * std::exp(-0.5 * x * x) = e^(-x²/2)
         * INV_SQRT_2PI = 1/√(2π)
         */
    }

    /*
     * TRAITEMENT PAR LOTS (BATCH PROCESSING)
     * ======================================
     * Calcule la CDF pour tout un tableau de valeurs d'un coup
     * Plus efficace que de faire une boucle manuelle
     */
    static void norm_cdf_batch(std::span<const double> inputs, std::span<double> outputs) {
        /*
         * std::span = "vue" sur un tableau (C++20)
         * - Plus sûr que les pointeurs bruts
         * - Connaît sa taille
         * - const double = on ne modifie pas les entrées
         */
        
        /*
         * std::transform = applique une fonction à chaque élément
         * inputs.begin() à inputs.end() = tous les éléments d'entrée
         * outputs.begin() = où mettre les résultats
         * norm_cdf = la fonction à appliquer à chaque élément
         */
        std::transform(inputs.begin(), inputs.end(), outputs.begin(), norm_cdf);
        
        /*
         * Exemple d'utilisation :
         * std::vector<double> x_values = {-1.0, 0.0, 1.0, 2.0};
         * std::vector<double> cdf_results(4);
         * norm_cdf_batch(x_values, cdf_results);
         * // cdf_results contient maintenant [0.159, 0.5, 0.841, 0.977]
         */
    }

    /*
     * HELPER POUR BLACK-SCHOLES : CALCUL DE D1 ET D2
     * ===============================================
     * Dans le modèle Black-Scholes, on a besoin de calculer deux valeurs
     * spéciales appelées d1 et d2. Cette fonction les calcule ensemble.
     */
    [[nodiscard]] static std::pair<double, double> black_scholes_d1_d2(
        double S,     // Prix actuel de l'actif (Spot price)
        double K,     // Prix d'exercice de l'option (Strike)  
        double T,     // Temps jusqu'à expiration (Time to maturity)
        double r,     // Taux sans risque (Risk-free rate)
        double vol    // Volatilité (Volatility)
    ) noexcept {
        
        // Cas dégénérés : si le temps ou la volatilité sont nuls/négatifs
        if (T <= 0 || vol <= 0) return {0.0, 0.0};
        
        /*
         * FORMULES DE BLACK-SCHOLES POUR D1 ET D2
         * =======================================
         */
        
        const double sqrt_T = std::sqrt(T);  // √T (racine carrée du temps)
        
        // d1 = [ln(S/K) + (r + σ²/2)×T] / (σ×√T)
        const double d1 = (std::log(S / K) + (r + 0.5 * vol * vol) * T) / (vol * sqrt_T);
        
        // d2 = d1 - σ×√T  
        const double d2 = d1 - vol * sqrt_T;
        
        /*
         * std::pair<double, double> = retourne deux valeurs ensemble
         * {d1, d2} = syntaxe moderne pour créer la paire
         */
        return {d1, d2};
        
        /*
         * Interprétation financière :
         * - d1 : liée à la probabilité que l'option finisse "dans la monnaie"
         * - d2 : d1 ajusté pour le risque
         * Ces valeurs sont ensuite passées dans norm_cdf() pour calculer le prix
         */
    }
};

/*
 * RÉSUMÉ DE CE FICHIER :
 * ======================
 * 
 * 1. CONSTANTES : sqrt(2π) et 1/sqrt(2π) pré-calculées
 * 2. NORM_CDF : Fonction de répartition normale (approximation rapide)
 * 3. NORM_PDF : Fonction de densité normale
 * 4. BATCH : Traitement de tableaux entiers
 * 5. D1_D2 : Calculs spécifiques à Black-Scholes
 * 
 * POURQUOI C'EST IMPORTANT :
 * - Vitesse : Optimisé pour des millions de calculs par seconde
 * - Précision : Approximations mathématiquement validées
 * - Sécurité : Types modernes (span) et attributs (noexcept)
 * 
 * USAGE TYPIQUE :
 * auto [d1, d2] = FastMath::black_scholes_d1_d2(100, 105, 0.25, 0.05, 0.20);
 * double price = spot * FastMath::norm_cdf(d1) - strike * exp(-rate*time) * FastMath::norm_cdf(d2);
 */