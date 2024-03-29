#include <torch/extension.h>
#include <ATen/ATen.h>
#include <iostream>
#include <time.h>
#include <sys/time.h>
#include <vector>
#include <immintrin.h>
#include <cstdio>

// Uncomment for ISPC
//#include "module_ispc.h"
//using namespace ispc;

// ------------------------------------ //
// 	WARM-UP: ACCESSING TENSORS      //
// ------------------------------------ //

// Step #1: Understand Read/Write Accessors for a 2D Tensor
inline float twoDimRead(std::vector<float> &tensor, int &x, int &y, const int &sizeX) {
    // Note that sizeX is the size of a Row, not the number of rows
    return tensor[x * (sizeX)+ y];
}

inline void twoDimWrite(std::vector<float> &tensor, int &x, int &y, const int &sizeX, float &val) {
    tensor[x * (sizeX) + y] = val;
}

// Step #2: Implement Read/Write Accessors for a 4D Tensor
inline float fourDimRead(std::vector<float> &tensor, int &x, int &y, int &z, int &b, 
        const int &sizeX, const int &sizeY, const int &sizeZ) {
    return tensor[x * (sizeX * sizeY * sizeZ) + y * (sizeY * sizeZ) + z * sizeZ + b];
}

inline void fourDimWrite(std::vector<float> &tensor, int &x, int &y, int &z, int &b, 
        const int &sizeX, const int &sizeY, const int &sizeZ, float &val) {
    tensor[x * (sizeX * sizeY * sizeZ) + y * (sizeY * sizeZ) + z * sizeZ + b] = val;
}

// DO NOT EDIT THIS FUNCTION //
std::vector<float> formatTensor(torch::Tensor tensor) {
    tensor = tensor.flatten();
    tensor = tensor.contiguous();
    std::vector<float> vec(tensor.data_ptr<float>(), tensor.data_ptr<float>() + tensor.numel());
    return vec;
}

/* Programming Your Attention Modules.
 * 
 * You are given Q, K, and V Tensors as inputs that are formatted as vectors. We have also created O and QK^t Tensors 
 * that are formatted as vectors. After you have implemented your accessors in the Warm-Up you should be able to
 * read/write to these tensors via the read/write functions above.
 *
 * You are also given 4 integers as parameters: B, H, N, d:
 *
 * B (Batch Size) - The number of samples for your attention layer. Think of it this way - if I asked my dnn
 * a question and it output 5 different answers it had a batch size of 5. These samples are independent of each
 * other and thus can be parallelized.
 *
 * H (Number of Heads) - Each head runs on its own set of Q, K, V matrices. This effectively allows each head
 * to operate the same attention algorithm, but each with each head using different hyperparameters. These
 * allow each head to have their own definition of what relevance is when looking at a token. These heads
 * can operate independently of one another and thus can be parallized.
 *
 * N (Sequence Length) - The number of tokens. You may think of this as the number of words in a sample.
 *
 * d (Embedding Dimensionality) - The number of features each token encodes per attention head. Let's
 * say I encoded a word using the follow (length, number of vowels, has a capital letters). The
 * emvedded dimensionaliy would be 3.
 * */

// ---------------------------------------------------------- //
//                  PART 1: NAIVE ATTENTION                   //
// ---------------------------------------------------------- //

