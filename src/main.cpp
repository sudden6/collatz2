#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <time.h>
#include <inttypes.h>
#include "math256.h"
#include "candidates_file.h"
#include "cleared_file.h"
#include "worktodo_file.h"
#include <iostream>

using namespace std;

#if defined BOINC
#include <boinc/boinc_api.h>
#endif


//Maximale Anzahl an Iterationen vor Abbruch (zur Vermeidung einer Endlosschleife)
#define MAX_NR_OF_ITERATIONS 2000

typedef __uint128_t uint128_t;
//#define CHECKPOINTS

#ifdef CHECKPOINTS
#define CHECK(x) (x)
#else
#define CHECK(x)
#endif

// Checkpoints to verify correctness against a known good version

uint64_t checkpoint1 = 0;   // checks how many candidates survive after 3 multistep iterations
uint64_t checkpoint2 = 0;   // checks how many candidates survive after 6 multistep iterations
uint64_t checkpoint3 = 0;   // checks how often the multistep function is called
uint64_t checkpoint4 = 0;   // checks the sum of all res64 values after 3 multistep iterations
double checkpoint5 = 0.0;

// File-Handler für Ausgabedateien für betrachtete Reste (cleared) und
// Kandidatenzahlen (candidate)
cleared_file f_cleared;
candidates_file f_candidate;
// bzw. für Einlesen der zu bearbeitenden Reste (worktodo)
worktodo_file f_worktodo;

// globale Variablen für Start und Ende des Bereichs der zu bearbeitenden Reste
uint_fast32_t idx_min;
uint_fast32_t idx_max;

// vorberechnete Dreier-Potenzen
uint128_t pot3[64];

#define pot3_32Bit(x) ((uint32_t)(pot3[x]))
#define pot3_64Bit(x) ((uint64_t)(pot3[x]))

// Siebtiefe, bevor einzelne Startzahlen in den übrigbleibenden Restklassen
// erzuegt werden
#define SIEVE_DEPTH 58 // <=60
#define SIEVE_DEPTH_FIRST 32 // <=32
#define SIEVE_DEPTH_SECOND 40// <=40

#define MAX_PARALLEL_FACTOR 4   // wird für die speicher reservierungen benutzt

// Gesucht wird bis 87 * 2^60
#define SEARCH_LIMIT 87
//#define INNER_LOOP_OUTPUT

#define LOOP_END (SEARCH_LIMIT * (1 << (60 - SIEVE_DEPTH))) // Für Schleife im Siebausgang in sieve_third_stage

//#define INNER_LOOP_OUTPUT

#ifdef INNER_LOOP_OUTPUT
    // max_no_of_numbers = Anzahl der Zahlen in jeder Restklasse mod 9, die im Siebausgang in
    // sieve_third_stage auf einmal erzeugt und danach in first_multistep parallel ausgewertet werden
    #define MAX_NO_OF_NUMBERS ((LOOP_END+8)/9)
#else
    // max_no_of_numbers = Anzahl der Zahlen aller[!] Restklassen mod 9, die im Siebausgang in
    // sieve_third_stage auf einmal erzeugt und danach in first_multistep parallel ausgewertet werden
    #define MAX_NO_OF_NUMBERS ((LOOP_END+8)/9)*5
#endif

#define MINDIGITS_START  20
#define MINDIGITS_RECORD 39

// Arrays zum Rausschreiben der Restklassen nach sieve_depth_first Iterationen
// reicht bis sieve_depth_first = 32;
// Das wären maximal 42 Millionen, wenn man corfactor = 1 setzt; für Vergleich mit etwa
// http://www.ams.org/journals/mcom/1999-68-225/S0025-5718-99-01031-5/S0025-5718-99-01031-5.pdf
// Seite 6/14 (Tabelle 1)

uint_fast32_t * reste_array;
uint64_t * it32_rest;
uint32_t * it32_odd;
uint32_t * cleared_res;

// Anzahl übriger Restklassen nach sieve_depth_first Iterationen
uint64_t restcnt_it32;
// Anzahl in einer Restklasse gefundener Kandidaten
uint_fast32_t no_found_candidates;

#define MS_DEPTH 10 // 9 <= ms_depth <= 10

// Reste-Arrays für Multistep
uint32_t multistep_it_rest[1 << MS_DEPTH];
uint32_t multistep_pot3_odd[1 << MS_DEPTH];
float multistep_it_f[1 << MS_DEPTH];
float multistep_it_maxf[1 << MS_DEPTH];
float multistep_it_minf[1 << MS_DEPTH];

// get the current wall clock time in seconds
double get_time() {
  struct timeval tp;
  gettimeofday(&tp, NULL);
  return tp.tv_sec + tp.tv_usec / 1000000.0;
}


// Füllt das 128-Bit-Dreier-Potenz-Array
void init_potarray()
{
    uint128_t p3 = 1;
    unsigned int i = 0;

    for ( ; i < 64; i++)
    {
        pot3[i] = p3;
        p3 *= 3;
    }
}

// Gibt Nummer der Restklasse einer Startzahl im
// betrachteten Intervall zurück; andernfalls -1
int nr_residue_class(const uint128_t start)
{
    uint_fast32_t startres32 = start & UINT32_MAX;

    unsigned int i;
    for (i = 0; i < idx_max-idx_min; i++)
    {
        if (reste_array[i] == startres32)
            return (i + idx_min);
    }

    return -1;
}

uint256_t last_found_candidate_before_resume(0);

// Nachrechnen eines Rekords und Ausgabe; nach gonz
void print_candidate(const uint128_t start)
{
    //Anzahl gefundener Kandidaten um 1 hochzählen
    no_found_candidates++;

    uint256_t start_256(start);
    uint256_t myrecord(0);
    uint256_t myvalue = start_256;
    uint_fast32_t it = 0;

    while ((start_256 <= myvalue) && (it < MAX_NR_OF_ITERATIONS))
    {
        if (myvalue.is_odd())
        {
            myvalue = uint256_t::mul3p1(myvalue);
            if (myrecord < myvalue)
            {
                myrecord = myvalue;
            }
        }
        else
        {
            myvalue = uint256_t::div2(myvalue);
        }
    it++;
    }

#pragma omp critical
    {
        if (it >= MAX_NR_OF_ITERATIONS)
        {
            cout << "*** Maximum Number of Iterations reached! ***" << endl;
            // TODO: better way for this?
            // mark this error in output with record=0 and bits=0
            f_candidate.append(start, 0, 0, nr_residue_class(start));
        }

        cout << "** Start=" << start_256.to_string();
        cout << " Bit Record=" << uint256_t::bitnum(start_256);
        cout << myrecord.to_string();
        cout << " Bit " << uint256_t::bitnum(myrecord) << endl;
        f_candidate.append(start_256, myrecord, uint256_t::bitnum(myrecord),
                           nr_residue_class(start));
    }
}


