#include "types.hpp"
#include "math_utils.hpp" 
#include <unordered_map> 
#include <string> 
#include <cmath> 
#include <iostream>


class BlackScholesModel {
private:

    mutable std::unordered_map<std::string, double> cache_;

    [[nodiscard]] std::string make_cache_key(double S, double K, double T, double r, double vol) const {
        return std::to_string(S) + "_" + std::to_string(K) + "_" + 
               std::to_string(T) + "_" + std::to_string(r) + "_" + std::to_string(vol);
    }
    
public:
   
    // Nettoyage du cache
    void clear_cache() { cache_.clear(); }
    
    /*
     * CALCUL DU PRIX D'UNE OPTION (Call ou Put)
     * ========================================
     * 
     * CALL : C = S×N(d1) - K×e^(-rT)×N(d2)
     * PUT  : P = K×e^(-rT)×N(-d2) - S×N(-d1)
     * 
     */
    [[nodiscard]] expected<double, RiskError> price(
        double S,
        double K,
        double T,
        double r,
        double vol,
        bool is_call = true
    ) const {
      
        if (vol <= 0) return expected<double, RiskError>{RiskError::INVALID_VOLATILITY};
        if (T < 0) return expected<double, RiskError>{RiskError::NEGATIVE_TIME};
        if (K <= 0 || S <= 0) return expected<double, RiskError>{RiskError::INVALID_STRIKE};
        
        if (T == 0) {
            return expected<double, RiskError>{
                is_call ? std::max(S - K, 0.0) : std::max(K - S, 0.0)
            };
        }
        
        // Vérification du cache 
        const auto cache_key = make_cache_key(S, K, T, r, vol);
        if (auto it = cache_.find(cache_key); it != cache_.end()) {
            return expected<double, RiskError>{it->second};
        }
        
        // Calcul de d1 et d2
        const auto [d1, d2] = FastMath::black_scholes_d1_d2(S, K, T, r, vol);
       
        // Calcul du prix de l'option
        const double price = is_call 
            ? S * FastMath::norm_cdf(d1) - K * std::exp(-r * T) * FastMath::norm_cdf(d2)
            : K * std::exp(-r * T) * FastMath::norm_cdf(-d2) - S * FastMath::norm_cdf(-d1);

        // Stockage en cache pour les prochaines fois
        cache_[cache_key] = price; 

        // Retourne le résultat encapsulé dans expected<>
        return expected<double, RiskError>{price}; 
    }
    
    /*
     * CALCUL DU DELTA
     * ===============
     * Delta = sensibilité du prix de l'option au prix de l'actif sous-jacent
     * "Si le sous-jacent monte de 1$, l'option monte de Delta$"
     * 
     * FORMULE :
     * - Call Delta = N(d1)
     * - Put Delta = N(d1) - 1
     */
    [[nodiscard]] double delta(
        double S, 
        double K, 
        double T, 
        double r, 
        double vol, 
        bool is_call = true
    ) const {

        if (T <= 0 || vol <= 0) {
            return is_call ? (S > K ? 1.0 : 0.0) : (S < K ? -1.0 : 0.0);
        }

        // Calcul de d1 et d2
        const auto [d1, d2] = FastMath::black_scholes_d1_d2(S, K, T, r, vol);

        // Retourne le Delta selon le type d'option
        return is_call ? FastMath::norm_cdf(d1) : FastMath::norm_cdf(d1) - 1.0;
    }
    
    /*
     * CALCUL DU GAMMA
     * ===============
     * Gamma = sensibilité du Delta au prix du sous-jacent
     * "Comment le Delta change quand le prix change"
     * 
     * FORMULE : Γ = N(d1) / (S × σ × √T)
     */
    [[nodiscard]] double gamma(
        double S, 
        double K, 
        double T, 
        double r, 
        double vol
    ) const {

        if (T <= 0 || vol <= 0) return 0.0;
        
        // Calcul de d1 et d2
        const auto [d1, d2] = FastMath::black_scholes_d1_d2(S, K, T, r, vol);

        // Retourne le Gamma
        return FastMath::norm_pdf(d1) / (S * vol * std::sqrt(T));

    }
    
    /*
     * CALCUL DU VEGA
     * ==============
     * Vega = sensibilité du prix à la volatilité
     * "Si la volatilité monte de 1%, l'option monte de Vega$"
     * 
     * FORMULE : ν = S × N(d1) × √T / 100
     */
    [[nodiscard]] double vega(
        double S, 
        double K, 
        double T, 
        double r, 
        double vol
    ) const {

        if (T <= 0 || vol <= 0) return 0.0; 
        
        const auto [d1, d2] = FastMath::black_scholes_d1_d2(S, K, T, r, vol);
        return S * FastMath::norm_pdf(d1) * std::sqrt(T) / 100.0; // Per 1% vol change
    }
    
