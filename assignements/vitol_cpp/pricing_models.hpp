/*
 * pricing_models.hpp - Modèle de pricing Black-Scholes
 * 
 * Ce fichier implémente le célèbre modèle Black-Scholes pour pricer les options.
 * C'est LE modèle de référence en finance, Prix Nobel d'économie 1997.
 * 
 * RAPPEL : Une option donne le DROIT (pas l'obligation) d'acheter/vendre un actif
 * à un prix fixé (strike) avant une date donnée (expiration).
 */

#pragma once

#include "types.hpp"       // Pour expected<>, RiskError, etc.
#include "math_utils.hpp"  // Pour FastMath::norm_cdf(), etc.
#include <unordered_map>   // Pour le cache des résultats
#include <string>          // Pour les clés du cache
#include <cmath>          // Pour exp(), sqrt(), etc.

// Déclaration forward pour éviter dépendance circulaire
class ForwardCurve;

// ===== MODÈLE DE PRICING BLACK-SCHOLES =====
/*
 * Cette classe implémente le modèle Black-Scholes pour calculer :
 * 1. Le PRIX d'une option (combien elle vaut aujourd'hui)
 * 2. Les GREEKS (sensibilités aux différents paramètres)
 */
class BlackScholesModel {
private:
    /*
     * SYSTÈME DE CACHE POUR LES PERFORMANCES
     * ======================================
     * On stocke les résultats déjà calculés pour éviter de refaire
     * les mêmes calculs (très important en trading haute fréquence)
     */
    mutable std::unordered_map<std::string, double> cache_;
    /*
     * mutable = on peut modifier ce membre même dans une fonction const
     * unordered_map = dictionnaire rapide (hash table)
     * string = clé unique pour identifier chaque calcul
     * double = prix/résultat calculé
     */
    
    /*
     * GÉNÉRATION DE CLÉ UNIQUE POUR LE CACHE
     * ======================================
     * Combine tous les paramètres en une seule chaîne unique
     */
    [[nodiscard]] std::string make_cache_key(double S, double K, double T, double r, double vol) const {
        /*
         * Paramètres :
         * S = Prix spot (prix actuel de l'actif)
         * K = Strike (prix d'exercice)
         * T = Time to maturity (temps jusqu'à expiration)
         * r = Risk-free rate (taux sans risque)
         * vol = Volatility (volatilité)
         */
        
        // Concatène tous les paramètres avec "_" comme séparateur
        return std::to_string(S) + "_" + std::to_string(K) + "_" + 
               std::to_string(T) + "_" + std::to_string(r) + "_" + std::to_string(vol);
        /*
         * Exemple de clé : "100.0_105.0_0.25_0.05_0.20"
         * En production, on utiliserait un hash plus sophistiqué
         */
    }
    
public:
    /*
     * NETTOYAGE DU CACHE
     * ==================
     * Vide le cache pour éviter qu'il devienne trop gros en mémoire
     */
    void clear_cache() { cache_.clear(); }
    
