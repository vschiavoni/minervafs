import sys
import subprocess
import os
import csv

def du(dir, fs=None):

    res = ''
    # H: is headerles
    # p: is to get it in byte
    # fs = None assume EXT4
    if not fs == 'zfs':
        result = subprocess.check_output(['du', '-sb', dir], encoding="utf-8")
        result = (result.split('\t'))[0]
    else:
        res = subprocess.check_output(['zfs', 'get', '-Hp', 'used', 'zfs'], encoding="utf-8")
        print(res)
        res = (res.split('\t'))[2]
    print(res)
    return res

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

    fs = None

    if len(sys.argv) == 6:
        fs = sys.argv[5]

    if not outputDir.endswith('/'):
        outputDir = outputDir + '/'

    files = []

    with open(fileList) as filesList:
        files = filesList.readlines()
        
    writeCsv(['file', 'origin size', 'dest size'], resultFile)

    originSize = 0
    i = 1

        
    
    for file in files:

        if file.endswith("\n"):
            file = file[:-1]
        print('File {!s}/{!s} - File name: {!s}'.format(i, len(files), file))
        file = './' + file
        originSize += os.path.getsize(file)
        copy(file, outputDir)

        fSize = 0
        if ((i % 10) == 0) or (i == len(files)):
            fSize = du(minervaDir, fs)

        writeCsv([i, originSize, fSize], resultFile)
        i += 1
            
        
