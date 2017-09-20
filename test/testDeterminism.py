import sys
import argparse
import subprocess
import re

def run(input, count, infomapArgs):

    firstCodelength = 0.0

    for i in range(1, count + 1):
        
        print("\nStarting run {}...".format(i))

        res = subprocess.run(['./Infomap', input, infomapArgs], stdout=subprocess.PIPE)

        stdout = res.stdout.decode('utf-8')
        match = re.search('Per level codelength total:.*\(sum: (\d+\.\d+)\)', stdout)
        if not match:
            sys.exit("No match for codelength from Infomap output '{}'".format(stdout))

        codelength = float(match.group(1))
        print("****************************************************************")
        print("{}: Found codelength: {}".format(i, codelength))
        if i == 1:
            firstCodelength = codelength
        elif codelength != firstCodelength:
            print("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!")
            print("{} != {}".format(codelength, firstCodelength))
            if abs(codelength - firstCodelength) > 1e-10:
                print("Found non-deterministic behavior!")
                return 1
            print("Less than 1e-10, continue...")
        print("****************************************************************")

    print("Done! No non-deterministic behaviour found!")
    return 0


def main(argv):
    parser = argparse.ArgumentParser(description='Test determinism.')
    # parser.add_argument('input', nargs='?', default='../ninetriangles.net', help='input network (default: ninetriangles.net)')
    parser.add_argument('input', help='input network')
    parser.add_argument('-c', '--count', type=int, default=50, help='max count of iterations (deafult: 40)')

    mainArgv = argv
    infomapArgv = []
    if '--' in argv:
        mainArgv = argv[:argv.index('--')]
        infomapArgv = argv[argv.index('--')+1:]
    args = parser.parse_args(mainArgv)
    return run(args.input, args.count, infomapArgs = ' '.join(infomapArgv))

if __name__ == "__main__":
   main(sys.argv[1:])
