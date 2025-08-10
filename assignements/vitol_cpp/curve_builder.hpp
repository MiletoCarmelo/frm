/*
 * curve_builder.hpp - Construction et gestion des courbes forward
 * 
 * Module essentiel pour le trading de commodités où les contrats à terme
 * dominent le marché. Ce fichier gère la construction des courbes de prix
 * forward à partir des cotations de marché.
 * 
 * IMPORTANCE CHEZ VITOL :
 * - Pétrole/Gaz tradent principalement en contrats à terme
 * - Courbes forward reflètent les anticipations de marché
 * - Calendar spreads = stratégies de trading principales
 * - Stockage/Transport = arbitrages temporels basés sur les courbes
 */

#pragma once

#include "types.hpp"
#include "math_utils.hpp"
#include <vector>
#include <map>
#include <span>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <string>
#include <optional>

// ===== TYPES DE DONNÉES DE MARCHÉ =====

/*
 * COTATION DE CONTRAT À TERME
 * ============================
 * Représente une cotation de marché pour un contrat à terme
 */
struct FutureQuote {
    std::string contract_id;    // "WTI_Mar2025", "BRENT_Dec2024"
    double maturity;           // Temps jusqu'à expiration (années)
    double price;              // Prix du contrat à terme
    double bid;                // Prix d'achat (optionnel)
    double ask;                // Prix de vente (optionnel)
    double volume;             // Volume échangé
    
    [[nodiscard]] bool is_valid() const noexcept {
        return maturity >= 0.0 && price > 0.0 && 
               !contract_id.empty() && volume >= 0.0;
    }
    
    [[nodiscard]] double mid_price() const noexcept {
        return (bid > 0.0 && ask > 0.0) ? (bid + ask) / 2.0 : price;
    }
};

/*
 * COTATION DE TAUX D'INTÉRÊT
 * ===========================
 * Pour bootstrap des courbes de taux sans risque
 */
struct RateQuote {
    double maturity;           // Maturité en années
    double rate;              // Taux (ex: 0.05 = 5%)
    std::string instrument;   // "LIBOR", "OIS", "TREASURY"
    
    [[nodiscard]] bool is_valid() const noexcept {
        return maturity >= 0.0 && rate >= -0.1 && rate <= 1.0; // Taux entre -10% et 100%
    }
};

// ===== ÉNUMÉRATIONS POUR LES MÉTHODES =====

/*
 * MÉTHODES D'INTERPOLATION
 * ========================
 * Différentes façons de connecter les points de marché
 */
enum class InterpolationType {
    LINEAR,              // Interpolation linéaire (simple)
    LOG_LINEAR,          // Log-linéaire (pour éviter taux négatifs)
    CUBIC_SPLINE,        // Spline cubique (smooth)
    MONOTONIC_CUBIC,     // Spline monotone (pas d'oscillations)
    PIECEWISE_CONSTANT   // Constant par morceaux (forward rates)
};

/*
 * MÉTHODES D'EXTRAPOLATION
 * =========================
 * Comment prolonger la courbe au-delà des points observés
 */
enum class ExtrapolationType {
    CONSTANT,            // Dernier point constant
    LINEAR,              // Extrapolation linéaire
    EXPONENTIAL_DECAY    // Décroissance exponentielle vers niveau long terme
};

// ===== CLASSE PRINCIPALE : COURBE FORWARD =====

/*
 * COURBE DE PRIX FORWARD
 * ======================
 * Contient tous les prix forward pour différentes maturités
 * et permet l'interpolation/extrapolation
 */
class ForwardCurve {
private:
    std::string underlying_;                    // Actif sous-jacent ("WTI", "BRENT")
    std::map<double, double> curve_points_;     // maturité → prix forward
    InterpolationType interp_method_;           // Méthode d'interpolation
    ExtrapolationType extrap_method_;           // Méthode d'extrapolation
    double long_term_level_;                   // Niveau long terme pour extrapolation
    
