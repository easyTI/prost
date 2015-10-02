#ifndef SPARSE_MATRIX_HPP_
#define SPARSE_MATRIX_HPP_

#include <cuda_runtime.h>
#include <cusparse.h>
#include <cassert>
#include <cmath>
#include <memory.h>
#include <cstdlib>
#include <iostream>
#include <stdint.h>

#include "util/cuwrap.hpp"

/**
 * @brief converts CSR format to CSC format, not in-place,
 *                 if a == NULL, only pattern is reorganized.
 *                 the size of matrix is n x m.
 */

template<typename T>
void csr2csc(int n, int m, int nz, T *a, int *col_idx, int *row_start,
             T *csc_a, int *row_idx, int *col_start)
{
  int i, j, k, l;
  int *ptr;

  for (i=0; i<=m; i++) col_start[i] = 0;

  /* determine column lengths */
  for (i=0; i<nz; i++) col_start[col_idx[i]+1]++;

  for (i=0; i<m; i++) col_start[i+1] += col_start[i];


  /* go through the structure once more. Fill in output matrix. */

  for (i=0, ptr=row_start; i<n; i++, ptr++)
    for (j=*ptr; j<*(ptr+1); j++){
      k = col_idx[j];
      l = col_start[k]++;
      row_idx[l] = i;
      if (a) csc_a[l] = a[j];
    }

  /* shift back col_start */
  for (i=m; i>0; i--) col_start[i] = col_start[i-1];

  col_start[0] = 0;
}


               

/**
 * @brief Wrapper class around cuSPARSE API.
 */
template<typename real>
class SparseMatrix {
  SparseMatrix() { }
  
public:
  virtual ~SparseMatrix() {
    cudaFree(d_ind_);
    cudaFree(d_val_);
    cudaFree(d_ptr_);

    delete [] h_ind_;
    delete [] h_val_;
    delete [] h_ptr_;

    delete [] h_ind_t_;
    delete [] h_val_t_;
    delete [] h_ptr_t_;
  }

  static SparseMatrix<real> *CreateFromCSC(
      int m,
      int n,
      int nnz,
      real *val,
      int32_t *ptr,
      int32_t *ind)
  {
    SparseMatrix<real> *mat = new SparseMatrix<real>;

    cusparseCreate(&mat->cusp_handle_);
    cusparseCreateMatDescr(&mat->descr_);
    cusparseSetMatType(mat->descr_, CUSPARSE_MATRIX_TYPE_GENERAL);
    cusparseSetMatIndexBase(mat->descr_, CUSPARSE_INDEX_BASE_ZERO);

    mat->m_ = m;
    mat->n_ = n;
    mat->nnz_ = nnz;

    cudaMalloc((void **)&mat->d_ind_, sizeof(int32_t) * 2 * mat->nnz_);
    cudaMalloc((void **)&mat->d_ptr_, sizeof(int32_t) * (mat->m_ + mat->n_ + 2));
    cudaMalloc((void **)&mat->d_val_, sizeof(real) * 2 * mat->nnz_);

    mat->d_ind_t_ = &mat->d_ind_[mat->nnz_];
    mat->d_ptr_t_ = &mat->d_ptr_[mat->m_ + 1];
    mat->d_val_t_ = &mat->d_val_[mat->nnz_];

    cudaMemcpy(mat->d_ind_t_, ind, sizeof(int32_t) * mat->nnz_, cudaMemcpyHostToDevice);
    cudaMemcpy(mat->d_ptr_t_, ptr, sizeof(int32_t) * (mat->n_ + 1), cudaMemcpyHostToDevice);
    cudaMemcpy(mat->d_val_t_, val, sizeof(real) * mat->nnz_, cudaMemcpyHostToDevice);

    mat->h_ind_ = new int32_t[mat->nnz_];
    mat->h_ptr_ = new int32_t[mat->m_ + 1];
    mat->h_val_ = new real[mat->nnz_];

    mat->h_ind_t_ = new int32_t[mat->nnz_];
    mat->h_ptr_t_ = new int32_t[mat->n_ + 1];
    mat->h_val_t_ = new real[mat->nnz_];

    memcpy(mat->h_ind_t_, ind, sizeof(int32_t) * mat->nnz_);
    memcpy(mat->h_ptr_t_, ptr, sizeof(int32_t) * (mat->n_ + 1));
    memcpy(mat->h_val_t_, val, sizeof(real) * mat->nnz_);

    // fill h_ind_, h_ptr_, h_val_ from transpose. csr to csc
    csr2csc<real>(mat->n_,
                  mat->m_,
                  mat->nnz_,
                  mat->h_val_t_,
                  mat->h_ind_t_,
                  mat->h_ptr_t_,
                  mat->h_val_,
                  mat->h_ind_,
                  mat->h_ptr_);
    
    cudaMemcpy(mat->d_ind_, mat->h_ind_, sizeof(int32_t) * mat->nnz_, cudaMemcpyHostToDevice);
    cudaMemcpy(mat->d_ptr_, mat->h_ptr_, sizeof(int32_t) * (mat->m_ + 1), cudaMemcpyHostToDevice);
    cudaMemcpy(mat->d_val_, mat->h_val_, sizeof(real) * mat->nnz_, cudaMemcpyHostToDevice);

    /*        
    mat->FillTranspose();
    cudaMemcpy(mat->h_ind_, mat->d_ind_, sizeof(int32_t) * mat->nnz_, cudaMemcpyDeviceToHost);
    cudaMemcpy(mat->h_ptr_, mat->d_ptr_, sizeof(int32_t) * (mat->m_ + 1), cudaMemcpyDeviceToHost);
    cudaMemcpy(mat->h_val_, mat->d_val_, sizeof(real) * mat->nnz_, cudaMemcpyDeviceToHost);
    */
    
    return mat;
  }

