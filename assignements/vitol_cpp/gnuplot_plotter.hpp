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