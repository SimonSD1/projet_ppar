# projet_ppar
Pour compiler le makefile :  
make  

Pour lancer en local :  
./mpi_mitm --n 14 --C0 04a58a4838c8994e --C1 88dec3e5408b5707

Pour lancer le script sur grid 5000 :  
oarsub -l core=150 './mpi_script.sh'
