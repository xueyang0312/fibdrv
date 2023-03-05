set term png enhanced font 'Verdana,10'
set output 'Fibonacci_recursive.png'
set title "Fibonacci recursive"
set xlabel "F(n)"
set ylabel "Time(ns)"
plot "Fibonacci_recursive.txt" using 1:2 with points title "utime", \
     "Fibonacci_recursive.txt" using 1:3 with points title "ktime", \
     "Fibonacci_recursive.txt" using 1:4 with points title "kernel to user"