    /*
     * CACHE POUR OPTIMISATION
     * =======================
     * Évite de recalculer les interpolations répétées
     */
    mutable std::map<double, double> interpolation_cache_;
    
public:
    /*
     * CONSTRUCTEUR
     * ============
     */
    explicit ForwardCurve(const std::string& underlying,
                         InterpolationType interp = InterpolationType::CUBIC_SPLINE,
                         ExtrapolationType extrap = ExtrapolationType::CONSTANT)
        : underlying_(underlying), interp_method_(interp), extrap_method_(extrap), long_term_level_(0.0) {}
    
    /*
     * AJOUT DE POINTS DE MARCHÉ
     * =========================
     * Construction point par point de la courbe
     */
    void add_point(double maturity, double forward_price) {
        if (maturity >= 0.0 && forward_price > 0.0) {
            curve_points_[maturity] = forward_price;
            interpolation_cache_.clear(); // Invalide le cache
        }
    }
    
    /*
     * CONSTRUCTION À PARTIR DE COTATIONS
     * ==================================
     * Bootstrap automatique depuis les données de marché
     */
    void build_from_futures(std::span<const FutureQuote> quotes) {
        curve_points_.clear();
        interpolation_cache_.clear();
        
        for (const auto& quote : quotes) {
            if (quote.is_valid()) {
                add_point(quote.maturity, quote.mid_price());
            }
        }
        
        // Estimation du niveau long terme (moyenne des derniers points)
        if (curve_points_.size() >= 3) {
            auto it = curve_points_.rbegin();
            double sum = 0.0;
            int count = 0;
            for (int i = 0; i < 3 && it != curve_points_.rend(); ++i, ++it, ++count) {
                sum += it->second;
            }
            long_term_level_ = sum / count;
        }
    }
    
    /*
     * INTERPOLATION PRINCIPALE
     * ========================
     * Calcule le prix forward pour n'importe quelle maturité
     */
    [[nodiscard]] double get_forward(double maturity) const {
        // Vérification du cache
        if (auto it = interpolation_cache_.find(maturity); it != interpolation_cache_.end()) {
            return it->second;
        }
        
        if (curve_points_.empty()) return 0.0;
        
        double result = 0.0;
        
        // Point exact dans la courbe
        if (auto exact = curve_points_.find(maturity); exact != curve_points_.end()) {
            result = exact->second;
        }
        // Extrapolation avant le premier point
        else if (maturity < curve_points_.begin()->first) {
            result = extrapolate_left(maturity);
        }
        // Extrapolation après le dernier point
        else if (maturity > curve_points_.rbegin()->first) {
            result = extrapolate_right(maturity);
        }
        // Interpolation entre deux points
        else {
            result = interpolate_between_points(maturity);
        }
        
        // Mise en cache
        interpolation_cache_[maturity] = result;
        return result;
    }
    
    /*
     * CALCUL DU DISCOUNT FACTOR
     * =========================
     * DF(T) = exp(-∫[0,T] f(t)dt) où f(t) est le taux forward instantané
     */
    [[nodiscard]] double discount_factor(double maturity, double risk_free_rate = 0.0) const {
        if (maturity <= 0.0) return 1.0;
        
        // Approximation : DF ≈ exp(-r×T) pour taux sans risque constant
        // En production, intégration numérique de la courbe de taux
        return std::exp(-risk_free_rate * maturity);
    }
    
    /*
     * CALCUL DU TAUX FORWARD
     * ======================
     * Forward rate entre deux dates : F(T1,T2) = (P(T1)/P(T2) - 1)/(T2-T1)
     */
    [[nodiscard]] double forward_rate(double T1, double T2) const {
        if (T2 <= T1) return 0.0;
        
        const double P1 = get_forward(T1);
        const double P2 = get_forward(T2);
        
        if (P1 <= 0.0 || P2 <= 0.0) return 0.0;
        
        return (P1 / P2 - 1.0) / (T2 - T1);
    }
    
    /*
     * ANALYTICS DE COURBE
     * ===================
     * Métriques importantes pour le risk management
     */
    
