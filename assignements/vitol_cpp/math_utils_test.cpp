#include <cmath>
#include <numbers>
#include <span>
#include <algorithm> 
#include <execution> 
#include <iostream>

class FastMath {
public:
    static constexpr double SQRT_2PI = 2.506628274631000502415765284811;
    static constexpr double INV_SQRT_2PI = 0.3989422804014326779399460599344;
 
    [[nodiscard]] static double norm_cdf(double x) noexcept {
        /*
         * Fonction de répartition normale (CDF)
         * => Retourne la probabilité que X soit inférieur ou égal à x
         * 
         * Formule : P(X ≤ x) = 0.5 × [1 + erf(x / √2)]
         * => erf est la fonction d'erreur
         *
         * mais on approximera erf avec une série de Taylor
         * => erf(x) ≈ 1 - (1 / (1 + p * x²)) * (a1 + a2 * x + a3 * x² + a4 * x³ + a5 * x⁴) * e^(-x²)
         * 
         */

        if (x < -8.0) return 0.0;
        if (x > 8.0) return 1.0; 
        
        const double a1 = 0.254829592;
        const double a2 = -0.284496736;
        const double a3 = 1.421413741;
        const double a4 = -1.453152027;
        const double a5 = 1.061405429;
        const double p = 0.3275911;
        
        const double sign = (x >= 0) ? 1.0 : -1.0;
        
        x = std::abs(x) / std::numbers::sqrt2;  // sqrt2 = √2 (constante C++20)

        const double t = 1.0 / (1.0 + p * x); 
        const double y = 1.0 - (((((a5 * t + a4) * t) + a3) * t + a2) * t + a1) * t * std::exp(-x * x);

        // Résultat final
        return 0.5 * (1.0 + sign * y);
    }

    [[nodiscard]] static double norm_pdf(double x) noexcept {
        /*
         * Fonction de densité normale (PDF)
         * => Retourne la "hauteur" de la courbe en cloche au point x
         * 
         * Formule : (1/√(2π)) × e^(-x²/2)
         * 
         * On utilise notre constante pré-calculée INV_SQRT_2PI
         * au lieu de recalculer 1/√(2π) à chaque fois
         */
        return std::exp(-0.5 * x * x) * INV_SQRT_2PI;
    }

    
    static void norm_cdf_batch(std::span<const double> inputs, std::span<double> outputs) {        
        /*
         * std::transform = applique une fonction à chaque élément
         * inputs.begin() à inputs.end() = tous les éléments d'entrée
         * outputs.begin() = où mettre les résultats
         * norm_cdf = la fonction à appliquer à chaque élément
         */
        std::transform(inputs.begin(), inputs.end(), outputs.begin(), norm_cdf);
    }

    static void norm_pdf_batch(std::span<const double> inputs, std::span<double> outputs) {
        /*
         * Même principe que pour norm_cdf_batch
         */
        std::transform(inputs.begin(), inputs.end(), outputs.begin(), norm_pdf);
    }


    [[nodiscard]] static std::pair<double, double> black_scholes_d1_d2(
        double S,     // Prix actuel de l'actif (Spot price)
        double K,     // Prix d'exercice de l'option (Strike)  
        double T,     // Temps jusqu'à expiration (Time to maturity)
        double r,     // Taux sans risque (Risk-free rate)
        double vol    // Volatilité (Volatility)
    ) noexcept {
        
        if (T <= 0 || vol <= 0) return {0.0, 0.0};
        
        /*
         * FORMULES DE BLACK-SCHOLES POUR D1 ET D2
         * =======================================
         * 
         * sqrt_T = √T
         * d1 = [ln(S/K) + (r + σ²/2)×T] / (σ×sqrt_T)
         * d2 = d1 - σ×sqrt_T
         */
        const double sqrt_T = std::sqrt(T); 
        const double d1 = (std::log(S / K) + (r + 0.5 * vol * vol) * T) / (vol * sqrt_T);
        const double d2 = d1 - vol * sqrt_T;
        return {d1, d2};
    
    }
};


int main() {
    double x =0.95;
    double cdf = FastMath::norm_cdf(x);
    double pdf = FastMath::norm_pdf(x);

    std::cout << "CDF(" << x << ") = " << cdf << std::endl;
    std::cout << "PDF(" << x << ") = " << pdf << std::endl;

    double S = 100.0;  // Prix actuel de l'actif
    double K = 100.0;  // Prix d'exercice de l'option
    double T = 1.0;    // Temps jusqu'à expiration (1 an)
    double r = 0.05;   // Taux sans risque (5%)
    double vol = 0.2;  // Volatilité (20%)

    auto [d1, d2] = FastMath::black_scholes_d1_d2(S, K, T, r, vol);

    std::cout << "d1 = " << d1 << std::endl;
    std::cout << "d2 = " << d2 << std::endl;

    return 0;
}