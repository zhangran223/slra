#include <memory.h>

extern "C" {

#include <gsl/gsl_vector.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_errno.h>

#include <gsl/gsl_blas.h>
#include <gsl/gsl_math.h>

}

#include "slra.h"

slraFlexDerivativeComputations::slraFlexDerivativeComputations( const slraWkInterface *s, int r, int K ) :
    myK(K),/* myN(r),*/ myD(s->getNplusD() - r), myW(s) {
  
  myTempWkColRow = gsl_vector_alloc(myW->getNplusD());
  myDGamma = gsl_matrix_alloc(myD, myD * (2 * myW->getS() - 1));
  
  myWk_R =  gsl_matrix_alloc(myW->getNplusD(), myD);
  myWkT_R = gsl_matrix_alloc(myW->getNplusD(), myD);
  myN_k = gsl_matrix_alloc(myD, myD);
}

slraFlexDerivativeComputations::~slraFlexDerivativeComputations() {
  gsl_vector_free(myTempWkColRow);
  gsl_matrix_free(myDGamma);
  gsl_matrix_free(myWk_R);
  gsl_matrix_free(myWkT_R);
  gsl_matrix_free(myN_k);
}


void slraFlexDerivativeComputations::computeYrtDgammaYr( gsl_matrix *mgrad_r, 
         gsl_matrix *R, gsl_vector *yr ) {
  int k, ik;
  gsl_vector_view yr_sub;
  gsl_matrix_view yr_sub_matr, yr_sub_matr1, yr_sub_matr2;
  int d_times_m_div_k = yr->size / myK;
  int m_div_k = d_times_m_div_k / myD;

  

  gsl_matrix_set_zero(mgrad_r);
         
  for (k = 0; k < myW->getS(); k++) {
    gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, 2.0, myW->getWk(k), R, 0.0,  myWk_R);

    if (k > 0) {
      gsl_blas_dgemm(CblasTrans, CblasNoTrans, 2.0, myW->getWk(k), R, 0.0, myWkT_R);
    }

    for (ik = 0; ik < myK; ik++) {
      yr_sub = gsl_vector_subvector(yr, ik * d_times_m_div_k, d_times_m_div_k);
      yr_sub_matr = gsl_matrix_view_vector(&yr_sub.vector, m_div_k, myD);
      yr_sub_matr1 = gsl_matrix_submatrix(&yr_sub_matr.matrix, 0, 0, m_div_k - k, myD);
      yr_sub_matr2 = gsl_matrix_submatrix(&yr_sub_matr.matrix, k, 0, m_div_k - k, myD);
      gsl_blas_dgemm(CblasTrans, CblasNoTrans, 1.0, &yr_sub_matr1.matrix, &yr_sub_matr2.matrix, 
		     0.0, myN_k);

      gsl_blas_dgemm(CblasNoTrans, CblasTrans, 1.0, myWk_R, myN_k, 1.0, mgrad_r);
      if (k > 0) {
        gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, 1.0, myWkT_R, myN_k, 1.0, mgrad_r);
      }
    }
  }       
         
}

