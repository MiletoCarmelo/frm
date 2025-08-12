#pragma once

#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <numeric> 

class GnuplotPlotter {
private:
    std::string output_dir_;
    
public:
    explicit GnuplotPlotter(const std::string& output_dir = "./plots/") 
        : output_dir_(output_dir) {
        std::cout << "Plots will be saved to: " << output_dir_ << std::endl;
    }
    
    /*
     * PLOT SÉRIE TEMPORELLE AVEC GNUPLOT
     * ===================================
     * 100% C++, pas de Python !
     */
    void plot_timeseries(
        const std::vector<double>& values,
        const std::string& filename = "timeseries",
        const std::string& title = "Time Series",
        const std::string& ylabel = "Value",
        const std::string& xlabel = "Time"
    ) const {
        
        // 1. Sauvegarder les données
        std::string data_file = output_dir_ + filename + ".dat";
        save_data(values, data_file);
        
        // 2. Créer le script Gnuplot
        std::string gnuplot_script = output_dir_ + filename + ".gp";
        create_gnuplot_script(data_file, gnuplot_script, title, xlabel, ylabel, filename);
        
        // 3. Exécuter Gnuplot
        std::string command = "gnuplot " + gnuplot_script;
        std::cout << "Executing: " << command << std::endl;
        
        int result = std::system(command.c_str());
        if (result == 0) {
            std::cout << "Plot saved as: " << output_dir_ << filename << ".png" << std::endl;
        } else {
            std::cout << "Error: Make sure gnuplot is installed" << std::endl;
            std::cout << "Install with: sudo apt install gnuplot (Linux) or brew install gnuplot (Mac)" << std::endl;
        }
    }
    
    /*
     * PLOT MULTIPLE SÉRIES
     */
    void plot_multiple_timeseries(
        const std::vector<std::vector<double>>& series,
        const std::vector<std::string>& labels,
        const std::string& filename = "multiple_series",
        const std::string& title = "Multiple Time Series",
        const std::string& ylabel = "Value",
        const std::string& xlabel = "Time"
    ) const {
        
        // Sauvegarder données multi-colonnes
        std::string data_file = output_dir_ + filename + ".dat";
        save_multiple_data(series, data_file);
        
        // Script Gnuplot pour multiple series
        std::string gnuplot_script = output_dir_ + filename + ".gp";
        create_multiple_gnuplot_script(data_file, gnuplot_script, title, xlabel, ylabel, filename, labels);
        
        // Exécuter
        std::string command = "gnuplot " + gnuplot_script;
        int result = std::system(command.c_str());
        
        if (result == 0) {
            std::cout << "Multiple series plot saved as: " << output_dir_ << filename << ".png" << std::endl;
        }
    }

    /*
     * PLOT MULTIPLE DRAWS (TRAJECTOIRES) MONTE CARLO
     * ==============================================
     * Chaque draw = une trajectoire de prix simulée
     */
    void plot_multiple_draws(
        const std::vector<std::vector<double>>& draws,  // Chaque vector = une trajectoire
        const std::vector<std::string>& labels = {},
        const std::string& filename = "mc_draws",
        const std::string& title = "Monte Carlo Price Paths",
        const std::string& ylabel = "Price ($)",
        const std::string& xlabel = "Time Steps",
        size_t max_draws_to_plot = 10  // Limite pour lisibilité
    ) const {
        
        std::cout << "Plotting " << std::min(draws.size(), max_draws_to_plot) 
                  << " draws out of " << draws.size() << " total draws\n";
        
        // Sélectionner un échantillon de draws pour éviter surcharge visuelle
        std::vector<std::vector<double>> selected_draws;
        std::vector<std::string> draw_labels;
        if (labels.size() == 0){
            
            size_t step = std::max(size_t(1), draws.size() / max_draws_to_plot);
            
            for (size_t i = 0; i < draws.size(); i += step) {
                if (selected_draws.size() >= max_draws_to_plot) break;
                
                selected_draws.push_back(draws[i]);
                draw_labels.push_back("Draw " + std::to_string(i + 1));
            }
        }
        else {
            selected_draws = draws;
            draw_labels = labels;
        }

        // Utiliser la fonction multiple_timeseries existante
        plot_multiple_timeseries(selected_draws, draw_labels, filename, title, ylabel, xlabel);
    }
    
