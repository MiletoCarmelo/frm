/*
 * timeseries_simulator.hpp - Générateur de time series réalistes pour commodités
 * 
 * Émule des données historiques avec :
 * - Volatility clustering (périodes de forte/faible volatilité)
 * - Mean reversion (tendance à revenir vers un niveau)
 * - Seasonality (saisonnalité énergétique)
 * - Fat tails (événements extrêmes plus fréquents)
 */

#pragma once
#include <vector>
#include <random>
#include <cmath>
#include <string>
#include <iostream>
#include <iomanip>

struct TimeSeriesParams {
    // Paramètres de base
    double initial_price = 75.0;        // Prix initial (ex: WTI $75)
    size_t n_periods = 252;             // Nombre de périodes (1 an = 252 jours)
    
    // Modèle GBM (Geometric Brownian Motion)
    double drift = 0.05;                // Tendance annuelle (5%)
    double base_volatility = 0.25;      // Volatilité de base (25%)
    
    // Mean Reversion (Ornstein-Uhlenbeck)
    double mean_reversion_speed = 0.1;  // Vitesse de retour à la moyenne
    double long_term_mean = 75.0;       // Prix d'équilibre long terme
    
    // Volatility Clustering (GARCH-like)
    double vol_persistence = 0.85;      // Persistance volatilité (0.85 = forte)
    double vol_mean_reversion = 0.10;   // Retour vol vers moyenne
    
    // Seasonality (pour gaz naturel/électricité)
    bool enable_seasonality = false;
    double seasonal_amplitude = 0.15;   // Amplitude saisonnière (15%)
    
    // Fat Tails (événements extrêmes)
    double jump_probability = 0.02;     // 2% chance de jump par jour
    double jump_mean = 0.0;             // Jump moyen neutre
    double jump_std = 0.05;             // Amplitude des jumps (5%)
};

class TimeSeriesSimulator {
private:
    mutable std::mt19937_64 rng_;
    mutable std::normal_distribution<double> normal_{0.0, 1.0};
    mutable std::uniform_real_distribution<double> uniform_{0.0, 1.0};

public:
    explicit TimeSeriesSimulator(uint64_t seed = std::random_device{}())
        : rng_(seed) {}

    /*
     * GÉNÉRATION SIMPLE : GBM CLASSIQUE
     * ==================================
     * Pour commencer avec quelque chose de basique
     */
    std::vector<double> generate_gbm(const TimeSeriesParams& params) const {
        std::vector<double> prices;
        prices.reserve(params.n_periods + 1);
        prices.push_back(params.initial_price);
        
        const double dt = 1.0 / 252.0;  // 1 jour
        const double drift_dt = params.drift * dt;
        const double vol_sqrt_dt = params.base_volatility * std::sqrt(dt);
        
        for (size_t i = 0; i < params.n_periods; ++i) {
            const double dW = normal_(rng_);
            const double return_rate = drift_dt + vol_sqrt_dt * dW;
            
            const double new_price = prices.back() * std::exp(return_rate);
            prices.push_back(new_price);
        }
        
        return prices;
    }
    
    /*
     * GÉNÉRATION RÉALISTE : COMMODITÉS AVEC MEAN REVERSION
     * ====================================================
     * Plus réaliste pour WTI, Brent, Gas
     */
    std::vector<double> generate_mean_reverting(const TimeSeriesParams& params) const {
        std::vector<double> prices;
        prices.reserve(params.n_periods + 1);
        prices.push_back(params.initial_price);
        
        const double dt = 1.0 / 252.0;
        double current_vol = params.base_volatility;  // Volatilité dynamique
        
        for (size_t i = 0; i < params.n_periods; ++i) {
            const double current_price = prices.back();
            
            // MEAN REVERSION : force qui tire vers long_term_mean
            const double mean_reversion_force = params.mean_reversion_speed * 
                (params.long_term_mean - current_price) * dt;
            
            // VOLATILITY CLUSTERING : volatilité qui évolue
            const double vol_innovation = 0.1 * normal_(rng_) * std::sqrt(dt);
            current_vol = params.base_volatility * (1 - params.vol_mean_reversion * dt) +
                         params.vol_persistence * current_vol * dt + vol_innovation;
            current_vol = std::max(0.05, std::min(1.0, current_vol));  // Bornes [5%, 100%]
            
            // SEASONALITY (optionnel)
            double seasonal_factor = 1.0;
            if (params.enable_seasonality) {
                const double seasonal_phase = 2.0 * M_PI * i / 252.0;  // Cycle annuel
                seasonal_factor = 1.0 + params.seasonal_amplitude * std::sin(seasonal_phase);
            }
            
            // INNOVATION PRINCIPALE
            const double dW = normal_(rng_);
            const double price_change = mean_reversion_force + 
                                       current_vol * std::sqrt(dt) * dW * seasonal_factor;
            
            // JUMPS (événements extrêmes)
            double jump_contribution = 0.0;
            if (uniform_(rng_) < params.jump_probability) {
                jump_contribution = params.jump_mean + params.jump_std * normal_(rng_);
                std::cout << "JUMP at day " << i << ": " << jump_contribution * 100 << "%\n";
            }
            
            // NOUVEAU PRIX
            const double new_price = current_price * std::exp(price_change + jump_contribution);
            prices.push_back(std::max(1.0, new_price));  // Prix minimum $1
        }
        
        return prices;
    }
    
