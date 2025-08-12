#include <iostream>
#include <iomanip>
#include <vector>
#include <chrono>
#include <string>

// Tes headers
#include "gnuplot_plotter.hpp"
#include "bootstrap.hpp"
#include "timeseries_simulator.hpp"

// g++ -std=c++20 -Wall -Wextra -Wpedantic -O3 -march=native -DNDEBUG -I. main_bsp.cpp  -o main_test

/*
 * DEMONSTRATION DU BOOTSTRAP POUR VAR/ES
 * ==========================================
 * Montre comment utiliser le bootstrap pour estimer la VaR et l'ES
 * avec des intervalles de confiance
 */
void demonstrate_bootstrap_var_es() {
    std::cout << "\n=== BOOTSTRAP VAR/ES DEMONSTRATION ===\n";

    // Générer des données test
    TimeSeriesSimulator sim;
    TimeSeriesParams params;
    params.n_periods = 1000;  // 4 ans de data
    auto prices = sim.generate_mean_reverting(params);
    auto returns = TimeSeriesSimulator::prices_to_returns(prices);

    // Tester votre bootstrap
    SimpleBootstrap bootstrap;
    auto result = bootstrap.bootstrap_var_es(BootstrapMethod::BLOCK, returns);

    // Afficher les résultats
    std::cout << "VaR (95%): " << result.original_var << "\n";
    std::cout << "ES (95%): " << result.original_es << "\n";

    std::cout << "95% CI for ES: [" << result.ci_lower_95 << ", " << result.ci_upper_95 << "]\n";
    std::cout << "Bootstrap ES values: ";
    // for (const auto& val : result.bootstrap_es_values) {
    //     std::cout << val << " ";
    // }
    std::cout << "\n";

    // Plotter pour visualiser les résultats
    GnuplotPlotter plotter("./plots/");
    
    GnuplotPlotter::RiskMetrics bootstrap_metrics;
    bootstrap_metrics.es = result.original_es;
    bootstrap_metrics.var = result.original_var;  // Pour référence
    bootstrap_metrics.es_ci_lower = result.ci_lower_95;
    bootstrap_metrics.es_ci_upper = result.ci_upper_95;
    bootstrap_metrics.has_es = true;
    bootstrap_metrics.has_var = true;
    bootstrap_metrics.has_es_ci = true;

    std::cout << "\n=== DEBUG METRICS ===\n";
    std::cout << "ES: " << bootstrap_metrics.es << " (will be plotted at " << bootstrap_metrics.es * 100 << ")\n";
    std::cout << "ES CI lower: " << bootstrap_metrics.es_ci_lower << " (will be plotted at " << bootstrap_metrics.es_ci_lower * 100 << ")\n";
    std::cout << "ES CI upper: " << bootstrap_metrics.es_ci_upper << " (will be plotted at " << bootstrap_metrics.es_ci_upper * 100 << ")\n";
    std::cout << "has_es: " << bootstrap_metrics.has_es << "\n";
    std::cout << "has_es_ci: " << bootstrap_metrics.has_es_ci << "\n";
    
    plotter.plot_histogram_with_risk_metrics(returns, bootstrap_metrics, 
                                             "bootstrap_risk_histogram");
    
}

int main() {
    std::cout << "=== MODERN C++ RISK ENGINE ===\n";
    std::cout << "Demonstrating C++20/23 features for bootstrap VAR/ES estimation\n";
    try {
        demonstrate_bootstrap_var_es();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;  // Code de retour d'erreur
    }

    return 0;
}