    /*
     * CALCUL DU THETA
     * ===============
     * Theta = sensibilité du prix au passage du temps
     * "Combien l'option perd de valeur chaque jour"
     * 
     * FORMULE :
     * - term1 = -S × N(d1) × σ / (2√T)
     * - term2 = r × K × e^(-rT)
     * 
     * - Call : θ = [term1 - term2 × N(d2)] / 365
     * - Put  : θ = [term1 + term2 × N(-d2)] / 365
     */
    [[nodiscard]] double theta(double S, double K, double T, double r, double vol, bool is_call = true) const {
        if (T <= 0 || vol <= 0) return 0.0;  // Plus de décroissance temporelle
        
        const auto [d1, d2] = FastMath::black_scholes_d1_d2(S, K, T, r, vol);
        const double sqrt_T = std::sqrt(T);
        
        // Composants de la formule Theta
        const double term1 = -S * FastMath::norm_pdf(d1) * vol / (2 * sqrt_T);
        const double term2 = r * K * std::exp(-r * T);
        
        if (is_call) {
            return (term1 - term2 * FastMath::norm_cdf(d2)) / 365.0; // Per day
        } else {
            return (term1 + term2 * FastMath::norm_cdf(-d2)) / 365.0; // Per day
        }
    }
    /*
     * STRUCTURE POUR GROUPER TOUS LES GREEKS
     * ======================================
     * Plus efficace de calculer tout d'un coup que un par un
     */
    struct Greeks {
        double delta;   // Sensibilité au prix
        double gamma;   // Sensibilité du delta
        double vega;    // Sensibilité à la volatilité
        double theta;   // Décroissance temporelle
    };
    
    /*
     * CALCUL OPTIMISÉ DE TOUS LES GREEKS
     * ===================================
     * Calcule Delta, Gamma, Vega, Theta en une seule passe
     * Plus efficace car on réutilise d1, d2, norm_pdf(d1), etc.
     */
    [[nodiscard]] Greeks calculate_all_greeks(
        double S, 
        double K, 
        double T, 
        double r, 
        double vol, 
        bool is_call = true
    ) const {
        
        if (T <= 0 || vol <= 0) {
            return {
                .delta = is_call ? (S > K ? 1.0 : 0.0) : (S < K ? -1.0 : 0.0),
                .gamma = 0.0,
                .vega = 0.0,
                .theta = 0.0
            };
        }
        
        const auto [d1, d2] = FastMath::black_scholes_d1_d2(S, K, T, r, vol);
        const double sqrt_T = std::sqrt(T);
        const double pdf_d1 = FastMath::norm_pdf(d1);  // Réutilisé pour Gamma, Vega, Theta
        
        const double delta_val = is_call ? FastMath::norm_cdf(d1) : FastMath::norm_cdf(d1) - 1.0;
        const double gamma_val = pdf_d1 / (S * vol * sqrt_T);
        const double vega_val = S * pdf_d1 * sqrt_T / 100.0;
        
        const double term1 = -S * pdf_d1 * vol / (2 * sqrt_T);
        const double term2 = r * K * std::exp(-r * T);
        const double theta_val = is_call 
            ? (term1 - term2 * FastMath::norm_cdf(d2)) / 365.0
            : (term1 + term2 * FastMath::norm_cdf(-d2)) / 365.0;
        
        return {
            .delta = delta_val,
            .gamma = gamma_val,
            .vega = vega_val,
            .theta = theta_val
        };
    }
};


int main() {

    double S = 100.0;  // Prix actuel de l'actif
    double K = 100.0;  // Prix d'exercice de l'option
    double T = 1.0;    // Temps jusqu'à expiration (1 an)
    double r = 0.05;   // Taux sans risque (5%)
    double vol = 0.2;  // Volatilité (20%)

    std::cout << "Informations sur l'option :" << std::endl;
    std::cout << " => Prix Spot (S) : " << S << std::endl;
    std::cout << " => Prix d'Exercice (K) : " << K << std::endl;
    std::cout << " => Temps jusqu'à expiration (T) : " << T << " ans" << std::endl;
    std::cout << " => Taux sans risque (r) : " << r * 100 << "%" << std::endl;
    std::cout << " => Volatilité (σ) : " << vol * 100 << "%" << std::endl;

    std::cout << "Calcul des greeks pour l'option Call :" << std::endl;
    std::cout << " => Delta : " << BlackScholesModel().delta(S, K, T, r, vol, true) << std::endl;
    std::cout << " => Gamma : " << BlackScholesModel().gamma(S, K, T, r, vol) << std::endl;
    std::cout << " => Vega  : " << BlackScholesModel().vega(S, K, T, r, vol) << std::endl;
    std::cout << " => Theta : " << BlackScholesModel().theta(S, K, T, r, vol, true) << std::endl;

    auto [d1, d2] = FastMath::black_scholes_d1_d2(S, K, T, r, vol);

    std::cout << "Calcul de d1 et d2 :" << std::endl;
    std::cout << " => d1 = " << d1 << std::endl;
    std::cout << " => d2 = " << d2 << std::endl;

    BlackScholesModel model;
    auto price = model.price(S, K, T, r, vol, true);
    std::cout << "Calcul du prix de l'option Call :" << std::endl;
    if (price.has_value()) {
        std::cout << " => Prix de l'option Call : " << price.value() << std::endl;
    } else {
        std::cerr << " => Erreur lors du calcul du prix : " << static_cast<int>(price.error()) << std::endl;
    }

    return 0;
}