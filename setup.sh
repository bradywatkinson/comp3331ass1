xterm -hold -title "Peer 1" -e "`pwd`/cdht 1 3 4" &
xterm -hold -title "Peer 3" -e "`pwd`/cdht 3 4 5" &
xterm -hold -title "Peer 4" -e "`pwd`/cdht 4 5 8" &
xterm -hold -title "Peer 5" -e "`pwd`/cdht 5 8 10" &
xterm -hold -title "Peer 8" -e "`pwd`/cdht 8 10 12" &
xterm -hold -title "Peer 10" -e "`pwd`/cdht 10 12 15" &
xterm -hold -title "Peer 12" -e "`pwd`/cdht 12 15 1" &
xterm -hold -title "Peer 15" -e "`pwd`/cdht 15 1 3" &
