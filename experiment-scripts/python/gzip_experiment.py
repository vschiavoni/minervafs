import os
import sys
import subprocess
import csv

import pathlib

def writeCsv(row, file):

    with open(file, 'a') as csvFile:
        writer = csv.writer(csvFile)
        writer.writerow(row)

def du(dir):

    result = subprocess.check_output(['du', '-sb', dir], encoding="utf-8")
    result = (result.split('\t'))[0]
    return result

def delta(input, output):

    dcmd = 'gzip'

    subprocess.call([dcmd, '{!s}'.format(input)])

    input = input + '.gz'
    move(input, output)

def copy(input, output):

    subprocess.call(['cp', input, output])

def move(input, output):

    subprocess.call(['mv', input, output])
        

if __name__ == '__main__':

    inputFile = sys.argv[1]
    outputDest = sys.argv[2]
    outputFile = sys.argv[3]

    if not outputDest.endswith('/'):
        outputDest = outputDest + '/'

    files = []

    with open(inputFile, 'r') as input:
        files = input.readlines()

    writeCsv(['file', 'origin size', 'final size'], outputFile)
    i = 1
    origin_size = 0
    for file in files:
        if file.endswith('\n'):
            file = file[:-1]
        output = '{!s}{!s}_file'.format(outputDest, i)

        copy(file, './')

        tmp = os.path.basename(file)
        delta(tmp, output)
        origin_size += os.path.getsize(file)
        delta_size = 0

        if (i % 10 == 0) or (i == len(files)):
            delta_size = du(outputDest)
        writeCsv([i, origin_size, delta_size], outputFile)
        i = i + 1
        

    
    
    
        
    
        