torch::Tensor myNaiveAttention(torch::Tensor QTensor, torch::Tensor KTensor, torch::Tensor VTensor, torch::Tensor QK_tTensor,
                int B, int H, int N, int d){

    // Q, K, V are passed in with Shape: (B, H, N, d)
    //QK^t Intermediate Tensor has Shape (N, N)
    
    //Make O Tensor with Shape (B, H, N, d) 
    at::Tensor OTensor = at::zeros({B, H, N, d}, at::kFloat);

    //Format O, Q, K, and V tensors into 4D vectors
    std::vector<float> O = formatTensor(OTensor);
    std::vector<float> Q = formatTensor(QTensor);
    std::vector<float> K = formatTensor(KTensor);
    std::vector<float> V = formatTensor(VTensor);

    //Format QK_t Tensor into a 2D vector.
    std::vector<float> QK_t = formatTensor(QK_tTensor);

    /* Here is an example of how to read/write 0's to  Q (B, H, N, d) using the 4D accessors

        //loop over Batch Size
         for (int b = 0; b < B; b++) {

             //loop over Heads
             for (int h = 0; h < H; h++) {

                 //loop over Sequence Length
                 for (int i = 0; i < N; i++) {

                     //loop over Embedding Dimensionality
                     for (int j = 0; j < d; j++) {
                        float val = fourDimRead(Q, b, h, i, j, H, N, d);
                        val = 0.0;
                        fourDimWrite(Q, b, h, i, j, H, N, d, val);
                     }
                 }
             }
         }
    */

    /* Here is an example of how to read/write 0's to  QK_t (N, N) using the 2D accessors

           for (int i = 0; i < N; i++) {
	       for (int j = 0; j < N; j++) {
	           float val = twoDimRead(QK_t, i, j, N);
               val = 0.0;
	           twoDimWrite(QK_t, i, j, N, val);
             }
         }
    */
    
    // -------- YOUR CODE HERE  -------- //

    for (int b = 0; b < B; b++) {
        for (int h = 0; h < H; h++) {
            // 1. calculate QK^T
            for (int i = 0; i < N; i++) {
                for (int j = 0; j < N; j++) {
                    float sum = 0.0f;
                    for (int k = 0 ; k < d; k++) {
                        float q_ik = fourDimRead(Q, b, h, i, k, H, N, d);
                        float k_jk = fourDimRead(K, b, h, j, k, H, N, d);
                        sum += q_ik * k_jk;
                    }
                    twoDimWrite(QK_t, i, j, N, sum);
                }
            }
            // 2. apply softmax to each row of QK^T
            for (int i = 0 ; i < N; i++)  {
                float sum = 0;
                for (int j = 0 ; j < N ; j++) {
                    float QKt_ij = twoDimRead(QK_t, i, j, N);
                    QKt_ij = std::exp(QKt_ij);
                    twoDimWrite(QK_t, i, j, N, QKt_ij);
                    sum += QKt_ij;
                }
                for (int j = 0 ; j < N ; j++) {
                    float QKt_ij = twoDimRead(QK_t, i, j, N);
                    QKt_ij /= sum;
                    twoDimWrite(QK_t, i, j, N, QKt_ij);
                }
            }
            // 3. multiply QK^T(N x N) with V(N x d)
            for (int i = 0; i < N; i++) {
                for (int j = 0; j < d; j++) {
                    float sum = 0.0f;
                    for (int k = 0 ; k < N; k++) {
                        float qkt_ik = twoDimRead(QK_t, i, k, N); ;
                        float v_kj = fourDimRead(V, b, h, k, j, H, N, d);
                        sum += qkt_ik * v_kj;
                    }
                    fourDimWrite(O, b, h, i, j, H, N, d, sum);
                }
            }
        }
    }

    // DO NOT EDIT THIS RETURN STATEMENT //
    // It formats your C++ Vector O back into a Tensor of Shape (B, H, N, d) and returns it //
    return torch::from_blob(O.data(), {B, H, N, d}, torch::TensorOptions().dtype(torch::kFloat32)).clone();
}


// ---------------------------------------------------------- //
//     PART 2: BLOCKED MATRIX MULTIPLY AND UNFUSED SOFTMAX    //
// ---------------------------------------------------------- //
size_t cache_line_size(){
    std::FILE *p;
    p = fopen("/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size", "r");
    unsigned int i = 0;
    if (p) {
        fscanf(p, "%d", &i);
        fclose(p);
    }
    return i;
}

