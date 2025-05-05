import numpy as np
import matplotlib.pyplot as plt
from scipy.stats import norm
import pandas as pd
import seaborn as sns

class CreditRiskMonteCarlo:
    def __init__(self, loans, pd_mean, pd_std, lgd_mean, lgd_std, correlation=0.15):
        """
        Initialisation du modèle de simulation Monte Carlo pour le risque de crédit
        basé sur le modèle à un facteur de Vasicek/Merton
        
        Paramètres:
        - loans: DataFrame avec les colonnes 'amount' (montant du prêt) et 'rating' (notation)
        - pd_mean: probabilité de défaut moyenne
        - pd_std: écart-type de la probabilité de défaut
        - lgd_mean: perte en cas de défaut moyenne (Loss Given Default) = 1 - taux de recouvrement moyen
        - lgd_std: écart-type de la perte en cas de défaut
        - correlation: corrélation entre les défauts (ρ)
        """
        self.loans = loans
        self.pd_mean = pd_mean
        self.lgd_mean = lgd_mean
        self.lgd_std = lgd_std
        self.correlation = correlation
        self.num_loans = len(loans)
        self.results = []
        
    def generate_systematic_factor(self):
        """Génère un facteur systématique (F) qui affecte tous les prêts"""
        return np.random.normal(0, 1)
    
    def generate_idiosyncratic_factor(self):
        """Génère un facteur idiosyncratique (ε) pour chaque prêt"""
        return np.random.normal(0, 1)
    
    def simulate_single_scenario(self, F=None):
        """
        Simule un seul scénario économique avec un facteur systémique donné
        
        Paramètres:
        - z: facteur systémique (si None, il sera généré aléatoirement)
        
        Retourne:
        - Un dictionnaire avec les résultats du scénario
        """
        # Facteur systématique (état de l'économie)
        if F is None:
            F = self.generate_systematic_factor()
            
        # Initialiser les résultats
        total_loss = 0
        defaults = 0
        
        # Pour chaque prêt
        for i in range(self.num_loans):
            loan_amount = self.loans.loc[i, 'amount']
            
            # Facteur idiosyncratique (spécifique au prêt)
            Z_i = self.generate_idiosyncratic_factor()
            
            # Variable latente selon la formulation de Vasicek
            U_i = np.sqrt(self.correlation) * F + np.sqrt(1 - self.correlation) * Z_i
            
            # Utiliser une approche plus cohérente avec le modèle de Merton:
            default_threshold = norm.ppf(self.pd_mean)  # Seuil de défaut basé sur PD moyenne
            is_default = U_i < default_threshold  # Défaut si U_i sous le seuil
            
            # Simulation du défaut
            if is_default:
                defaults += 1
                # Perte en cas de défaut (LGD)
                lgd = np.random.normal(self.lgd_mean, self.lgd_std)
                lgd = max(0, min(1, lgd))  # Limiter entre 0 et 1
                
                # Calcul de la perte
                loss = loan_amount * lgd
                total_loss += loss
        
        return {
            'z_factor': F,
            'total_loss': total_loss,
            'defaults': defaults,
            'default_rate': defaults / self.num_loans
        }
    
    def simulate_defaults(self, num_simulations):
        """Exécute la simulation Monte Carlo en utilisant simulate_single_scenario"""
        self.results = []
        
        for _ in range(num_simulations):
            # Simuler un seul scénario et ajouter les résultats
            scenario_result = self.simulate_single_scenario()
            self.results.append(scenario_result)
        
        return pd.DataFrame(self.results)
        
    def plot_loss_distribution(self):
        """Affiche la distribution des pertes et calcule les mesures de risque"""
        if not self.results:
            raise ValueError("Exécutez d'abord la simulation avec simulate_defaults()")
        
        results_df = pd.DataFrame(self.results)
        
        plt.figure(figsize=(14, 8))
        
        # Distribution des pertes
        plt.subplot(2, 2, 1)
        sns.histplot(results_df['total_loss'], kde=True)
        plt.title('Distribution des Pertes Totales')
        plt.xlabel('Perte Totale')
        plt.ylabel('Fréquence')
        
        # Distribution du taux de défaut
        plt.subplot(2, 2, 2)
        sns.histplot(results_df['default_rate'], kde=True)
        plt.title('Distribution du Taux de Défaut')
        plt.xlabel('Taux de Défaut')
        plt.ylabel('Fréquence')
        
        # Calcul de la VaR et ES
        var_95 = np.percentile(results_df['total_loss'], 95)
        var_99 = np.percentile(results_df['total_loss'], 99)
        var_999 = np.percentile(results_df['total_loss'], 99.9)  # Ajout du niveau de confiance 99.9%
        es_95 = results_df[results_df['total_loss'] >= var_95]['total_loss'].mean()
        
        plt.subplot(2, 2, 3)
        sns.histplot(results_df['total_loss'], kde=True)
        plt.axvline(x=var_95, color='r', linestyle='--', label=f'VaR 95%: {var_95:.2f}')
        plt.axvline(x=var_99, color='g', linestyle='--', label=f'VaR 99%: {var_99:.2f}')
        plt.axvline(x=var_999, color='b', linestyle='--', label=f'VaR 99.9%: {var_999:.2f}')
        plt.axvline(x=es_95, color='orange', linestyle='--', label=f'ES 95%: {es_95:.2f}')
        plt.title('Mesures de Risque')
        plt.xlabel('Perte Totale')
        plt.ylabel('Fréquence')
        plt.legend()
        
        # QQ-plot pour vérifier la normalité
        plt.subplot(2, 2, 4)
        from scipy import stats
        stats.probplot(results_df['total_loss'], dist="norm", plot=plt)
        plt.title('QQ-Plot des Pertes (vs Distribution Normale)')
        
        plt.tight_layout()
        plt.show()
        
        # Ajout d'un graphique pour visualiser la relation entre le facteur systémique et les pertes
        plt.figure(figsize=(10, 6))
        plt.scatter(results_df['z_factor'], results_df['total_loss'], alpha=0.3)
        plt.title('Relation entre Facteur Économique et Pertes')
        plt.xlabel('Facteur Systémique (F)')
        plt.ylabel('Perte Totale')
        plt.axhline(y=var_95, color='r', linestyle='--', label=f'VaR 95%: {var_95:.2f}')
        plt.axhline(y=var_99, color='g', linestyle='--', label=f'VaR 99%: {var_99:.2f}')
        plt.legend()
        plt.show()
        
        return {
            'VaR_95': var_95,
            'VaR_99': var_99,
            'VaR_999': var_999,  # Ajout du niveau de confiance 99.9%
            'ES_95': es_95,
            'mean_loss': results_df['total_loss'].mean(),
            'max_loss': results_df['total_loss'].max(),
            'mean_default_rate': results_df['default_rate'].mean()
        }
    
    def run_stress_testing(self, stress_levels=[-1, -2, -3]):
        """
        Exécute des simulations avec des facteurs économiques stressés
        
        Paramètres:
        - stress_levels: liste des niveaux de stress systémique à tester
        
        Retourne:
        - DataFrame avec les résultats par scénario
        """
        # Scénario de base (z = 0)
        base_scenario = self.simulate_single_scenario(F=0)
        
        # Scénarios de stress
        stress_results = [base_scenario]
        
        for stress_level in stress_levels:
            # Exécuter une simulation avec ce niveau de stress
            stress_scenario = self.simulate_single_scenario(F=stress_level)
            stress_results.append(stress_scenario)
        
        # Créer un DataFrame avec les résultats
        scenarios = ['Base'] + [f'Stress {-level}σ' for level in stress_levels]
        stress_df = pd.DataFrame({
            'Scénario': scenarios,
            'Facteur systémique': [0] + stress_levels,
            'Perte totale': [result['total_loss'] for result in stress_results],
            'Taux de défaut': [result['default_rate'] for result in stress_results]
        })
        
        # Visualisation des résultats du stress testing
        plt.figure(figsize=(12, 8))
        
        plt.subplot(2, 1, 1)
        sns.barplot(x='Scénario', y='Perte totale', data=stress_df)
        plt.title('Pertes par Scénario de Stress')
        plt.ylabel('Perte Totale')
        
        plt.subplot(2, 1, 2)
        sns.barplot(x='Scénario', y='Taux de défaut', data=stress_df)
        plt.title('Taux de Défaut par Scénario de Stress')
        plt.ylabel('Taux de Défaut')
        
        plt.tight_layout()
        plt.show()
        
        return stress_df