    /*
     * PLOT DRAWS AVEC STATISTIQUES (moyenne, percentiles)
     * ===================================================
     */
    void plot_draws_with_statistics(
        const std::vector<std::vector<double>>& draws,
        const std::vector<std::string>& labels,
        const std::string& filename = "mc_draws_stats",
        const std::string& title = "Monte Carlo Paths with Statistics",
        size_t n_sample_draws = 5  // Nombre de draws individuels à afficher
    ) const {
        
        if (draws.empty()) return;
        
        // Calculer statistiques à chaque step
        size_t n_steps = draws[0].size();
        std::vector<double> mean_path(n_steps);
        std::vector<double> percentile_5(n_steps);
        std::vector<double> percentile_95(n_steps);
        
        for (size_t step = 0; step < n_steps; ++step) {
            std::vector<double> values_at_step;
            for (const auto& draw : draws) {
                if (step < draw.size()) {
                    values_at_step.push_back(draw[step]);
                }
            }
            
            if (!values_at_step.empty()) {
                std::sort(values_at_step.begin(), values_at_step.end());
                
                mean_path[step] = std::accumulate(values_at_step.begin(), values_at_step.end(), 0.0) / values_at_step.size();
                
                size_t idx_5 = static_cast<size_t>(0.05 * values_at_step.size());
                size_t idx_95 = static_cast<size_t>(0.95 * values_at_step.size());
                
                percentile_5[step] = values_at_step[idx_5];
                percentile_95[step] = values_at_step[idx_95];
            }
        }
        
        // Préparer séries pour le plot
        std::vector<std::vector<double>> all_series;
        std::vector<std::string> all_labels;
        
        // Ajouter statistiques
        all_series.push_back(mean_path);
        all_labels.push_back("Mean Path");
        
        all_series.push_back(percentile_5);
        all_labels.push_back("5th Percentile");
        
        all_series.push_back(percentile_95);
        all_labels.push_back("95th Percentile");
        
        // Ajouter quelques draws échantillons
        size_t step = std::max(size_t(1), draws.size() / n_sample_draws);
        for (size_t i = 0; i < draws.size() && all_series.size() < (3 + n_sample_draws); i += step) {
            all_series.push_back(draws[i]);
            if (labels.size() > 0) {
                all_labels.push_back(labels[i]);
            } else {
                all_labels.push_back("Sample Path " + std::to_string(i + 1));
            }
        }
        
        plot_multiple_timeseries(all_series, all_labels, filename, title, "Price ($)", "Time Steps");
    }

    /*
     * STRUCTURE POUR VaR/ES AVEC INTERVALLES DE CONFIANCE
     * ====================================================
     */
    struct RiskMetrics {
        double var = 0.0;
        double es = 0.0;
        double var_ci_lower = 0.0;
        double var_ci_upper = 0.0;
        double es_ci_lower = 0.0;
        double es_ci_upper = 0.0;
        
        // Flags pour savoir quoi afficher
        bool has_var = false;
        bool has_es = false;
        bool has_var_ci = false;
        bool has_es_ci = false;
    };

    /*
     * PLOT HISTOGRAMME SIMPLE
     * =======================
     * Pour distributions de rendements, VaR, etc.
     */
    void plot_histogram(
        const std::vector<double>& data,
        const std::string& filename = "histogram",
        const std::string& title = "Histogram",
        const std::string& xlabel = "Value",
        const std::string& ylabel = "Frequency",
        size_t n_bins = 50
    ) const {
        
        if (data.empty()) {
            std::cout << "Warning: Empty data for histogram" << std::endl;
            return;
        }
        
        // 1. Calculer les bins
        auto [min_val, max_val] = std::minmax_element(data.begin(), data.end());
        double range = *max_val - *min_val;
        double bin_width = range / n_bins;
        
        std::cout << "Histogram range: [" << *min_val << ", " << *max_val << "]" << std::endl;
        std::cout << "Bin width: " << bin_width << std::endl;
        
        // 2. Créer les bins
        std::vector<double> bin_centers(n_bins);
        std::vector<int> bin_counts(n_bins, 0);
        
        for (size_t i = 0; i < n_bins; ++i) {
            bin_centers[i] = *min_val + (i + 0.5) * bin_width;
        }
        
        // 3. Compter les occurrences
        for (double value : data) {
            int bin_idx = static_cast<int>((value - *min_val) / bin_width);
            bin_idx = std::max(0, std::min(static_cast<int>(n_bins - 1), bin_idx));
            bin_counts[bin_idx]++;
        }
        
        // 4. Sauvegarder données histogram
        std::string data_file = output_dir_ + filename + ".dat";
        save_histogram_data(bin_centers, bin_counts, data_file);
        
        // 5. Créer script Gnuplot
        std::string gnuplot_script = output_dir_ + filename + ".gp";
        create_histogram_script(data_file, gnuplot_script, title, xlabel, ylabel, filename, bin_width);
        
        // 6. Exécuter
        std::string command = "gnuplot " + gnuplot_script;
        int result = std::system(command.c_str());
        if (result == 0) {
            std::cout << "Histogram saved as: " << output_dir_ << filename << ".png" << std::endl;
        } else {
            std::cout << "Error executing gnuplot for histogram" << std::endl;
        }

    }
        