torch::Tensor myUnfusedAttentionBlocked(torch::Tensor QTensor, torch::Tensor KTensor, torch::Tensor VTensor, torch::Tensor QK_tTensor,
                int B, int H, int N, int d){
    
    // Q, K, V are passed in with Shape: (B, H, N, d)
    //QK^t Intermediate Tensor has Shape (N, N)

    //Make O Tensor with Shape (B, H, N, d) 
    at::Tensor OTensor = at::zeros({B, H, N, d}, at::kFloat);

    //Format O, Q, K, and V tensors into 4D vectors
    std::vector<float> O = formatTensor(OTensor);
    std::vector<float> Q = formatTensor(QTensor);
    std::vector<float> K = formatTensor(KTensor);
    std::vector<float> V = formatTensor(VTensor);

    //Format QK_t Tensor into a 2D vector.
    std::vector<float> QK_t = formatTensor(QK_tTensor);

    // -------- YOUR CODE HERE  -------- //
    size_t cacheLineSize = cache_line_size();
    int blockSize = cacheLineSize / sizeof(float);
    for (int b = 0; b < B; b++) {
        for (int h = 0; h < H; h++) {
            std::fill(QK_t.begin(), QK_t.end(), 0.0f);
            // 1. calculate QK^T
            for (int i = 0; i < (N + blockSize - 1)/ blockSize; i++) {
                for (int j = 0; j < (N + blockSize - 1) / blockSize; j++) {
                    for (int k = 0; k < (d + blockSize - 1) / blockSize; k++) {
                        for (int ii = i * blockSize; ii < std::min((i + 1) * blockSize, N); ii++) {
                            for (int jj = j * blockSize; jj < std::min((j + 1) * blockSize, N); jj++) {
                                float sum = twoDimRead(QK_t, ii, jj, N);
                                for (int kk = k * blockSize; kk < std::min((k + 1) * blockSize, d); kk++) {
                                    float q_ik = fourDimRead(Q, b, h, ii, kk, H, N, d);
                                    float k_jk = fourDimRead(K, b, h, jj, kk, H, N, d);
                                    sum += q_ik * k_jk;
                                }
                                twoDimWrite(QK_t, ii, jj, N, sum);
                            }
                        }
                    }
                }
            }
            // 2. apply softmax to each row of QK^T
            for (int i = 0 ; i < N; i++)  {
                float sum = 0;
                for (int j = 0 ; j < N ; j++) {
                    float QKt_ij = twoDimRead(QK_t, i, j, N);
                    QKt_ij = std::exp(QKt_ij);
                    twoDimWrite(QK_t, i, j, N, QKt_ij);
                    sum += QKt_ij;
                }
                for (int j = 0 ; j < N ; j++) {
                    float QKt_ij = twoDimRead(QK_t, i, j, N);
                    QKt_ij /= sum;
                    twoDimWrite(QK_t, i, j, N, QKt_ij);
                }
            }
            // 3. multiply QK^T(N x N) with V(N x d)
            for (int i = 0; i < (N + blockSize - 1)/ blockSize; i++) {
                for (int j = 0; j < (d + blockSize - 1) / blockSize; j++) {
                    for (int k = 0; k < (N + blockSize - 1) / blockSize; k++) {
                        for (int ii = i * blockSize; ii < std::min((i + 1) * blockSize, N); ii++) {
                            for (int jj = j * blockSize; jj < std::min((j + 1) * blockSize, d); jj++) {
                                if (k == 0){
                                    float zero = 0;
                                    fourDimWrite(O, b, h, ii, jj, H, N, d, zero);
                                }
                                float sum = fourDimRead(O, b, h, ii, jj, H, N, d);
                                for (int kk = k * blockSize; kk < std::min((k + 1) * blockSize, N); kk++) {
                                    float qkt_ik = twoDimRead(QK_t, ii, kk, N); 
                                    float v_kj = fourDimRead(V, b, h, kk, jj, H, N, d);
                                    sum += qkt_ik * v_kj;
                                }
                                fourDimWrite(O, b, h, ii, jj, H, N, d, sum);
                            }
                        }
                    }
                }
            }
        }
    }
    
    // DO NOT EDIT THIS RETURN STATEMENT //
    // It formats your C++ Vector O back into a Tensor of Shape (B, H, N, d) and returns it //
    return torch::from_blob(O.data(), {B, H, N, d}, torch::TensorOptions().dtype(torch::kFloat32)).clone();
}