# Exemple d'utilisation:
if __name__ == "__main__":
    # Création d'un portefeuille de prêts
    np.random.seed(42)
    num_loans = 1000
    
    # Générer un portefeuille diversifié
    ratings = np.random.choice(['AAA', 'AA', 'A', 'BBB', 'BB', 'B', 'CCC'], size=num_loans, 
                              p=[0.05, 0.1, 0.2, 0.3, 0.2, 0.1, 0.05])
    
    # Associer des probabilités de défaut aux notations (plus réaliste)
    rating_to_pd = {
        'AAA': 0.0001,
        'AA': 0.0010,
        'A': 0.0050,
        'BBB': 0.0200,
        'BB': 0.0800,
        'B': 0.1500,
        'CCC': 0.3000
    }
    
    # Les montants suivent une distribution log-normale
    loan_amounts = np.random.lognormal(mean=11, sigma=1, size=num_loans)
    
    # Créer le DataFrame des prêts
    loans_df = pd.DataFrame({
        'amount': loan_amounts,
        'rating': ratings,
        'pd': [rating_to_pd[r] for r in ratings]  # Probabilité de défaut basée sur la notation
    })
    
    # Paramètres de la simulation
    pd_mean = 0.05  # 5% probabilité de défaut moyenne
    pd_std = 0.02   # écart-type
    lgd_mean = 0.45  # 45% perte moyenne en cas de défaut (1 - recovery rate)
    lgd_std = 0.1    # écart-type
    correlation = 0.2  # corrélation entre les défauts
    
    # Créer et exécuter la simulation
    sim = CreditRiskMonteCarlo(loans_df, pd_mean, pd_std, lgd_mean, lgd_std, correlation)
    results = sim.simulate_defaults(10000)  # 10,000 simulations
    
    # Afficher les résultats
    risk_metrics = sim.plot_loss_distribution()
    print("\nRésultats de la simulation:")
    for metric, value in risk_metrics.items():
        print(f"{metric}: {value:,.2f}")
    
    # Exécuter le stress testing
    print("\nExécution du stress testing...")
    stress_results = sim.run_stress_testing(stress_levels=[-1, -2, -3, -4, -5, -6])
    print("\nRésultats du stress testing:")
    print(stress_results)