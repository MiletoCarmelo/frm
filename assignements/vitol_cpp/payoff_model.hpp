/*
 * payoff_model.hpp - Classe pour calculs de payoffs d'options
 * 
 * Cette classe contient la logique métier pure pour calculer
 * les payoffs de différents types d'options.
 */

#pragma once

#include "types.hpp"
#include <span>
#include <algorithm>
#include <numeric>

// ===== TYPES D'OPTIONS SUPPORTÉES =====
enum class OptionType {
    EUROPEAN_CALL,
    EUROPEAN_PUT,
    ASIAN_CALL,
    ASIAN_PUT,
    BARRIER_CALL_KNOCKOUT,
    LOOKBACK_CALL,
    DIGITAL_CALL
};

// ===== CLASSE PAYOFF MODEL =====
/*
 * Classe statique pour calculer les payoffs d'options
 * Responsabilité unique : logique métier des payoffs
 */
class PayoffModel {
public:
    /*
     * FONCTION PRINCIPALE : CALCUL DE PAYOFF
     * ======================================
     * Interface unifiée pour tous types d'options
     */
    [[nodiscard]] static double calculate_payoff(
        OptionType option_type,
        double S_final,                           // Prix final (options européennes)
        double K,                                 // Strike price
        std::span<const double> price_path = {},  // Trajectoire (options path-dependent)
        double barrier = 0.0,                     // Barrière (options barrier)
        double payout_amount = 1.0                // Montant (options digitales)
    ) noexcept {
        
        switch (option_type) {
            
            case OptionType::EUROPEAN_CALL:
                return std::max(S_final - K, 0.0);
                
            case OptionType::EUROPEAN_PUT:
                return std::max(K - S_final, 0.0);
                
            case OptionType::ASIAN_CALL:
                return calculate_asian_call_payoff(price_path, K);
                
            case OptionType::ASIAN_PUT:
                return calculate_asian_put_payoff(price_path, K);
                
            case OptionType::BARRIER_CALL_KNOCKOUT:
                return calculate_barrier_knockout_payoff(price_path, S_final, K, barrier);
                
            case OptionType::LOOKBACK_CALL:
                return calculate_lookback_call_payoff(price_path, K);
                
            case OptionType::DIGITAL_CALL:
                return S_final > K ? payout_amount : 0.0;
                
            default:
                return 0.0;  // Type non supporté
        }
    }

private:
    /*
     * FONCTIONS PRIVÉES POUR PAYOFFS COMPLEXES
     * =========================================
     * Décomposition pour clarté et maintenabilité
     */
    
    [[nodiscard]] static double calculate_asian_call_payoff(
        std::span<const double> price_path, double K) noexcept {
        
        if (price_path.empty()) return 0.0;
        
        const double sum = std::accumulate(price_path.begin(), price_path.end(), 0.0);
        const double average_price = sum / price_path.size();
        
        return std::max(average_price - K, 0.0);
    }
    
    [[nodiscard]] static double calculate_asian_put_payoff(
        std::span<const double> price_path, double K) noexcept {
        
        if (price_path.empty()) return 0.0;
        
        const double sum = std::accumulate(price_path.begin(), price_path.end(), 0.0);
        const double average_price = sum / price_path.size();
        
        return std::max(K - average_price, 0.0);
    }
    
    [[nodiscard]] static double calculate_barrier_knockout_payoff(
        std::span<const double> price_path, double S_final, double K, double barrier) noexcept {
        
        if (price_path.empty()) return 0.0;
        
        // Vérifier si la barrière a été touchée
        for (const double price : price_path) {
            if (price <= barrier) {
                return 0.0;  // Option knocked out
            }
        }
        
        // Barrière pas touchée, payoff call normal
        return std::max(S_final - K, 0.0);
    }
    
    [[nodiscard]] static double calculate_lookback_call_payoff(
        std::span<const double> price_path, double K) noexcept {
        
        if (price_path.empty()) return 0.0;
        
        const double S_max = *std::max_element(price_path.begin(), price_path.end());
        return std::max(S_max - K, 0.0);
    }
};

/*
 * USAGE EXEMPLE :
 * ===============
 * 
 * // Call européenne simple
 * double eu_call = PayoffModel::calculate_payoff(
 *     OptionType::EUROPEAN_CALL, 
 *     110.0,  // S_final
 *     105.0   // K
 * );
 * // Résultat: 5.0
 * 
 * // Asian call avec trajectoire
 * std::vector<double> path = {100, 105, 110, 108, 112};
 * double asian_call = PayoffModel::calculate_payoff(
 *     OptionType::ASIAN_CALL,
 *     0.0,    // S_final ignoré pour Asian
 *     105.0,  // K
 *     path    // price_path nécessaire
 * );
 * // Résultat: max(107 - 105, 0) = 2.0
 * 
 * // Barrier call knockout
 * std::vector<double> path_barrier = {100, 105, 94, 110};  // Touche 94
 * double barrier_call = PayoffModel::calculate_payoff(
 *     OptionType::BARRIER_CALL_KNOCKOUT,
 *     110.0,  // S_final
 *     100.0,  // K
 *     path_barrier,  // price_path
 *     95.0    // barrier
 * );
 * // Résultat: 0.0 (knocked out car 94 < 95)
 * 
 * // Digital call
 * double digital = PayoffModel::calculate_payoff(
 *     OptionType::DIGITAL_CALL,
 *     110.0,  // S_final
 *     105.0,  // K
 *     {},     // price_path pas nécessaire
 *     0.0,    // barrier pas nécessaire
 *     1000.0  // payout_amount
 * );
 * // Résultat: 1000.0 (car 110 > 105)
 */