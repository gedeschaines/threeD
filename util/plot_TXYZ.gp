#FILE:  plot_TXYZ.gp
#DATE:  15 OCT 2023
#AUTH:  G. E. Deschaines
#PROG:  A gnuplot command script.
#DESC:  This script generates a 3D plot of missile/target engagement from
#       TXYZ.OUT.#### trajectory data file for run case #### specified as
#       the first argument.
#NOTE:  Invoke this script as follows.
#
#  1. On Linux/Cygwin platforms:
#     $ gnuplot -e "set term x11 size 800,800" -s -p -c ./util/plot_TXYZ.gp "./txyz/TXYZ.OUT.####"
#
#  2. On Windows platforms:
#     > wgnuplot.exe -e "set term qt size 800,800" -p -c .\util\plot_TXYZ.gp "./txyz/TXYZ.OUT.####"
#
#set terminal x11 size 800,800  # Works for Linux and Cygwin
#set terminal qt size 800,800   # Works for Linux and Windows 10
set title ARG1
set view 65, 310, 1, 1
set xyplane at 25.0
set grid xtics ytics ztics vertical
set xlabel "X"
set ylabel "Y"
set zlabel "Z"
splot [0.0:2500.0] [0.0:-2500.0] [0.0:-1250.0] \
 ARG1 using 3:4:5 every 2 with lines lt 6 lw 2 dt 1 title "Missile", \
 ARG1 using 6:7:8 every 2 with lines lt 7 lw 2 dt 1 title "Target", \
 ARG1 using 3:4:(0.0) every 2 with lines lt 6 lw 1 dt 2 title "", \
 ARG1 using 6:7:(0.0) every 2 with lines lt 7 lw 1 dt 2 title "", \
 ARG1 using 3:(-2500.0):5 every 2 with lines lt 6 lw 1 dt 2 title "", \
 ARG1 using 6:(-2500.0):8 every 2 with lines lt 7 lw 1 dt 2 title ""
