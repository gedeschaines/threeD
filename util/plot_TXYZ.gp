#
# A gnuplot script to generate 3D plot of missile/target engagement
# from TXYZ.OUT.# trajectory data file specified as first argument.
#
#   $ gnuplot -s -p -c ./util/plot_TXYZ.gp "./txyz/TXYZ.OUT.#"
#
set terminal qt size 800,800
set title ARG1
set view 65, 310, 1, 1
set xyplane at 25.0
set grid xtics ytics ztics vertical
set xlabel "X"
set ylabel "Y"
set zlabel "Z"
splot [0.0:2500.0] [0.0:-2500.0] [0.0:-1250.0] ARG1 using 3:4:5 every 2 with lines lt 6 lw 2 dt 1 title "Missile", ARG1 using 6:7:8 every 2 with lines lt 7 lw 2 dt 1 title "Target", ARG1 using 3:4:(0.0)  every 2 with lines lt 6 lw 1 dt 2 title "", ARG1 using 6:7:(0.0)  every 2 with lines lt 7 lw 1 dt 2 title "", ARG1 using 3:(-2500.0):5 every 2 with lines lt 6 lw 1 dt 2 title "", ARG1 using 6:(-2500.0):8 every 2 with lines lt 7 lw 1 dt 2 title ""
