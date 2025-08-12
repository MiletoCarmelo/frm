set terminal png size 1200,800 enhanced font 'Arial,12'
set output './plots/bootstrap_es_distribution.png'

set title 'Bootstrap ES Distribution' font 'Arial,16'
set xlabel 'ES Value'
set ylabel 'Frequency'
set grid
set style fill solid 0.7 border -1
set boxwidth 0.000189201

plot './plots/bootstrap_es_distribution.dat' using 1:2 with boxes lc rgb 'steelblue' title 'Frequency'
