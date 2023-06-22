reset
set terminal pngcairo size 800,600 enhanced font 'Verdana,16'
set output 'perf.png'
set title "perf"
set xlabel "n-th fibonacci number"
set ylabel "time (ns)"
set key left top
set grid
set style data linespoints
plot filename using 1:2 title "naive-fib", \
     filename using 1:3 title "fast-fib w/o clz", \
     filename using 1:4 title "fast-fib w/ clz"
