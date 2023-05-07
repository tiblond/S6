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

// Taille de la matrice de travail (un côté)
static const int MATRIX_SIZE = 4;
static const int BUFFER_SIZE = MATRIX_SIZE * MATRIX_SIZE * sizeof(double);
// Tampon générique à utiliser pour créer le fichier
char buffer_[BUFFER_SIZE];

int main(int argc, char** argv)
    {
        wait_signal();
        // Do some math
        std::cerr << "La tache";
        ack_signal();
    }

void wait_signal()
{
    // Attend une entrée (ligne complète avec \n) sur stdin.
    std::string msg;
    std::cin >> msg;
    std::cerr << "CPP:I got you big boy PY" << std::endl;
}
void ack_signal()
{
    // Répond avec un message vide.
    std::cout << "CPP: *Avec la voix du dude dans heartStone* : Travail termine" << std::endl;
}

// Fonction pour le curl_E
std::vector<std::vector<std::vector<std::vector<double>>>> curl_E(std::vector<std::vector<std::vector<std::vector<double>>>> E) {
    int n1 = E.size();
    int n2 = E[0].size();
    int n3 = E[0][0].size();
    int n4 = E[0][0][0].size();

    std::vector<std::vector<std::vector<std::vector<double>>>> curl_E(n1, std::vector<std::vector<std::vector<double>>>(n2, std::vector<std::vector<double>>(n3, std::vector<double>(n4, 0.0))));

    for (int i = 0; i < n1; i++) {
        for (int j = 0; j < n2 - 1; j++) {
            for (int k = 0; k < n3; k++) {
                curl_E[i][j][k][0] += E[i][j + 1][k][2] - E[i][j][k][2];
            }
        }
    }

    for (int i = 0; i < n1; i++) {
        for (int j = 0; j < n2; j++) {
            for (int k = 0; k < n3 - 1; k++) {
                curl_E[i][j][k][0] -= E[i][j][k + 1][1] - E[i][j][k][1];
            }
        }
    }

    for (int i = 0; i < n1; i++) {
        for (int j = 0; j < n2; j++) {
            for (int k = 0; k < n3 - 1; k++) {
                curl_E[i][j][k][1] += E[i][j][k + 1][0] - E[i][j][k][0];
            }
        }
    }

    for (int i = 0; i < n1 - 1; i++) {
        for (int j = 0; j < n2; j++) {
            for (int k = 0; k < n3; k++) {
                curl_E[i][j][k][1] -= E[i + 1][j][k][2] - E[i][j][k][2];
            }
        }
    }

    for (int i = 0; i < n1 - 1; i++) {
        for (int j = 0; j < n2; j++) {
            for (int k = 0; k < n3; k++) {
                curl_E[i][j][k][2] += E[i + 1][j][k][1] - E[i][j][k][1];
            }
        }
    }

    for (int i = 0; i < n1; i++) {
        for (int j = 0; j < n2 - 1; j++) {
            for (int k = 0; k < n3; k++) {
                curl_E[i][j][k][2] -= E[i][j + 1][k][0] - E[i][j][k][0];
            }
        }
    }

    return curl_E;
}

// Section de convertion pour Curl H
std::vector<std::vector<std::vector<std::vector<double>>>> 
curl_H(std::vector<std::vector<std::vector<std::vector<double>>>> H) {
    std::vector<std::vector<std::vector<std::vector<double>>>> curl_H(H.size(), std::vector<std::vector<std::vector<double>>>(H[0].size(), std::vector<std::vector<double>>(H[0][0].size(), std::vector<double>(H[0][0][0].size(), 0.0))));

    for (int i = 0; i < H.size(); i++) {
        for (int j = 1; j < H[0][0].size(); j++) {
            for (int k = 0; k < H[0][0][0].size(); k++) {
                curl_H[i][j][0][k] += H[i][j][k][2] - H[i][j-1][k][2];
                curl_H[i][j][k][0] -= H[i][j][k][1] - H[i][j][k-1][1];
            }
        }
    }

    for (int i = 0; i < H.size(); i++) {
        for (int j = 1; j < H[0].size(); j++) {
            for (int k = 0; k < H[0][0][0].size(); k++) {
                curl_H[i][j][k][1] += H[i][j][k][0] - H[i][j-1][k][0];
                curl_H[i][0][k][1] -= H[i][0][k][2] - H[i][j][k][2];
            }
        }
    }

    for (int i = 1; i < H.size(); i++) {
        for (int j = 0; j < H[0][0].size(); j++) {
            for (int k = 0; k < H[0][0][0].size(); k++) {
                curl_H[i][j][k][2] += H[i][j][k][1] - H[i-1][j][k][1];
                curl_H[0][j][k][2] -= H[0][j][k][1] - H[i][j][k][1];
            }
        }
    }

    for (int i = 1; i < H.size(); i++) {
        for (int j = 0; j < H[0].size(); j++) {
            for (int k = 0; k < H[0][0][0].size(); k++) {
                curl_H[i][j][k][0] -= H[i][j][k][2] - H[i-1][j][k][2];
                curl_H[i][j][k][1] += H[i][j][k][0] - H[i][j][k-1][0];
            }
        }
    }

    return curl_H;
}

std::vector<std::vector<std::vector<std::vector<double>>>> curl_E(std::vector<std::vector<std::vector<std::vector<double>>>> E);
std::vector<std::vector<std::vector<std::vector<double>>>> curl_H(std::vector<std::vector<std::vector<std::vector<double>>>> H);

std::vector<std::vector<std::vector<std::vector<double>>>> timestep(std::vector<std::vector<std::vector<std::vector<double>>>>& E,
         std::vector<std::vector<std::vector<std::vector<double>>>>& H, 
         double courant_number, std::vector<int> source_pos, std::vector<double> source_val) {
    
    // Calculate the curl of H and update E
    std::vector<std::vector<std::vector<std::vector<double>>>> curl_H_vec = curl_H(H);
    for (int i = 0; i < E.size(); i++) {
        for (int j = 0; j < E[0].size(); j++) {
            for (int k = 0; k < E[0][0].size(); k++) {
                for (int l = 0; l < 3; l++) {
                    E[i][j][k][l] += courant_number * curl_H_vec[i][j][k][l];
                }
            }
        }
    }
    
    // Add the source term to E
    E[source_pos[0]][source_pos[1]][source_pos[2]][source_pos[3]] += source_val[0];
    
    // Calculate the curl of E and update H
    std::vector<std::vector<std::vector<std::vector<double>>>> curl_E_vec = curl_E(E);
    for (int i = 0; i < H.size(); i++) {
        for (int j = 0; j < H[0].size(); j++) {
            for (int k = 0; k < H[0][0].size(); k++) {
                for (int l = 0; l < 3; l++) {
                    H[i][j][k][l] -= courant_number * curl_E_vec[i][j][k][l];
                }
            }
        }
    }
    
    return {E, H};
}