    /*
     * PLOT HISTOGRAMME AVEC VaR/ES ET INTERVALLES DE CONFIANCE
     * =========================================================
     * Affiche distribution + lignes VaR/ES + zones de confiance
     */
    void plot_histogram_with_risk_metrics(
        const std::vector<double>& data,
        const RiskMetrics& metrics,
        const std::string& filename = "risk_histogram",
        const std::string& title = "Distribution with Risk Metrics",
        const std::string& xlabel = "Returns (%)",
        const std::string& ylabel = "Frequency",
        size_t n_bins = 50
    ) const {
        
        if (data.empty()) {
            std::cout << "Warning: Empty data for histogram" << std::endl;
            return;
        }
        
        // 1. Calculer les bins (même logique que histogram simple)
        auto [min_val, max_val] = std::minmax_element(data.begin(), data.end());
        double range = *max_val - *min_val;
        double bin_width = range / n_bins;
        
        // 2. Créer les bins
        std::vector<double> bin_centers(n_bins);
        std::vector<int> bin_counts(n_bins, 0);
        
        for (size_t i = 0; i < n_bins; ++i) {
            bin_centers[i] = *min_val + (i + 0.5) * bin_width;
        }
        
        // 3. Compter les occurrences
        for (double value : data) {
            int bin_idx = static_cast<int>((value - *min_val) / bin_width);
            bin_idx = std::max(0, std::min(static_cast<int>(n_bins - 1), bin_idx));
            bin_counts[bin_idx]++;
        }
        
        // 4. Trouver max frequency pour scaling des lignes
        int max_freq = *std::max_element(bin_counts.begin(), bin_counts.end());
        
        // 5. Sauvegarder données
        std::string data_file = output_dir_ + filename + ".dat";
        save_histogram_data(bin_centers, bin_counts, data_file);
        
        // 6. Créer script avec VaR/ES
        std::string gnuplot_script = output_dir_ + filename + ".gp";
        create_risk_histogram_script(data_file, gnuplot_script, title, xlabel, ylabel, 
                                    filename, bin_width, metrics, max_freq);
        
        // 7. Exécuter
        std::string command = "gnuplot " + gnuplot_script;
        int result = std::system(command.c_str());
        
        if (result == 0) {
            std::cout << "Risk histogram saved as: " << output_dir_ << filename << ".png" << std::endl;
        }
    }


private:
    
    // Sauvegarder une série
    void save_data(const std::vector<double>& values, const std::string& filename) const {
        std::ofstream file(filename);
        for (size_t i = 0; i < values.size(); ++i) {
            file << i << " " << std::fixed << std::setprecision(6) << values[i] << "\n";
        }
        file.close();
    }
    
    // Sauvegarder plusieurs séries
    void save_multiple_data(const std::vector<std::vector<double>>& series, const std::string& filename) const {
        std::ofstream file(filename);
        
        // Trouver la taille max
        size_t max_size = 0;
        for (const auto& serie : series) {
            max_size = std::max(max_size, serie.size());
        }
        
        // Écrire les données
        for (size_t i = 0; i < max_size; ++i) {
            file << i;  // Colonne temps
            for (const auto& serie : series) {
                if (i < serie.size()) {
                    file << " " << std::fixed << std::setprecision(6) << serie[i];
                } else {
                    file << " ?";  // Valeur manquante
                }
            }
            file << "\n";
        }
        file.close();
    }
    