// Gibt an, um welchen Faktor ein Collatz-Faktor kleiner ist als die aktuell
// betrachtete Restklasse; Der Wert odd gibt dabei die Anzahl der
// bisher erfolgten (3x+1)-Schritte, und damit den Exponenten der
// Dreier-Potenz an, durch die das Modul der Restklasse teilbar ist
// laststepodd = 1 <==> Zahl entstand durch (3x+1)/2-Schritt. Dann muss
// der Fall Zahl == 2 (mod 3) nicht untersucht werden, da dies bei
// der Vorgängerzahl schon getan wurde.
double corfactor(const unsigned int odd, const uint64_t it_rest, const int laststepodd)
{
    const unsigned int rest = it_rest % 729;

    double minfactor = 1.0;
    double factor;

    if (odd >= 1)
    {
        if (!laststepodd && (rest % 3 == 2)) //2k+1 --> 3k+2
        {
            factor = 2.0 / 3.0 * corfactor(odd - 1, it_rest / 3 * 2 + 1, 0);

            if (factor < minfactor)
                minfactor = factor;
        }
    }
    else
        return minfactor;

    if (odd >= 2)
    {
        if (rest % 9 == 4) //8k+3 --> 9k+4
        {
            factor = 8.0 / 9.0 * corfactor(odd - 2, it_rest / 9 * 8 + 3, 0);

            if (factor < minfactor)
                minfactor = factor;
        }
    }
    else
        return minfactor;

    if (odd >= 4) //64k+7 --> 81k+10
    {
        if (rest % 81 == 10)
        {
            factor = 64.0 / 81.0 * corfactor(odd - 4, it_rest / 81 * 64 + 7, 0);

            if (factor < minfactor)
                minfactor = factor;
        }
    }
    else
        return minfactor;

    if (odd >= 5) //128k+95 --> 243k+182
    {
        if (rest % 243 == 182)
        {
            factor = 128.0 / 243.0 * corfactor(odd - 5, it_rest / 243 * 128 + 95, 0);

            if (factor < minfactor)
                minfactor = factor;
        }
    }
    else
        return minfactor;

    if (odd >= 6)
    {
        unsigned int p3 = 0;
        unsigned int rest2;

        switch (rest) // = "mod 729"
        {
            case  91: p3=6; rest2= 63; break; //512k+ 63 --> 729k+ 91
            case 410: p3=6; rest2=287; break; //512k+287 --> 729k+410
            case 433: p3=6; rest2=303; break; //512k+303 --> 729k+433
            case 524: p3=6; rest2=367; break; //512k+367 --> 729k+524
            case 587: p3=6; rest2=411; break; //512k+411 --> 729k+587
            case 604: p3=6; rest2=423; break; //512k+423 --> 729k+604
            case 661: p3=6; rest2=463; break; //512k+463 --> 729k+661
            case 695: p3=6; rest2=487; break; //512k+487 --> 729k+695
        }

        if (p3 == 6)
        {
            factor = 512.0 / 729.0
                    * corfactor(odd-6,it_rest/729 * 512 + rest2, 0);

            if (factor < minfactor)
                minfactor = factor;
        }
    }
    else
        return minfactor;

    return minfactor;
}

// Initialisiert die Arrays für die Multisteps
// Alle Restklassen mod 2^ms_depth werden durchgegangen, ihre Reste
// nach ms_depth Iterationen sowie auf diesem Weg das Maximum und
// Minimum (inkl. Betrachtung von möglichen Rückwärtsiterationen)
// berechnet und in den entsprechenden globalen Arrays gespeichert.
void init_multistep()
{
    unsigned int it;
    unsigned int odd;
    unsigned int it_rest;
    double min_f;
    double max_f;
    double it_f;
    double cormin;

    unsigned int rest;
    for (rest = 0; rest < (1 << MS_DEPTH); rest++)
    {
        min_f = 1.0;
        max_f = 1.0;
        it_f  = 1.0;

        odd = 0;
        it_rest = rest;
        for (it = 1; it <= MS_DEPTH; it++)
        {
            if (it_rest % 2 == 0)
            {
                it_rest = it_rest >> 1;
                it_f *= 0.5;
            }
            else
            {
                odd++;
                it_rest += (it_rest >> 1) + 1;
                it_f *= 1.5;
                if (it_f > max_f)
                {
                    max_f = it_f;
                }
            }
            cormin = it_f * corfactor(it_rest, odd, 0);
            if (cormin < min_f)
                min_f = cormin;
        }

        multistep_it_rest[rest] = it_rest;
        multistep_pot3_odd[rest] = pot3_64Bit(odd);
        multistep_it_f[rest] = it_f;
        multistep_it_maxf[rest] = max_f;
        multistep_it_minf[rest] = min_f;
    }
}

// Entscheidungswert für die Maximumsprüfung
#define MS_MAX_CHECK_VAL    ((float)1e16)

// Entscheidungswert für die Minimumsprüfung
#define MS_MIN_CHECK_VAL    ((float)0.98)

// Nach drei 10er Multisteps wird anhand diesem Wert entschieden
// ob die Minimums- oder Maximumsprüfung ausgelassen werden kann
#define MS_DECIDE_VAL       ((float)1e09)

#define restrict