    // Pente de la courbe (contango vs backwardation)
    [[nodiscard]] double curve_slope() const {
        if (curve_points_.size() < 2) return 0.0;
        
        const auto& first = *curve_points_.begin();
        const auto& last = *curve_points_.rbegin();
        
        const double dt = last.first - first.first;
        return dt > 0.0 ? (last.second - first.second) / dt : 0.0;
    }
    
    // Convexité de la courbe
    [[nodiscard]] double curve_convexity() const {
        if (curve_points_.size() < 3) return 0.0;
        
        double sum_curvature = 0.0;
        int count = 0;
        
        auto it = curve_points_.begin();
        auto prev = it++;
        auto curr = it++;
        
        while (it != curve_points_.end()) {
            const double dt1 = curr->first - prev->first;
            const double dt2 = it->first - curr->first;
            
            if (dt1 > 0.0 && dt2 > 0.0) {
                const double slope1 = (curr->second - prev->second) / dt1;
                const double slope2 = (it->second - curr->second) / dt2;
                const double curvature = (slope2 - slope1) / (dt1 + dt2);
                
                sum_curvature += std::abs(curvature);
                count++;
            }
            
            prev = curr;
            curr = it++;
        }
        
        return count > 0 ? sum_curvature / count : 0.0;
    }
    
    /*
     * SENSIBILITÉS DE RISQUE
     * ======================
     * Impact des mouvements de courbe sur les positions
     */
    
    // DV01 par bucket de maturité
    [[nodiscard]] std::vector<double> calculate_dv01_by_tenor(std::span<const double> tenors) const {
        std::vector<double> dv01_vector;
        dv01_vector.reserve(tenors.size());
        
        for (double tenor : tenors) {
            const double base_forward = get_forward(tenor);
            
            // Shock de +1bp (0.01%) sur ce point de courbe
            ForwardCurve shocked_curve = *this;
            if (auto it = shocked_curve.curve_points_.find(tenor); it != shocked_curve.curve_points_.end()) {
                it->second += 0.0001 * base_forward; // +1bp
            }
            
            const double shocked_forward = shocked_curve.get_forward(tenor);
            dv01_vector.push_back(shocked_forward - base_forward);
        }
        
        return dv01_vector;
    }
    
    // Shift parallèle de toute la courbe
    void parallel_shift(double shift_amount) {
        for (auto& [maturity, price] : curve_points_) {
            price += shift_amount;
        }
        interpolation_cache_.clear();
    }
    
    /*
     * UTILITIES ET DIAGNOSTICS
     * ========================
     */
    
    [[nodiscard]] size_t size() const noexcept { return curve_points_.size(); }
    [[nodiscard]] bool empty() const noexcept { return curve_points_.empty(); }
    [[nodiscard]] const std::string& underlying() const noexcept { return underlying_; }
    
    // Export des points pour visualisation/debug
    [[nodiscard]] std::vector<std::pair<double, double>> get_all_points() const {
        return std::vector<std::pair<double, double>>(curve_points_.begin(), curve_points_.end());
    }
    
    // Validation de la courbe
    [[nodiscard]] bool is_valid() const noexcept {
        if (curve_points_.empty()) return false;
        
        // Vérification que tous les prix sont positifs
        return std::all_of(curve_points_.begin(), curve_points_.end(),
                          [](const auto& point) { return point.second > 0.0; });
    }

private:
    /*
     * MÉTHODES D'INTERPOLATION PRIVÉES
     * =================================
     */
    
