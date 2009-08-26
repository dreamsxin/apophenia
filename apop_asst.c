/** \file apop_asst.c  The odds and ends bin. 
Copyright (c) 2005--2007 by Ben Klemens.  Licensed under the modified GNU GPL v2; see COPYING and COPYING2.  */

#include "asst.h"
#include "types.h"
#include <gsl/gsl_math.h>
#include <gsl/gsl_randist.h>

/** Calculate \f$\sum_{n=1}^N {1\over n^s}\f$
*/
double apop_generalized_harmonic(int N, double s){
/* There are no doubt efficient shortcuts do doing this, but I use brute force. [Though Knuth's Art of Programming v1 doesn't offer anything, which is strong indication of nonexistence.] To speed things along, I save the results so that they can later just be looked up. Each row in the saved structure is an \f$s\f$, and each column is \f$1\dots n\f$, up to the largest \f$n\f$ calculated to date.

\todo Look up the tricks for calculating this.

When reading the code, remember that the zeroth element holds the value for N=1, and so on.
*/
  static double * 	eses	= NULL;
  static int * 		lengths	= NULL;
  static int		  count	= 0;
  static double **	precalced=NULL;
  int			        j, old_len, i;
	for (i=0; i< count; i++)
		if (eses == NULL || eses[i] == s) 	
            break;
	if (i == count){	//you need to build the vector from scratch.
		count			++;
        i               = count - 1;
		precalced 		= realloc(precalced, sizeof (double*) * count);
		lengths 		= realloc(lengths, sizeof (int*) * count);
		eses 			= realloc(eses, sizeof (double) * count);
		precalced[i]	= malloc(sizeof(double) * N);
		lengths[i]	    = N;
		eses[i]		    = s;
		precalced[i][0]	= 1;
		old_len			= 1;
	}
	else {	//then you found it.
		old_len		= lengths[i];
	}
	if (N-1 >= old_len){	//It's there, but you need to extend what you have.
		precalced[i]	= realloc(precalced[i],sizeof(double) * N);
		for (j=old_len; j<N; j++)
			precalced[i][j] = precalced[i][j-1] + 1/pow((j+1),s);
	}
	return 	precalced[i][N-1];
}


/** Strip dots from a name.

\param  in          A string
\param  strip_type  'd': replace all '.' with '_'.<br>
                    'b': return only the string before the '.', so 'table.col' becomes 'table'. If there are multiple dots, cuts off at the first dot.
                    'a': return only the string after the '.', so 'table.col' becomes 'col'. If there are multiple dots, cuts off at the last dot.
\ingroup convenience_fns
 */
char * apop_strip_dots(char *in, char strip_type){
int     i;
char    *out    = NULL;
    if ((strip_type ==0) || (strip_type == 'd') || (strip_type == 'D')){
        out    = malloc(strlen(in)+1);
        for (i=0; i< strlen(in)+1; i++)   //will copy over the '/0' too.
            out[i] = (in[i] == '.') ? '_' : in[i];
    }
    else if ((strip_type ==1) || (strip_type == 'b') || (strip_type == 'B')){
        out    = malloc(strlen(in)+1);
        strcpy(out, in);
        for (i=strlen(in)+1; i--; )
            if (in[i] == '.'){
                out[i] = '\0';
                break;
            }
    }
    else if ((strip_type ==2) || (strip_type == 'a') || (strip_type == 'A')){
        for (i=0; i< strlen(in)+1; i++)
            if (in[i] == '.')
                break;
        out    = malloc(strlen(in)-i);
        strcpy(out, (in+i+1));
    }
    return out;
}

