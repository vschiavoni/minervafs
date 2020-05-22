import sys
import os


if __name__ == '__main__':

    inputDir = sys.argv[1]
    outputFile = sys.argv[2]

    if not inputDir.endswith('/'):
        inputDir = inputDir + '/'

    with open(outputFile, 'w') as output:
        for file in os.listdir(inputDir):
            output.write('{!s}\n'.format((inputDir + file)))
            

        
        