// Berechnet drei 10er Multisteps ohne Maximums-Prüfung
void ms_mark_min(uint_fast32_t *restrict last_small_res, uint8_t *restrict mark,
                 uint64_t *restrict res64, float *restrict new_it_f)
{
    uint_fast32_t small_res = (*res64) & ((1 << MS_DEPTH) - 1);
    *mark = (*new_it_f) * multistep_it_minf[small_res] < MS_MIN_CHECK_VAL;

    *res64 = ((*res64) >> MS_DEPTH) * multistep_pot3_odd[small_res]
            + multistep_it_rest[small_res];
    *new_it_f *= multistep_it_f[small_res];

    small_res = (*res64) & ((1 << MS_DEPTH) - 1);
    *mark |= (*new_it_f) * multistep_it_minf[small_res] < MS_MIN_CHECK_VAL;

    *res64 = ((*res64) >> MS_DEPTH) * multistep_pot3_odd[small_res]
            + multistep_it_rest[small_res];
    *new_it_f *= multistep_it_f[small_res];

    *last_small_res = (*res64) & ((1 << MS_DEPTH) - 1);
    *mark |= (*new_it_f) * multistep_it_minf[*last_small_res] < MS_MIN_CHECK_VAL;
}

// Berechnet drei 10er-Multisteps ohne Minimums-Prüfung
void ms_mark_max(uint_fast32_t *restrict last_small_res, uint8_t *restrict mark,
                 uint64_t *restrict res64, float *restrict new_it_f)
{
    uint_fast32_t small_res = (*res64) & ((1 << MS_DEPTH) - 1);
    *mark = (*new_it_f) * multistep_it_maxf[small_res] > MS_MAX_CHECK_VAL;

    *res64 = ((*res64) >> MS_DEPTH) * multistep_pot3_odd[small_res]
            + multistep_it_rest[small_res];
    *new_it_f *= multistep_it_f[small_res];

    small_res = (*res64) & ((1 << MS_DEPTH) - 1);
    *mark |= (*new_it_f) * multistep_it_maxf[small_res] > MS_MAX_CHECK_VAL;

    *res64 = ((*res64) >> MS_DEPTH) * multistep_pot3_odd[small_res]
            + multistep_it_rest[small_res];
    *new_it_f *= multistep_it_f[small_res];

    *last_small_res = (*res64) & ((1 << MS_DEPTH) - 1);
    *mark |= (*new_it_f) * multistep_it_maxf[*last_small_res] > MS_MAX_CHECK_VAL;
}


//Rechnet nach 6 10er-Multisteps, die nur mod 2^64 berechnet wurden, das
//Ergebnis mod 2^128 nach, damit die nächsten Schritte korrekt ermittelt werden
void recalc_128(const uint128_t *restrict number, uint128_t *restrict new_nr)
{
    // Nun muss genau nachgerechnet werden: Dies geschieht in 2 Schritten, wo je 30
    // Iterationen zusammengefasst werden:

    //small_res[], res, res64 rekonstruieren
    uint64_t res64 = (uint64_t) *number;
    uint64_t res = (uint_fast32_t) res64;
    uint_fast32_t small_res[6];
    uint_fast32_t i;
    for (i=0; i<6; i++)
    {
        small_res[i] = res64 & ((1 << MS_DEPTH) - 1);
        res64 = (res64 >> MS_DEPTH)
                * multistep_pot3_odd[small_res[i]]
                + multistep_it_rest[small_res[i]];
    }

    // fest für 32/ms_depth = 3 implementiert!

    // Idee: a*2^3m + b*2^2m + c*2^m +small_res[0]
    //   --> a*3^p_0*2^2m + b*3^p_0*2^m + c*3^p_0 + it_rest[0]
    //     = a*3^p_0*2^2m + b*3^p_0*2^m + uebertrag[0]*2^m + small_res[1]
    //   --> a*3^p_0*3^p_1*2^m + b*3^p_0*3^p_1 + uebertrag[0]*3^p_1 + it_rest[1]
    //     = a*3^(p_0+p_1)*2^m + uebertrag[1]*2^m + small_res[2]
    //   --> a*3^(p_0+p_1+p_2) + uebertrag[1]*3^p_2 + it_rest[2];
    //
    // mit   uebertrag[0] = (c*3^p_0 + it_rest[0]) >> m
    // und   uebertrag[1] = ((b*3^p_0 + uebertrag[0])* 3^p_1 + it_rest[1]) >> m

    uint_fast32_t res32 = ((uint_fast32_t) (res)) >> MS_DEPTH;
    uint_fast32_t c = res32 & ((1 << MS_DEPTH) - 1);
    res32 = res32 >> MS_DEPTH;
    uint_fast32_t b = res32 & ((1 << MS_DEPTH) - 1);

    uint_fast32_t uebertrag_0 = (c * multistep_pot3_odd[small_res[0]]
                                   + multistep_it_rest[small_res[0]]) >> MS_DEPTH;

    uint_fast32_t uebertrag_1 = b * multistep_pot3_odd[small_res[0]]
                                   + uebertrag_0;

    uint64_t uebertrag = ((uint64_t) uebertrag_1
                                   * multistep_pot3_odd[small_res[1]]
                                   + multistep_it_rest[small_res[1]]) >> MS_DEPTH;

    uebertrag *= multistep_pot3_odd[small_res[2]]; //uebertrag[1]*3^p_2
    uebertrag +=  multistep_it_rest[small_res[2]];        //uebertrag[1]*3^p_2 + it_rest[2]

    uint128_t int_nr = (*number) >> (3 * MS_DEPTH);  //a

    int_nr *= ((uint64_t) multistep_pot3_odd[small_res[0]]) 	  //a*3^(p_0+p_1+p_2)
             * multistep_pot3_odd[small_res[1]]
             * multistep_pot3_odd[small_res[2]];

    int_nr += uebertrag;



    res32 = ((uint_fast32_t) int_nr) >> MS_DEPTH;
    c = res32 & ((1 << MS_DEPTH) - 1);
    res32 = res32 >> MS_DEPTH;
    b = res32 & ((1 << MS_DEPTH) - 1);

    uebertrag_0 = (c * multistep_pot3_odd[small_res[3]]
                   + multistep_it_rest[small_res[3]]) >> MS_DEPTH;

    uebertrag_1 = b * multistep_pot3_odd[small_res[3]]
                  + uebertrag_0;

    uebertrag = ((uint64_t) uebertrag_1
                  * multistep_pot3_odd[small_res[4]]
                  + multistep_it_rest[small_res[4]]) >> MS_DEPTH;

    uebertrag *= multistep_pot3_odd[small_res[5]]; //uebertrag[1]*3^p_2
    uebertrag +=  multistep_it_rest[small_res[5]];        //uebertrag[1]*3^p_2 + it_rest[2]

    *new_nr = int_nr >> (3 * MS_DEPTH);  //a

    *new_nr *= ((uint64_t)multistep_pot3_odd[small_res[3]]) 	  //a*3^(p_0+p_1+p_2)
            * multistep_pot3_odd[small_res[4]]
            * multistep_pot3_odd[small_res[5]];

    *new_nr += uebertrag;
}


