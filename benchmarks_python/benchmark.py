# Simon Sepiol-Duchemin, Joshua Setia

import matplotlib.pyplot as plt
import numpy as np

# Données
inputSize = [n for n in range(10, 37) for _ in range(5)]

temps_execution = np.array([
    670, 245, 203, 221, 169,  # n=10
    381, 283, 244, 289, 328,  # n=11
    376, 390, 363, 339, 598,  # n=12
    656, 634, 792, 559, 718,  # n=13
    1125, 1182, 1138, 1095, 1126,  # n=14
    1586, 1894, 2119, 2014, 1544,  # n=15
    2754, 2702, 2701, 2715, 3454,  # n=16
    5198, 5251, 6290, 5021, 6495,  # n=17
    9843, 13313, 9345, 9016, 9360,  # n=18
    24510, 17749, 17890, 17622, 25090,  # n=19
    41423, 41755, 41460, 41964, 32890,  # n=20
    74079, 72038, 65340, 73139, 63768,  # n=21
    140310, 143536, 144258, 134526, 143800,  # n=22
    278042, 305619, 302090, 285902, 291412,  # n=23
    583904, 584905, 570942, 581374, 598116,  # n=24
    1260318, 1256058, 1242913, 1259922, 1267871,  # n=25
    2593358, 2608632, 2608243, 2579195, 2560172,  # n=26
    5291688, 5241822, 5264790, 5304710, 5296234,  # n=27
    10677260, 10694673, 10733471, 10675719, 10711583,  # n=28
    21273252, 21366815, 21326243, 21336199, 21323442,  # n=29
    29105193, 28070603, 28038304, 28269417, 28904075,  # n=30
    44462501, 42624525, 42478467, 45179956, 42522355,  # n=31
    72579889, 71453017, 73228680, 69512551, 72245986,  # n=32
    120654754, 120662451, 120806611, 120601677, 120776946,  # n=33
    219499885, 219194127, 219122833, 219253806, 219076609,  # n=34
    442292020, 440774921, 440633266, 441210047, 438576773,  # n=35
    773276893, 1399771717, 780005348, 778789508, 778091829  # n=36
])

memoire_max = np.array([
    19964, 19836, 19900, 19864, 19820,  # n=10
    19836, 19972, 19860, 20032, 21884,  # n=11
    20032, 19836, 19928, 20036, 19872,  # n=12
    19864, 20136, 19836, 19876, 19900,  # n=13
    20036, 21880, 20088, 20016, 19876,  # n=14
    19968, 20008, 20160, 20008, 20104,  # n=15
    21956, 19984, 20020, 20132, 19892,  # n=16
    19972, 19928, 20148, 19920, 19976,  # n=17
    20232, 20124, 20008, 19920, 19912,  # n=18
    19916, 22008, 19988, 20124, 20044,  # n=19
    20132, 22252, 20332, 20312, 19968,  # n=20
    20224, 20116, 20060, 20008, 20316,  # n=21
    21116, 21256, 22060, 21164, 21332,  # n=22
    25112, 25180, 25480, 25120, 25116,  # n=23
    32292, 32396, 32012, 32164, 34100,  # n=24
    46204, 46208, 46256, 46908, 46272,  # n=25
    75844, 75204, 76136, 76080, 74036,  # n=26
    131708, 133540, 132288, 133156, 133844,  # n=27
    221316, 221320, 221144, 222296, 221108,  # n=28
    479776, 480076, 479324, 480184, 479924,  # n=29
    207396, 209156, 206196, 209948, 206080,  # n=30
    331740, 331432, 331788, 333216, 324780,  # n=31
    581328, 581516, 579724, 581612, 581604,  # n=32
    1026396, 1025976, 1026236, 1026028, 1026160,  # n=33
    1837292, 1836428, 1836396, 1836396, 1836412,  # n=34
    3651696, 3651176, 3651224, 3651496, 3651576,  # n=35
    7281512, 7281436, 7281848, 7281324, 7281756  # n=36
])



# Calcul des moyennes
unique_sizes = np.unique(inputSize)
mean_temps = [temps_execution[np.array(inputSize) == size].mean() for size in unique_sizes]
mean_memoire = [memoire_max[np.array(inputSize) == size].mean() for size in unique_sizes]

# Fonction pour tracer les graphes
def plot_results():
    plt.figure(figsize=(12, 6))
    plt.plot(unique_sizes, mean_temps, marker='o', label='Temps d\'exécution (moyenne)')
    plt.xlabel('Taille d\'entrée (Input size)')
    plt.ylabel('Temps d\'exécution (x10⁶ clock ticks)')
    plt.title('Temps d\'exécution vs taille d\'entrée n')
    plt.legend()
    plt.grid()

    plt.figure(figsize=(12, 6))
    plt.plot(unique_sizes, mean_memoire, marker='o', label='Mémoire maximale (moyenne)')
    plt.xlabel('Taille d\'entrée (Input size)')
    plt.ylabel('Mémoire maximale (x10⁶ KB)')
    plt.title('Mémoire maximale vs taille d\'entrée n')
    plt.legend()
    plt.grid()

    plt.show()

# Exécution des graphes
if __name__ == "__main__":
    plot_results()