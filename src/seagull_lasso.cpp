#include <RcppArmadillo.h>
#include <cmath>
// [[Rcpp::depends(RcppArmadillo)]]

inline static double sqrt_double(double x) { return ::sqrt(x); }

using namespace Rcpp;
using namespace arma;

//' Lasso, group lasso, and sparse-group lasso
//' 
//' @name lasso_variants
//' 
//' @aliases lasso
//' 
//' @param VECTOR_Yc numeric vector of observations.
//' 
//' @param MATRIX_Xc numeric design matrix relating y to fixed and random
//' effects \eqn{[X Z]}. The columns may be permuted corresponding to their
//' group assignments.
//' 
//' @param VECTOR_WEIGHTS_FEATURESc numeric vector of weights for the vectors
//' of fixed and random effects \eqn{[b^T, u^T]^T}. The entries may be permuted
//' corresponding to their group assignments.
//' 
//' @param VECTOR_BETAc numeric vector whose partitions will be returned
//' (partition 1: estimates of fixed effects, partition 2: predictions of random
//' effects). During the computation the entries may be in permuted order. But
//' they will be returned according to the order of the user's input.
//' 
//' @param EPSILON_CONVERGENCE value for relative accuracy of the solution to
//' stop the algortihm for the current value of \eqn{\lambda}. The algorithm
//' stops after iteration m, if: \deqn{||sol^{(m)} - sol^{(m-1)}||_\infty <
//' \epsilon_c * ||sol1{(m-1)}||_2.}
//' 
//' @param ITERATION_MAX maximum number of iterations for each value of the
//' penalty parameter \eqn{\lambda}. Determines the end of the calculation if
//' the algorithm didn't converge according to \code{EPSILON_CONVERGENCE}
//' before.
//' 
//' @param GAMMA multiplicative parameter to decrease the step size during
//' backtracking line search. Has to satisfy: \eqn{0 < \gamma < 1}.
//' 
//' @param LAMBDA_MAX maximum value for the penalty parameter. This is the start
//' value for the grid search of the penalty parameter \eqn{\lambda}.
//' 
//' @param PROPORTION_XI multiplicative parameter to determine the minimum value
//' of \eqn{\lambda} for the grid search, i.e. \eqn{\lambda_{min} = \xi *
//' \lambda_{max}}. Has to satisfy: \eqn{0 < \xi \le 1}. If \code{xi=1}, only a
//' single solution for \eqn{\lambda = \lambda_{max}} is calculated.
//' 
//' @param NUMBER_INTERVALS number of lambdas for the grid search between
//' \eqn{\lambda_{max}} and \eqn{\xi * \lambda_{max}}. Loops are performed on a 
//' logarithmic grid.
//' 
//' @param NUMBER_FIXED_EFFECTS non-negative integer to determine the number of
//' fixed effects present in the mixed model.
//' 
//' @param TRACE_PROGRESS if \code{TRUE}, a messsage will occur on the screen
//' after each finished loop of the \eqn{\lambda} grid. This is particularly
//' useful for larger data sets.
//' 
// [[Rcpp::export]]
List seagull_lasso(
  NumericVector VECTOR_Yc,
  NumericMatrix MATRIX_Xc,
  NumericVector VECTOR_WEIGHTS_FEATURESc,
  NumericVector VECTOR_BETAc,
  double EPSILON_CONVERGENCE,
  int ITERATION_MAX,
  double GAMMA,
  double LAMBDA_MAX,
  double PROPORTION_XI,
  int NUMBER_INTERVALS,
  int NUMBER_FIXED_EFFECTS,
  bool TRACE_PROGRESS
  ) {
  
  
  /*********************************************************
   **     First initialization based on input variables:  **
   *********************************************************/
  int n = VECTOR_Yc.size();
  int p = VECTOR_WEIGHTS_FEATURESc.size();
  colvec VECTOR_Y(VECTOR_Yc.begin(), n, false);
  colvec VECTOR_WEIGHTS_FEATURES(VECTOR_WEIGHTS_FEATURESc.begin(), p, false);
  colvec VECTOR_BETA(VECTOR_BETAc.begin(), p, false);
  mat MATRIX_X(MATRIX_Xc.begin(), n, p, false);
  
  
  /*********************************************************
   **     Declaration and initialization of new internal  **
   **     variables:                                      **
   *********************************************************/
  int index_i              = 0;
  int index_j              = 0;
  int index_interval       = 0;
  int COUNTER              = 0;
  double LAMBDA            = 0.0;
  double TIME_STEP_T       = 0.0;
  double TEMP1             = 0.0;
  double TEMP2             = 0.0;
  double TEMP3             = 0.0;
  bool ACCURACY_REACHED    = false;
  bool CRITERION_FULFILLED = false;
  
  NumericVector VECTOR_BETA_NEWc (p);
  NumericVector VECTOR_GRADIENTc (p);
  NumericVector VECTOR_X_TRANSP_Yc (p);
  NumericVector VECTOR_TEMP1c (p);
  NumericVector VECTOR_TEMP2c (p);
  NumericVector VECTOR_TEMP3c (n);
  NumericVector VECTOR_TEMP_GRADIENTc (n);
  
  colvec VECTOR_BETA_NEW(VECTOR_BETA_NEWc.begin(), p, false);
  colvec VECTOR_GRADIENT(VECTOR_GRADIENTc.begin(), p, false);
  colvec VECTOR_X_TRANSP_Y(VECTOR_X_TRANSP_Yc.begin(), p, false);
  colvec VECTOR_TEMP1(VECTOR_TEMP1c.begin(), p, false);
  colvec VECTOR_TEMP2(VECTOR_TEMP2c.begin(), p, false);
  colvec VECTOR_TEMP3(VECTOR_TEMP3c.begin(), n, false);
  colvec VECTOR_TEMP_GRADIENT(VECTOR_TEMP_GRADIENTc.begin(), n, false);
  
  //Additional output variables:
  IntegerVector VECTOR_ITERATIONS (NUMBER_INTERVALS);
  NumericVector VECTOR_LAMBDA (NUMBER_INTERVALS);
  NumericMatrix MATRIX_SOLUTION (NUMBER_INTERVALS, p);
  
  
  /*********************************************************
   **     Beginning of proximal gradient descent:         **
   *********************************************************/
  //Calculate t(X)*y:
  VECTOR_X_TRANSP_Y = MATRIX_X.t() * VECTOR_Y;
  
  for (index_interval = 0; index_interval < NUMBER_INTERVALS; index_interval++) {
    Rcpp::checkUserInterrupt();
    ACCURACY_REACHED = false;
    COUNTER = 1;
    if (NUMBER_INTERVALS > 1) {
      LAMBDA = LAMBDA_MAX * exp((static_cast<double>(index_interval) / static_cast<double>(NUMBER_INTERVALS - 1)) * log(PROPORTION_XI));
    } else {
      LAMBDA = LAMBDA_MAX;
    }
    
    while ((!ACCURACY_REACHED) && (COUNTER <= ITERATION_MAX)) {
      //Calculate unscaled gradient t(X)*X*beta - t(X):
      VECTOR_TEMP_GRADIENT = MATRIX_X * VECTOR_BETA;
      VECTOR_GRADIENT      = MATRIX_X.t() * VECTOR_TEMP_GRADIENT;
      VECTOR_GRADIENT      = VECTOR_GRADIENT - VECTOR_X_TRANSP_Y;
      
      //Scale gradient with n, i.e. (t(X)*X*beta - t(X)y)/n:
      for (index_j = 0; index_j < p; index_j++) {
        VECTOR_GRADIENT(index_j) = VECTOR_GRADIENT(index_j) / static_cast<double>(n);
      }
      CRITERION_FULFILLED = false;
      TIME_STEP_T = 1.0;
      
      
      /*****************************************************
       **     Backtracking line search:                   **
       *****************************************************/
      while (!CRITERION_FULFILLED) {
        //Preparation for soft-thresholding:
        for (index_j = 0; index_j < p; index_j++) {
          VECTOR_TEMP1(index_j) = VECTOR_BETA(index_j) - TIME_STEP_T * VECTOR_GRADIENT(index_j);
          VECTOR_TEMP2(index_j) = LAMBDA * TIME_STEP_T * VECTOR_WEIGHTS_FEATURES(index_j);
        }
        
        //Soft-thresholding to obtain beta_new:
        for (index_j = 0; index_j < p; index_j++) {
          if (VECTOR_TEMP1(index_j) > VECTOR_TEMP2(index_j)) {
            VECTOR_BETA_NEW(index_j) = VECTOR_TEMP1(index_j) - VECTOR_TEMP2(index_j);
          } else if (VECTOR_TEMP1(index_j) < -VECTOR_TEMP2(index_j)) {
            VECTOR_BETA_NEW(index_j) = VECTOR_TEMP1(index_j) + VECTOR_TEMP2(index_j);
          } else {
            VECTOR_BETA_NEW(index_j) = 0.0;
          }
        }
        
        //beta-beta_new:
        VECTOR_TEMP1 = VECTOR_BETA - VECTOR_BETA_NEW;
        TEMP1 = 0.0;
        TEMP2 = 0.0;
        TEMP3 = 0.0;
        
        //loss_function(beta):
        VECTOR_TEMP3 = VECTOR_Y - MATRIX_X * VECTOR_BETA;
        for (index_i = 0; index_i < n; index_i++) {
          TEMP1 = TEMP1 + VECTOR_TEMP3(index_i) * VECTOR_TEMP3(index_i);
        }
        TEMP1 = (0.5 * TEMP1) / static_cast<double>(n);
        
        //t(gradient)*(beta-beta_new) and l2_norm_squared(beta-beta_new):
        for (index_j = 0; index_j < p; index_j++) {
          TEMP2 = TEMP2 + VECTOR_GRADIENT(index_j) * VECTOR_TEMP1(index_j);
          TEMP3 = TEMP3 + VECTOR_TEMP1(index_j) * VECTOR_TEMP1(index_j);
        }
        TEMP1 = TEMP1 - TEMP2 + (0.5 * TEMP3 / TIME_STEP_T);
        
        //loss_function(beta_new):
        TEMP2 = 0.0;
        VECTOR_TEMP3 = VECTOR_Y - MATRIX_X * VECTOR_BETA_NEW;
        for (index_i = 0; index_i < n; index_i++) {
          TEMP2 = TEMP2 + VECTOR_TEMP3(index_i) * VECTOR_TEMP3(index_i);
        }
        TEMP2 = (0.5 * TEMP2) / static_cast<double>(n);
        
        //Decrease time step t by a factor of gamma, if necessary:
        if (TEMP2 > TEMP1) {
          TIME_STEP_T = TIME_STEP_T * GAMMA;
          
        //Check for convergence, if time step size is alright:
        } else {
          //l_inf_norm(beta-beta_new):
          for (index_j = 0; index_j < p; index_j++) {
            if (VECTOR_TEMP1(index_j) < 0.0) {
              VECTOR_TEMP1(index_j) = -1.0 * VECTOR_TEMP1(index_j);
            }
          }
          TEMP1 = max(VECTOR_TEMP1);
          TEMP2 = 0.0;
          
          //l2_norm(beta)*epsilon_conv:
          for (index_j = 0; index_j < p; index_j++) {
            TEMP2 = TEMP2 + VECTOR_BETA(index_j) * VECTOR_BETA(index_j);
          }
          TEMP2 = sqrt_double(TEMP2) * EPSILON_CONVERGENCE;
          
          if (TEMP1 <= TEMP2) {
            ACCURACY_REACHED = true;
          }
          
          //Update: beta=beta_new:
          VECTOR_BETA = VECTOR_BETA_NEW;
          CRITERION_FULFILLED = true;
        }
      }
      
      COUNTER = COUNTER + 1;
    }
    if (TRACE_PROGRESS) {
      Rcout << "Loop: " << index_interval + 1 << " of " << NUMBER_INTERVALS << " finished." << std::endl;
    }
    
    //Store solution as single row in a matrix:
    for (index_j = 0; index_j < p; index_j++) {
      MATRIX_SOLUTION(index_interval, index_j) = VECTOR_BETA(index_j);
    }
    
    //Store information about iterations and lambda in a vector:
    VECTOR_ITERATIONS(index_interval) = COUNTER - 1;
    VECTOR_LAMBDA(index_interval) = LAMBDA;
  }
  
  
  /*********************************************************
   **     Prepare results as list and return list:        **
   *********************************************************/
  if (NUMBER_FIXED_EFFECTS == 0) {
    return List::create(Named("random_effects") = MATRIX_SOLUTION,
                        Named("lambda")         = VECTOR_LAMBDA,
                        Named("iterations")     = VECTOR_ITERATIONS,
                        Named("rel_acc")        = EPSILON_CONVERGENCE,
                        Named("max_iter")       = ITERATION_MAX,
                        Named("gamma_bls")      = GAMMA,
                        Named("xi")             = PROPORTION_XI,
                        Named("loops_lambda")   = NUMBER_INTERVALS);
  } else {
    NumericMatrix MATRIX_SOLUTION_FIXED (NUMBER_INTERVALS, NUMBER_FIXED_EFFECTS);
    NumericMatrix MATRIX_SOLUTION_RANDOM (NUMBER_INTERVALS, (p - NUMBER_FIXED_EFFECTS));
    
    for (index_i = 0; index_i < NUMBER_INTERVALS; index_i++) {
      for (index_j = 0; index_j < NUMBER_FIXED_EFFECTS; index_j++) {
        MATRIX_SOLUTION_FIXED(index_i, index_j) = MATRIX_SOLUTION(index_i, index_j);
      }
      for (index_j = 0; index_j < (p - NUMBER_FIXED_EFFECTS); index_j++) {
        MATRIX_SOLUTION_RANDOM(index_i, index_j) = MATRIX_SOLUTION(index_i, NUMBER_FIXED_EFFECTS + index_j);
      }
    }
    
    return List::create(Named("fixed_effects")  = MATRIX_SOLUTION_FIXED,
                        Named("random_effects") = MATRIX_SOLUTION_RANDOM,
                        Named("lambda")         = VECTOR_LAMBDA,
                        Named("iterations")     = VECTOR_ITERATIONS,
                        Named("rel_acc")        = EPSILON_CONVERGENCE,
                        Named("max_iter")       = ITERATION_MAX,
                        Named("gamma_bls")      = GAMMA,
                        Named("xi")             = PROPORTION_XI,
                        Named("loops_lambda")   = NUMBER_INTERVALS);
  }
}