    [[nodiscard]] double interpolate_between_points(double maturity) const {
        auto upper = curve_points_.upper_bound(maturity);
        auto lower = std::prev(upper);
        
        const double t1 = lower->first, p1 = lower->second;
        const double t2 = upper->first, p2 = upper->second;
        const double dt = t2 - t1;
        const double alpha = (maturity - t1) / dt;
        
        switch (interp_method_) {
            case InterpolationType::LINEAR:
                return p1 + alpha * (p2 - p1);
                
            case InterpolationType::LOG_LINEAR:
                return p1 * std::pow(p2 / p1, alpha);
                
            case InterpolationType::CUBIC_SPLINE:
                // Simplification : interpolation cubique locale
                return cubic_interpolate(t1, p1, t2, p2, maturity);
                
            case InterpolationType::MONOTONIC_CUBIC:
                // Assure que l'interpolation preserve la monotonie
                return monotonic_cubic_interpolate(t1, p1, t2, p2, maturity);
                
            case InterpolationType::PIECEWISE_CONSTANT:
                return p1; // Forward rate constant jusqu'au prochain point
                
            default:
                return p1 + alpha * (p2 - p1); // Fallback linéaire
        }
    }
    
    [[nodiscard]] double extrapolate_left(double maturity) const {
        const auto& first_point = *curve_points_.begin();
        
        switch (extrap_method_) {
            case ExtrapolationType::CONSTANT:
                return first_point.second;
                
            case ExtrapolationType::LINEAR:
                if (curve_points_.size() >= 2) {
                    auto second = std::next(curve_points_.begin());
                    const double slope = (second->second - first_point.second) / 
                                       (second->first - first_point.first);
                    return first_point.second + slope * (maturity - first_point.first);
                }
                return first_point.second;
                
            case ExtrapolationType::EXPONENTIAL_DECAY:
                return first_point.second; // Simplification
                
            default:
                return first_point.second;
        }
    }
    
    [[nodiscard]] double extrapolate_right(double maturity) const {
        const auto& last_point = *curve_points_.rbegin();
        
        switch (extrap_method_) {
            case ExtrapolationType::CONSTANT:
                return last_point.second;
                
            case ExtrapolationType::LINEAR:
                if (curve_points_.size() >= 2) {
                    auto second_last = std::next(curve_points_.rbegin());
                    const double slope = (last_point.second - second_last->second) / 
                                       (last_point.first - second_last->first);
                    return last_point.second + slope * (maturity - last_point.first);
                }
                return last_point.second;
                
            case ExtrapolationType::EXPONENTIAL_DECAY:
                if (long_term_level_ > 0.0) {
                    const double decay_rate = 0.1; // Paramètre à calibrer
                    const double dt = maturity - last_point.first;
                    return long_term_level_ + (last_point.second - long_term_level_) * 
                           std::exp(-decay_rate * dt);
                }
                return last_point.second;
                
            default:
                return last_point.second;
        }
    }
    
    // Interpolations spécialisées
    [[nodiscard]] double cubic_interpolate(double t1, double p1, double t2, double p2, double t) const {
        // Simplification : interpolation cubique de Hermite
        const double dt = t2 - t1;
        const double alpha = (t - t1) / dt;
        const double alpha2 = alpha * alpha;
        const double alpha3 = alpha2 * alpha;
        
        // Estimation des dérivées (simplifiée)
        double m1 = 0.0, m2 = 0.0;
        
        // Coefficients de Hermite
        const double h00 = 2*alpha3 - 3*alpha2 + 1;
        const double h10 = alpha3 - 2*alpha2 + alpha;
        const double h01 = -2*alpha3 + 3*alpha2;
        const double h11 = alpha3 - alpha2;
        
        return h00*p1 + h10*dt*m1 + h01*p2 + h11*dt*m2;
    }
    
    [[nodiscard]] double monotonic_cubic_interpolate(double t1, double p1, double t2, double p2, double t) const {
        // Assure que l'interpolation est monotone
        const double linear_result = p1 + (p2 - p1) * (t - t1) / (t2 - t1);
        return linear_result; // Simplification
    }
};

// ===== BUILDER DE COURBES =====

/*
 * CONSTRUCTEUR DE COURBES FORWARD
 * ===============================
 * Factory class pour créer des courbes à partir de différentes sources
 */
