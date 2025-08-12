set terminal png size 1200,800 enhanced font 'Arial,12'
set output './plots/mc_draws.png'

set title 'Monte Carlo Price Paths' font 'Arial,16'
set xlabel 'Time Steps'
set ylabel 'Price ($)'
set grid
set key outside right

plot './plots/mc_draws.dat' using 1:2 with lines lc rgb 'blue' lw 2 title 'Price ($)', './plots/mc_draws.dat' using 1:3 with lines lc rgb 'red' lw 2 title 'Error (%)', './plots/mc_draws.dat' using 1:4 with lines lc rgb 'green' lw 2 title 'BS Price ($)'
