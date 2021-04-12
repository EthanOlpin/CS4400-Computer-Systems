/*******************************************
 * Solutions for the CS:APP Performance Lab
 ********************************************/

#include <stdio.h>
#include <stdlib.h>
#include "defs.h"

/* 
 * Please fill in the following student struct 
 */
student_t student = {
  "Ethan Olpin",     /* Full name */
  "u1018382@umail.utah.edu",  /* Email address */
};

/***************
 * COMPLEX KERNEL
 ***************/

/******************************************************
 * Your different versions of the complex kernel go here
 ******************************************************/

/* 
 * naive_complex - The naive baseline version of complex 
 */
char naive_complex_descr[] = "naive_complex: Naive baseline implementation";
void naive_complex(int dim, pixel *src, pixel *dest)
{
  int i, j;

  for(i = 0; i < dim; i++)
    for(j = 0; j < dim; j++)
    {

      dest[RIDX(dim - j - 1, dim - i - 1, dim)].red = ((int)src[RIDX(i, j, dim)].red +
						      (int)src[RIDX(i, j, dim)].green +
						      (int)src[RIDX(i, j, dim)].blue) / 3;
      
      dest[RIDX(dim - j - 1, dim - i - 1, dim)].green = ((int)src[RIDX(i, j, dim)].red +
							(int)src[RIDX(i, j, dim)].green +
							(int)src[RIDX(i, j, dim)].blue) / 3;
      
      dest[RIDX(dim - j - 1, dim - i - 1, dim)].blue = ((int)src[RIDX(i, j, dim)].red +
						       (int)src[RIDX(i, j, dim)].green +
						       (int)src[RIDX(i, j, dim)].blue) / 3;

    }
}
/* 
 * complex - Your current working version of complex
 * IMPORTANT: This is the version you will be graded on
 */
char complex_descr[] = "complex: Current working version";
void complex(int dim, pixel *src, pixel *dest)
{
  int i, j, k, l, w, v, dst_i, src_i, col;
  unsigned short avg[2];
  for(i = 0; i < dim; i += 8){
    w = i + 8;
    for(j = 0; j < dim; j += 8){
      v = j + 8;
      for(k = i; k < w; ++k) {
	col = dim - k - 1;
	for(l = j; l < v; l += 2){
	  dst_i = RIDX(dim - l - 1, col, dim);
	  src_i = RIDX(k, l, dim);

	  avg[0] = (src[src_i].red + 
		    src[src_i].green + 
		    src[src_i].blue) / 3;

	  dest[dst_i].red = avg[0];
	  dest[dst_i].green = avg[0];
          dest[dst_i].blue = avg[0];

	  avg[1] = (src[src_i + 1].red + 
		    src[src_i + 1].green + 
		    src[src_i + 1].blue) / 3;

	  dest[dst_i - dim].red = avg[1];
	  dest[dst_i - dim].green = avg[1];
          dest[dst_i - dim].blue = avg[1];
	}
      }
    }
  }
}
/*********************************************************************
 * register_complex_functions - Register all of your different versions
 *     of the complex kernel with the driver by calling the
 *     add_complex_function() for each test function. When you run the
 *     driver program, it will test and report the performance of each
 *     registered test function.  
 *********************************************************************/

void register_complex_functions() {
  add_complex_function(&complex, complex_descr);
  add_complex_function(&naive_complex, naive_complex_descr);
}
/***************
 * MOTION KERNEL
 **************/
/***************************************************************
 * Various helper functions for the motion kernel
 * You may modify these or add new ones any way you like.
 **************************************************************/
/* 
 * weighted_combo - Returns new pixel value at (i,j) 
 */
