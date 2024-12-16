#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <assert.h>
#include <getopt.h>
#include <err.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

#include <mpi.h>

// shrding : each process create its own dictonary
// for query : ask the value of certain key and send value of key
// use mpi

typedef uint64_t u64; /* portable 64-bit integer */
typedef uint32_t u32; /* portable 32-bit integer */
struct __attribute__((packed)) entry
{
    u32 k;
    u64 v;
}; /* hash table entry */

/***************************** global variables ******************************/

u64 n = 0; /* block size (in bits) */
u64 local_n = 0;
u64 mask; /* this is 2**n - 1 */

u64 dict_size;   /* number of slots in the hash table */
struct entry *A; /* the hash table */

int surplus;

/* (P, C) : two plaintext-ciphertext pairs */
u32 P[2][2] = {{0, 0}, {0xffffffff, 0xffffffff}};
u32 C[2][2];

int my_rank; /* rank of the process */
int p;       /* number of processes */

int local_size;
int size;
int debut;

/************************ tools and utility functions *************************/

double wtime()
{
    struct timeval ts;
    gettimeofday(&ts, NULL);
    return (double)ts.tv_sec + ts.tv_usec / 1E6;
}

// murmur64 hash functions, tailorized for 64-bit ints / Cf. Daniel Lemire
u64 murmur64(u64 x)
{
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdull;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ull;
    x ^= x >> 33;
    return x;
}

/* represent n in 4 bytes */
void human_format(u64 n, char *target)
{
    if (n < 1000)
    {
        sprintf(target, "%" PRId64, n);
        return;
    }
    if (n < 1000000)
    {
        sprintf(target, "%.1fK", n / 1e3);
        return;
    }
    if (n < 1000000000)
    {
        sprintf(target, "%.1fM", n / 1e6);
        return;
    }
    if (n < 1000000000000ll)
    {
        sprintf(target, "%.1fG", n / 1e9);
        return;
    }
    if (n < 1000000000000000ll)
    {
        sprintf(target, "%.1fT", n / 1e12);
        return;
    }
}

/******************************** SPECK block cipher **************************/

#define ROTL32(x, r) (((x) << (r)) | (x >> (32 - (r))))
#define ROTR32(x, r) (((x) >> (r)) | ((x) << (32 - (r))))

#define ER32(x, y, k) (x = ROTR32(x, 8), x += y, x ^= k, y = ROTL32(y, 3), y ^= x)
#define DR32(x, y, k) (y ^= x, y = ROTR32(y, 3), x ^= k, x -= y, x = ROTL32(x, 8))

void Speck64128KeySchedule(const u32 K[], u32 rk[])
{
    u32 i, D = K[3], C = K[2], B = K[1], A = K[0];
    for (i = 0; i < 27;)
    {
        rk[i] = A;
        ER32(B, A, i++);
        rk[i] = A;
        ER32(C, A, i++);
        rk[i] = A;
        ER32(D, A, i++);
    }
}

// encrypte
void Speck64128Encrypt(const u32 Pt[], u32 Ct[], const u32 rk[])
{
    u32 i;
    Ct[0] = Pt[0];
    Ct[1] = Pt[1];
    for (i = 0; i < 27;)
        ER32(Ct[1], Ct[0], rk[i++]);
}

// decrypte
void Speck64128Decrypt(u32 Pt[], const u32 Ct[], u32 const rk[])
{
    int i;
    Pt[0] = Ct[0];
    Pt[1] = Ct[1];
    for (i = 26; i >= 0;)
        DR32(Pt[1], Pt[0], rk[i--]);
}

/******************************** dictionary ********************************/

/*
 * "classic" hash table for 64-bit key-value pairs, with linear probing.
 * It operates under the assumption that the keys are somewhat random 64-bit integers.
 * The keys are only stored modulo 2**32 - 5 (a prime number), and this can lead
 * to some false positives.
 */
static const u32 EMPTY = 0xffffffff;
static const u64 PRIME = 0xfffffffb;