// Erhält die Startzahl sowie das Ergebnis "number" ihrer nr_it-ten Iteration
// sowie eine Abschätzung des Quotienten it_f = number / start und berechnet
// via Zusamenfassung mehrerer Schritte die nächsten 6 * ms_depth (= 60) Schritte.
// Steigt it_f über 10^16, wird eine Ausgabe des Kandidaten mit dem in diesem
// Multistep (ms_depth = 10 Schritte) maximalen Wert erzeugt und danach abgebrochen.
// Diese Rechnungen erfolgen mit 64-Bit-Arithmetik.
// Wurde noch nicht die maximale Anzahl an Iterationen erreicht, muss der volle
// 128-Bit-Rest mit 128-Bit-Arithmetik nachgerechnet werden, um dann einen korrekten
// Rekursionsaufruf zu starten. Dies geschieht in zwei Etappen, in denen je 3 * ms_depth
// Iterationen zusammengefasst und gemeinsam berechnet werden.

uint64_t multistep(const uint128_t start, const uint128_t number,
                        const float it_f, const uint_fast32_t nr_it)
{
    uint64_t res = (uint64_t) number;
    float new_it_f = it_f;
    uint64_t res64 = res;
    uint8_t mark;
    uint_fast32_t last_small_res;

    // Die ersten 30 Iterationen: Wenn new_it_f < 5*10^10, dann kann kein neuer
    // Kandidat in diesen 30 Iterationen gefunden werden ==> keine Maximums-Prüfung
    // notwendig. Sonst kann in diesen Iterationen nicht der Startwert unterschritten
    // werden ==> keine Minimums-Prüfung notwendig.
    if (new_it_f < MS_DECIDE_VAL)
    {
        ms_mark_min(&last_small_res, &mark, &res64, &new_it_f);
        if (mark)
        {
            return 1;
        }
    }
    else
    {
        ms_mark_max(&last_small_res, &mark, &res64, &new_it_f);

        if (mark) // Kandidat gefunden, nun genaue Nachrechnung, daher hier
        {				  // keine Fortführung nötig
            print_candidate(start);
            return 1;
        }
    }
    res64 = (res64 >> MS_DEPTH) * multistep_pot3_odd[last_small_res]
            + multistep_it_rest[last_small_res];
    new_it_f *= multistep_it_f[last_small_res];

    // Nun die zweiten 30 Iterationen analog den ersten 30.
    if (new_it_f < MS_DECIDE_VAL)
    {
        ms_mark_min(&last_small_res, &mark, &res64, &new_it_f);
        if (mark) return 1;
    }
    else
    {
        ms_mark_max(&last_small_res, &mark, &res64, &new_it_f);
        if (mark) // Kandidat gefunden, nun genaue Nachrechnung, daher hier
        {				  // keine Fortführung nötig
            print_candidate(start);
            return 1;
        }
    }
    new_it_f *= multistep_it_f[last_small_res];


    //Allgemein:
    //unsigned int small_res[32/ms_depth];
    //int i;
    //
    //for (i = 0; i < 32/ms_depth; i++ )
    //{
    //	small_res[i] = res32 & ((1 << ms_depth) - 1);
    //
    //	min_f = new_it_f * multistep_it_minf[small_res[i]];
    //	if (min_f <= 0.98) break;
    //
    //	res32 = (res32 >> ms_depth) * pot3_32Bit[multistep_odd[small_res[i]]]
    //	        + multistep_it_rest[small_res[i]];
    //	max_f = new_it_f * multistep_it_maxf[small_res[i]];
    //	new_it_f *= multistep_it_f[small_res[i]];
    //
    //	if (max_f > 1e15) found_candidate(start, max_f, nr_it + i*ms_depth + multistep_nr_it_max[small_res[i]]);
    //}



    if (nr_it > MAX_NR_OF_ITERATIONS)
    {
        print_candidate(start);
        return 1;
    }

    uint128_t new_nr;
    recalc_128(&number, &new_nr);
    CHECK(checkpoint3++);

    return (1 + multistep(start, new_nr, new_it_f, nr_it + 6 * MS_DEPTH));

//Allgemein wie folgt:
//		 unsigned __int128 new_nr = number;
//		for (i = 0; i < 64/ms_depth; i++ )
//		{
//			new_nr = (new_nr >> ms_depth) * pot3[multistep_odd[small_res[i]]]
//			         + multistep_it_rest[small_res[i]];
//		}

}

uint64_t first_multistep_4_6(const uint128_t start, const uint128_t number,
                                const float it_f, const uint_fast32_t nr_it, uint64_t res64);

// Datentypen für die Variablen der Multisteps
// als define zum leichten Experimentieren
#define SMALL_RES_T uint32_t
#define POT3_ODD_T uint32_t
#define IT_REST_T uint32_t
#define IT_F_T float
#define IT_MINF_T float
#define MARK_T uint32_t
#define NEW_IT_F_T float

// Cache für die Werte aus den Multistep-Lookup-Tables
IT_REST_T it_rest_arr[MAX_PARALLEL_FACTOR];
POT3_ODD_T pot3_odd_arr[MAX_PARALLEL_FACTOR];
IT_F_T it_f_arr[MAX_PARALLEL_FACTOR];
IT_MINF_T it_minf_arr[MAX_PARALLEL_FACTOR];

// Variablen die in einem Multistep laufend aktualisiert werden
uint64_t res64_arr[MAX_PARALLEL_FACTOR];
NEW_IT_F_T new_it_f_arr[MAX_PARALLEL_FACTOR];
MARK_T marks_arr[MAX_PARALLEL_FACTOR];
SMALL_RES_T small_res_arr[MAX_PARALLEL_FACTOR];

