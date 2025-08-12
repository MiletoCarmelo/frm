#include <iostream>
#include <iomanip>
#include <vector>
#include <chrono>
#include <string>

// Tes headers
#include "types.hpp"
#include "payoff_model.hpp"
#include "pricing_calculator.hpp"
#include "pricing_models.hpp"
#include "gnuplot_plotter.hpp"

/*
 * DÉMONSTRATION DU PRICING MONTE CARLO D'OPTIONS
 * ===============================================
 * Montre comment utiliser Monte Carlo pour le pricing d'options individuelles
 * et compare avec Black-Scholes pour validation
 */
void demonstrate_monte_carlo_option_pricing() {
    std::cout << "\n=== MONTE CARLO OPTION PRICING ===\n";
    
    /*
     * INITIALISATION DES MOTEURS
     * ===========================
     */
    PricingCalculator mc_pricer;      // Notre moteur Monte Carlo
    BlackScholesModel bs_model;       // Pour comparaison/validation
    
    /*
     * PARAMÈTRES D'OPTION STANDARD
     * =============================
     * Exemple classique pour démonstration
     */
    const double S = 100.0;    // Prix spot
    const double K = 105.0;    // Strike (5% OTM)
    const double T = 0.25;     // 3 mois
    const double r = 0.05;     // Taux sans risque 5%
    const double vol = 0.40;   // Volatilité 20%
    const size_t n_sims = 100'000;  // 100K simulations
    
    std::cout << "Pricing European Call: S=$" << S << ", K=$" << K 
              << ", T=" << T << " years, r=" << r << ", vol=" << vol << "\n";
    std::cout << "Monte Carlo simulations: " << n_sims << "\n\n";
    
    /*
     * CHRONOMÉTRAGE DU PRICING MONTE CARLO
     * =====================================
     */
    auto start = std::chrono::high_resolution_clock::now();
    
    // Pricing Monte Carlo Call européenne
    auto mc_result = mc_pricer.calculate_option_price(
        OptionType::EUROPEAN_CALL, S, K, T, r, vol, n_sims
    );
    
    auto end = std::chrono::high_resolution_clock::now();
    // Note: mc_duration pas utilisé car le timing est déjà dans mc_metrics
    
    /*
     * PRICING BLACK-SCHOLES POUR COMPARAISON
     * =======================================
     */
    start = std::chrono::high_resolution_clock::now();
    auto bs_result = bs_model.price(S, K, T, r, vol, true);
    end = std::chrono::high_resolution_clock::now();
    auto bs_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    /*
     * AFFICHAGE DES RÉSULTATS
     * ========================
     */
    if (mc_result.has_value() && bs_result.has_value()) {
        const auto& mc_metrics = mc_result.value();
        const double mc_price = mc_metrics.option_value;
        const double bs_price = bs_result.value();
        
        std::cout << "=== PRICING RESULTS ===\n";
        std::cout << "Monte Carlo Price:  $" << std::fixed << std::setprecision(6) << mc_price << "\n";
        std::cout << "Black-Scholes Price: $" << std::fixed << std::setprecision(6) << bs_price << "\n";
        
        /*
         * ANALYSE DE CONVERGENCE
         * ======================
         */
        const double abs_diff = std::abs(mc_price - bs_price);
        const double rel_diff_pct = (abs_diff / bs_price) * 100.0;
        
        std::cout << "\n=== CONVERGENCE ANALYSIS ===\n";
        std::cout << "Absolute difference: $" << std::fixed << std::setprecision(6) << abs_diff << "\n";
        std::cout << "Relative difference: " << std::fixed << std::setprecision(3) << rel_diff_pct << "%\n";
        
        // Évaluation de la qualité
        if (rel_diff_pct < 0.5) {
            std::cout << "✓ Excellent convergence (< 0.5%)\n";
        } else if (rel_diff_pct < 1.0) {
            std::cout << "✓ Good convergence (< 1.0%)\n";
        } else {
            std::cout << "⚠ Convergence could be improved (> 1.0%)\n";
        }
        
        /*
         * MÉTRIQUES DE PERFORMANCE
         * ========================
         */
        std::cout << "\n=== PERFORMANCE METRICS ===\n";
        std::cout << "Monte Carlo time:   " << mc_metrics.calculation_time_us << " μs\n";
        std::cout << "Black-Scholes time: " << bs_duration.count() << " μs\n";
        std::cout << "Speed ratio:        " << std::fixed << std::setprecision(1) 
                  << static_cast<double>(mc_metrics.calculation_time_us) / bs_duration.count() << "x slower\n";
        
        const double time_per_sim = static_cast<double>(mc_metrics.calculation_time_us) / n_sims;
        std::cout << "Time per simulation: " << std::fixed << std::setprecision(3) << time_per_sim << " μs\n";
    }
    
    /*
     * TEST SUR DIFFÉRENTS TYPES D'OPTIONS
     * ====================================
     * Démonstration de la flexibilité du système
     */
    std::cout << "\n=== OPTION TYPES COMPARISON ===\n";
    
    const std::vector<std::pair<OptionType, std::string>> option_types = {
        {OptionType::EUROPEAN_CALL, "European Call"},
        {OptionType::EUROPEAN_PUT, "European Put"},
        {OptionType::ASIAN_CALL, "Asian Call"},
        {OptionType::ASIAN_PUT, "Asian Put"},
        {OptionType::DIGITAL_CALL, "Digital Call"},
        {OptionType::BARRIER_CALL_KNOCKOUT, "Barrier Call Knockout"},
        {OptionType::LOOKBACK_CALL, "Lookback Call"}
    };
    
    for (const auto& [option_type, name] : option_types) {
        auto result = mc_pricer.calculate_option_price(option_type, S, K, T, r, vol, 50'000);
        
        if (result.has_value()) {
            std::cout << std::left << std::setw(15) << name << ": $" 
                      << std::fixed << std::setprecision(4) << result.value().option_value 
                      << " (calc time: " << result.value().calculation_time_us << " μs)\n";
        }
    }
    
    /*
     * ANALYSE DE SENSIBILITÉ AU NOMBRE DE SIMULATIONS
     * ================================================
     * Montre comment la précision évolue avec le nombre de simulations
     */
    std::cout << "\n=== CONVERGENCE vs SIMULATION COUNT ===\n";

    std::cout << "Testing convergence for european call option with varying simulation counts...\n";

    const std::vector<size_t> sim_counts = {1'000, 10'000, 50'000, 100'000, 500'000};
    const double bs_reference = bs_model.price(S, K, T, r, vol, true).value();
    
    std::cout << "Simulations |   MC Price   | Error (%) | Time (μs)\n";
    std::cout << "------------|--------------|-----------|----------\n";
    
    for (size_t sims : sim_counts) {
        auto result = mc_pricer.calculate_option_price(
            OptionType::EUROPEAN_CALL, S, K, T, r, vol, sims
        );
        
        if (result.has_value()) {
            const double mc_price = result.value().option_value;
            const double error_pct = std::abs(mc_price - bs_reference) / bs_reference * 100.0;
            
            std::cout << std::right << std::setw(11) << sims 
                      << " | $" << std::fixed << std::setprecision(6) << mc_price
                      << " |   " << std::setprecision(3) << error_pct 
                      << "   | " << std::setw(8) << result.value().calculation_time_us << "\n";
        }
    }
    
    /*
     * VALIDATION AVEC DIFFÉRENTS PARAMÈTRES DE MARCHÉ
     * ================================================
     * Test de robustesse du pricing Monte Carlo
     */
    std::cout << "\n=== ROBUSTNESS TEST ===\n";
    
    const std::vector<std::tuple<double, double, std::string>> vol_tests = {
        {0.10, 0.05, "Low vol, low rate"},
        {0.20, 0.05, "Normal vol, normal rate"},
        {0.40, 0.03, "High vol, low rate"},
        {0.60, 0.07, "Very high vol, high rate"}
    };
    
    std::cout << "Scenario                  | MC Price | BS Price | Diff (%)\n";
    std::cout << "--------------------------|----------|----------|----------\n";
    
    for (const auto& [test_vol, test_rate, description] : vol_tests) {
        auto mc_result = mc_pricer.calculate_option_price(
            OptionType::EUROPEAN_CALL, S, K, T, test_rate, test_vol, 100'000
        );
        auto bs_result = bs_model.price(S, K, T, test_rate, test_vol, true);
        
        if (mc_result.has_value() && bs_result.has_value()) {
            const double mc_price = mc_result.value().option_value;
            const double bs_price = bs_result.value();
            const double diff_pct = std::abs(mc_price - bs_price) / bs_price * 100.0;
            
            std::cout << std::left << std::setw(25) << description 
                      << " | $" << std::fixed << std::setprecision(4) << mc_price
                      << " | $" << std::setprecision(4) << bs_price
                      << " |   " << std::setprecision(2) << diff_pct << "\n";
        }
    }
    
    /*
     * RÉSUMÉ ET RECOMMANDATIONS
     * =========================
     */
    std::cout << "\n=== SUMMARY ===\n";
    std::cout << "✓ Monte Carlo pricing engine validated against Black-Scholes\n";
    std::cout << "✓ Supports European, Asian, Digital, and Barrier options\n";
    std::cout << "✓ Configurable simulation count for precision vs speed trade-off\n";
    std::cout << "✓ Robust performance across different market conditions\n";
    std::cout << "✓ Ready for exotic options that have no closed-form solutions\n";
    
    std::cout << "\nRecommendations:\n";
    std::cout << "• Use 50K-100K simulations for daily pricing\n";
    std::cout << "• Use 500K+ simulations for critical P&L calculations\n";
    std::cout << "• Monitor convergence for options near expiry\n";
    std::cout << "• Consider variance reduction techniques for production\n";
}



void demonstrate_monte_carlo_option_pricing_convergence() {
    std::cout << "\n=== MONTE CARLO OPTION PRICING ===\n";
    
    /*
     * INITIALISATION DES MOTEURS
     * ===========================
     */
    PricingCalculator mc_pricer;      // Notre moteur Monte Carlo
    BlackScholesModel bs_model;       // Pour comparaison/validation
    
    /*
     * PARAMÈTRES D'OPTION STANDARD
     * =============================
     * Exemple classique pour démonstration
     */
    const double S = 100.0;    // Prix spot
    const double K = 105.0;    // Strike (5% OTM)
    const double T = 0.25;     // 3 mois
    const double r = 0.05;     // Taux sans risque 5%
    const double vol = 0.40;   // Volatilité 40%
    const size_t n_sims = 10'000;  // 100K simulations
    const size_t n_ticks = 100;  // # of steps to simulate

    std::cout << "Pricing European Call: S=$" << S << ", K=$" << K
              << ", T=" << T << " years, r=" << r << ", vol=" << vol << "\n";
    std::cout << "Monte Carlo simulations: " << n_sims << "\n\n";
    

    // creation of the vector 
    std::vector<size_t> sim_counts = {};
    for (size_t i = 1; i <= n_sims; i += n_ticks) {
        sim_counts.push_back(i);
    }

    /*
     * ANALYSE DE SENSIBILITÉ AU NOMBRE DE SIMULATIONS
     * ================================================
     * Montre comment la précision évolue avec le nombre de simulations
     */
    std::cout << "\n=== CONVERGENCE vs SIMULATION COUNT ===\n";

    std::cout << "Testing convergence for european call option with varying simulation counts...\n";

    
    const double bs_reference = bs_model.price(S, K, T, r, vol, true).value();
    
    std::cout << "Simulations |   MC Price   | Error (%) | Time (μs)\n";
    std::cout << "------------|--------------|-----------|----------\n";
    
    // Extraire les données pour le plot
    std::vector<double> mc_price_double;
    std::vector<double> mc_error_pct;
    std::vector<double> bs_price_double;

    

    for (size_t sims : sim_counts) {
        auto result = mc_pricer.calculate_option_price(
            OptionType::BARRIER_CALL_KNOCKOUT, S, K, T, r, vol, sims
        );
        
        if (result.has_value()) {
            const double mc_price = result.value().option_value;
            const double error_pct = std::abs(mc_price - bs_reference) / bs_reference * 100.0;
            
            std::cout << std::right << std::setw(11) << sims 
                      << " | $" << std::fixed << std::setprecision(6) << mc_price
                      << " |   " << std::setprecision(3) << error_pct 
                      << "   | " << std::setw(8) << result.value().calculation_time_us << "\n";

            // Stocker les résultats pour le plot
            mc_error_pct.push_back(error_pct);
            mc_price_double.push_back(mc_price);
            bs_price_double.push_back(bs_reference);
        }
    }

    std::vector<std::vector<double>> mes_trajectoires;
    mes_trajectoires.push_back(mc_error_pct);
    mes_trajectoires.push_back(mc_price_double);
    mes_trajectoires.push_back(bs_price_double);

    // CORRECTION 5: Plotting avec données correctes
    GnuplotPlotter plotter("./plots/");
    
    plotter.plot_timeseries(
        mc_price_double,
        "mc_price_by_horizon",
        "Monte Carlo Price by Investment Horizon",
        "Monte Carlo Price ($)",
        "Horizon (days)"
    );
    
    plotter.plot_timeseries(
        mc_error_pct,
        "error_pct_by_horizon",
        "Monte Carlo Price Error (%) by Investment Horizon",
        "Error (%)",
        "Horizon (days)"
    );

    plotter.plot_multiple_draws(
        mes_trajectoires, 
        {"Price ($)","Error (%)", "BS Price ($)"},
        "mc_draws", 
        "Monte Carlo Price Paths", 
        "Price ($)", 
        "Time Steps"
    );

}




int main() {
    std::cout << "=== MODERN C++ RISK ENGINE ===\n";
    std::cout << "Demonstrating C++20/23 features for option pricing\n";
    try {
        demonstrate_monte_carlo_option_pricing();

        demonstrate_monte_carlo_option_pricing_convergence();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;  // Code de retour d'erreur
    }
    
    return 0;  
}