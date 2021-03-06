/**
 * Command-line utility to use profit
 *
 * ICRAR - International Centre for Radio Astronomy Research
 * (c) UWA - The University of Western Australia, 2016
 * Copyright by UWA (in the framework of the ICRAR)
 * All rights reserved
 *
 * Contributed by Aaron Robotham, Rodrigo Tobar
 *
 * This file is part of libprofit.
 *
 * libprofit is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libprofit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libprofit.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "profit.h"
#include "psf.h"
#include "sersic.h"
#include "sky.h"

char **_parse_profile_value(char *token) {
	char **key_and_val;
	char *equals = strchr(token, '=');
	if( !equals ) {
		fprintf(stderr, "Parameter %s doesn't give a value\n", token);
		return NULL;
	}
	if( strlen(equals) == 1 ) {
		fprintf(stderr, "Parameter %s gives an empty value\n", token);
		return NULL;
	}

	size_t equals_idx = (size_t)(equals - token);
	key_and_val = (char **)malloc(sizeof(char *) * 2);
	key_and_val[0] = strndup(token, equals_idx);
	key_and_val[1] = strndup(equals + 1, strlen(token) - equals_idx);
	return key_and_val;
}

#define _READ_DOUBLE_OR_FAIL(key, val, name, dst) \
	do { \
		char *endptr; \
		double tmp; \
		if ( !strcmp(key, name) ) { \
			tmp = strtod(val, &endptr); \
			if( tmp == 0 && endptr == val ) { \
				fprintf(stderr, "Invalid double value for %s: %s\n", key, val); \
				return -1;\
			} \
			dst = tmp;\
			return 1; \
		} \
	} while(0);

#define _READ_FROM_LONGINT_OR_FAIL(key, val, name, dst, TYPE) \
	do { \
		char *endptr; \
		long int tmp; \
		if ( !strcmp(key, name) ) { \
			tmp = strtol(val, &endptr, 10); \
			if( tmp == 0 && endptr == val ) { \
				fprintf(stderr, "Invalid integer value for %s: %s\n", key, val); \
				return -1;\
			} \
			dst = (TYPE)tmp;\
			return 1; \
		} \
	} while(0);

#define _READ_BOOL_OR_FAIL(key, val, name, dst) _READ_FROM_LONGINT_OR_FAIL(key, val, name, dst, bool)
#define _READ_UINT_OR_FAIL(key, val, name, dst) _READ_FROM_LONGINT_OR_FAIL(key, val, name, dst, unsigned int)


short _keyval_to_sersic(profit_profile *p, char *key, char *val) {
	profit_sersic_profile *s = (profit_sersic_profile *)p;
	_READ_DOUBLE_OR_FAIL(key, val, "xcen",  s->xcen);
	_READ_DOUBLE_OR_FAIL(key, val, "ycen",  s->ycen);
	_READ_DOUBLE_OR_FAIL(key, val, "mag",   s->mag);
	_READ_DOUBLE_OR_FAIL(key, val, "re",    s->re);
	_READ_DOUBLE_OR_FAIL(key, val, "nser",  s->nser);
	_READ_DOUBLE_OR_FAIL(key, val, "ang",   s->ang);
	_READ_DOUBLE_OR_FAIL(key, val, "axrat", s->axrat);
	_READ_DOUBLE_OR_FAIL(key, val, "box",   s->box);

	_READ_BOOL_OR_FAIL(  key, val, "rough",          s->rough);
	_READ_DOUBLE_OR_FAIL(key, val, "acc",            s->acc);
	_READ_DOUBLE_OR_FAIL(key, val, "re_switch",      s->re_switch);
	_READ_UINT_OR_FAIL(  key, val, "resolution",     s->resolution);
	_READ_UINT_OR_FAIL(  key, val, "max_recursions", s->max_recursions);
	_READ_BOOL_OR_FAIL(  key, val, "adjust",         s->adjust);

	_READ_DOUBLE_OR_FAIL(key, val, "re_max",       s->re_max);
	_READ_BOOL_OR_FAIL(  key, val, "rescale_flux", s->rescale_flux);

	_READ_BOOL_OR_FAIL(key, val, "convolve", p->convolve);
	return 0;
}

short _keyval_to_sky(profit_profile *p, char *key, char *val) {
	profit_sky_profile *s = (profit_sky_profile *)p;
	_READ_DOUBLE_OR_FAIL(key, val, "bg",  s->bg);
	_READ_BOOL_OR_FAIL(key, val, "convolve", p->convolve);
	return 0;
}

short _keyval_to_psf(profit_profile *p, char *key, char *val) {
	profit_psf_profile *s = (profit_psf_profile *)p;
	_READ_DOUBLE_OR_FAIL(key, val, "xcen",  s->xcen);
	_READ_DOUBLE_OR_FAIL(key, val, "ycen",  s->ycen);
	_READ_DOUBLE_OR_FAIL(key, val, "mag",   s->mag);
	return 0;
}

profit_profile *desc_to_profile(
	profit_model *model,
	char *description,
	const char* name,
	unsigned short allow_empty_profile,
	short (keyval_to_param)(profit_profile *, char *, char *)
) {

	char *tok;
	char **key_and_val;
	short assigned;
	profit_profile *p;

	if( !description && !allow_empty_profile ) {
		fprintf(stderr, "Empty %s profile description\n", name);
		return NULL;
	}

	p = profit_create_profile(model, name);
	if( !description ) {
		return p;
	}

	while( (tok = strtok(description, ":")) ) {

		key_and_val = _parse_profile_value(tok);
		if( !key_and_val ) {
			fprintf(stderr, "Invalid token found in %s profile description: %s\n", name, description);
			return NULL;
		}

		assigned = keyval_to_param(p, key_and_val[0], key_and_val[1]);
		if( assigned == -1 ) {
			return NULL;
		}
		if( !assigned ) {
			fprintf(stderr, "Ignoring unknown %s profile parameter: %s\n", name, key_and_val[0]);
		}
		free(key_and_val[0]);
		free(key_and_val[1]);
		free(key_and_val);

		/* Otherwise we'll always start strtok from the beginning */
		description = NULL;
	}
	return p;
}

