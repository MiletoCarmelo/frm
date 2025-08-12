# Gnuplot script auto-generated
set terminal png size 1200,800 enhanced font 'Arial,12'
set output './plots/error_pct_by_horizon.png'

set title 'Monte Carlo Price Error (%) by Investment Horizon' font 'Arial,16'
set xlabel 'Horizon (days)'
set ylabel 'Error (%)'
set grid
set style line 1 lc rgb 'blue' lw 2

plot './plots/error_pct_by_horizon.dat' using 1:2 with lines linestyle 1 title 'Data'