static pixel weighted_combo(int dim, int i, int j, pixel *src)
{
  int ii, jj;
  pixel current_pixel;

  int red, green, blue;
  red = green = blue = 0;

  int num_neighbors = 0;
  for(ii=0; ii < 3; ii++)
    for(jj=0; jj < 3; jj++)
      if ((i + ii < dim) && (j + jj < dim))
	{
	  num_neighbors++;
	  red += (int) src[RIDX(i+ii,j+jj,dim)].red;
	  green += (int) src[RIDX(i+ii,j+jj,dim)].green;
	  blue += (int) src[RIDX(i+ii,j+jj,dim)].blue;
	}

  current_pixel.red = (unsigned short) (red / num_neighbors);
  current_pixel.green = (unsigned short) (green / num_neighbors);
  current_pixel.blue = (unsigned short) (blue / num_neighbors);

  return current_pixel;
}
/******************************************************
 * Your different versions of the motion kernel go here
 ******************************************************/
/*
 * naive_motion - The naive baseline version of motion 
 */
char naive_motion_descr[] = "naive_motion: Naive baseline implementation";
void naive_motion(int dim, pixel *src, pixel *dst) 
{
  int i, j;
    
  for (i = 0; i < dim; i++)
    for (j = 0; j < dim; j++)
      dst[RIDX(i, j, dim)] = weighted_combo(dim, i, j, src);
}

/*
 * motion - Your current working version of motion. 
 * IMPORTANT: This is the version you will be graded on
 */
char motion_descr[] = "motion: Current working version";
void motion(int dim, pixel *src, pixel *dst) 
{
  int i, ii, j, jj, offset;
  int red[3];
  int green[3];
  int blue[3];
  pixel* dst_ptr;
  float num_neighbors;

  for (i = 0; i < dim; ++i){
    for (j = 0; j < dim; ++j){
	  dst_ptr = &dst[RIDX(i, j, dim)];
	  red[0] = green[0] = blue[0] = 0;

	  if((j + 2 < dim) && (i + 2 < dim)) {      
	    red[1] = green[1] = blue[1] = 0;
	    red[2] = green[2] = blue[2] = 0;
	    num_neighbors = 9;

	    offset = RIDX(i, j, dim);
	    red[0] += src[offset].red + src[offset + 1].red + src[offset + 2].red;
	    green[0] += src[offset].green + src[offset + 1].green + src[offset + 2].green;
	    blue[0] += src[offset].blue + src[offset + 1].blue + src[offset + 2].blue;

	    offset += dim;
	    red[1] += src[offset].red + src[offset + 1].red + src[offset + 2].red;
	    green[1] += src[offset].green + src[offset + 1].green + src[offset + 2].green;
	    blue[1] += src[offset].blue + src[offset + 1].blue + src[offset + 2].blue;
	      
	    offset += dim;
	    red[2] += src[offset].red + src[offset + 1].red + src[offset + 2].red;
	    green[2] += src[offset].green + src[offset + 1].green + src[offset + 2].green;
	    blue[2] += src[offset].blue + src[offset + 1].blue + src[offset + 2].blue;
 
	    red[0] += red[1] + red[2];
	    green[0] += green[1] + green[2];
	    blue[0] += blue[1] + blue[2];
	  }
	  else{
	    int w = (i + 2 < dim) ? 3 : dim - i; 
	    int v = (j + 2 < dim) ? 3 : dim - j;
	    num_neighbors = w * v;
	    for(ii=0; ii < w; ++ii){
	      for(jj = 0; jj < v; ++jj){
		offset = RIDX(i + ii,j + jj,dim);
		red[0] += (int) src[offset].red;
		green[0] += (int) src[offset].green;
		blue[0] += (int) src[offset].blue;
	      }
	    }
	  }
	  dst_ptr->red = (unsigned short) (red[0] / num_neighbors);
	  dst_ptr->green = (unsigned short) (green[0] / num_neighbors);
	  dst_ptr->blue = (unsigned short) (blue[0] / num_neighbors);
      }
  }
}

/********************************************************************* 
 * register_motion_functions - Register all of your different versions
 *     of the motion kernel with the driver by calling the
 *     add_motion_function() for each test function.  When you run the
 *     driver program, it will test and report the performance of each
 *     registered test function.  
 *********************************************************************/

void register_motion_functions() {
  add_motion_function(&motion, motion_descr);
  add_motion_function(&naive_motion, naive_motion_descr);
}
