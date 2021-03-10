import sys

fileName = sys.argv[1]

with open(fileName) as file:
	lines = file.readlines()
	for line in lines:
		if not line.endswith(',0\r\n'):
			print(line)