profit_profile *parse_profile(profit_model *model, char *description) {

	/* The description might be only a name */
	char *subdesc = NULL;
	size_t name_end = strlen(description);
	char *colon = strchr(description, ':');
	if( colon ) {
		name_end = (size_t)(colon - description);
		subdesc = colon + 1;
	}

	if( !strncmp(description, "sersic", name_end) ) {
		return desc_to_profile(model, subdesc, "sersic", 0, &_keyval_to_sersic);
	}
	else if( !strncmp(description, "sky", name_end) ) {
		return desc_to_profile(model, subdesc, "sky", 0, &_keyval_to_sky);
	}
	else if( !strncmp(description, "psf", name_end) ) {
		return desc_to_profile(model, subdesc, "psf", 0, &_keyval_to_psf);
	}

	fprintf(stderr, "Unknown profile name in profile description: %s\n", description);
	return NULL;

}

double *parse_psf(char *optarg, unsigned int *psf_width, unsigned int *psf_height) {

	char *tok, *values, *endptr;
	unsigned int size, i = 0;
	double *psf;

	/* format is w:h:val1,val2... */
	tok = strtok(optarg, ":");
	if( !tok ) {
		fprintf(stderr, "Missing psf's width\n");
		return NULL;
	}
	*psf_width = (unsigned int)strtoul(tok, &endptr, 10);
	if( tok == endptr ) {
		fprintf(stderr, "Invalid value for psf's width: %s\n", tok);
		return NULL;
	}

	tok = strtok(NULL, ":");
	if( !tok ) {
		fprintf(stderr, "Missing psf's height\n");
		return NULL;
	}
	*psf_height = (unsigned int)strtoul(tok, &endptr, 10);
	if( tok == endptr ) {
		fprintf(stderr, "Invalid value for psf's height: %s\n", tok);
		return NULL;
	}

	values = strtok(NULL, ":");
	if( !values ) {
		fprintf(stderr, "Missing psf's values\n");
		return NULL;
	}

	size = *psf_width * *psf_height;
	psf = (double *)malloc(sizeof(double) * size);
	while( (tok = strtok(values, ",")) ) {
		values = NULL;
		psf[i] = strtod(tok, &endptr);
		if( psf[i] == 0 && tok == endptr ) {
			fprintf(stderr, "Invalid floating-point value for psf: %s\n", tok);
			free(psf);
			return NULL;
		}
		i++;
	}

	if( i != size ) {
		fprintf(stderr, "Not enough values provided for PSF. Provided: %u, expected: %u\n", i, size);
		free(psf);
		return NULL;
	}

	return psf;
}

