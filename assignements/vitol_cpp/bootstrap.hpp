/*
 * bootstrap_es.hpp - Bootstrap simple pour Expected Shortfall (VERSION CORRIGÉE)
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
    double original_var;  // VaR originale pour référence
    double ci_lower_95;  // 2.5% percentile
    double ci_upper_95;  // 97.5% percentile
    std::vector<double> bootstrap_es_values;  // Pour analyse ultérieure
};

// ===== TYPES DE BOOTSTRAP METHODS =====
enum class BootstrapMethod {
    CLASSIC,      // Point par point avec remise
    BLOCK,        // Blocs de taille fixe
    STATIONARY    // Blocs de taille variable (avancé)
};

class SimpleBootstrap {
private:
    MonteCarloEngine mc_;
    mutable std::mt19937 rng_;

public:
    explicit SimpleBootstrap() : rng_(std::random_device{}()) {}
    
    BootstrapResult bootstrap_var_es(
        BootstrapMethod bp_method,
        const std::vector<double>& returns, 
        double confidence = 0.95,
        size_t n_bootstrap = 1000, 
        size_t block_size = 20  // 20 jours par défaut
        ) {
        
        // 1) ES original
        auto [var_orig, es_orig] = mc_.calculate_var_es(returns, confidence);
        
        // 2) Bootstrap process
        std::vector<double> bootstrap_es_values;
        std::uniform_int_distribution<size_t> dist(0, returns.size() - 1);
        
        for (size_t i = 0; i < n_bootstrap; ++i) {
            std::vector<double> boot_sample;
            
            switch (bp_method) {
                case BootstrapMethod::CLASSIC: {
                    // Bootstrap classique : échantillonnage avec remise
                    for (size_t j = 0; j < returns.size(); ++j) {  // CORRECTION: j au lieu de i
                        boot_sample.push_back(returns[dist(rng_)]);
                    }
                    break;
                }
                
                case BootstrapMethod::BLOCK: {
                    // Bootstrap par blocs : préserve l'autocorrélation
                    while (boot_sample.size() < returns.size()) {
                        size_t start_index = dist(rng_);
                        
                        // Copie un bloc complet
                        for (size_t k = 0; k < block_size && boot_sample.size() < returns.size(); ++k) {
                            size_t idx = (start_index + k) % returns.size();
                            boot_sample.push_back(returns[idx]);
                        }
                    }
                    break;
                }
                
                case BootstrapMethod::STATIONARY: {
                    // Bootstrap stationnaire : blocs de longueur géométrique
                    while (boot_sample.size() < returns.size()) {
                        size_t start_index = dist(rng_);
                        
                        // Longueur géométrique (moyenne ≈ 20)
                        size_t block_length = generate_geometric_length(0.05);  // 5% prob d'arrêt
                        
                        for (size_t k = 0; k < block_length && boot_sample.size() < returns.size(); ++k) {
                            size_t idx = (start_index + k) % returns.size();
                            boot_sample.push_back(returns[idx]);
                        }
                    }
                    break;
                }
            }  // CORRECTION: Fermeture du switch manquait
            
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
            var_orig,
            bootstrap_es_values[idx_025],
            bootstrap_es_values[idx_975],
            bootstrap_es_values
        };
    }

private:
    // Helper pour le bootstrap stationnaire
    size_t generate_geometric_length(double p) {
        std::uniform_real_distribution<double> uniform(0.0, 1.0);
        size_t length = 1;
        while (uniform(rng_) > p) {  // Continue avec proba (1-p)
            ++length;
        }
        return length;
    }
};

/*
 * USAGE RECOMMANDÉ POUR VITOL :
 * 
 * SimpleBootstrap bootstrap;
 * 
 * // Pour commodités (volatility clustering) → BLOCK
 * auto result_block = bootstrap.bootstrap_var_es(
 *     BootstrapMethod::BLOCK, wti_returns, 0.95, 1000, 20);
 * 
 * // Pour comparaison → CLASSIC  
 * auto result_classic = bootstrap.bootstrap_var_es(
 *     BootstrapMethod::CLASSIC, wti_returns, 0.95, 1000);
 * 
 * std::cout << "ES Block: " << result_block.original_es * 100 << "%\n";
 * std::cout << "CI Block 95%: [" << result_block.ci_lower_95 * 100 
 *           << "%, " << result_block.ci_upper_95 * 100 << "%]\n";
 */