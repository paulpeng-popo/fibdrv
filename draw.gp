reset
set terminal pngcairo size 800,600 enhanced font 'Verdana,10'
set output 'naive_vs_fast.png'
set title "perf"
set xlabel "n-th fibonacci number"
set ylabel "time (ns)"
set key left top
set grid
set style data linespoints
plot "data.txt" using 1:2 title "naive-fib", \
     "data.txt" using 1:3 title "fast-fib"


reset
set terminal pngcairo size 800,600 enhanced font 'Verdana,10'
set output 'no_clz_vs_clz.png'
set title "perf"
set xlabel "n-th fibonacci number"
set ylabel "time (ns)"
set key left top
set grid
set style data linespoints
plot "data.txt" using 1:3 title "fib w/o clz", \
     "data.txt" using 1:4 title "fib w/ clz"
