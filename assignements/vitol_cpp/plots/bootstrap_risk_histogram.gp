set terminal png size 1400,900 enhanced font 'Arial,12'
set output './plots/bootstrap_risk_histogram.png'

set title 'Distribution with Risk Metrics' font 'Arial,16'
set xlabel 'Returns (%)'
set ylabel 'Frequency'
set grid
set style fill solid 0.6 border -1
set boxwidth 0.00301175
set key outside right

set style line 1 lc rgb '#FF0000' lw 3 dt 2
set style line 2 lc rgb '#8B0000' lw 3 dt 1
set style line 3 lc rgb '#FFA500' lw 2 dt 3
set style line 4 lc rgb '#800080' lw 2 dt 3
set arrow from 0.0350862,0 to 0.0350862,73.6 nohead lc rgb '#000000' lw 5
set label 'ES' at 0.0350862,75.44 center tc rgb '#000000'
set arrow from 0.0260576,0 to 0.0260576,73.6 nohead lc rgb '#FF0000' lw 5
set label 'VaR' at 0.0260576,75.44 center tc rgb '#FF0000'
set arrow from 0.0312103,0 to 0.0312103,73.6 nohead lc rgb '#000000' lw 2 dt 5
set arrow from 0.0396876,0 to 0.0396876,73.6 nohead lc rgb '#000000' lw 2 dt 5
plot './plots/bootstrap_risk_histogram.dat' using 1:2 with boxes lc rgb 'steelblue' title 'Distribution'

