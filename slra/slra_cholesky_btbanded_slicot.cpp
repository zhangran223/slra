#include <memory.h>
#include <cstdarg>
extern "C" {
#include <gsl/gsl_vector.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_blas.h>
#include <gsl/gsl_math.h>
}
#include "slra.h"

#ifdef USE_SLICOT
GammaCholeskyBTBandedSlicot::GammaCholeskyBTBandedSlicot( const StationaryStructure *s,
    int D, double reg_gamma  ) : 
    GammaCholeskyBTBanded(s, D, reg_gamma)  {
  myGammaVec = (double*) malloc(getD() * getD() * (getS() + 1) * sizeof(double));
  myCholeskyWorkSize = 1 + getS() * getD() * getD() + /* pDW */ 
                       3 * getD() + /* 3 * K */
                       mymax(getS(), getM() - getS()) * getD() * getD();
  myCholeskyWork = (double *)malloc(myCholeskyWorkSize * sizeof(double));                       
}

GammaCholeskyBTBandedSlicot::~GammaCholeskyBTBandedSlicot() {
  free(myGammaVec);
  free(myCholeskyWork);
}

void GammaCholeskyBTBandedSlicot::calcGammaCholesky( gsl_matrix *R )  {
  size_t info = 0;
  const size_t zero = 0;

  computeGammak(R);

  size_t D = getD(), Mg = getM();
  gsl_matrix_vectorize(myGammaVec, myGamma);
    
  mb02gd_("R", "N", &D, &Mg, &s_minus_1, &zero, 
          &Mg, myGammaVec, &D, myPackedCholesky, &d_times_s, 
          myCholeskyWork, &myCholeskyWorkSize, &info); /**/

  if (info) { 
    PRINTF("Error: info = %d", info); /* TO BE COMPLETED */
  }
}

#endif  