void usage(FILE *file, char *argv[]) {
	fprintf(file,"\n%s: utility program to generate an image out of a model and a set of profiles\n\n", argv[0]);
	fprintf(file,"This program is licensed under the GPLv3 license.\n\n");
	fprintf(file,"Usage: %s [options] -p <spec> [-p <spec> ...]\n\n",argv[0]);
	fprintf(file,"Options:\n");
	fprintf(file,"  -t        Output image as text values on stdout\n");
	fprintf(file,"  -b        Output image as binary content on stdout\n");
	fprintf(file,"  -f <file> Output image as fits file\n");
	fprintf(file,"  -i <n>    Output performance information after evaluating the model n times\n");
	fprintf(file,"  -w        Image width. Defaults to 100\n");
	fprintf(file,"  -H        Image height. Defaults to 100\n");
	fprintf(file,"  -m        Zero magnitude. Defaults to 0.\n");
	fprintf(file,"  -P        PSF function (specified as w:h:val1,val2..., or as a FITS filename)\n");
	fprintf(file,"  -h,-?     Show this help and exit\n");
	fprintf(file,"  -v        Show the program version and exit\n\n");
	fprintf(file,"Profiles should be specified as follows:\n\n");
	fprintf(file,"-p name:param1=val1:param2=val2:...\n\n");
	fprintf(file,"The following profiles (and parameters) are currently accepted:\n\n");
	fprintf(file," * psf: xcen, ycen, mag\n");
	fprintf(file," * sky: bg\n");
	fprintf(file,"\
 * sersic: xcen, ycen, mag, re, nser, box, ang, axrat,\n\
           rough, re_switch, max_recursions, resolution, acc,\n\
           re_max, rescale_flux,\n\
           adjust\n\n");
	fprintf(file,"For more information visit https://libprofit.readthedocs.io.\n\n");
}

static inline
bool is_little_endian() {
	volatile uint32_t i=0x01234567;
	return (*((uint8_t*)(&i))) == 0x67;
}

static inline
double swap_bytes(double v) {
	double r;
	char *vbytes = (char *)(&v);
	char *rbytes = (char *)(&r);
	rbytes[0] = vbytes[7];
	rbytes[1] = vbytes[6];
	rbytes[2] = vbytes[5];
	rbytes[3] = vbytes[4];
	rbytes[4] = vbytes[3];
	rbytes[5] = vbytes[2];
	rbytes[6] = vbytes[1];
	rbytes[7] = vbytes[0];
	return r;
}

#define FITS_BLOCK_SIZE (36*80)

double *read_image_from_fits_file(char *filename, unsigned int *width, unsigned int *height) {

	FILE *f;
	unsigned int i, pos, padding;
	char hdr[80];

	*width = *height = 0;

	f = fopen(filename, "rb");
	if( !f ) {
		perror("Couldn't open file for reading");
		return NULL;
	}

	/*
	 * Standard headers, we're assuming they say 'T" for SIMPLE, -64 for BITPIX
	 * and 2 for NAXIS.
	 */
	while( fread(hdr, 1, 80, f) ) {

		if( !strncmp("NAXIS1", hdr, 6) ) {
			sscanf(hdr, "NAXIS1 = %u", width);
			printf("%u\n", *width);
		}
		else if( !strncmp("NAXIS2", hdr, 6) ) {
			sscanf(hdr, "NAXIS2 = %u", height);
			printf("%u\n", *height);
		}
		else if( !strncmp("END", hdr, 3) ) {
			break;
		}
	}

	pos = (unsigned int)ftell(f);
	padding = FITS_BLOCK_SIZE - (pos % FITS_BLOCK_SIZE);
	fseek(f, padding, SEEK_CUR);

	unsigned int size = *width * *height;
	double *out = (double *)malloc(sizeof(double) * size);
	fread(out, size, sizeof(double), f);
	fclose(f);

	/* data has to be big-endian */
	if( is_little_endian() ) {
		double *it = out;
		for(i=0; i!=size; i++) {
			*it = swap_bytes(*it);
			it++;
		}
	}

	return out;
}