    /*
     * GÉNÉRATION AVEC CORRÉLATIONS
     * =============================
     * Pour générer WTI et Brent corrélés
     */
    std::pair<std::vector<double>, std::vector<double>> generate_correlated_pair(
        const TimeSeriesParams& params1,
        const TimeSeriesParams& params2,
        double correlation = 0.85) const {
        
        std::vector<double> series1, series2;
        series1.reserve(params1.n_periods + 1);
        series2.reserve(params2.n_periods + 1);
        
        series1.push_back(params1.initial_price);
        series2.push_back(params2.initial_price);
        
        const double dt = 1.0 / 252.0;
        const double sqrt_dt = std::sqrt(dt);
        
        // Matrice de corrélation de Cholesky
        const double sqrt_1_minus_corr2 = std::sqrt(1.0 - correlation * correlation);
        
        for (size_t i = 0; i < params1.n_periods; ++i) {
            // Deux innovations indépendantes
            const double z1 = normal_(rng_);
            const double z2 = normal_(rng_);
            
            // Corrélation via Cholesky
            const double corr_z1 = z1;
            const double corr_z2 = correlation * z1 + sqrt_1_minus_corr2 * z2;
            
            // Série 1
            const double return1 = params1.drift * dt + params1.base_volatility * sqrt_dt * corr_z1;
            const double new_price1 = series1.back() * std::exp(return1);
            series1.push_back(new_price1);
            
            // Série 2  
            const double return2 = params2.drift * dt + params2.base_volatility * sqrt_dt * corr_z2;
            const double new_price2 = series2.back() * std::exp(return2);
            series2.push_back(new_price2);
        }
        
        return {series1, series2};
    }
    
    /*
     * HELPER : CONVERSION PRIX → RENDEMENTS
     * =====================================
     */
    static std::vector<double> prices_to_returns(const std::vector<double>& prices) {
        if (prices.size() < 2) return {};
        
        std::vector<double> returns;
        returns.reserve(prices.size() - 1);
        
        for (size_t i = 1; i < prices.size(); ++i) {
            const double ret = (prices[i] - prices[i-1]) / prices[i-1];
            returns.push_back(ret);
        }
        
        return returns;
    }
    
    /*
     * HELPER : STATISTIQUES DESCRIPTIVES
     * ===================================
     */
    static void print_statistics(const std::vector<double>& returns, const std::string& name) {
        if (returns.empty()) return;
        
        const double mean = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
        
        double variance = 0.0;
        for (double ret : returns) {
            variance += (ret - mean) * (ret - mean);
        }
        variance /= (returns.size() - 1);
        const double vol_daily = std::sqrt(variance);
        const double vol_annual = vol_daily * std::sqrt(252);
        
        std::cout << "\n=== " << name << " STATISTICS ===\n";
        std::cout << "Periods: " << returns.size() << "\n";
        std::cout << "Mean return: " << std::fixed << std::setprecision(4) << mean * 100 << "%\n";
        std::cout << "Daily vol: " << vol_daily * 100 << "%\n";
        std::cout << "Annual vol: " << vol_annual * 100 << "%\n";
        
        auto sorted_returns = returns;
        std::sort(sorted_returns.begin(), sorted_returns.end());
        const size_t idx_5 = static_cast<size_t>(0.05 * sorted_returns.size());
        const size_t idx_95 = static_cast<size_t>(0.95 * sorted_returns.size());
        
        std::cout << "5th percentile: " << sorted_returns[idx_5] * 100 << "%\n";
        std::cout << "95th percentile: " << sorted_returns[idx_95] * 100 << "%\n";
    }
};

/*
 * USAGE EXEMPLES :
 * 
 * TimeSeriesSimulator sim;
 * 
 * // 1) WTI simple
 * TimeSeriesParams wti_params;
 * wti_params.initial_price = 75.0;
 * wti_params.base_volatility = 0.35;
 * auto wti_prices = sim.generate_mean_reverting(wti_params);
 * auto wti_returns = TimeSeriesSimulator::prices_to_returns(wti_prices);
 * 
 * // 2) WTI + Brent corrélés
 * TimeSeriesParams brent_params = wti_params;
 * brent_params.initial_price = 78.0;
 * auto [wti, brent] = sim.generate_correlated_pair(wti_params, brent_params, 0.85);
 * 
 * // 3) Statistics
 * TimeSeriesSimulator::print_statistics(wti_returns, "WTI");
 */