    /*
     * FONCTION PRINCIPALE : CALCUL DU PRIX D'UNE OPTION
     * =================================================
     * C'est LE calcul central du modèle Black-Scholes
     */
    [[nodiscard]] expected<double, RiskError> price(
        double S,              // Prix spot de l'actif sous-jacent
        double K,              // Prix d'exercice (strike price)
        double T,              // Temps jusqu'à expiration (en années)
        double r,              // Taux sans risque (risk-free rate)
        double vol,            // Volatilité (annualisée)
        bool is_call = true    // true = Call, false = Put
    ) const {
        /*
         * expected<double, RiskError> = retourne SOIT un prix SOIT une erreur
         * Plus sûr que de lancer des exceptions
         */
        
        /*
         * VALIDATION DES PARAMÈTRES D'ENTRÉE
         * ===================================
         * On vérifie que tous les paramètres ont du sens économiquement
         */
        if (vol <= 0) return expected<double, RiskError>{RiskError::INVALID_VOLATILITY};
        if (T < 0) return expected<double, RiskError>{RiskError::NEGATIVE_TIME};
        if (K <= 0 || S <= 0) return expected<double, RiskError>{RiskError::INVALID_STRIKE};
        /*
         * Logique métier :
         * - Volatilité doit être positive (pas de variance négative)
         * - Temps ne peut pas être négatif (on ne voyage pas dans le passé)
         * - Prix et strike doivent être positifs (pas de prix négatifs)
         */
        
        /*
         * CAS SPÉCIAL : EXPIRATION IMMÉDIATE
         * ===================================
         * Si T = 0, l'option expire maintenant → valeur intrinsèque seulement
         */
        if (T == 0) {
            return expected<double, RiskError>{
                is_call ? std::max(S - K, 0.0) : std::max(K - S, 0.0)
            };
            /*
             * Valeur intrinsèque :
             * - Call : max(S-K, 0) = profit si on exerce maintenant
             * - Put  : max(K-S, 0) = profit si on exerce maintenant
             * 
             * Exemple : S=110, K=100
             * - Call vaut 10 (on peut acheter à 100 et revendre à 110)
             * - Put vaut 0 (pas intéressant de vendre à 100 quand le marché est à 110)
             */
        }
        
        /*
         * VÉRIFICATION DU CACHE
         * =====================
         * On regarde si on a déjà calculé ce prix avant
         */
        const auto cache_key = make_cache_key(S, K, T, r, vol);
        if (auto it = cache_.find(cache_key); it != cache_.end()) {
            return expected<double, RiskError>{it->second};
        }
        /*
         * auto it = itérateur sur le cache
         * cache_.find() cherche la clé
         * it != cache_.end() = "on a trouvé quelque chose"
         * it->second = la valeur stockée (le prix calculé précédemment)
         */
        
        /*
         * CALCUL BLACK-SCHOLES PROPREMENT DIT
         * ====================================
         * On utilise les formules mathématiques du modèle
         */
        const auto [d1, d2] = FastMath::black_scholes_d1_d2(S, K, T, r, vol);
        /*
         * Structured binding (C++17) : récupère d1 et d2 d'un coup
         * d1 et d2 sont les paramètres clés du modèle Black-Scholes
         */
        
        /*
         * FORMULES DE BLACK-SCHOLES
         * =========================
         */
        const double price = is_call 
            ? S * FastMath::norm_cdf(d1) - K * std::exp(-r * T) * FastMath::norm_cdf(d2)
            : K * std::exp(-r * T) * FastMath::norm_cdf(-d2) - S * FastMath::norm_cdf(-d1);
        /*
         * CALL : C = S×N(d1) - K×e^(-rT)×N(d2)
         * PUT  : P = K×e^(-rT)×N(-d2) - S×N(-d1)
         * 
         * Où :
         * - S = prix spot
         * - K×e^(-rT) = valeur présente du strike
         * - N(x) = fonction de répartition normale
         * - d1, d2 = paramètres calculés par FastMath
         * 
         * Interprétation économique :
         * - Premier terme : valeur espérée de l'actif si exercé
         * - Deuxième terme : coût d'exercice actualisé
         */
        
        /*
         * STOCKAGE EN CACHE ET RETOUR
         * ============================
         */
        cache_[cache_key] = price;  // Sauvegarde pour la prochaine fois
        return expected<double, RiskError>{price};  // Retourne le résultat
    }
    