    // Créer script Gnuplot simple
    void create_gnuplot_script(
        const std::string& data_file,
        const std::string& script_file,
        const std::string& title,
        const std::string& xlabel,
        const std::string& ylabel,
        const std::string& output_name
    ) const {
        std::ofstream script(script_file);
        
        script << "# Gnuplot script auto-generated\n";
        script << "set terminal png size 1200,800 enhanced font 'Arial,12'\n";
        script << "set output '" << output_dir_ << output_name << ".png'\n\n";
        
        script << "set title '" << title << "' font 'Arial,16'\n";
        script << "set xlabel '" << xlabel << "'\n";
        script << "set ylabel '" << ylabel << "'\n";
        script << "set grid\n";
        script << "set style line 1 lc rgb 'blue' lw 2\n\n";


        script << "plot '" << data_file << "' using 1:2 with lines linestyle 1 title 'Data'\n";
        
        script.close();
    }
    
    // Créer script Gnuplot multiple series
    void create_multiple_gnuplot_script(
        const std::string& data_file,
        const std::string& script_file,
        const std::string& title,
        const std::string& xlabel,
        const std::string& ylabel,
        const std::string& output_name,
        const std::vector<std::string>& labels
    ) const {
        std::ofstream script(script_file);
        
        script << "set terminal png size 1200,800 enhanced font 'Arial,12'\n";
        script << "set output '" << output_dir_ << output_name << ".png'\n\n";
        
        script << "set title '" << title << "' font 'Arial,16'\n";
        script << "set xlabel '" << xlabel << "'\n";
        script << "set ylabel '" << ylabel << "'\n";
        script << "set grid\n";
        script << "set key outside right\n\n";
        
        // Définir les couleurs
        std::vector<std::string> colors = {"blue", "red", "green", "orange", "purple", "brown"};
        
        script << "plot ";
        for (size_t i = 0; i < labels.size(); ++i) {
            if (i > 0) script << ", ";
            script << "'" << data_file << "' using 1:" << (i + 2) 
                   << " with lines lc rgb '" << colors[i % colors.size()] 
                   << "' lw 2 title '" << labels[i] << "'";
        }
        script << "\n";
        
        script.close();
    }

    // Helper pour sauvegarder données histogram
    void save_histogram_data(
        const std::vector<double>& bin_centers, 
        const std::vector<int>& bin_counts, 
        const std::string& filename
    ) const {
        std::ofstream file(filename);
        for (size_t i = 0; i < bin_centers.size(); ++i) {
            file << std::fixed << std::setprecision(6) 
                 << bin_centers[i] << " " << bin_counts[i] << "\n";
        }
        file.close();
    }
    
    // Helper pour script histogram
    void create_histogram_script(
        const std::string& data_file,
        const std::string& script_file,
        const std::string& title,
        const std::string& xlabel,
        const std::string& ylabel,
        const std::string& output_name,
        double bin_width
    ) const {
        std::ofstream script(script_file);
        
        script << "set terminal png size 1200,800 enhanced font 'Arial,12'\n";
        script << "set output '" << output_dir_ << output_name << ".png'\n\n";
        
        script << "set title '" << title << "' font 'Arial,16'\n";
        script << "set xlabel '" << xlabel << "'\n";
        script << "set ylabel '" << ylabel << "'\n";
        script << "set grid\n";
        script << "set style fill solid 0.7 border -1\n";
        script << "set boxwidth " << bin_width * 0.8 << "\n\n";
        
        script << "plot '" << data_file << "' using 1:2 with boxes lc rgb 'steelblue' title 'Frequency'\n";

        script.close();
    }
        
