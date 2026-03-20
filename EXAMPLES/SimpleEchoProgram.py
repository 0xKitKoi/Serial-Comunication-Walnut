import sys

while True:
    line = sys.stdin.readline()
    if line.strip():  # ignore blank/whitespace-only lines
        sys.stdout.write("got: " + line)