/* allocate a hash table with `size` slots (12*size bytes) */
void dict_setup(u64 size)
{
    dict_size = size;
    char hdsize[8];
    human_format(dict_size * sizeof(*A), hdsize);
    printf("Dictionary size: %sB\n", hdsize);

    A = malloc(sizeof(*A) * dict_size);
    if (A == NULL)
        err(1, "impossible to allocate the dictionnary");
    for (u64 i = 0; i < dict_size; i++)
    {
        A[i].k = EMPTY;
        A[i].v = 0;
    }
}

/* Insert the binding key |----> value in the dictionnary */
void dict_insert(u64 key, u64 value)
{
    u64 h = murmur64(key) % dict_size;
    for (;;)
    {
        if (A[h].k == EMPTY)
            break;
        h += 1;
        if (h == dict_size)
            h = 0;
    }
    assert(A[h].k == EMPTY);
    if (my_rank == 0)
    {
        // printf("insert en %ld\n", h);
    }

    A[h].k = key % PRIME;
    A[h].v = value;
}

/* Query the dictionnary with this `key`.  Write values (potentially)
 *  matching the key in `values` and return their number. The `values`
 *  array must be preallocated of size (at least) `maxval`.
 *  The function returns -1 if there are more than `maxval` results.
 */
int dict_probe(u64 key, int maxval, u64 values[])
{
    u32 k = key % PRIME;
    u64 h = murmur64(key) % dict_size;
    int nval = 0;
    for (;;)
    {
        if (A[h].k == EMPTY)
            return nval;
        if (A[h].k == k)
        {
            if (nval == maxval)
                return -1;
            values[nval] = A[h].v;
            nval += 1;
        }
        h += 1;
        if (h == dict_size)
            h = 0;
    }
}

u64 f(u64 k)
{
    assert((k & mask) == k);
    u32 K[4] = {k & 0xffffffff, k >> 32, 0, 0};
    u32 rk[27];
    Speck64128KeySchedule(K, rk);
    u32 Ct[2];
    Speck64128Encrypt(P[0], Ct, rk);
    return ((u64)Ct[0] ^ ((u64)Ct[1] << 32)) & mask;
}

void affiche_dico()
{

    fflush(stdout);
    printf("rank=%d\n", my_rank);
    for (int i = 0; i < dict_size; i++)
    {
        printf("%u, %ld\n", A[i].k, A[i].v);
    }
    fflush(stdout);
}

u64 g(u64 k)
{
    assert((k & mask) == k);
    u32 K[4] = {k & 0xffffffff, k >> 32, 0, 0};
    u32 rk[27];
    Speck64128KeySchedule(K, rk);
    u32 Pt[2];
    Speck64128Decrypt(Pt, C[0], rk);
    return ((u64)Pt[0] ^ ((u64)Pt[1] << 32)) & mask;
}

