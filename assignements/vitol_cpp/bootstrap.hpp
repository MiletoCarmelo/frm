/*
 * bootstrap_es.hpp - Bootstrap simple pour Expected Shortfall
 * 
 * VOTRE LOGIQUE EN 3 ÉTAPES :
 * 1) Calculer l'ES original
 * 2) Bootstrap avec remise N fois  
 * 3) Distribution → CI 95%
 */

#pragma once
#include "monte_carlo.hpp"
#include <vector>
#include <random>
#include <algorithm>

struct BootstrapResult {
    double original_es;
    double ci_lower_95;  // 2.5% percentile
    double ci_upper_95;  // 97.5% percentile
};

class SimpleBootstrap {
private:
    MonteCarloEngine mc_;
    mutable std::mt19937 rng_;

public:
    SimpleBootstrap() : rng_(std::random_device{}()) {}
    
    BootstrapResult bootstrap_es(
        const std::vector<double>& returns, 
        double confidence = 0.95,
        size_t n_bootstrap = 1000) {
        
        // 1) ES original
        auto [var_orig, es_orig] = mc_.calculate_var_es(returns, confidence);
        
        // 2) Bootstrap process
        std::vector<double> bootstrap_es_values;
        std::uniform_int_distribution<size_t> dist(0, returns.size() - 1);
        
        for (size_t i = 0; i < n_bootstrap; ++i) {
            // Ré-échantillonnage avec remise
            std::vector<double> boot_sample;
            for (size_t j = 0; j < returns.size(); ++j) {
                boot_sample.push_back(returns[dist(rng_)]);
            }
            
            // ES sur cet échantillon
            auto [var_boot, es_boot] = mc_.calculate_var_es(boot_sample, confidence);
            bootstrap_es_values.push_back(es_boot);
        }
        
        // 3) CI 95% 
        std::sort(bootstrap_es_values.begin(), bootstrap_es_values.end());
        size_t idx_025 = static_cast<size_t>(0.025 * n_bootstrap);
        size_t idx_975 = static_cast<size_t>(0.975 * n_bootstrap);
        
        return {
            es_orig,
            bootstrap_es_values[idx_025],
            bootstrap_es_values[idx_975]
        };
    }
};

/*
 * USAGE SIMPLE :
 * 
 * SimpleBootstrap bootstrap;
 * auto result = bootstrap.bootstrap_es(my_returns, 0.95, 1000);
 * 
 * std::cout << "ES: " << result.original_es * 100 << "%\n";
 * std::cout << "CI 95%: [" << result.ci_lower_95 * 100 
 *           << "%, " << result.ci_upper_95 * 100 << "%]\n";
 */