int to_fits(profit_model *m, char *fits_output) {

	FILE *f;
	unsigned int i, j, pos, padding;
	char hdr[80];
	char *filename = fits_output;

	/* Append .fits if not in the name yet */
	if( strlen(fits_output) < 5 ||
	    (strstr(fits_output, ".fits") != &fits_output[strlen(fits_output) - 5]) ) {
		filename = (char *)malloc(strlen(fits_output) + 6);
		sprintf(filename, "%s.fits", fits_output);
	}

	f = fopen(filename, "w+");
	if( filename != fits_output ) {
		free(filename);
	}
	if( !f ) {
		return 1;
	}

	/*
	 * Standard headers
	 *
	 * The first five headers are required, and must be in "fixed format",
	 * meaning that their values must be right-indented on column 30, sigh...
	 */
	fprintf(f, "%-80s", "SIMPLE  =                    T / File conforms to FITS standard");
	fprintf(f, "%-80s", "BITPIX  =                  -64 / Bits per pixel");
	fprintf(f, "%-80s", "NAXIS   =                    2 / Number of axes");
	sprintf(hdr, "NAXIS1  =           %10.0u / Width", m->width);
	fprintf(f, "%-80s", hdr);
	sprintf(hdr, "NAXIS2  =           %10.0u / Height", m->height);
	fprintf(f, "%-80s", hdr);
	fprintf(f, "%-80s", "CRPIX1  = 1");
	fprintf(f, "%-80s", "CRVAL1  = 1");
	fprintf(f, "%-80s", "CDELT1  = 1");
	fprintf(f, "%-80s", "CTYPE1  = ' '");
	fprintf(f, "%-80s", "CUNIT1  = ' '");
	fprintf(f, "%-80s", "CRPIX2  = 1");
	fprintf(f, "%-80s", "CRVAL2  = 1");
	fprintf(f, "%-80s", "CDELT2  = 1");
	fprintf(f, "%-80s", "CTYPE2  = ' '");
	fprintf(f, "%-80s", "CUNIT2  = ' '");
	fprintf(f, "%-80s", "END");

	pos = (unsigned int)ftell(f);
	padding = FITS_BLOCK_SIZE - (pos % FITS_BLOCK_SIZE);
	for(i=0; i<padding; i++) {
		fprintf(f, " ");
	}

	/* data has to be big-endian */
	if( is_little_endian() ) {
		double *big_endian_image = (double *)malloc(sizeof(double) * m->width * m->height);
		for(j=0; j!=m->height; j++) {
			for(i=0; i!=m->width; i++) {
				pos = i + j*m->width;
				big_endian_image[pos] = swap_bytes(m->image[pos]);
			}
		}

		/* Simply replace the model's image, nobody will use it but us */
		free(m->image);
		m->image = big_endian_image;
	}

	fwrite(m->image, sizeof(double), m->width * m->height, f);

	/* Pad with zeroes until we complete the current 36*80 block */
	padding = FITS_BLOCK_SIZE - (((unsigned int)sizeof(double) * m->width * m->height) % FITS_BLOCK_SIZE);
	void *zeros = calloc(padding, 1);
	fwrite(zeros, 1, padding, f);
	free(zeros);
	fclose(f);

	return 0;
}

typedef enum _output_type {
	none = 0,
	binary = 1,
	text = 2,
	fits = 3,
	performance = 4
} output_t;

