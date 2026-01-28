#!/bin/bash
# Production et génération bench par bench des graphes des résultats des prédicteurs de branchement
# (c) 2017-2023 Frédéric Pétrot <frederic.petrot@univ-grenoble-alpes.fr>
# This software is open source, feel free to use it and modify it at your own risk
##################################################################################

num_parallel_jobs=4

# Paramètres que l'on cherche à faire varier, d'où le $(seq x y).
# Pour l'exemple du BHT simple, avec les valeurs ci-dessous on n'a qu'un unique
# "point" :
#  le nombre d'entrée dans la table est de 2⁷ = 128
array_sizes=($(seq 6 2 16))
#  la valeur max atteinte par le compteur est 2¹ - 1 = 1
count_sizes=($(seq 1 4))
predictor_types=($(seq 0 3)) # 0: Simple, 1: Gshare, 2: Local, 3: Combined

if test 1 -ne 0 ; then
   # Nombre de bits du compteur
   for i in ${count_sizes[*]} ; do
      # Taille du tableau
      for j in ${array_sizes[*]} ; do
         for type in ${predictor_types[*]} ; do
            ./runall.pl -s ../sim/predictor -w isi2024 -f  $num_parallel_jobs -d ../results/type${type}_${i}-${j} -p $j $i $type
         done
      done
   done
fi

# Liste des benchs
benchs=$(ls -1 ../results/type0_1-6 | awk -F\. '{print $1}')

# Fichier temporaire
# tmpfile=$(mktemp)

tmpfile=1
rm -f 1

for type in ${predictor_types[*]} ; do
   for i in ${count_sizes[*]} ; do
      for j in ${array_sizes[*]} ; do   
         ./getdata.pl -w isi2024 -d ../results/type${type}_${i}-${j} >> ${tmpfile}_$i
      done
   done
   # Format des résultats
   # ResultDirs ==>                	./results/2-16	
   # SHORT_MOBILE-56     	       1.930 	
   # SHORT_MOBILE-59     	       0.030 

   # Récupération des résultats et écriture en python
   t=$(mktemp)
   for i in ${count_sizes[*]} ; do
      rm -f $t
      for bench in $benchs ; do
         echo -n "$bench = [" | tr '-' '_'                               >> $t
         grep $bench ${tmpfile}_$i | awk '{printf("%s, ", $NF)}'         >> $t
         echo "]"                                                        >> $t
      done
      rm -f ${tmpfile}_$i
      echo "import matplotlib.pyplot as plt"                              > plot_$i.py
      cat $t                                                             >> plot_$i.py
      rm $t
      echo "f = plt.figure()"                                            >> plot_$i.py
      echo "plt.xlabel(r'$\log_2\$(table size)')"                        >> plot_$i.py
      echo "plt.ylabel('Miss predictions per K instructions')"           >> plot_$i.py
      echo "i = 0"                                                       >> plot_$i.py
      echo "colors = ['b', 'g', 'r', 'c', 'm', 'y', 'k']"                >> plot_$i.py
      echo "marks  = ['p', 'o', '*', 'h', 'H', '8', '^', 'v', 's']"      >> plot_$i.py
      for bench in $benchs ; do
         b=$(echo -n "$bench" | tr '-' '_')

         echo "plt.plot([$(echo ${array_sizes[*]} | sed -e 's/ /, /g')], $b, marker=marks[i%9], linestyle='--', color=colors[i%7], label='"$b"')" >> plot_$i.py
         echo "i = i + 1"                                                >> plot_$i.py
      done
      echo "plt.title('counter size" $i"')"                              >> plot_$i.py
      echo "plt.legend(loc='upper right')"                               >> plot_$i.py
      echo "plt.subplot().set_yscale('log', nonpositive='clip')"         >> plot_$i.py
      echo "plt.legend(loc='upper right')"                               >> plot_$i.py
      echo "f.savefig('"graph_type${type}_$i.pdf"', bbox_inches='tight')"            >> plot_$i.py
      python3 plot_$i.py
      rm plot_$i.py
   done
done