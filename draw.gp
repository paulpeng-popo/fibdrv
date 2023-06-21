reset
set terminal pngcairo size 800,600 enhanced font 'Verdana,10'
set output 'fibonacci.png'
set title "perf"
set xlabel "n-th fibonacci number"
set ylabel "time (ns)"
set key left top
set grid
set style data linespoints
plot "data.txt" using 1:2 title "naive-fib", \
     "data.txt" using 1:3 title "fast-bit-fib", \
     "data.txt" using 1:4 title "fast-clz-fib"