/** Inform the user of a faux pas. See also \ref Apop_assert, which allows the function to return a value.


 \param level   At what verbosity level should the user be warned? E.g., if level==2, then print iff apop_opts.verbosity >= 2. You can set apop_opts.verbose==-1 to turn off virtually all messages, but this is probably ill-advised.
 \param stop   Either 's' or 'c', indicating whether the program should stop or continue. If stopping, uses \c assert(0) for easy debugging. You can use 'h' (halt) as a synonym for 's'.
 \param msg The message to write to STDERR (presuming the verbosity level is high enough). This can be a printf-style format with following arguments. You can produce much more informative error messages this way, e.g., \c apop_error(0, 's', "Beta is %g but should be greater than zero.", beta);.
*/
void apop_error(int level, char stop, char *msg, ...){
  va_list   argp;
  char      *message;
    va_start(argp, msg);
    vasprintf(&message, msg, argp);
    va_end(argp);

    if (apop_opts.verbose >= level)
        fprintf(stderr, message);
    free(message);
    if (stop == 's' || stop == 'h')
        assert(0);
}


/** Call \c system(), but with <tt>printf</tt>-style arguments. E.g.,
  
 \code
char filenames[] = "apop_asst.c apop_asst.o"
apop_system("ls -l %s", filenames);
\endcode

\return The return value of the \c system() call.
 */
int apop_system(const char *fmt, ...){
  char 		*q;
  va_list   argp;
	va_start(argp, fmt);
	vasprintf(&q, fmt, argp);
	va_end(argp);
    int out = system(q);
    free(q);
    return out;
}


/* \defgroup sorting Sorting functions

 A few functions to sort data. One sorts an \c apop_data set in place, and one returns percentiles for a sorted vector.
  \{ */

#include "variadic.h"
#include "likelihoods.h"
#include <gsl/gsl_sort_vector.h>

static int find_min_unsorted(size_t *sorted, size_t height, size_t min){
    while (min<height)
        if (!sorted[min])   return min;
        else                min++;
    return -1;
}

static void temp_in_out(apop_data *d, gsl_vector *tv, double *tvd, size_t i, char **namein, char inout){
    if (inout == 'i'){
        if (d->matrix){
            APOP_ROW(d, i, row);
            gsl_vector_memcpy(tv, row);
        }
        if (d->vector)
            *tvd    = apop_data_get(d, i, -1);
        if (d->names && d->names->rowct > i){
            *namein = realloc(*namein, strlen(d->names->row[i])+1);
            strcpy(*namein, d->names->row[i]);
        }
        return;
    }//else writing out
    if (d->matrix){
        APOP_ROW(d, i, row);
        gsl_vector_memcpy(row, tv);
    }
    if (d->vector){
        double *j   = apop_data_ptr(d, i, -1);
        *j          = *tvd;
    }
    if (d->names && d->names->rowct > i){
        d->names->row[i] = realloc(d->names->row[i], strlen(*namein)+1);
        strcpy(d->names->row[i], *namein);
    }
}

static void shift(apop_data *d, size_t from,  size_t to){
  double *dp    = d->vector ? apop_data_ptr(d, to, -1): 0;
  char *namep  = d->names && d->names->rowct > to ? d->names->row[to] : NULL;
    if (d->matrix){
        APOP_ROW(d, to, tov);
        temp_in_out(d, tov, dp, from, &namep, 'i');
        return;
    }
    temp_in_out(d, NULL, dp , from, &namep, 'i');
}

/** This function sorts the whole of a \c apop_data set based on one column. Sorts in place, with little additional memory used.

 Uses the \c gsl_sort_vector_index function internally, and that function just ignores NaNs; therefore this function just leaves NaNs exactly where they lay.

 \param data    The input set to be modified. (No default, must not be \c NULL.)
 \param sortby  The column of data by which the sorting will take place. As usual, -1 indicates the vector element. (default: column zero of the matrix)
 \param asc   If 'd' or 'D', sort in descending order; else sort in ascending order. (Default: ascending)
 \return A pointer to the data set, so you can do things like \c apop_data_show(apop_data_sort(d, -1)).

This function uses the \ref designated syntax for inputs.
*/

