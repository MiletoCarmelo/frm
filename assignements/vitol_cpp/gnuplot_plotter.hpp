#pragma once

#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <cstdlib>

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