void remplit_dico()
{
    debut = my_rank * (size / p);

    u64 *cles = malloc(local_size * sizeof(u64));
    u64 *valeures = malloc(local_size * sizeof(u64));

    int i = 0;
    for (int x = debut; x < debut + local_size; x++)
    {
        u64 z = f(x);
        cles[i] = z;
        valeures[i] = x;
        i++;
    }

    // taille un peu au hasard
    int fixed_size = (((size / p + surplus) / p) + 5) * 1.3;

    // Trier les clés et les valeurs par propriétaire
    int *owner_counts = calloc(p, sizeof(int));
    u64 *owner_keys = calloc(p * fixed_size, sizeof(u64));
    u64 *owner_values = calloc(p * fixed_size, sizeof(u64));

    // On compte combien de clés on doit envoyer à un processus
    for (int i = 0; i < local_size; i++)
    {
        int owner = cles[i] % p;
        if (owner != my_rank)
        {
            owner_counts[owner]++;
        }
    }

    // on met en premiere valeure la taille reel envoyé
    for (int i = 0; i < p; i++)
    {
        owner_keys[i * fixed_size] = owner_counts[i];
        owner_values[i * fixed_size] = owner_counts[i];
    }

    // remplit les buffers
    int *owner_offsets = calloc(p, sizeof(int));
    for (i = 0; i < local_size; i++)
    {
        int owner = cles[i] % p;
        if (owner != my_rank)
        {
            // +1 car la premier est la taille
            owner_keys[owner * fixed_size + owner_offsets[owner] + 1] = cles[i];
            owner_values[owner * fixed_size + owner_offsets[owner] + 1] = valeures[i];
            owner_offsets[owner]++;
        }
    }

    // for(int i=0; i<p;i++){
    //     printf("fixed = %d, woner=%d\n",fixed_size,owner_offsets[i]);
    // }

    u64 *recv_keys = calloc(p * fixed_size, sizeof(u64));
    u64 *recv_values = calloc(p * fixed_size, sizeof(u64));

    MPI_Alltoall(owner_keys, fixed_size, MPI_UNSIGNED_LONG,
                 recv_keys, fixed_size, MPI_UNSIGNED_LONG,
                 MPI_COMM_WORLD);
    MPI_Alltoall(owner_values, fixed_size, MPI_UNSIGNED_LONG,
                 recv_values, fixed_size, MPI_UNSIGNED_LONG,
                 MPI_COMM_WORLD);

    // libere la memoire
    free(cles);
    free(valeures);

    // qu'on reutilise pour le dictionaire
    dict_setup(1.125 * (size / p + surplus) + 100);

    // on fait les dict insert de ce qu'on a recu
    for (i = 0; i < p; i++)
    {
        if (i == my_rank)
        {
            for (int j = 0; j < owner_counts[my_rank]; j++)
            {
                dict_insert(owner_keys[my_rank * fixed_size + j], owner_values[my_rank * fixed_size + j]);
            }
        }
        else
        {
            int count = recv_keys[i * fixed_size];
            for (int j = 1; j <= count; j++)
            {
                dict_insert(recv_keys[i * fixed_size + j], recv_values[i * fixed_size + j]);
            }
        }
    }

    free(owner_keys);
    free(owner_values);
    free(owner_counts);
    free(recv_keys);
    free(recv_values);
}

bool is_good_pair(u64 k1, u64 k2)
{
    u32 Ka[4] = {k1 & 0xffffffff, k1 >> 32, 0, 0};
    u32 Kb[4] = {k2 & 0xffffffff, k2 >> 32, 0, 0};
    u32 rka[27];
    u32 rkb[27];
    Speck64128KeySchedule(Ka, rka);
    Speck64128KeySchedule(Kb, rkb);
    u32 mid[2];
    u32 Ct[2];
    Speck64128Encrypt(P[1], mid, rka);
    Speck64128Encrypt(mid, Ct, rkb);
    return (Ct[0] == C[1][0]) && (Ct[1] == C[1][1]);
}

int golden_claw_search(int maxres, u64 k1[], u64 k2[])
{
    double start = wtime();
    remplit_dico();
    double mid = wtime();

    printf("Fill: %.001fs\n", mid - start);

    MPI_Status status;
    MPI_Request request;

    int fixed_size = (((size / p + surplus) / p) + 5) * 1.3;

    // contient les tableaux qui contiennent les demandes pour un process
    u64 *g_de_z = malloc(p * fixed_size * sizeof(u64));
    u64 *z_buff = malloc(p * fixed_size * sizeof(u64));

    // contient le nombre de demande qu'on va faire a un process
    int *nb_demandes_p = calloc(sizeof(int), p);

    // trouve le nombre de demande a faire pour un process
    for (u64 z = debut; z < debut + local_size; z++)
    {
        u64 y = g(z);

        int owner = y % p;

        g_de_z[owner * fixed_size + nb_demandes_p[owner] + 1] = y;
        z_buff[owner * fixed_size + nb_demandes_p[owner] + 1] = z;

        nb_demandes_p[owner]++;
    }

    // on met le nombre de demande en premiere valeure
    for (int i = 0; i < p; i++)
    {
        g_de_z[i * fixed_size] = nb_demandes_p[i];
        z_buff[i * fixed_size] = nb_demandes_p[i];
    }

    u64 *reception_z = calloc(p * fixed_size, sizeof(u64));
    u64 *reception_g_z = calloc(p * fixed_size, sizeof(u64));

    // for(int i=0; i<p;i++){
    //     printf("fixed = %d, demande=%d\n",fixed_size,nb_demandes_p[i]);
    // }

    MPI_Alltoall(g_de_z, fixed_size, MPI_UNSIGNED_LONG,
                 reception_g_z, fixed_size, MPI_UNSIGNED_LONG,
                 MPI_COMM_WORLD);
    MPI_Alltoall(z_buff, fixed_size, MPI_UNSIGNED_LONG,
                 reception_z, fixed_size, MPI_UNSIGNED_LONG,
                 MPI_COMM_WORLD);

    int *counts = malloc(p * sizeof(int));

    u64 x[256];
    int nres = 0;
    u64 ncandidates = 0;

    // pour chaque processus
    for (int i = 0; i < p; i++)
    {

        // pour chaque reception
        int taille = reception_g_z[i * fixed_size];
        for (int j = 1; j < taille; j++)
        {
            u64 y;
            u64 z;

            if (i == my_rank)
            {
                y = g_de_z[i * fixed_size + j];
                z = z_buff[i * fixed_size + j];
            }
            else
            {
                y = reception_g_z[i * fixed_size + j];
                z = reception_z[i * fixed_size + j];
            }

            int nx = dict_probe(y, 256, x);
            assert(nx >= 0);
            ncandidates += nx;

            for (int a = 0; a < nx; a++)
            {
                if (is_good_pair(x[a], z))
                {
                    if (nres == maxres)
                        return -1;

                    k1[nres] = x[a];
                    k2[nres] = z;
                    printf("solution found : (%ld,%ld)\n", x[a], z);

                    nres++;
                }
            }
        }
    }

    return nres;
}

