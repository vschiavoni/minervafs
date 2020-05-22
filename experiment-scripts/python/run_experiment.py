import sys
import os
import subprocess

import csv

def du(dir):

    result = subprocess.check_output(['du', '-sb', dir])
    result = (result.split('\t'))[0]
    return int(result)

def copy(src, dest):

    subprocess.call(['cp', src, dest])

def writeCsv(row, file):

    with open(file, 'a') as csvFile:
        writer = csv.writer(csvFile)
        writer.writerow(row)



if __name__ == '__main__':

    fileList = sys.argv[1]
    outputDir = sys.argv[2]
    minervaDir = sys.argv[3]
    resultFile = sys.argv[4]
    

    files = []

    with open(fileList) as filesList:
        files = filesList.readlines()

    writeCsv(['file', 'origin size', 'mfs size'], resultFile)
    
    originSize = 0;
    i = 1
    for file in files:

        if file.endswith("\n"):
            file = file[:-1]
        print('File {!s}/{!s} - File name: {!s}'.format(i, len(files), file))
        file = './' + file
        originSize += os.path.getsize(file)
        copy(file, outputDir)

        mfsSize = 0
        if ((i % 10) == 0) or (i == len(files)):
            mfsSize = du(minervaDir)

        writeCsv([i, originSize, mfsSize], resultFile)
        i += 1
            
        


    
    
    


