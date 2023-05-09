// BERA1107 & LERS0601
// Calcul en parallele

#include <iostream>
#include <memory>
#include <vector>
#include <queue>
#include <unordered_map>
#include <string>
#include <cstring>
#include <thread>
#include <cmath>

#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

typedef struct {             // Structure declaration
  double x_value = 0;         // First Array
  double y_value = 0;         // Second Array
  double z_value = 0;         // Third Array
} e_vector;       // Structure variable 

static const int MATRIX_SIZE = 100;
typedef std::vector<std::vector<std::vector<e_vector>>> fourD; // 4th dimension 
fourD E_object(MATRIX_SIZE,std::vector<std::vector<e_vector>>(MATRIX_SIZE,std::vector<e_vector>(MATRIX_SIZE))); 

// Taille de la matrice de travail (un côté)

static const int BUFFER_SIZE = MATRIX_SIZE * MATRIX_SIZE * MATRIX_SIZE * sizeof(e_vector);
// Tampon générique à utiliser pour créer le fichier
char buffer_[BUFFER_SIZE];



void wait_signal()
{
    // Attend une entrée (ligne complète avec \n) sur stdin.
    std::string msg;
    std::cin >> msg;
    std::cerr << "CPP:I got you big boy PY " << std::endl;
}
void ack_signal()
{
    // Répond avec un message vide.
    std::cout << "CPP: *Avec la voix du dude dans heartStone* : Travail termine" << std::endl;
}



fourD math_conc(fourD E_param)
{   
    fourD curl_E(MATRIX_SIZE,std::vector<std::vector<e_vector>>(MATRIX_SIZE,std::vector<e_vector>(MATRIX_SIZE)));
    //Second Line Python
    for (int i = 0; i < MATRIX_SIZE; i++) {
        for (int j = 0; j < MATRIX_SIZE - 1; j++) {
            for (int k = 0; k < MATRIX_SIZE; k++) {
                curl_E[i][j][k].x_value += E_param[i][j+1][k].z_value - E_param[i][j][k].z_value;
                
            }
        }
    }
    //Third Line Python
    for (int i = 0; i < MATRIX_SIZE; i++) {
        for (int j = 0; j < MATRIX_SIZE; j++) {
            for (int k = 0; k < MATRIX_SIZE - 1; k++) {
                curl_E[i][j][k].x_value -= E_param[i][j][k+1].y_value - E_param[i][j][k].y_value;
            }
        }
    }
    //Fourth Line Python
    for (int i = 0; i < MATRIX_SIZE; i++) {
        for (int j = 0; j < MATRIX_SIZE; j++) {
            for (int k = 0; k < MATRIX_SIZE - 1; k++) {
                curl_E[i][j][k].y_value += E_param[i][j][k+1].x_value - E_param[i][j][k].x_value;
            }
        }
    }
    //Fifth Line Python
    for (int i = 0; i < MATRIX_SIZE - 1; i++) {
        for (int j = 0; j < MATRIX_SIZE; j++) {
            for (int k = 0; k < MATRIX_SIZE; k++) {
                curl_E[i][j][k].y_value -= E_param[i+1][j][k].z_value - E_param[i][j][k].z_value;
            }
        }
    }
    //Sixth Line Python
    for (int i = 0; i < MATRIX_SIZE - 1; i++) {
        for (int j = 0; j < MATRIX_SIZE; j++) {
            for (int k = 0; k < MATRIX_SIZE; k++) {
                curl_E[i][j][k].z_value += E_param[i+1][j][k].y_value - E_param[i][j][k].y_value;
            }
        }
    }
    //SeventhLine Python
    for (int i = 0; i < MATRIX_SIZE; i++) {
        for (int j = 0; j < MATRIX_SIZE - 1; j++) {
            for (int k = 0; k < MATRIX_SIZE; k++) {
                curl_E[i][j][k].z_value -= E_param[i][j+1][k].x_value - E_param[i][j][k].x_value;
            }
        }
    }
   return curl_E; 
}
int main(int argc, char** argv)
    {
        wait_signal();
        memset(buffer_, 0, BUFFER_SIZE);
        FILE* shm_f = fopen("GIF642-problematique-sharedMemory", "w");
        fwrite(buffer_, sizeof(char), BUFFER_SIZE, shm_f);       
        fclose(shm_f);

        // On signale que le fichier est prêt.
        std::cerr << "CPP:  Check moi le bo fichier touer" << std::endl;
        ack_signal();
        
        // Pointeur format double qui représente la matrice partagée:
        int shm_fd = open("GIF642-problematique-sharedMemory", O_RDWR);
        void* shm_mmap = mmap(NULL, BUFFER_SIZE, PROT_WRITE | PROT_READ, MAP_SHARED, shm_fd, 0);
        close(shm_fd);
        double* mtx = (double*)shm_mmap;
        fourD mtxPlusMC(MATRIX_SIZE,std::vector<std::vector<e_vector>>(MATRIX_SIZE,std::vector<e_vector>(MATRIX_SIZE)));
        
        while(true) {
            wait_signal();
            
            for (int i = 0; i < MATRIX_SIZE; i++) {
                for (int j = 0; j < MATRIX_SIZE; j++) {
                    for (int k = 0; k < MATRIX_SIZE; k++) {
                        mtxPlusMC[i][j][k].x_value = mtx[i*MATRIX_SIZE*MATRIX_SIZE*3 + j*MATRIX_SIZE*3 + k*3+0];
                        mtxPlusMC[i][j][k].y_value = mtx[i*MATRIX_SIZE*MATRIX_SIZE*3 + j*MATRIX_SIZE*3 + k*3+1];
                        mtxPlusMC[i][j][k].z_value = mtx[i*MATRIX_SIZE*MATRIX_SIZE*3 + j*MATRIX_SIZE*3 + k*3+2];
                    }
                }
            }
            mtxPlusMC = math_conc(mtxPlusMC);
        
            for (int i = 0; i < MATRIX_SIZE; i++) {
                for (int j = 0; j < MATRIX_SIZE; j++) {
                    for (int k = 0; k < MATRIX_SIZE; k++) {
                        mtx[i*MATRIX_SIZE*MATRIX_SIZE*3 + j*MATRIX_SIZE*3 + k*3+0]=mtxPlusMC[i][j][k].x_value;
                        mtx[i*MATRIX_SIZE*MATRIX_SIZE*3 + j*MATRIX_SIZE*3 + k*3+1]=mtxPlusMC[i][j][k].y_value;
                        mtx[i*MATRIX_SIZE*MATRIX_SIZE*3 + j*MATRIX_SIZE*3 + k*3+2]=mtxPlusMC[i][j][k].z_value;
                    }
                }
            }
            std::cerr << "CPP:  Un tour de roue dans la machine a math" << std::endl;
            ack_signal();
        }
    munmap(shm_mmap, BUFFER_SIZE);
    return 0;
    }