void load_res64(const uint128_t *restrict number, uint64_t *restrict res64)
{
    *res64 = (uint64_t) (*number);
}

void update_small_res(const uint64_t *restrict res64, SMALL_RES_T *restrict small_res)
{
    *small_res = (*res64) & ((1 << MS_DEPTH) - 1);
}

void fetch_ms_data(const SMALL_RES_T *restrict small_res_p,
                   IT_REST_T *restrict it_rest, POT3_ODD_T *restrict pot3_odd,
                   IT_F_T *restrict it_f, IT_MINF_T *restrict it_minf)
{
    uint32_t small_res = *small_res_p;
    *it_f = multistep_it_f[small_res];
    *it_minf = multistep_it_minf[small_res];
    *it_rest = multistep_it_rest[small_res];
    *pot3_odd = multistep_pot3_odd[small_res];
}

void update_res64(uint64_t *restrict res64, const IT_REST_T *restrict it_rest,
                  const POT3_ODD_T *restrict pot3_odd)
{
    *res64 = ((*res64) >> MS_DEPTH) * (*pot3_odd) + (*it_rest);
}

void update_new_it_f(NEW_IT_F_T *restrict new_it_f, const IT_F_T *restrict it_f)
{
    *new_it_f *= *it_f;
}

void load_new_it_f(NEW_IT_F_T *restrict new_it_f, const IT_F_T *restrict it_f, const float g_it_f)
{
    *new_it_f = g_it_f * (*it_f);
}

void load_mark_min(MARK_T *restrict mark, const float g_it_f, const IT_MINF_T *restrict it_minf)
{
    *mark = ((g_it_f * (*it_minf)) < MS_MIN_CHECK_VAL) ? 0 : UINT32_MAX;
}

void mark_min(MARK_T *restrict mark, const NEW_IT_F_T *restrict new_it_f, const IT_MINF_T *restrict it_minf)
{
    *mark &= (((*new_it_f) * (*it_minf)) < MS_MIN_CHECK_VAL) ? 0 : UINT32_MAX;
}

// führt die ersten 10 Multisteps aus
// es werden jeweils cand_cnt Schritte ausgeführt werden, deswegen
// sollte cand_cnt eine Konstante zur Kompilierzeit sein -> Vektorisierung
void ms_iter_1(const uint128_t *restrict number, const float it_f, const uint_fast32_t cand_cnt)
{
    // prefetch 1
    for(uint_fast32_t ms_idx = 0; ms_idx < cand_cnt; ms_idx++)
    {
        load_res64(&(number[ms_idx]),  &(res64_arr[ms_idx]));
        update_small_res(&(res64_arr[ms_idx]), &(small_res_arr[ms_idx]));
        fetch_ms_data(&(small_res_arr[ms_idx]), &(it_rest_arr[ms_idx]),
                      &(pot3_odd_arr[ms_idx]), &(it_f_arr[ms_idx]),
                      &(it_minf_arr[ms_idx]));
    }

    // compute 1
    for(uint_fast32_t ms_idx = 0; ms_idx < cand_cnt; ms_idx++)
    {
        load_mark_min(&(marks_arr[ms_idx]),it_f, &(it_minf_arr[ms_idx]));
        update_res64(&(res64_arr[ms_idx]), &(it_rest_arr[ms_idx]), &(pot3_odd_arr[ms_idx]));
        load_new_it_f(&(new_it_f_arr[ms_idx]), &(it_f_arr[ms_idx]), it_f);

        update_small_res(&(res64_arr[ms_idx]), &(small_res_arr[ms_idx]));
    }
}

// die zweiten 10 Multisteps, gleich wie ms_iter_1
void ms_iter_2(const uint_fast32_t cand_cnt)
{
    // prefetch
    for(uint_fast32_t ms_idx = 0; ms_idx < cand_cnt; ms_idx++)
    {
        fetch_ms_data(&(small_res_arr[ms_idx]), &(it_rest_arr[ms_idx]), &(pot3_odd_arr[ms_idx]),
                      &(it_f_arr[ms_idx]), &(it_minf_arr[ms_idx]));
    }

    // compute
    for(uint_fast32_t ms_idx = 0; ms_idx < cand_cnt; ms_idx++)
    {
        mark_min(&(marks_arr[ms_idx]), &(new_it_f_arr[ms_idx]), &(it_minf_arr[ms_idx]));
        update_res64(&(res64_arr[ms_idx]), &(it_rest_arr[ms_idx]), &(pot3_odd_arr[ms_idx]));
        update_new_it_f(&(new_it_f_arr[ms_idx]), &(it_f_arr[ms_idx]));
        update_small_res(&(res64_arr[ms_idx]), &(small_res_arr[ms_idx]));
    }
}

// die dritten 10 Multisteps, update_small_res weggelassen da unnötig,
// update_new_it_f und update_res64 müssen nur für markierte startzahlen ausgeführt werden
void ms_iter_3(const uint_fast32_t cand_cnt)
{
    // prefetch
    for(uint_fast32_t ms_idx = 0; ms_idx < cand_cnt; ms_idx++)
    {
        fetch_ms_data(&(small_res_arr[ms_idx]), &(it_rest_arr[ms_idx]), &(pot3_odd_arr[ms_idx]),
                      &(it_f_arr[ms_idx]), &(it_minf_arr[ms_idx]));
    }

    // compute
    for(uint_fast32_t ms_idx = 0; ms_idx < cand_cnt; ms_idx++)
    {
        mark_min(&(marks_arr[ms_idx]), &(new_it_f_arr[ms_idx]), &(it_minf_arr[ms_idx]));
    }
}