    /*
     * CALCUL DU DELTA
     * ===============
     * Delta = sensibilité du prix de l'option au prix de l'actif sous-jacent
     * "Si le sous-jacent monte de 1$, l'option monte de Delta$"
     */
    [[nodiscard]] double delta(double S, double K, double T, double r, double vol, bool is_call = true) const {
        // Cas dégénérés
        if (T <= 0 || vol <= 0) {
            return is_call ? (S > K ? 1.0 : 0.0) : (S < K ? -1.0 : 0.0);
            /*
             * À l'expiration :
             * - Call in-the-money : Delta = 1 (suit parfaitement le sous-jacent)
             * - Call out-of-the-money : Delta = 0 (ne bouge pas)
             * - Put in-the-money : Delta = -1 (mouvement inverse)
             * - Put out-of-the-money : Delta = 0
             */
        }
        
        const auto [d1, d2] = FastMath::black_scholes_d1_d2(S, K, T, r, vol);
        return is_call ? FastMath::norm_cdf(d1) : FastMath::norm_cdf(d1) - 1.0;
        /*
         * FORMULE :
         * - Call Delta = N(d1)
         * - Put Delta = N(d1) - 1
         * 
         * Plage de valeurs :
         * - Call : 0 à 1 (toujours positif)
         * - Put : -1 à 0 (toujours négatif)
         * 
         * Interprétation :
         * - Delta = 0.5 → si sous-jacent +1$, option +0.50$
         * - Delta = -0.3 → si sous-jacent +1$, option -0.30$
         */
    }
    
    /*
     * CALCUL DU GAMMA
     * ===============
     * Gamma = sensibilité du Delta au prix du sous-jacent
     * "Comment le Delta change quand le prix change"
     */
    [[nodiscard]] double gamma(double S, double K, double T, double r, double vol) const {
        if (T <= 0 || vol <= 0) return 0.0;  // Pas de convexité à l'expiration
        
        const auto [d1, d2] = FastMath::black_scholes_d1_d2(S, K, T, r, vol);
        return FastMath::norm_pdf(d1) / (S * vol * std::sqrt(T));
        /*
         * FORMULE : Γ = φ(d1) / (S × σ × √T)
         * 
         * Où φ(d1) = densité normale (forme de cloche)
         * 
         * Interprétation :
         * - Gamma élevé → Delta change beaucoup (risque de convexité)
         * - Gamma faible → Delta stable
         * - Toujours positif pour calls ET puts
         * - Maximum pour les options at-the-money
         */
    }
    
    /*
     * CALCUL DU VEGA
     * ==============
     * Vega = sensibilité du prix à la volatilité
     * "Si la volatilité monte de 1%, l'option monte de Vega$"
     */
    [[nodiscard]] double vega(double S, double K, double T, double r, double vol) const {
        if (T <= 0 || vol <= 0) return 0.0;  // Pas de sensibilité vol à l'expiration
        
        const auto [d1, d2] = FastMath::black_scholes_d1_d2(S, K, T, r, vol);
        return S * FastMath::norm_pdf(d1) * std::sqrt(T) / 100.0; // Per 1% vol change
        /*
         * FORMULE : ν = S × φ(d1) × √T / 100
         * 
         * Division par 100 = pour un changement de 1% (pas 100%)
         * 
         * Interprétation :
         * - Vega élevé → très sensible à la volatilité
         * - Vega = 0 → pas affecté par les changements de volatilité
         * - Toujours positif (plus de volatilité = plus de valeur)
         * - Plus élevé pour les options à long terme
         */
    }
    
