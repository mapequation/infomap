import sys
import argparse
import subprocess
import re
import time
try:
    from infomap import infomap
except ImportError:
    noLibrary = True


def runInfomapAsSubProcess(input, infomapArgs):
    res = subprocess.run(['./Infomap', input, infomapArgs], stdout=subprocess.PIPE)

    stdout = res.stdout.decode('utf-8')
    match = re.search('Per level codelength total:.*\(sum: (\d+\.\d+)\)', stdout)
    if not match:
        sys.exit("No match for codelength from Infomap output '{}'".format(stdout))

    codelength = float(match.group(1))
    return codelength

def runInfomapAsLibrary(input, infomapArgs):
    if (noLibrary):
        sys.exit("Infomap library not available")

    infomapWrapper = infomap.Infomap("{} {}".format(input, infomapArgs), True)

    infomapWrapper.run()

    return infomapWrapper.codelength()

def run(input, count, infomapArgs, asLibrary = False):

    # print("\n== Starting at {} ==".format(time.strftime("%Y-%m-%d %H:%M:%S", time.gmtime())))
    # print("Running Infomap as {}".format("library" if asLibrary else "standalone binary"))
    t0clock, t0proc = time.perf_counter(), time.process_time()

    firstCodelength = 0.0

    for i in range(1, count + 1):
        
        print("=" * 120)
        print("{}: Run Infomap ({}) on '{}' with options '{}', starting at {}...".format(i, "library" if asLibrary else "standalone", input, infomapArgs, time.strftime("%Y-%m-%d %H:%M:%S", time.gmtime())))
        t1clock, t1proc = time.perf_counter(), time.process_time()

        if asLibrary:
            codelength = runInfomapAsLibrary(input, infomapArgs)
        else:
            codelength = runInfomapAsSubProcess(input, infomapArgs)
        
        print("Finished in {:.2e}s (process time {:.2e}) with codelength: {}".format(
            time.perf_counter() - t1clock, time.process_time() - t1proc, codelength))
        if i == 1:
            firstCodelength = codelength
        elif codelength != firstCodelength:
            print("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!")
            print("Codelength differ from first codelength {} with {:.2e}".format(
                firstCodelength, codelength - firstCodelength))
            if abs(codelength - firstCodelength) > 1e-10:
                print("Found non-deterministic behavior!")
                return 1
            print("Less than 1e-10, continue...")
        print("=" * 120)
        print("")

    print("Done! No non-deterministic behaviour found!")

    print("Clock time:          {:.2e}s".format(time.perf_counter() - t0clock))
    print("Process time:        {:.2e}s".format(time.process_time() - t0proc))
    print("\n== Finished at {} ==".format(time.strftime("%Y-%m-%d %H:%M:%S", time.gmtime())))

    return 0


def main(argv):
    parser = argparse.ArgumentParser(description='Test determinism.')
    # parser.add_argument('input', nargs='?', default='../ninetriangles.net', help='input network (default: ninetriangles.net)')
    parser.add_argument('input', help='input network')
    parser.add_argument('-c', '--count', type=int, default=50, help='max count of iterations (deafult: 40)')
    parser.add_argument('--library', action='store_true', help='Test running Infomap as library instead of standalone binary')

    mainArgv = argv
    infomapArgv = []
    if '--' in argv:
        mainArgv = argv[:argv.index('--')]
        infomapArgv = argv[argv.index('--')+1:]
    args = parser.parse_args(mainArgv)
    return run(args.input, args.count, infomapArgs = ' '.join(infomapArgv), asLibrary = args.library)

if __name__ == "__main__":
   main(sys.argv[1:])