//Erster Multistep ohne Maximums-Prüfung in den ersten 30 Iterationen; nach Amateur
uint64_t first_multistep_parallel(const uint128_t*restrict start, const uint128_t*restrict number,
                                const float it_f, const uint_fast32_t nr_it, const uint_fast32_t parallel_count, const uint_fast32_t cand_cnt)
{
    uint64_t credits = cand_cnt;


    ms_iter_1(number, it_f, parallel_count);
    ms_iter_2(parallel_count);
    ms_iter_3(parallel_count);


    for(uint_fast32_t ms_idx = 0; ms_idx < cand_cnt; ms_idx++)
    {
        // finde markierte startwerte
        if(marks_arr[ms_idx])
        {
            // update von res64 und new_it_f nachholen
            update_res64(&(res64_arr[ms_idx]), &(it_rest_arr[ms_idx]), &(pot3_odd_arr[ms_idx]));
            update_new_it_f(&(new_it_f_arr[ms_idx]), &(it_f_arr[ms_idx]));
            // nächsten 30 Multisteps ausführen
            credits += first_multistep_4_6(start[ms_idx], number[ms_idx], new_it_f_arr[ms_idx], nr_it, res64_arr[ms_idx]);
        }
    }

    return credits;
}



//Erster Multistep ohne Maximums-Prüfung in den ersten 30 Iterationen; nach Amateur
uint64_t first_multistep_4_6(const uint128_t start, const uint128_t number,
                                const float it_f, const uint_fast32_t nr_it, const uint64_t g_res64)
{
    float new_it_f = it_f;
    uint64_t res64 = g_res64;
    uint8_t mark;

    // fest für 64/ms_depth = 6 implementiert!

    uint_fast32_t last_small_res;

    CHECK(checkpoint1++);

    if (new_it_f < MS_DECIDE_VAL)
    {
        ms_mark_min(&last_small_res, &mark, &res64, &new_it_f);
        if (mark)
        {
            return 0;
        }
    }
    else
    {
        ms_mark_max(&last_small_res, &mark, &res64, &new_it_f);
        if (mark) // Kandidat gefunden, nun genaue Nachrechnung, daher hier
        {				  // keine Fortführung nötig
            print_candidate(start);
            return 0;
        }
    }

    // update new_it_f nachholen, res64 wird nicht mehr benötigt
    new_it_f *= multistep_it_f[last_small_res];

    CHECK(checkpoint2++);
    CHECK(checkpoint5 += new_it_f);

    uint128_t new_nr;
    recalc_128(&number, &new_nr);
    CHECK(checkpoint4 += new_nr);

    return multistep(start, new_nr, new_it_f, nr_it + 6 * MS_DEPTH);
}
// Siebt Restklassen bis Iteration sieve_depth_first (32) vor und speichert die
// Ergebnisse der übrigbleibenden Restklassen in den folgenden globalen Arrays:
// k * 2^sieve_depth_first + reste_array[i] --> k * 3^it32_odd[i] + it32_rest[i]
// Dabei wird der Zähler restcnt_it32 bei jeder neuen hier gefundenen Restklasse,
// die noch zu betrachten ist, um Eins hochgezählt, sodass nach dem Ende der init-
// Methode dieser Wert die Anzahl aller dieser Restklassen mod 2^sieve_depth_first
// angibt.
void sieve_first_stage (const int nr_it, const uint_fast32_t rest,
                        const uint64_t it_rest,
                        const double it_f, const uint_fast32_t odd)
{
    if (nr_it >= SIEVE_DEPTH_FIRST)
    {
        // Nur Daten für Restklassen herausschreiben, die gerade betrachtet werden
        if ((idx_min <= restcnt_it32) && (restcnt_it32 < idx_max))
        {
            reste_array[restcnt_it32 - idx_min] = rest;
            it32_rest[restcnt_it32 - idx_min] = it_rest;
            it32_odd[restcnt_it32 - idx_min] = odd;
        }
        restcnt_it32++;
    }
    else
    {
        //new_rest = 0 * 2^nr_it + rest
        uint_fast32_t new_rest = rest;
        uint64_t new_it_rest = it_rest;
        double new_it_f = it_f;
        uint_fast32_t new_odd = odd;
        int laststepodd = 0;

        if ((new_it_rest & 1) == 0)
        {
            new_it_rest = new_it_rest >> 1;
            new_it_f *= 0.5;
        }
        else
        {
            new_it_rest += (new_it_rest >> 1) + 1;
            new_it_f *= 1.5;
            new_odd++;
            laststepodd = 1;
        }

        if (new_it_f * corfactor(new_odd, new_it_rest, laststepodd) > 0.98)
            sieve_first_stage(nr_it + 1, new_rest, new_it_rest, new_it_f, new_odd);

        //new_rest = 1 * 2^nr_it + rest

        new_rest = rest + (((uint32_t)1) << nr_it);//pot2_32Bit[nr_it];
        new_it_rest = it_rest + pot3_64Bit(odd);

        new_it_f = it_f;
        new_odd = odd;
        laststepodd = 0;

        if ((new_it_rest & 1) == 0)
        {
            new_it_rest = new_it_rest >> 1;
            new_it_f *= 0.5;
        }
        else
        {
            new_it_rest += (new_it_rest >> 1) + 1;
            new_it_f *= 1.5;
            new_odd++;
            laststepodd = 1;
        }

        if (new_it_f * corfactor(new_odd, new_it_rest, laststepodd) > 0.98)
            sieve_first_stage(nr_it + 1, new_rest, new_it_rest, new_it_f, new_odd);
    }
}

const uint_fast8_t testmod9[90] = {
                          1, 1, 0, 1, 0, 0, 1, 1, 0, // Um Verzweigungen nach startmod9 >=9 zu vermeiden
                          1, 1, 0, 1, 0, 0, 1, 1, 0,
                          1, 1, 0, 1, 0, 0, 1, 1, 0,
                          1, 1, 0, 1, 0, 0, 1, 1, 0,
                          1, 1, 0, 1, 0, 0, 1, 1, 0,
                          1, 1, 0, 1, 0, 0, 1, 1, 0,
                          1, 1, 0, 1, 0, 0, 1, 1, 0,
                          1, 1, 0, 1, 0, 0, 1, 1, 0,
                          1, 1, 0, 1, 0, 0, 1, 1, 0,
                          1, 1, 0, 1, 0, 0, 1, 1, 0};

const int POT2MOD9 = (1 << (SIEVE_DEPTH % 6)) % 9;
const uint128_t POT2_SIEVE_DEPTH = (((uint128_t) 1) << 31) << (SIEVE_DEPTH - 31);
const uint128_t NINE_TIMES_POT2_SIEVE_DEPTH = (((uint128_t) 9) << 31) << (SIEVE_DEPTH - 31);

