reset
set terminal pngcairo size 800,600 enhanced font 'Verdana,16'
set output 'bignum.png'
set title "perf"
set xlabel "n-th fibonacci number"
set ylabel "time (ns)"
set key left top
set grid
set style data linespoints
plot filename using 1:2 title "user time", \
     filename using 1:3 title "kernel time", \
     filename using 1:4 title "kernel to user time", \