  // d_result = alpha * K * d_rhs + beta * d_result
  bool MultVec(
      real *d_rhs,
      real *d_result,
      bool trans,
      real alpha = 1,
      real beta = 0) const;
  
  int nrows() const { return m_; }
  int ncols() const { return n_; }

  real row_sum(int row, real alpha) const {
    real sum = 0;

    assert(h_ptr_[row] >= 0);
    assert(h_ptr_[row] <= nnz_);
    assert(h_ptr_[row + 1] >= 0);
    assert(h_ptr_[row + 1] <= nnz_);

    if(alpha == 1) {
      for(int i = h_ptr_[row]; i < h_ptr_[row + 1]; i++)
      {
        sum += std::abs(h_val_[i]);
      }
    }
    else {
      for(int i = h_ptr_[row]; i < h_ptr_[row + 1]; i++)
      {
        sum += std::pow(std::abs(h_val_[i]), alpha);
      }
    }
    
    return sum;
  }

  real col_sum(int col, real alpha) const {
    real sum = 0;

    assert(h_ptr_t_[col] >= 0);
    assert(h_ptr_t_[col] <= nnz_);
    assert(h_ptr_t_[col + 1] >= 0);
    assert(h_ptr_t_[col + 1] <= nnz_);

    for(int i = h_ptr_t_[col]; i < h_ptr_t_[col + 1]; i++) {
      sum += std::pow(std::abs(h_val_t_[i]), alpha);
    }
    
    return sum;
  }

  int gpu_mem_amount() const {
    int total_bytes = 0;

    total_bytes += 2 * nnz_ * sizeof(int);
    total_bytes += (m_ + n_ + 2) * sizeof(int);
    total_bytes += 2 * nnz_ * sizeof(real);

    return total_bytes;
  }

protected:
  void FillTranspose() const;
  
  int m_; // number of rows
  int n_; // number of cols
  int nnz_; // number of non zero elements

  cusparseHandle_t cusp_handle_;
  cusparseMatDescr_t descr_;

  int32_t *d_ind_, *d_ind_t_;
  int32_t *d_ptr_, *d_ptr_t_;
  real *d_val_, *d_val_t_;

  int32_t *h_ind_, *h_ind_t_;
  int32_t *h_ptr_, *h_ptr_t_;
  real *h_val_, *h_val_t_;
};

