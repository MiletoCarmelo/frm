/*
 * pricing_calculator.hpp - Calculateur de pricing d'options Monte Carlo
 */

#pragma once

#include "types.hpp"
#include "payoff_model.hpp"
#include "monte_carlo.hpp"
#include <chrono>

struct PricingMetrics {
    double option_value{0.0};
    size_t calculation_time_us{0};
    size_t monte_carlo_simulations{0};
};

class PricingCalculator {
private:
    MonteCarloEngine mc_engine_;
    
public:
    explicit PricingCalculator(uint64_t seed = std::random_device{}()) 
        : mc_engine_(seed) {}
    
    /*
     * PRICING MONTE CARLO PRINCIPAL
     */
    [[nodiscard]] expected<PricingMetrics, RiskError> calculate_option_price(
        OptionType option_type,
        double S,
        double K, 
        double T,
        double r,
        double vol,
        size_t n_simulations = 100'000
    ) const {
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Validation
        if (vol <= 0) return expected<PricingMetrics, RiskError>{RiskError::INVALID_VOLATILITY};
        if (T < 0) return expected<PricingMetrics, RiskError>{RiskError::NEGATIVE_TIME};
        if (K <= 0 || S <= 0) return expected<PricingMetrics, RiskError>{RiskError::INVALID_STRIKE};
        
        PricingMetrics metrics;
        metrics.monte_carlo_simulations = n_simulations;
        
        // Cas expiration
        if (T == 0) {
            metrics.option_value = PayoffModel::calculate_payoff(option_type, S, K);
            auto end_time = std::chrono::high_resolution_clock::now();
            metrics.calculation_time_us = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
            return expected<PricingMetrics, RiskError>{metrics};
        }
        
        // Simulation selon type d'option
        if (option_type == OptionType::EUROPEAN_CALL || option_type == OptionType::EUROPEAN_PUT) {
            calculate_european_option(S, K, T, r, vol, option_type, n_simulations, metrics);
        } else {
            calculate_path_dependent_option(S, K, T, r, vol, option_type, n_simulations, metrics);
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        metrics.calculation_time_us = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
        
        return expected<PricingMetrics, RiskError>{metrics};
    }    

private:
    void calculate_european_option(
        double S, double K, double T, double r, double vol,
        OptionType option_type, size_t n_simulations,
        PricingMetrics& metrics) const {
        
        // Simulation prix finaux seulement
        std::vector<double> final_prices(n_simulations);
        
        // Pour chaque simulation, on utilise la formule fermée GBM
        mc_engine_.simulate_final_prices(final_prices, S, r, vol, T);
        
        // Calcul payoffs
        double sum_payoffs = 0.0;
        for (const auto& S_final : final_prices) {
            sum_payoffs += PayoffModel::calculate_payoff(option_type, S_final, K);
        }
        
        const double mean_payoff = sum_payoffs / n_simulations;
        metrics.option_value = std::exp(-r * T) * mean_payoff;
    }
    
    void calculate_path_dependent_option(
        double S, double K, double T, double r, double vol,
        OptionType option_type, size_t n_simulations,
        PricingMetrics& metrics) const {
        
        // Simulation trajectoires complètes
        const size_t n_steps = static_cast<size_t>(T * 252);
        std::vector<double> paths(n_simulations * (n_steps + 1));
        mc_engine_.simulate_gbm_paths(paths, S, r, vol, T, n_steps, n_simulations);
        
        // Calcul payoffs avec trajectoires
        double sum_payoffs = 0.0;
        for (size_t i = 0; i < n_simulations; ++i) {
            const size_t start_idx = i * (n_steps + 1);
            std::span<const double> path_i(paths.data() + start_idx, n_steps + 1);
            const double S_final = paths[start_idx + n_steps];
            
            sum_payoffs += PayoffModel::calculate_payoff(option_type, S_final, K, path_i);
        }
        
        const double mean_payoff = sum_payoffs / n_simulations;
        metrics.option_value = std::exp(-r * T) * mean_payoff;
    }
};