int main(int argc, char *argv[]) {

	int opt;
	unsigned int width = 100, height = 100, iterations = 1;
	long duration;
	double magzero = 0;
	unsigned int i, j;
	char *endptr, *error, *fits_output = NULL;
	output_t output = none;
	profit_profile *profile;
	profit_model *m = profit_create_model();
	struct stat stat_buf;

#define CLEAN_AND_EXIT(code) \
	profit_cleanup(m); \
	free(fits_output); \
	return code

	while( (opt = getopt(argc, argv, "h?vP:p:w:H:m:tbf:i:")) != -1 ) {
		switch(opt) {

			case 'h':
			case '?':
				usage(stdout, argv);
				CLEAN_AND_EXIT(0);

			case 'v':
				printf("libprofit version %s\n", PROFIT_VERSION);
				CLEAN_AND_EXIT(0);

			case 'p':
				profile = parse_profile(m, optarg);
				if( profile == NULL ) {
					CLEAN_AND_EXIT(1);
				}
				break;

			case 'P':
				if( !stat(optarg, &stat_buf) ) {
					m->psf = read_image_from_fits_file(optarg, &m->psf_width, &m->psf_height);
					printf("w/h: %u/%u\n", m->psf_width, m->psf_height);
					printf("first/before-last: %g/%g\n", m->psf[0], m->psf[m->psf_width * m->psf_height - 2]);
				}
				else {
					m->psf = parse_psf(optarg, &m->psf_width, &m->psf_height);
				}
				if( !m->psf ) {
					usage(stderr, argv);
					CLEAN_AND_EXIT(1);
				}
				break;

			case 'w':
				width = (unsigned int)atoi(optarg);
				break;

			case 'H':
				height = (unsigned int)atoi(optarg);
				break;

			case 'm':
				magzero = strtod(optarg, &endptr);
				if( magzero == 0 && endptr == optarg ) {
					fprintf(stderr, "Invalid magzero value: %s\n", optarg);
					CLEAN_AND_EXIT(1);
				}
				break;

			case 't':
				if( output != none ) {
					fprintf(stderr, "-t and -b cannot be used together\n");
					CLEAN_AND_EXIT(1);
				}
				output = text;
				break;

			case 'b':
				if( output != none ) {
					fprintf(stderr, "-b and -t cannot be used together\n");
					CLEAN_AND_EXIT(1);
				}
				output = binary;
				break;

			case 'f':
				free(fits_output);
				fits_output = strdup(optarg);
				output = fits;
				break;

			case 'i':
				iterations = (unsigned int)atoi(optarg);
				output = performance;
				break;

			default:
				usage(stderr, argv);
				CLEAN_AND_EXIT(1);

		}
	}

	/* No profiles given */
	if( !m->n_profiles ) {
		usage(stderr, argv);
		CLEAN_AND_EXIT(1);
	}

	/* We default to text output */
	if( output == none ) {
		output = text;
	}

	m->width      = width;
	m->height     = height;
	m->res_x      = width;
	m->res_y      = height;
	m->magzero    = magzero;

	/* This means that we evaluated the model once, but who cares */
	struct timeval start, end;
	gettimeofday(&start, NULL);
	for(i=0; i!=iterations; i++) {
		free(m->image);
		free(m->error);
		profit_eval_model(m);
	}
	gettimeofday(&end, NULL);
	duration = (end.tv_sec - start.tv_sec)*1000000 + (end.tv_usec - start.tv_usec);

	/* Check for any errors */
	error = profit_get_error(m);
	if( error ) {
		fprintf(stderr, "Error while calculating model: %s\n", error);
		CLEAN_AND_EXIT(1);
	}

	switch(output) {

		case binary:
			fwrite(m->image, sizeof(double), m->width * m->height, stdout);
			break;

		case text:
			for(j=0; j!=m->height; j++) {
				for(i=0; i!=m->width; i++) {
					printf("%g ", m->image[j*m->width + i]);
				}
				printf("\n");
			}
			break;

		case fits:
			if( to_fits(m, fits_output) ) {
				perror("Error while saving image to FITS file");
				CLEAN_AND_EXIT(1);
			}
			break;

		case performance:
			printf("Ran %d iterations in %.3f [s] (%.3f [ms] per iteration)\n", iterations, (double)duration/1000000., (double)duration/1000./iterations);
			break;

		default:
			fprintf(stderr, "Output not currently supported: %d\n", output);
	}

	CLEAN_AND_EXIT(0);

}