template<>
inline void SparseMatrix<float>::FillTranspose() const {
  cusparseStatus_t stat = cusparseScsr2csc(cusp_handle_, n_, m_, nnz_,
                   d_val_t_, d_ptr_t_, d_ind_t_,
                   d_val_, d_ind_, d_ptr_,
                   CUSPARSE_ACTION_NUMERIC,
                   CUSPARSE_INDEX_BASE_ZERO);

  assert(stat == CUSPARSE_STATUS_SUCCESS);
}

template<>
inline void SparseMatrix<double>::FillTranspose() const {
  cusparseStatus_t stat = cusparseDcsr2csc(cusp_handle_, n_, m_, nnz_,
                   d_val_t_, d_ptr_t_, d_ind_t_,
                   d_val_, d_ind_, d_ptr_,
                   CUSPARSE_ACTION_NUMERIC,
                   CUSPARSE_INDEX_BASE_ZERO);

  assert(stat == CUSPARSE_STATUS_SUCCESS);
}

template<>
inline bool SparseMatrix<float>::MultVec(
    float *d_x,
    float *d_y,
    bool trans,
    float alpha,
    float beta) const
{
  cusparseStatus_t stat;
  
  if(trans)
    stat = cusparseScsrmv(cusp_handle_,
                          CUSPARSE_OPERATION_NON_TRANSPOSE,
                          n_,
                          m_,
                          nnz_,
                          &alpha,
                          descr_,
                          d_val_t_,
                          d_ptr_t_,
                          d_ind_t_,
                          d_x,
                          &beta,
                          d_y);
  else
    stat = cusparseScsrmv(cusp_handle_,
                          CUSPARSE_OPERATION_NON_TRANSPOSE,
                          m_,
                          n_,
                          nnz_,
                          &alpha,
                          descr_,
                          d_val_,
                          d_ptr_,
                          d_ind_,
                          d_x,
                          &beta,
                          d_y);

  return (stat == CUSPARSE_STATUS_SUCCESS);
}

template<>
inline bool SparseMatrix<double>::MultVec(
    double *d_x,
    double *d_y,
    bool trans,
    double alpha,
    double beta) const
{
  cusparseStatus_t stat;
  
  if(trans)
    stat = cusparseDcsrmv(cusp_handle_,
                          CUSPARSE_OPERATION_NON_TRANSPOSE,
                          n_,
                          m_,
                          nnz_,
                          &alpha,
                          descr_,
                          d_val_t_,
                          d_ptr_t_,
                          d_ind_t_,
                          d_x,
                          &beta,
                          d_y);
  else
    stat = cusparseDcsrmv(cusp_handle_,
                          CUSPARSE_OPERATION_NON_TRANSPOSE,
                          m_,
                          n_,
                          nnz_,
                          &alpha,
                          descr_,
                          d_val_,
                          d_ptr_,
                          d_ind_,
                          d_x,
                          &beta,
                          d_y);

  return (stat == CUSPARSE_STATUS_SUCCESS);
}

// TODO: understand how this works. power iteration?
template<typename real>
real MatrixNormest(const SparseMatrix<real>& A, real tol = 1e-6, int max_iter = 100)
{
  cublasHandle_t handle;
  cublasCreate(&handle);

  int n = A.ncols();
  int m = A.nrows();
  
  real *x, *Ax, *h_x;
  cudaMalloc((void **)&x, sizeof(real) * n);
  cudaMalloc((void **)&Ax, sizeof(real) * m);

  h_x = new real[n];
  for(int i = 0; i < n; ++i)
    h_x[i] = (real) (rand()) / (real)(RAND_MAX);
  cudaMemcpy(x, h_x, sizeof(real) * n, cudaMemcpyHostToDevice);
  
  real norm = 0, norm_prev;

  for(int i = 0; i < max_iter; i++)
  {
    norm_prev = norm;
    
    A.MultVec(x, Ax, false, 1, 0);
    A.MultVec(Ax, x, true, 1, 0); 
    
    real nx = cuwrap::nrm2<real>(handle, x, n);
    real nAx = cuwrap::nrm2<real>(handle, Ax, m);
    cuwrap::scal<real>(handle, x, real(1) / nx, n);
    norm = nx / nAx;

    if(std::abs(norm_prev - norm) < tol * norm)
      break;
  }

  delete [] h_x;
  
  cudaFree(x);
  cudaFree(Ax);
  cublasDestroy(handle);

  return norm;
}

#endif