// Siebt Reste von Iteration sieve_depth_second bis sieve_depth (40 bis 58)
// Für die übrigbleibenen Restklassen werden alle Zahlen bis 87*2^60 erzeugt und
// , wenn sie nicht kongruent 2 (mod 3) oder 4 (mod 9) sind, zur weiteren
// Berechnung der Multistep-Methode übergeben.

uint128_t start_arr[MAX_NO_OF_NUMBERS+MAX_PARALLEL_FACTOR];
uint128_t it_arr[MAX_NO_OF_NUMBERS+MAX_PARALLEL_FACTOR];

uint64_t sieve_third_stage (const uint64_t nr_it, const uint64_t rest,
                                const uint128_t it_rest,
                                const double it_f, const uint64_t odd)
{
    // Zählt, wie oft in den Multisteps die teuren 128-Bit-Nachrechnungen durchgeführt werden
    uint64_t credits = 0;

    if (nr_it >= SIEVE_DEPTH)
    {
        // Siebausgang
        // k * 2^sieve_depth + rest --> k * 3^odd + it_rest
        // sinngemäß:
        // for (k = 0; k < 2^(67-sieve_depth); k++)
        //   {Teste Startzahl k * 2^sieve_depth + rest}


        float new_it_f = (float) it_f;

        uint128_t start_0 = rest;
        uint128_t it_0 = it_rest;

        uint128_t start;
        uint128_t it;

        uint_fast32_t ms_start_count = 0;

        uint_fast32_t startmod9 = rest % 9;

        int j;	//Umgruppierung der Reihenfolge nach Amateur
        int k;

        for (j = 0; j < 9; j++)
        {
            if (testmod9[startmod9])
            {
                start = start_0;
                it    = it_0;
#ifdef INNER_LOOP_OUTPUT
                ms_start_count = 0;
#endif


                for (k=0; 9 * k + j < 87 * (1 << (60 - SIEVE_DEPTH)); k++)
                {
                    start_arr[ms_start_count] = start;
                    it_arr[ms_start_count] = it;
                    ms_start_count++;

                    start += NINE_TIMES_POT2_SIEVE_DEPTH;
                    it    += pot3[odd+2]; // = " ... + 9*pot3[odd]
                }

#ifdef INNER_LOOP_OUTPUT
                credits += first_multistep_parallel(start_arr, it_arr, it_f, SIEVE_DEPTH, ms_start_count);
#endif
            }

            start_0 += POT2_SIEVE_DEPTH;
            it_0    += pot3[odd];
            startmod9 += POT2MOD9; // startmod9 <= 8 + 9 * 8 < 90
        }
#ifndef INNER_LOOP_OUTPUT
        for(uint64_t i = 0; i < ms_start_count; i += MAX_PARALLEL_FACTOR)
        {
            uint_fast32_t count = ms_start_count - i >= MAX_PARALLEL_FACTOR ? MAX_PARALLEL_FACTOR : ms_start_count - i;
            credits += first_multistep_parallel(&(start_arr[i]), &(it_arr[i]), new_it_f, SIEVE_DEPTH, MAX_PARALLEL_FACTOR, count);
        }
#endif
    }
    else
    {
        //new_rest = 0 * 2^nr_it + rest
        uint64_t  new_rest = rest;
        uint128_t new_it_rest = it_rest;
        double new_it_f = it_f;
        unsigned int new_odd = odd;
        int laststepodd = (new_it_rest & 1);

        if ( laststepodd == 0)
        {
            new_it_rest = new_it_rest >> 1;
            new_it_f *= 0.5;
        }
        else
        {
            new_it_rest += (new_it_rest >> 1) + 1;
            new_it_f *= 1.5;
            new_odd++;
        }


        if ((new_it_f > 10) || //nachfolgende Bedingung benötigt sieve_depth <= 60
            (new_it_f * corfactor(new_odd, (uint64_t) new_it_rest, laststepodd) > 0.98))
        {
            credits += sieve_third_stage(nr_it + 1, new_rest, new_it_rest, new_it_f, new_odd);
        }

        //new_rest = 1 * 2^nr_it + rest
        new_rest = rest + (((uint64_t)1) << nr_it); //pot2[nr_it];
        new_it_rest = it_rest + pot3[odd];
        new_it_f = it_f;
        new_odd = odd;
        laststepodd = (new_it_rest & 1);

        if (laststepodd == 0)
        {
            new_it_rest = new_it_rest >> 1;
            new_it_f *= 0.5;
        }
        else
        {
            new_it_rest += (new_it_rest >> 1) + 1;
            new_it_f *= 1.5;
            new_odd++;
        }

        if ((new_it_f > 10) || //nachfolgende Bedingung benötigt sieve_depth <= 60
            (new_it_f * corfactor(new_odd, (uint64_t) new_it_rest, laststepodd) > 0.98))
        {
            credits += sieve_third_stage(nr_it + 1, new_rest, new_it_rest, new_it_f, new_odd);
        }
    }

    return credits;
}

#if defined BOINC
    double amount_of_work_done;
#endif