// ---------------------------------------------------------- //
//                 PART 3: FUSED ATTENTION     	              //
// ---------------------------------------------------------- //

torch::Tensor myFusedAttention(torch::Tensor QTensor, torch::Tensor KTensor, torch::Tensor VTensor, torch::Tensor temp,
                int B, int H, int N, int d){

    // Q, K, V are passed in with Shape: (B, H, N, d)

    //Make O Tensor with Shape (B, H, N, d)
    //and O Row Tensor with Shape (N)
    at::Tensor OTensor = at::zeros({B, H, N, d}, at::kFloat);
    at::Tensor ORowTensor = at::zeros({N}, at::kFloat);

    //Format Y, Q, K, and V tensors into 4D vectors
    std::vector<float> O = formatTensor(OTensor);
    std::vector<float> Q = formatTensor(QTensor);
    std::vector<float> K = formatTensor(KTensor);
    std::vector<float> V = formatTensor(VTensor);
    
    //Format ORow Tensor into a 1D vector
    // You can simply access this as ORow[i]
    std::vector<float> ORow = formatTensor(ORowTensor);


    // -------- YOUR CODE HERE  -------- //
    // We give you a template of the first three loops for your convenience
    // loop over batch
    #pragma omp parallel for collapse(3)
    for (int b = 0; b < B; b++){

        //loop over heads
        for (int h = 0; h < H; h++){
            for (int i = 0; i < N ; i++){

        		// YRow is moved inside so each OpenMP thread gets a local copy.
                at::Tensor ORowTensor = temp.index({torch::indexing::Slice(omp_get_thread_num(), torch::indexing::None)});      
                std::vector<float> ORow = formatTensor(ORowTensor);
        		// YOUR CODE HERE
                // 1. calculate Q[i]K^T 
                for (int j = 0 ; j < N; j++) {
                    float sum = 0.0f;
                    for (int k = 0; k < d; k++) {
                        float q_ik = fourDimRead(Q, b, h, i, k, H, N, d);
                        float k_jk = fourDimRead(K, b, h, j, k, H, N, d);
                        sum += q_ik * k_jk;
                    }
                    ORow[j] = sum;
                }
                // 2. apply softmax to ORow
                float sum = 0.0f;
                for (int j = 0; j < N; j++) {
                    ORow[j] = std::exp(ORow[j]);
                    sum += ORow[j];
                }
                for (int j = 0; j < N; j++) {
                    ORow[j] = ORow[j] / sum;
                }
                // 3. multiply ORow x V(N x d)
                for (int j = 0; j < d; j++) {
                    float sum = 0.0f;
                    for (int k = 0; k < N; k++) {
                        sum += ORow[k] * fourDimRead(V, b, h, k, j, H, N, d);
                    }
                    fourDimWrite(O, b, h, i, j, H, N, d, sum);
                }

            }
	}
    }
	    
	
    // DO NOT EDIT THIS RETURN STATEMENT //
    // It formats your C++ Vector O back into a Tensor of Shape (B, H, N, d) and returns it //
    return torch::from_blob(O.data(), {B, H, N, d}, torch::TensorOptions().dtype(torch::kFloat32)).clone();
}


// ---------------------------------------------------------- //
//                PART 4: FLASH ATTENTION 		      //
// ---------------------------------------------------------- //

