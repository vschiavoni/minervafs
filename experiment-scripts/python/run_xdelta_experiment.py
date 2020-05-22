import sys
import os
import subprocess
import csv


def du(dir, fs=None):

    res = ''
    # H: is headerles
    # p: is to get it in byte
    # fs = None assume EXT4
    if not fs == 'zfs':
        res = subprocess.check_output(['du', '-sb', dir], encoding="utf-8")
        res = (res.split('\t'))[0]
    else:
        res = subprocess.check_output(['zfs', 'get', '-Hp', 'used', 'zfs'], encoding="utf-8")
        res = (res.split('\t'))[2]
    return res


def writeCsv(row, file):

    with open(file, 'a') as csvFile:
        writer = csv.writer(csvFile)
        writer.writerow(row)


def xdelta(src, dest):
    with open(dest, 'wb') as out:
        subprocess.check_call(['xdelta3', '-e', src], stdout=out, stderr=out)
        
if __name__ == '__main__':

    fileList = sys.argv[1]
    outputDir = sys.argv[2]
    resultFile = sys.argv[3]

    if not outputDir.endswith('/'):
        outputDir = outputDir + '/'

    files = []

    with open(fileList) as filesList:
        files = filesList.readlines()


    writeCsv(['file', 'origin size', 'dest size'], resultFile)

    originSize = 0
    i = 1

    for file in files:
        print('l')
        if file.endswith('\n'):
            file = file[:-1]

        print('File {!s}/{!s} - File name: {!s}'.format(i, len(files), file))

        destName = outputDir + (os.path.basename(file))
        xdelta(file, destName)
        originSize += os.path.getsize(file)

        outSize = 0
        if ((i % 10) == 0) or (i == len(files)):
            outSize = du(outputDir)

        writeCsv([i, originSize, outSize], resultFile)
        i += 1            
        
    