/************************** command-line options ****************************/

void usage(char **argv)
{
    printf("%s [OPTIONS]\n\n", argv[0]);
    printf("Options:\n");
    printf("--n N                       block size [default 24]\n");
    printf("--C0 N                      1st ciphertext (in hex)\n");
    printf("--C1 N                      2nd ciphertext (in hex)\n");
    printf("\n");
    printf("All arguments are required\n");
    exit(0);
}

void process_command_line_options(int argc, char **argv)
{
    struct option longopts[4] = {
        {"n", required_argument, NULL, 'n'},
        {"C0", required_argument, NULL, '0'},
        {"C1", required_argument, NULL, '1'},
        {NULL, 0, NULL, 0}};
    char ch;
    int set = 0;
    while ((ch = getopt_long(argc, argv, "", longopts, NULL)) != -1)
    {
        switch (ch)
        {
        case 'n':
            n = atoi(optarg);
            mask = (1ull << n) - 1;
            break;
        case '0':
            set |= 1;
            u64 c0 = strtoull(optarg, NULL, 16);
            C[0][0] = c0 & 0xffffffff;
            C[0][1] = c0 >> 32;
            break;
        case '1':
            set |= 2;
            u64 c1 = strtoull(optarg, NULL, 16);
            C[1][0] = c1 & 0xffffffff;
            C[1][1] = c1 >> 32;
            break;
        default:
            errx(1, "Unknown option\n");
        }
    }
    if (n == 0 || set != 3)
    {
        usage(argv);
        exit(1);
    }
}

int main(int argc, char **argv)
{

    /* Initialisation */
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &p);

    process_command_line_options(argc, argv);
    // printf("Running with n=%d, C0=(%08x, %08x) and C1=(%08x, %08x)\n",
    //        (int)n, C[0][0], C[0][1], C[1][0], C[1][1]);

    mask = (1 << n) - 1;

    size = 1ull << n;

    local_size = size / p;
    surplus = size % p;
    // a changer
    if (my_rank == p - 1)
    {
        local_size += surplus;
    }

    /// !!!!!!!!!!!!!!!!!! la taille des dico peut etre bloquant
    // dict_setup(1.125 * dict_size + 40);

    u64 k1[16], k2[16];

    __clock_t start = clock();
    int nkey = golden_claw_search(16, k1, k2);
    __clock_t end = clock();

    printf("temps pris proc %d = %ld\n", my_rank, end - start);

    for (int i = 0; i < nkey; i++)
    {
        assert(f(k1[i]) == g(k2[i]));
        assert(is_good_pair(k1[i], k2[i]));
        printf("Solution found: (%" PRIx64 ", %" PRIx64 ") [checked OK]\n", k1[i], k2[i]);
    }

    MPI_Finalize();

    return 0;
}