    /*
     * CALCUL DU THETA
     * ===============
     * Theta = sensibilité du prix au passage du temps
     * "Combien l'option perd de valeur chaque jour"
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
        /*
         * FORMULE COMPLEXE avec deux termes :
         * - term1 : effet de la diffusion (toujours négatif)
         * - term2 : effet du taux d'intérêt (différent pour call/put)
         * 
         * Division par 365 = décroissance par jour (pas par année)
         * 
         * Interprétation :
         * - Theta généralement négatif (option perd de la valeur)
         * - "Time decay" = ennemi de l'acheteur d'option
         * - Plus important près de l'expiration
         */
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
    [[nodiscard]] Greeks calculate_all_greeks(double S, double K, double T, double r, double vol, bool is_call = true) const {
        // Cas dégénérés : expiration ou volatilité nulle
        if (T <= 0 || vol <= 0) {
            return {
                .delta = is_call ? (S > K ? 1.0 : 0.0) : (S < K ? -1.0 : 0.0),
                .gamma = 0.0,
                .vega = 0.0,
                .theta = 0.0
            };
            /*
             * Syntaxe C++20 : initialisation nommée (.delta = ...)
             * Plus claire que l'ordre positionnel
             */
        }
        
        /*
         * CALCULS COMMUNS RÉUTILISÉS
         * ===========================
         * On calcule une fois les valeurs utilisées plusieurs fois
         */
        const auto [d1, d2] = FastMath::black_scholes_d1_d2(S, K, T, r, vol);
        const double sqrt_T = std::sqrt(T);
        const double pdf_d1 = FastMath::norm_pdf(d1);  // Réutilisé pour Gamma, Vega, Theta
        
        /*
         * CALCUL DE CHAQUE GREEK
         * ======================
         */
        const double delta_val = is_call ? FastMath::norm_cdf(d1) : FastMath::norm_cdf(d1) - 1.0;
        const double gamma_val = pdf_d1 / (S * vol * sqrt_T);
        const double vega_val = S * pdf_d1 * sqrt_T / 100.0;
        
        // Theta nécessite des calculs supplémentaires
        const double term1 = -S * pdf_d1 * vol / (2 * sqrt_T);
        const double term2 = r * K * std::exp(-r * T);
        const double theta_val = is_call 
            ? (term1 - term2 * FastMath::norm_cdf(d2)) / 365.0
            : (term1 + term2 * FastMath::norm_cdf(-d2)) / 365.0;
        
        /*
         * RETOUR DE LA STRUCTURE COMPLÈTE
         * ================================
         */
        return {
            .delta = delta_val,
            .gamma = gamma_val,
            .vega = vega_val,
            .theta = theta_val
        };
        /*
         * Avantage : un seul appel pour obtenir tous les Greeks
         * Performance : ~3x plus rapide que 4 appels séparés
         */
    /*
     * PRICING BASÉ SUR COURBE FORWARD (NOUVEAUTÉ)
     * ============================================
     * Au lieu d'utiliser un prix spot, on utilise la courbe forward
     * Plus réaliste pour les commodités
     */
    [[nodiscard]] expected<double, RiskError> price_from_curve(
        const ForwardCurve& curve,
        double K,              // Prix d'exercice
        double T,              // Temps jusqu'à expiration
        double r,              // Taux sans risque
        double vol,            // Volatilité
        bool is_call = true    // true = Call, false = Put
    ) const {
        
        // Validation des paramètres
        if (vol <= 0) return expected<double, RiskError>{RiskError::INVALID_VOLATILITY};
        if (T < 0) return expected<double, RiskError>{RiskError::NEGATIVE_TIME};
        if (K <= 0) return expected<double, RiskError>{RiskError::INVALID_STRIKE};
        
        // Récupération du prix forward depuis la courbe
        const double F = curve.get_forward(T);  // Prix forward à maturité T
        if (F <= 0) return expected<double, RiskError>{RiskError::COMPUTATION_FAILED};
        
        // Cas spécial : expiration immédiate
        if (T == 0) {
            return expected<double, RiskError>{
                is_call ? std::max(F - K, 0.0) : std::max(K - F, 0.0)
            };
        }
        
        /*
         * MODÈLE BLACK-76 (VERSION FORWARD)
         * ==================================
         * Plus approprié pour commodités que Black-Scholes classique
         * Utilise directement le prix forward au lieu de spot + carry
         */
        const auto [d1, d2] = FastMath::black_scholes_d1_d2(F, K, T, r, vol);
        
        const double price = is_call 
            ? std::exp(-r * T) * (F * FastMath::norm_cdf(d1) - K * FastMath::norm_cdf(d2))
            : std::exp(-r * T) * (K * FastMath::norm_cdf(-d2) - F * FastMath::norm_cdf(-d1));
        
        return expected<double, RiskError>{price};
        /*
         * DIFFÉRENCE AVEC BLACK-SCHOLES CLASSIQUE :
         * - Utilise F (forward) au lieu de S (spot)
         * - Discount factor exp(-rT) appliqué à tout le payoff
         * - Plus approprié pour contrats à terme sur commodités
         */
    }
    
    /*
     * GREEKS BASÉS SUR COURBE FORWARD
     * ===============================
     * Sensibilités par rapport à la courbe forward
     */
    [[nodiscard]] double delta_from_curve(const ForwardCurve& curve, double K, double T, double r, double vol, bool is_call = true) const {
        if (T <= 0 || vol <= 0) {
            const double F = curve.get_forward(T);
            return is_call ? (F > K ? 1.0 : 0.0) : (F < K ? -1.0 : 0.0);
        }
        
        const double F = curve.get_forward(T);
        const auto [d1, d2] = FastMath::black_scholes_d1_d2(F, K, T, r, vol);
        
        return std::exp(-r * T) * (is_call ? FastMath::norm_cdf(d1) : FastMath::norm_cdf(d1) - 1.0);
    }
    
    /*
     * NOUVEAU : SENSIBILITÉ À LA COURBE (CURVE DELTA)
     * ================================================
     * Mesure l'impact d'un shift de la courbe forward sur le prix de l'option
     */
    [[nodiscard]] std::vector<double> calculate_curve_sensitivities(
        const ForwardCurve& curve,
        double K, double T, double r, double vol, bool is_call,
        std::span<const double> bump_tenors) const {
        
        std::vector<double> sensitivities;
        sensitivities.reserve(bump_tenors.size());
        
        // Prix de base
        const auto base_price = price_from_curve(curve, K, T, r, vol, is_call);
        if (!base_price.has_value()) {
            return std::vector<double>(bump_tenors.size(), 0.0);
        }
        
        // Test de choc sur chaque point de la courbe
        for (double tenor : bump_tenors) {
            ForwardCurve bumped_curve = curve;  // Copie
            
            // Shock de +1bp sur ce point de maturité
            const double original_forward = bumped_curve.get_forward(tenor);
            bumped_curve.add_point(tenor, original_forward * 1.0001); // +0.01%
            
            const auto bumped_price = price_from_curve(bumped_curve, K, T, r, vol, is_call);
            
            if (bumped_price.has_value()) {
                sensitivities.push_back(bumped_price.value() - base_price.value());
            } else {
                sensitivities.push_back(0.0);
            }
        }
        
        return sensitivities;
    }
};

// Déclaration forward pour éviter les dépendances circulaires
class ForwardCurve;

/*
 * RÉSUMÉ DE CE FICHIER :
 * ======================
 * 
 * CLASSE BlackScholesModel :
 * 1. PRICE() : Calcule le prix d'une option (Call ou Put)
 * 2. DELTA() : Sensibilité au prix du sous-jacent
 * 3. GAMMA() : Sensibilité du Delta (convexité)
 * 4. VEGA() : Sensibilité à la volatilité
 * 5. THETA() : Décroissance temporelle
 * 6. CALCULATE_ALL_GREEKS() : Tous les Greeks en une fois
 * 
 * OPTIMISATIONS :
 * - Cache des résultats (évite les recalculs)
 * - Validation robuste des entrées
 * - Gestion d'erreurs avec expected<>
 * - Calcul groupé des Greeks
 * 
 * USAGE TYPIQUE :
 * BlackScholesModel model;
 * auto price_result = model.price(100, 105, 0.25, 0.05, 0.20, true);
 * if (price_result.has_value()) {
 *     std::cout << "Prix du Call: " << price_result.value() << std::endl;
 *     auto greeks = model.calculate_all_greeks(100, 105, 0.25, 0.05, 0.20, true);
 *     std::cout << "Delta: " << greeks.delta << std::endl;
 * }
 */