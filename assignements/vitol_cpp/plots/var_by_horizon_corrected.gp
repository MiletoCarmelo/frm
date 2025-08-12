# Gnuplot script auto-generated
set terminal png size 1200,800 enhanced font 'Arial,12'
set output './plots/var_by_horizon_corrected.png'

set title 'VaR 99.5% by Investment Horizon' font 'Arial,16'
set xlabel 'Horizon (days)'
set ylabel 'VaR 99.5% (%)'
set grid
set style line 1 lc rgb 'blue' lw 2

plot './plots/var_by_horizon_corrected.dat' using 1:2 with lines linestyle 1 title 'Data'