class ForwardCurveBuilder {
public:
    /*
     * CONSTRUCTION À PARTIR DE CONTRATS À TERME
     * ==========================================
     * Méthode principale pour les commodités
     */
    [[nodiscard]] static ForwardCurve build_from_futures(
        const std::string& underlying,
        std::span<const FutureQuote> quotes,
        InterpolationType interp_method = InterpolationType::CUBIC_SPLINE) {
        
        ForwardCurve curve(underlying, interp_method);
        curve.build_from_futures(quotes);
        return curve;
    }
    
    /*
     * CONSTRUCTION À PARTIR DE TAUX D'INTÉRÊT
     * ========================================
     * Pour les courbes de discount
     */
    [[nodiscard]] static ForwardCurve build_from_rates(
        const std::string& currency,
        std::span<const RateQuote> rates) {
        
        ForwardCurve curve(currency + "_DISCOUNT", InterpolationType::LOG_LINEAR);
        
        for (const auto& rate : rates) {
            if (rate.is_valid()) {
                // Conversion rate → discount factor
                const double df = std::exp(-rate.rate * rate.maturity);
                curve.add_point(rate.maturity, df);
            }
        }
        
        return curve;
    }
    
    /*
     * CONSTRUCTION DE COURBE SYNTHÉTIQUE
     * ==================================
     * Pour les tests et la simulation
     */
    [[nodiscard]] static ForwardCurve build_synthetic_curve(
        const std::string& underlying,
        double spot_price,
        double storage_cost,
        double convenience_yield,
        std::span<const double> maturities) {
        
        ForwardCurve curve(underlying);
        
        for (double T : maturities) {
            // Formule théorique : F(T) = S * exp((r + storage - convenience) * T)
            const double forward_price = spot_price * std::exp((storage_cost - convenience_yield) * T);
            curve.add_point(T, forward_price);
        }
        
        return curve;
    }
    
    /*
     * CALIBRATION DE COURBE
     * =====================
     * Ajuste les paramètres pour matcher les prix de marché
     */
    static void calibrate_storage_parameters(
        ForwardCurve& curve,
        std::span<const FutureQuote> market_quotes,
        double& storage_cost,
        double& convenience_yield) {
        
        // Simplification : optimisation par moindres carrés
        double best_storage = storage_cost;
        double best_convenience = convenience_yield;
        double min_error = std::numeric_limits<double>::max();
        
        // Grid search grossier
        for (double s = 0.0; s <= 0.10; s += 0.01) {
            for (double c = 0.0; c <= 0.15; c += 0.01) {
                double error = 0.0;
                
                for (const auto& quote : market_quotes) {
                    const double theoretical = curve.get_forward(0.0) * 
                                             std::exp((s - c) * quote.maturity);
                    const double market = quote.mid_price();
                    error += (theoretical - market) * (theoretical - market);
                }
                
                if (error < min_error) {
                    min_error = error;
                    best_storage = s;
                    best_convenience = c;
                }
            }
        }
        
        storage_cost = best_storage;
        convenience_yield = best_convenience;
    }
};

/*
 * EXEMPLE D'UTILISATION :
 * ========================
 * 
 * // 1. Création de cotations de marché
 * vector<FutureQuote> wti_quotes = {
 *     {"WTI_Mar25", 0.25, 75.50, 75.45, 75.55, 10000},
 *     {"WTI_Jun25", 0.50, 76.20, 76.15, 76.25, 8500},
 *     {"WTI_Dec25", 1.00, 77.80, 77.75, 77.85, 6200}
 * };
 * 
 * // 2. Construction de la courbe
 * auto wti_curve = ForwardCurveBuilder::build_from_futures("WTI", wti_quotes);
 * 
 * // 3. Utilisation
 * double forward_9m = wti_curve.get_forward(0.75);  // Prix forward 9 mois
 * double slope = wti_curve.curve_slope();           // Contango/backwardation
 * 
 * // 4. Analytics de risque
 * vector<double> tenors = {0.25, 0.5, 1.0, 2.0};
 * auto dv01 = wti_curve.calculate_dv01_by_tenor(tenors);
 */