// Siebt Reste von Iteration sieve_depth_first bis Iteration
// sieve_depth_second (32 bis 40)
// Siebt Reste von Iteration sieve_depth_first bis Iteration sieve_depth_second (32 bis 40)
uint64_t sieve_second_stage (const uint_fast32_t nr_it, const uint64_t rest,
                             const uint64_t it_rest, const double it_f,
                             const uint_fast32_t odd)
{
    uint64_t credits = 0;

    if (nr_it >= SIEVE_DEPTH_SECOND)
    {
        credits += sieve_third_stage(nr_it, rest, it_rest, it_f, odd);
    }
    else // Verkürzte Darstellung des Siebs durch sudden6
    {
        uint64_t  new_rest[2] = {rest, rest + ((uint64_t) 1 << nr_it)};
        uint64_t new_it_rest[2] = {it_rest, it_rest + pot3_64Bit(odd)};
        double new_it_f[2] = {it_f, it_f};
        uint_fast32_t new_odd[2] = {odd, odd};
        uint_fast32_t laststepodd[2] = {(new_it_rest[0] & 1),
                                        (new_it_rest[1] & 1)};
        uint_fast32_t i;
        for(i = 0; i < 2; i++)
        {

            new_it_rest[i] = new_it_rest[i] >> 1;
            if (laststepodd[i])
            {
                new_it_rest[i] += 2 + (new_it_rest[i] << 1);
            }
            new_it_f[i] *= 0.5 + laststepodd[i];
            new_odd[i] += laststepodd[i];
        }

        for(i = 0; i < 2; i++)
        {
            if (new_it_f[i] * corfactor(new_odd[i], new_it_rest[i],
                                        laststepodd[i]) > 0.98)
            {
                credits += sieve_second_stage(nr_it + 1, new_rest[i],
                                              new_it_rest[i], new_it_f[i],
                                              new_odd[i]);

#if defined BOINC
                if (nr_it == SIEVE_DEPTH_FIRST + 2)
                {
                    amount_of_work_done += 1.0 / ((2 << 2) * (idx_max-idx_min));
                    boinc_fraction_done(amount_of_work_done);
                }
#endif
            }
#if defined BOINC
            else
            {
                if (nr_it <= SIEVE_DEPTH_FIRST + 2)
                {
                    amount_of_work_done += 1.0 / ((2 << (nr_it - SIEVE_DEPTH_FIRST)) * (idx_max-idx_min));
                    boinc_fraction_done(amount_of_work_done);
                }
            }
#endif
        }
    }

    return credits;
}

//Führt Initialisierung des Siebs aus
void init()
{
    // Liest schon abgearbeitete Restklassen ein, sofern sie existeren
    if (f_cleared.is_valid())
    {
        if(idx_min <= f_cleared.last_rest_class && (f_cleared.last_rest_class < idx_max))
        {
            for(uint_fast32_t i = 0; i <= (f_cleared.last_rest_class - idx_min); i++)
            {
                cleared_res[i] = 1;
            }
        }
    }

    // liest den letzten gefundenen Kandidaten ein
    if (f_candidate.is_valid())
    {
        last_found_candidate_before_resume = f_candidate.last_start;
    }

    // Bisher 0 betrachtete Restklassen mod 2^32
    restcnt_it32 = 0;

    // noch zu betrachtende Restklassen mod 2^sieve_depth_first erzeugen:

    // 2^1 * k + 1 --> 3^1 * k + 2
    sieve_first_stage(1, 1, 2, 1.5, 1);

    // Mehr Restklassen als restcnt_it32 gibt es nicht
    // --> idx_max nach oben dadurch abschneiden
    if (idx_max > restcnt_it32)
    {
        idx_max = restcnt_it32;

        // ggegebenenfalls muss dann auch idx_min angepasst werden
        if (idx_min > idx_max) idx_min = idx_max;
    }

    cout << endl << "Sieve initialized" << endl;
}

int main()
{
    // Initialisierungen
#if defined BOINC
    boinc_init();
#endif
    init_potarray();
    init_multistep();

    // Start und Ende des zu bearbeitendenen Bereichs aus Datei auslesen.
    if (!f_worktodo.is_valid())
    {
        cout << endl << "File 'worktodo.csv' is missing or invalid!" << endl;
#if defined BOINC
        boinc_finish(1);
#else
        return 1;
#endif
    }

    idx_min = f_worktodo.rest_class_start;
    idx_max = f_worktodo.rest_class_end;

    uint_fast32_t i;
    uint_fast32_t rescnt = 0;

    // Anzahl in diesem Durchlauf zu untersuchender Restklassen
    unsigned int size = idx_max - idx_min;

    //Speicher-Allokation für Ausgabe nach erstem Siebschritt
    reste_array = (uint_fast32_t*) malloc(size * sizeof(uint_fast32_t));
    it32_rest   = (uint64_t*) malloc(size * sizeof(uint64_t));
    it32_odd    = (uint32_t*) malloc(size * sizeof(uint32_t));
    cleared_res = (uint32_t*) calloc(size,  sizeof(uint32_t));

    if ((reste_array == NULL) || (it32_rest == NULL) || (it32_odd == NULL) || (cleared_res == NULL))
    {
        cout << endl << "Error while allocating memory!" << endl;
#if defined BOINC
        boinc_finish(1);
#else
        return 1;
#endif
    }


    // Initialisierung des Siebs
    init();

    uint64_t credits;

    cout << "Test of Residue Classes No. " << idx_min << endl;
    cout << " -- " << idx_max << endl;
    double start_time = get_time();

    // Möglichkeit zur Parallelisierung
    #pragma omp parallel for \
    private(i, it_rest_arr, pot3_odd_arr, it_f_arr, it_minf_arr, res64_arr, new_it_f_arr, marks_arr, \
            small_res_arr, credits, no_found_candidates, start_arr, it_arr) \
    shared(rescnt) schedule(dynamic)
    for (i = 0; i < idx_max - idx_min; i++)
    {
#if defined BOINC
        boinc_fraction_done((double) i/(idx_max-idx_min));
#endif
        if (!cleared_res[i])
        { // Nur, wenn Rest noch nicht abgearbeitet
            no_found_candidates = 0;
            //credits = sieve_second_stage(SIEVE_DEPTH_FIRST, reste_array[i], it32_rest[i],
            //                             ((double) pot3_64Bit(it32_odd[i])) / (((uint64_t)1) << SIEVE_DEPTH_FIRST),
            //                             it32_odd[i]);

            #pragma omp critical
            {
                rescnt++;
                cout << rescnt << ": Residue Class No. " << (i+idx_min)
                     << " is done. " << (get_time() - start_time) << "s"<<endl;

                f_cleared.append(i+idx_min, reste_array[i], credits, no_found_candidates);
#if defined BOINC
                boinc_checkpoint_completed();
#endif
            }
        }
    }

    //Speicherfreigabe nach getaner Arbeit
    free(reste_array);
    free(it32_rest);
    free(it32_odd);
    free(cleared_res);

    cout << "chk1: "  << checkpoint1 << " chk2: " << checkpoint2 <<
            " chk3: " << checkpoint3 << " chk4: " << checkpoint4 <<
            " chk5: " << checkpoint5 << endl;

#if defined BOINC
    boinc_finish(0);
#else
    return 0;
#endif
}