    // Helper pour script histogram avec risk metrics
    void create_risk_histogram_script(
        const std::string& data_file,
        const std::string& script_file,
        const std::string& title,
        const std::string& xlabel,
        const std::string& ylabel,
        const std::string& output_name,
        double bin_width,
        const RiskMetrics& metrics,
        int max_freq
    ) const {
        std::ofstream script(script_file);
        
        script << "set terminal png size 1400,900 enhanced font 'Arial,12'\n";
        script << "set output '" << output_dir_ << output_name << ".png'\n\n";
        
        script << "set title '" << title << "' font 'Arial,16'\n";
        script << "set xlabel '" << xlabel << "'\n";
        script << "set ylabel '" << ylabel << "'\n";
        script << "set grid\n";
        script << "set style fill solid 0.6 border -1\n";
        script << "set boxwidth " << bin_width * 0.8 << "\n";
        script << "set key outside right\n\n";
        
        // Style des lignes
        script << "set style line 1 lc rgb '#FF0000' lw 3 dt 2\n";      // VaR (rouge)
        script << "set style line 2 lc rgb '#8B0000' lw 3 dt 1\n";      // ES (rouge foncé)  
        script << "set style line 3 lc rgb '#FFA500' lw 2 dt 3\n";      // CI VaR (orange)
        script << "set style line 4 lc rgb '#800080' lw 2 dt 3\n";      // CI ES (violet)

        if (metrics.has_es) {
            script << "set arrow from " << metrics.es << ",0 to " << metrics.es  << "," << max_freq * 0.8  << " nohead lc rgb '#000000' lw 5\n";
            script << "set label 'ES' at " << metrics.es  << "," << max_freq * 0.82 << " center tc rgb '#000000'\n";
        }

        if (metrics.has_var) {
            script << "set arrow from " << metrics.var << ",0 to " << metrics.var  << "," << max_freq * 0.8 << " nohead lc rgb '#FF0000' lw 5\n";
            script << "set label 'VaR' at " << metrics.var  << "," << max_freq * 0.82 << " center tc rgb '#FF0000'\n";
        }

        if (metrics.has_es_ci) {
            script << "set arrow from " << metrics.es_ci_lower  << ",0 to " << metrics.es_ci_lower  << "," << max_freq * 0.8 << " nohead lc rgb '#000000' lw 2 dt 5\n";
            script << "set arrow from " << metrics.es_ci_upper  << ",0 to " << metrics.es_ci_upper  << "," << max_freq * 0.8 << " nohead lc rgb '#000000' lw 2 dt 5\n";
        }
        
        script << "plot '" << data_file << "' using 1:2 with boxes lc rgb 'steelblue' title 'Distribution'\n";
        
        script << "\n";
        script.close();
    }


};

// Exemple d'utilisation 100% C++
/*
int main() {
    GnuplotPlotter plotter("./plots/");
    
    // Données d'exemple
    std::vector<double> prices = {100, 102, 98, 105, 103, 107, 104, 109};
    
    // Plot simple
    plotter.plot_timeseries(prices, "stock_prices", 
                           "Stock Price Evolution", "Price ($)", "Days");
    
    return 0;
}
*/


/*
 * USAGE EXEMPLES :
 * 
 * GnuplotPlotter plotter("./plots/");
 * 
 * // 1) Histogram simple
 * plotter.plot_histogram(returns, "returns_hist", 
 *                       "Distribution des Rendements", 
 *                       "Rendement (%)", "Fréquence", 50);
 * 
 * // 2) Histogram avec VaR/ES complets
 * GnuplotPlotter::RiskMetrics metrics;
 * metrics.var = -0.032;  // VaR 95% = -3.2%
 * metrics.es = -0.045;   // ES 95% = -4.5%
 * metrics.has_var = true;
 * metrics.has_es = true;
 * 
 * // Optionnel : ajouter intervalles de confiance
 * metrics.var_ci_lower = -0.028;
 * metrics.var_ci_upper = -0.036;
 * metrics.es_ci_lower = -0.041;
 * metrics.es_ci_upper = -0.049;
 * metrics.has_var_ci = true;
 * metrics.has_es_ci = true;
 * 
 * plotter.plot_histogram_with_risk_metrics(
 *     returns, metrics, "risk_distribution",
 *     "Distribution avec VaR/ES et Intervalles de Confiance",
 *     "Rendements (%)", "Fréquence", 50);
 * 
 * // 3) Usage avec votre Bootstrap
 * SimpleBootstrap bootstrap;
 * auto result = bootstrap.bootstrap_var_es(BootstrapMethod::BLOCK, returns);
 * 
 * GnuplotPlotter::RiskMetrics bootstrap_metrics;
 * bootstrap_metrics.es = result.original_es;
 * bootstrap_metrics.es_ci_lower = result.ci_lower_95;
 * bootstrap_metrics.es_ci_upper = result.ci_upper_95;
 * bootstrap_metrics.has_es = true;
 * bootstrap_metrics.has_es_ci = true;
 * 
 * plotter.plot_histogram_with_risk_metrics(returns, bootstrap_metrics, 
 *                                          "bootstrap_risk_histogram");
 */