void slraFlexDerivativeComputations::computeDijGammaYr( gsl_vector *res, 
         gsl_matrix *R, gsl_matrix *perm, int i, int j,  gsl_vector *yr ) {
  gsl_matrix_view dgamma_k, dgamma_minus_k;
  gsl_vector_view tmp1_row, tmp1_col;
  gsl_vector_view w_k_row, w_k_col;
  int k, l;
  int d_times_m_div_k = yr->size / myK;
  int m_div_k = d_times_m_div_k / myD;


     
  /* form dgamma = d/d x_ij gamma */
  gsl_matrix_set_zero(myDGamma);

  for (k = 0; k < myW->getS(); k++) {
    dgamma_k = gsl_matrix_submatrix(myDGamma, 0, 
                   (myW->getS() - 1) * myD + k * myD, myD, myD);

    /* compute tmp1 = dx_ext' * w_k * x_ext * /
    /* Iterate over rows of dx_ext' * w_k */
    tmp1_row = gsl_matrix_row (&dgamma_k.matrix, j);
    w_k_row = gsl_matrix_column (perm, i);
    gsl_blas_dgemv(CblasTrans, 1.0, myW->getWk(k), &w_k_row.vector,
                   0.0, myTempWkColRow); 
    gsl_blas_dgemv(CblasTrans, 1.0, R, myTempWkColRow, 0.0, &tmp1_row.vector); 

    /* compute submat = submat  + x_ext' * tmp * dx_ext * /
    /* Iterate over rows of dx_ext' * w_k' */
    tmp1_col = gsl_matrix_column (&dgamma_k.matrix, j);
    gsl_blas_dgemv(CblasNoTrans, 1.0, myW->getWk(k), &w_k_row.vector,
                   0.0, myTempWkColRow); 
    gsl_blas_dgemv(CblasTrans, 1.0, R, myTempWkColRow, 1.0, &tmp1_col.vector); 
  }

  for (l = 0; l < myW->getS() - 1; l++) {
    dgamma_minus_k = gsl_matrix_submatrix(myDGamma, 0, l * myD, myD, myD);
    dgamma_k = gsl_matrix_submatrix(myDGamma, 0, 
                   (2 * myW->getS() - 2 - l) * myD, myD, myD);
    gsl_matrix_transpose_memcpy(&dgamma_minus_k.matrix, &dgamma_k.matrix);
  }
       
  /* compute st_ij = DGamma * yr */
  for (l = 0; l < myK; l++) { 
    gsl_vector_view res_v = gsl_vector_subvector(res, 
                                l * d_times_m_div_k, d_times_m_div_k);
    gsl_vector_view yr_v = gsl_vector_subvector(yr, 
                               l * d_times_m_div_k, d_times_m_div_k);
    tmv_prod_new(myDGamma, myW->getS(), &yr_v.vector, m_div_k, &res_v.vector);  
  }
}


slraFlexDerivativeComputationsExt::slraFlexDerivativeComputationsExt( slraFlexStructureExt *s, int r  ) :
    myStruct(s), myBase(myStruct->getWkInterface(), r, 1) {
  myTmpGrad = gsl_matrix_alloc(myStruct->getNplusD(), myBase.getD());  
}    

slraFlexDerivativeComputationsExt::~slraFlexDerivativeComputationsExt() {
  gsl_matrix_free(myTmpGrad);
 
}


void slraFlexDerivativeComputationsExt::computeYrtDgammaYr( gsl_matrix *grad, gsl_matrix *R, gsl_vector *yr ) {
  int n_row = 0;
  gsl_vector_view sub_yr;
  
  for (int k = 0; k < myStruct->getBlocksN(); n_row += myStruct->getMl(k), k++) {
    sub_yr = gsl_vector_subvector(yr, n_row * myBase.getD(), myStruct->getMl(k) * myBase.getD());    
  
    myBase.computeYrtDgammaYr(myTmpGrad, R, &sub_yr.vector);
    gsl_matrix_add(grad, myTmpGrad);
  }
}


void slraFlexDerivativeComputationsExt::computeDijGammaYr( gsl_vector *res, gsl_matrix *R, 
                   gsl_matrix *perm, int i, int j, gsl_vector *yr ) {
  int n_row = 0;
  gsl_vector_view sub_yr;
  gsl_vector_view sub_res;
  
  for (int k = 0; k < myStruct->getBlocksN(); n_row += myStruct->getMl(k), k++) {
    sub_yr = gsl_vector_subvector(yr, n_row * myBase.getD(), myStruct->getMl(k) * myBase.getD());    
    sub_res = gsl_vector_subvector(res, n_row * myBase.getD(), myStruct->getMl(k) * myBase.getD());    
  
    myBase.computeDijGammaYr(&sub_res.vector, R, perm, i, j, &sub_yr.vector);
  }                   
}