torch::Tensor myFlashAttention(torch::Tensor QTensor, torch::Tensor KTensor, torch::Tensor VTensor,
               torch::Tensor QiTensor, torch::Tensor KjTensor, torch::Tensor VjTensor,
               torch::Tensor SijTensor, torch::Tensor PijTensor, torch::Tensor PVTensor,
               torch::Tensor OiTensor, torch::Tensor LTensor,  torch::Tensor LiTensor, 
	       torch::Tensor LijTensor, torch::Tensor LnewTensor, int Bc, int Br,
                int B, int H, int N, int d) {
        
    // Q, K, V are passed in with Shape: (B, H, N, d)
    // Sij, Pij are passed in with Shape: (Br, Bc)
    // Kj, Vj are passed in with Shape: (Bc, d)
    // Qi, Oi, and PV  are passed in with Shape: (Br, d)
    // L in passed in with Shape: (N)
    // Li, Lij, and Lnew are passed in with shape (Br)

    //Make O Tensor with Shape (B, H, N, d)
    at::Tensor OTensor = at::zeros({B, H, N, d}, at::kFloat);
   
    //Format All Tensors into Vectors
    std::vector<float> O = formatTensor(OTensor);
    std::vector<float> Q = formatTensor(QTensor);
    std::vector<float> K = formatTensor(KTensor);
    std::vector<float> V = formatTensor(VTensor);
    std::vector<float> Sij = formatTensor(SijTensor);
    std::vector<float> Pij = formatTensor(PijTensor);
    std::vector<float> Kj = formatTensor(KjTensor);
    std::vector<float> Vj = formatTensor(VjTensor);
    std::vector<float> Qi = formatTensor(QiTensor);
    std::vector<float> Oi = formatTensor(OiTensor);
    std::vector<float> l = formatTensor(LTensor);
    std::vector<float> PV = formatTensor(PVTensor);
    std::vector<float> li = formatTensor(LiTensor);
    std::vector<float> lij = formatTensor(LijTensor);
    std::vector<float> lnew = formatTensor(LnewTensor);
    // -------- YOUR CODE HERE  -------- //
    int Tr = (N + Br - 1) / Br;
    int Tc = (N + Bc - 1) / Bc;
    for (int b = 0 ; b < B; b++) {
        for (int h = 0 ; h < H; h++) {
            std::fill(l.begin(), l.end(), 0.0f); // This is quite important!!!
            for (int j = 0 ; j < Tc; j++) {
                // Load K_j, V_j into local memory blocks
                for (int ii = 0 ; ii < std::min(Bc, N - j * Bc); ii++) {
                    for (int jj = 0; jj < d; jj++) {
                        int ii_abs = j * Bc + ii;
                        float kiijj_val = fourDimRead(K, b, h, ii_abs, jj, H, N, d);
                        float viijj_val = fourDimRead(V, b, h, ii_abs, jj, H, N, d);
                        twoDimWrite(Kj, ii, jj, d, kiijj_val);
                        twoDimWrite(Vj, ii, jj, d, viijj_val);
                    }
                }
                for (int i = 0 ; i < Tr ; i++) {
                    // Load Qi, Oi, li into local memory blocks
                    for (int ii = 0 ; ii < std::min(Br, N - i * Br); ii++) {
                        int ii_abs = i * Br +ii;
                        for (int jj = 0; jj < d; jj++) {
                            float qiijj_val = fourDimRead(Q, b, h, ii_abs, jj, H, N, d);
                            float oiijj_val = fourDimRead(O, b, h, ii_abs, jj, H, N, d);
                            twoDimWrite(Qi, ii, jj, d, qiijj_val);
                            twoDimWrite(Oi, ii, jj, d, oiijj_val);
                        }
                        li[ii] = l[ii_abs];
                    }
                    // Compute Sij=QiKj^T of size(Br x Bc) via matrix multiply
                    for (int r = 0 ; r < std::min(Br, N - i * Br); r++) {
                        for (int c = 0 ; c < std::min(Bc, N - j * Bc); c++) {
                            float sum = 0;
                            for (int k = 0 ; k < d; k++){
                                float q_rk = twoDimRead(Qi, r, k, d);
                                float k_ck = twoDimRead(Kj, c, k, d);
                                sum += q_rk * k_ck;
                            }
                            twoDimWrite(Sij, r, c, Bc, sum);
                        }
                    }
                    // Pij <- exp(Sij) of size (Br x Bc)
                    for (int r = 0 ; r < std::min(Br, N - i * Br); r++) {
                        for (int c = 0 ; c < std::min(Bc, N - j * Bc); c++) {
                            float val = twoDimRead(Sij, r, c, Bc);
                            val = std::exp(val);
                            twoDimWrite(Pij, r, c, Bc, val);
                        }
                    }
                    // lij <- rowsum(Pij) of size Br
                    for (int r = 0 ; r < std::min(Br, N - i * Br); r++) {
                        float sum = 0;
                        for (int c = 0 ; c < std::min(Bc, N - j * Bc); c++) {
                            float val = twoDimRead(Pij, r, c, Bc);
                            sum += val;
                        }
                        lij[r] = sum;
                    }
                    // lnew <- li + lij
                    for (int r = 0 ; r < std::min(Br, N - i * Br); r++) {
                        lnew[r] = li[r] + lij[r];
                    }


                    // calculate PV
                    for (int r = 0; r < std::min(Br, N - i * Br); r++) {
                        for (int c = 0 ; c < d; c++) {
                            float val = 0;
                            for (int k = 0 ; k < std::min(Bc, N - j * Bc); k++) {
                                val += twoDimRead(Pij, r, k, Bc) * twoDimRead(Vj, k, c, d);
                            }
                            twoDimWrite(PV, r, c, d, val);
                        }
                    }
                    // Oi <- (liOi + PijVj)/lnew
                    for (int r = 0 ; r < std::min(Br, N - i * Br); r++) {
                        for (int c = 0 ; c < d; c++) {
                            float pv_val = twoDimRead(PV, r, c, d);
                            float o_val = twoDimRead(Oi, r, c, d);
                            o_val = (li[r] * o_val +pv_val) / lnew[r];
                            twoDimWrite(Oi, r, c, d, o_val);
                        }
                    }
                    // Write blocks Oi and lnew back to O and l in main memory
                    for (int ii = 0 ; ii < std::min(Br, N - i * Br); ii++) {
                        int ii_abs = i * Br + ii;
                        for (int jj = 0; jj < d; jj++) {
                            float oiijj_val = twoDimRead(Oi, ii, jj, d);
                            fourDimWrite(O, b, h, ii_abs, jj, H, N, d, oiijj_val);
                        }
                        l[ii_abs] = lnew[ii];
                    }
                }
            }
        }
    }

    // DO NOT EDIT THIS RETURN STATEMENT //
    // It formats your C++ Vector O back into a Tensor of Shape (B, H, N, d) and returns it //
    return torch::from_blob(O.data(), {B, H, N, d}, torch::TensorOptions().dtype(torch::kFloat32)).clone();
}


/* DO NOT EDIT THESE BINDINGS */
PYBIND11_MODULE(TORCH_EXTENSION_NAME, m) {
  m.def("myNaiveAttention", &myNaiveAttention, "Naive Attention");
  m.def("myUnfusedAttentionBlocked", &myUnfusedAttentionBlocked, " Blocked Unfused Attention");
  m.def("myFusedAttention", &myFusedAttention, "Fused Attention");
  m.def("myFlashAttention", &myFlashAttention, "Flash Attention");
  m.def("twoDimRead", &twoDimRead, "twoDimRead");
  m.def("fourDimRead", &fourDimRead, "fourDimRead");
}
