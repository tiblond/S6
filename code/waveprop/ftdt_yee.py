# GPL3, Copyright (c) Max Hofheinz, UdeS, 2021

import fiddle
import numpy as np
from subprocess import Popen, PIPE
import mmap
import time

def subp():
    subproc = Popen(["./JasonSegel", FNAME], stdin=PIPE, stdout=PIPE)
    return subproc


def signal_and_wait(subproc):
    subproc.stdin.write("START\n".encode())
    subproc.stdin.flush()
    # Nécessaire pour vider le tampon de sortie
    res = subproc.stdout.readline()
    print(res)
    
# Mr.World wide    
FNAME       = "GIF642-problematique-sharedMemory"
shm_f = open(FNAME, "r+b")
shm_mm = mmap.mmap(shm_f.fileno(), 0)



def curl_E(E):
	subproc = subp()
	signal_and_wait(subproc)

	shared_matrix = np.ndarray(shape=E.shape,dtype=np.float64, buffer=shm_mm)
	shared_matrix[:] = E
	signal_and_wait(subproc)
	subproc.kill()
	return shared_matrix
	
def curl_H(H):
    curl_H = np.zeros(H.shape)

    curl_H[:,1:,:,0] += H[:,1:,:,2] - H[:,:-1,:,2]
    curl_H[:,:,1:,0] -= H[:,:,1:,1] - H[:,:,:-1,1]

    curl_H[:,:,1:,1] += H[:,:,1:,0] - H[:,:,:-1,0]
    curl_H[1:,:,:,1] -= H[1:,:,:,2] - H[:-1,:,:,2]

    curl_H[1:,:,:,2] += H[1:,:,:,1] - H[:-1,:,:,1]
    curl_H[:,1:,:,2] -= H[:,1:,:,0] - H[:,:-1,:,0]
    
    return curl_H


def timestep(E, H, courant_number, source_pos, source_val):
    E += courant_number * curl_H(H)
    E[source_pos] += source_val
    H -= courant_number * curl_E(E)
    return E, H


class WaveEquation:
    def __init__(self, s, courant_number, source):
        s = s + (3,)
        self.E = np.zeros(s)
        self.H = np.zeros(s)
        self.courant_number = courant_number
        self.source = source
        self.index = 0

    def __call__(self, figure, field_component, slice, slice_index, initial=False):
        if field_component < 3:
            field = self.E
        else:
            field = self.H
            field_component = field_component % 3
        if slice == 0:
            field = field[slice_index, :, :, field_component]
        elif slice == 1:
            field = field[:, slice_index, :, field_component]
        elif slice == 2:
            field = field[:, :, slice_index, field_component]
        source_pos, source_index = source(self.index)
        self.E, self.H = timestep(self.E, self.H, self.courant_number, source_pos, source_index)

        if initial:
            axes = figure.add_subplot(111)
            self.image = axes.imshow(field, vmin=-1e-2, vmax=1e-2)
        else:
            self.image.set_data(field)
        self.index += 1


if __name__ == "__main__":
    n = 100
    r = 0.01
    l = 30


    def source(index):
        return ([n // 3], [n // 3], [n // 2],[0]), 0.1*np.sin(0.1 * index)

    w = WaveEquation((n, n, n), 0.1, source)
    fiddle.fiddle(w, [('field component',{'Ex':0,'Ey':1,'Ez':2, 'Hx':3,'Hy':4,'Hz':5}),('slice',{'XY':2,'YZ':0,'XZ':1}),('slice index',0,n-1,n//2,1)], update_interval=0.01)
    shm_mm.close()