APOP_VAR_HEAD apop_data * apop_data_sort(apop_data *data, int sortby, char asc){
    apop_data * apop_varad_var(data, NULL);
    apop_assert(data, NULL, 0, 's', "You gave me NULL data to sort.");
    int apop_varad_var(sortby, 0);
    char apop_varad_var(asc, 0);
    return apop_data_sort_base(data, sortby, asc);
APOP_VAR_ENDHEAD
  size_t            height  = (sortby==-1) ? data->vector->size: data->matrix->size1;
  size_t            sorted[height];
  size_t            i, *perm, start=0;
  gsl_permutation   *p  = gsl_permutation_alloc(height);
  gsl_vector        *tv = data->matrix ? gsl_vector_alloc(data->matrix->size2) : NULL;
  double            tvd;
  char              *nametmp = malloc(1);
    memset(sorted, 0, sizeof(size_t)*height);
    if (sortby == -1)
        gsl_sort_vector_index (p, data->vector);
    else {
        APOP_COL(data, sortby, v);
        gsl_sort_vector_index (p, v);
    }
    perm    = p->data;
    if (asc=='d' || asc=='D')
        for (size_t j=0; j< height/2; j++){
            tvd            = perm[j];
            perm[j]        = perm[height-1-j];
            perm[height-1-j] = tvd;
        }
    while (1){
        i           =
        start       = find_min_unsorted(sorted, height, start);
        if (i==-1) goto finished;
        temp_in_out(data, tv, &tvd, start, &nametmp, 'i');
        sorted[start]++;
        while (perm[i]!=start){
            shift(data, perm[i], i);
            sorted[perm[i]]++;
            i   = perm[i];
        }
        temp_in_out(data, tv, &tvd, i, &nametmp, 'o');
    }
finished:
    if (tv) gsl_vector_free(tv);
    if (nametmp) free(nametmp);
    gsl_permutation_free(p);
    return data;
}


/** Returns a vector of size 101, where returned_vector[95] gives the value of the 95th percentile, for example. Returned_vector[100] is always the maximum value, and returned_vector[0] is always the min (regardless of rounding rule).

\param data	a gsl_vector of data. (No default, must not be \c NULL.)
\param rounding This will either be 'u', 'd', or 'a'. Unless your data is exactly a multiple of 101, some percentiles will be ambiguous. If 'u', then round up (use the next highest value); if 'd' (or anything else), round down to the next lowest value; if 'a', take the mean of the two nearest points. If 'u' or 'a', then you can say "5% or more  of the sample is below returned_vector[5]"; if 'd' or 'a', then you can say "5% or more of the sample is above returned_vector[5]".   (Default = 'd'.)

This function uses the \ref designated syntax for inputs.
*/ 
APOP_VAR_HEAD double * apop_vector_percentiles(gsl_vector *data, char rounding){
    gsl_vector *apop_varad_var(data, NULL);
    apop_assert(data, NULL, 0, 's', "You gave me NULL data to sort.");
    char apop_varad_var(rounding, 'd');
    return apop_vector_percentiles_base(data, rounding);
APOP_VAR_ENDHEAD
  gsl_vector	*sorted	= gsl_vector_alloc(data->size);
  double		*pctiles= malloc(sizeof(double) * 101);
	gsl_vector_memcpy(sorted,data);
	gsl_sort_vector(sorted);
	for(int i=0; i<101; i++){
		int index = i*(data->size-1)/100.0;
		if (rounding == 'u' && index != i*(data->size-1)/100.0)
			index ++; //index was rounded down, but should be rounded up.
		if (rounding == 'a' && index != i*(data->size-1)/100.0)
            pctiles[i]	= (gsl_vector_get(sorted, index)+gsl_vector_get(sorted, index+1))/2.;
        else pctiles[i]	= gsl_vector_get(sorted, index);
	}
	gsl_vector_free(sorted);
	return pctiles;
}

/** \} */
